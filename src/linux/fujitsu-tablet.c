/*
 * Copyright (C) 2006-2012 Robert Gerlach <khnz@gmx.de>
 * Copyright (C) 2005-2006 Jan Rychter <jan@rychter.com>
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/delay.h>
#include <linux/dmi.h>

#define MODULENAME "fujitsu-tablet"

#define ACPI_FUJITSU_CLASS "fujitsu"

#define INVERT_TABLET_MODE_BIT      0x01
#define FORCE_TABLET_MODE_IF_UNDOCK 0x02

static const struct acpi_device_id fujitsu_ids[] = {
	{ .id = "FUJ02BD" },
	{ .id = "FUJ02BF" },
	{ .id = "" }
};

struct fujitsu_config {
	struct key_entry *keymap;
	unsigned int quirks;
};

static struct key_entry keymap_Lifebook_Tseries[] __initconst = {
	{ KE_KEY, 0x0010, { KEY_SCROLLDOWN } },
	{ KE_KEY, 0x0020, { KEY_SCROLLUP } },
	{ KE_KEY, 0x0040, { KEY_DIRECTION } },
	{ KE_KEY, 0x0080, { KEY_LEFTCTRL } },
	{ KE_KEY, 0x0100, { KEY_BRIGHTNESSUP } },
	{ KE_KEY, 0x0200, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY, 0x8000, { KEY_LEFTALT } },
	{ KE_END }
};

static struct key_entry keymap_Lifebook_U810[] __initconst = {
	{ KE_KEY, 0x0010, { KEY_PROG1 } },
	{ KE_KEY, 0x0020, { KEY_PROG2 } },
	{ KE_KEY, 0x0040, { KEY_DIRECTION } },
	{ KE_KEY, 0x0400, { KEY_UP } },
	{ KE_KEY, 0x0800, { KEY_DOWN } },
	{ KE_KEY, 0x4000, { KEY_LEFTCTRL } },
	{ KE_KEY, 0x8000, { KEY_LEFTALT } },
	{ KE_END }
};

static struct key_entry keymap_Stylistic_Tseries[] __initconst = {
	{ KE_KEY, 0x0010, { KEY_PRINT } },
	{ KE_KEY, 0x0020, { KEY_BACKSPACE } },
	{ KE_KEY, 0x0040, { KEY_SPACE } },
	{ KE_KEY, 0x0080, { KEY_ENTER } },
	{ KE_KEY, 0x0100, { KEY_BRIGHTNESSUP } },
	{ KE_KEY, 0x0200, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY, 0x0400, { KEY_DOWN } },
	{ KE_KEY, 0x0800, { KEY_UP } },
	{ KE_KEY, 0x1000, { KEY_SCROLLUP } },
	{ KE_KEY, 0x2000, { KEY_SCROLLDOWN } },
	{ KE_KEY, 0x4000, { KEY_LEFTCTRL } },
	{ KE_KEY, 0x8000, { KEY_LEFTALT } },
	{ KE_END }
};

static struct key_entry keymap_Stylistic_ST5xxx[] __initconst = {
	{ KE_KEY, 0x0010, { KEY_MAIL } },
	{ KE_KEY, 0x0020, { KEY_DIRECTION } },
	{ KE_KEY, 0x0040, { KEY_ESC } },
	{ KE_KEY, 0x0080, { KEY_ENTER } },
	{ KE_KEY, 0x0100, { KEY_BRIGHTNESSUP } },
	{ KE_KEY, 0x0200, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY, 0x0400, { KEY_DOWN } },
	{ KE_KEY, 0x0800, { KEY_UP } },
	{ KE_KEY, 0x1000, { KEY_SCROLLUP } },
	{ KE_KEY, 0x2000, { KEY_SCROLLDOWN } },
	{ KE_KEY, 0x4000, { KEY_LEFTCTRL } },
	{ KE_KEY, 0x8000, { KEY_LEFTALT } },
	{ KE_END }
};

static struct {						/* fujitsu_t */
	struct input_dev *idev;
	struct fujitsu_config config;
	unsigned long prev_keymask;

	char phys[21];

	int irq;
	int io_base;
	int io_length;
} fujitsu;

static inline u8 fujitsu_ack(void)
{
	return inb(fujitsu.io_base + 2);
}

static inline u8 fujitsu_status(void)
{
	return inb(fujitsu.io_base + 6);
}

static inline u8 fujitsu_read_register(const u8 addr)
{
	outb(addr, fujitsu.io_base);
	return inb(fujitsu.io_base + 4);
}

static void fujitsu_send_state(void)
{
	int state;
	int dock, tablet_mode;

	state = fujitsu_read_register(0xdd);

	dock = !!(state & 0x02);

	if ((fujitsu.config.quirks & FORCE_TABLET_MODE_IF_UNDOCK) && (!dock)) {
		tablet_mode = 1;
	} else{
		tablet_mode = state & 0x01;
		if (fujitsu.config.quirks & INVERT_TABLET_MODE_BIT)
			tablet_mode = !tablet_mode;
	}

	input_report_switch(fujitsu.idev, SW_DOCK, dock);
	input_report_switch(fujitsu.idev, SW_TABLET_MODE, tablet_mode);
	input_sync(fujitsu.idev);
}

static void fujitsu_reset(void)
{
	int timeout = 50;

	fujitsu_ack();

	while ((fujitsu_status() & 0x02) && (--timeout))
		msleep(20);

	printk(KERN_DEBUG MODULENAME ": fujitsu_reset: time left: %dx 20ms", timeout);

	fujitsu_send_state();
}

static int __devinit input_fujitsu_setup(struct device *parent,
					 const char *name, const char *phys)
{
	struct input_dev *idev;
	int error;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	idev->dev.parent = parent;
	idev->phys = phys;
	idev->name = name;
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* Fujitsu Siemens Computer GmbH */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;

	__set_bit(EV_REP, idev->evbit);

	error = sparse_keymap_setup(idev, fujitsu.config.keymap, NULL);
	if (error)
		goto err_free_dev;

	input_set_capability(idev, EV_SW, SW_DOCK);
	input_set_capability(idev, EV_SW, SW_TABLET_MODE);

	error = input_register_device(idev);
	if (error)
		goto err_free_keymap;

	fujitsu.config.keymap = NULL;

	fujitsu.idev = idev;
	return 0;

err_free_keymap:
	sparse_keymap_free(idev);
err_free_dev:
	input_free_device(idev);
	return error;
}

static void input_fujitsu_remove(void)
{
	sparse_keymap_free(fujitsu.idev);
	input_unregister_device(fujitsu.idev);
}

static irqreturn_t fujitsu_interrupt(int irq, void *dev_id)
{
	unsigned int keymask;
	unsigned int changed;

	if (unlikely(!(fujitsu_status() & 0x01)))
		return IRQ_NONE;

	fujitsu_send_state();

	keymask  = fujitsu_read_register(0xde);
	keymask |= fujitsu_read_register(0xdf) << 8;
	keymask ^= 0xffff;

	printk(KERN_DEBUG MODULENAME ": state=0x%02x keymask=0x%04x\n",
			fujitsu_read_register(0xdd), keymask);

	changed = keymask ^ fujitsu.prev_keymask;
	if (changed) {
		unsigned int value = !!(keymask & changed);

		fujitsu.prev_keymask = keymask;

		printk(KERN_DEBUG MODULENAME ": code=0x%02x value=0x%02x\n",
				changed, value);

		sparse_keymap_report_event(fujitsu.idev, changed, value, 0);
	}

	fujitsu_ack();
	return IRQ_HANDLED;
}

static int __devinit fujitsu_dmi_default(const struct dmi_system_id *dmi)
{
	printk(KERN_DEBUG MODULENAME ": %s\n", dmi->ident);
	fujitsu.config.keymap = dmi->driver_data;
	return 1;
}

static int __devinit fujitsu_dmi_stylistic(const struct dmi_system_id *dmi)
{
	fujitsu_dmi_default(dmi);
	fujitsu.config.quirks |= FORCE_TABLET_MODE_IF_UNDOCK;
	fujitsu.config.quirks |= INVERT_TABLET_MODE_BIT;
	return 1;
}

static struct dmi_system_id dmi_ids[] __initconst = {
	{
		.callback = fujitsu_dmi_default,
		.ident = "Fujitsu Siemens P/T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK")
		},
		.driver_data = &keymap_Lifebook_Tseries
	},
	{
		.callback = fujitsu_dmi_default,
		.ident = "Fujitsu Lifebook T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook T")
		},
		.driver_data = &keymap_Lifebook_Tseries
	},
	{
		.callback = fujitsu_dmi_stylistic,
		.ident = "Fujitsu Siemens Stylistic T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic T")
		},
		.driver_data = &keymap_Stylistic_Tseries
	},
	{
		.callback = fujitsu_dmi_default,
		.ident = "Fujitsu LifeBook U810",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook U810")
		},
		.driver_data = &keymap_Lifebook_U810
	},
	{
		.callback = fujitsu_dmi_stylistic,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "STYLISTIC ST5")
		},
		.driver_data = &keymap_Stylistic_ST5xxx
	},
	{
		.callback = fujitsu_dmi_stylistic,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic ST5")
		},
		.driver_data = &keymap_Stylistic_ST5xxx
	},
	{
		.callback = fujitsu_dmi_default,
		.ident = "Unknown (using defaults)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, ""),
			DMI_MATCH(DMI_PRODUCT_NAME, "")
		},
		.driver_data = &keymap_Lifebook_Tseries
	},
	{ NULL }
};

static acpi_status __devinit
fujitsu_walk_resources(struct acpi_resource *res, void *data)
{
	switch (res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			fujitsu.irq = res->data.irq.interrupts[0];
			return AE_OK;

		case ACPI_RESOURCE_TYPE_IO:
			fujitsu.io_base = res->data.io.minimum;
			fujitsu.io_length = res->data.io.address_length;
			return AE_OK;

		case ACPI_RESOURCE_TYPE_END_TAG:
			if (fujitsu.irq && fujitsu.io_base)
				return AE_OK;
			else
				return AE_NOT_FOUND;

		default:
			return AE_ERROR;
	}
}

static int __devinit acpi_fujitsu_add(struct acpi_device *adev)
{
	acpi_status status;
	int error;

	if (!adev)
		return -EINVAL;

	status = acpi_walk_resources(adev->handle, METHOD_NAME__CRS,
			fujitsu_walk_resources, NULL);
	if (ACPI_FAILURE(status) || !fujitsu.irq || !fujitsu.io_base)
		return -ENODEV;

	sprintf(acpi_device_name(adev), "Fujitsu %s", acpi_device_hid(adev));
	sprintf(acpi_device_class(adev), "%s", ACPI_FUJITSU_CLASS);

	snprintf(fujitsu.phys, sizeof(fujitsu.phys),
			"%s/video/input0", acpi_device_hid(adev));

	error = input_fujitsu_setup(&adev->dev,
		acpi_device_name(adev), fujitsu.phys);
	if (error)
		return error;

	if (!request_region(fujitsu.io_base, fujitsu.io_length, MODULENAME)) {
		dev_err(&adev->dev, "region 0x%04x-0x%04x busy\n", fujitsu.io_base, fujitsu.io_base+fujitsu.io_length);
		input_fujitsu_remove();
		return -EBUSY;
	}

	fujitsu_reset();

	error = request_irq(fujitsu.irq, fujitsu_interrupt,
			IRQF_SHARED, MODULENAME, fujitsu_interrupt);
	if (error) {
		dev_err(&adev->dev, "unable to get irq %d\n", fujitsu.irq);
		release_region(fujitsu.io_base, fujitsu.io_length);
		input_fujitsu_remove();
		return error;
	}

	return 0;
}

static int __devexit acpi_fujitsu_remove(struct acpi_device *adev, int type)
{
	free_irq(fujitsu.irq, fujitsu_interrupt);
	release_region(fujitsu.io_base, fujitsu.io_length);
	input_fujitsu_remove();
	return 0;
}

static int acpi_fujitsu_resume(struct acpi_device *adev)
{
	fujitsu_reset();
	return 0;
}

static struct acpi_driver acpi_fujitsu_driver = {
	.name  = MODULENAME,
	.class = "hotkey",
	.ids   = fujitsu_ids,
	.ops   = {
		.add    = acpi_fujitsu_add,
		.remove	= acpi_fujitsu_remove,
		.resume = acpi_fujitsu_resume,
	}
};

static int __init fujitsu_module_init(void)
{
	int error;

	dmi_check_system(dmi_ids);

	error = acpi_bus_register_driver(&acpi_fujitsu_driver);
	if (ACPI_FAILURE(error))
		return error;

	return 0;
}

static void __exit fujitsu_module_exit(void)
{
	acpi_bus_unregister_driver(&acpi_fujitsu_driver);
}

module_init(fujitsu_module_init);
module_exit(fujitsu_module_exit);

MODULE_AUTHOR("Robert Gerlach <khnz@gmx.de>");
MODULE_DESCRIPTION("Fujitsu tablet pc extras driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("git");

MODULE_DEVICE_TABLE(acpi, fujitsu_ids);
