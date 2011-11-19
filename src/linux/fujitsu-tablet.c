/* Kernel driver for FSC Tablet PC buttons
 *
 * Copyright (C) 2006-2011 Robert Gerlach <khnz@gmx.de>
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
#include <linux/delay.h>
#include <linux/dmi.h>

#define MODULENAME "fujitsu-tablet"

static const struct acpi_device_id fujitsu_ids[] = {
	{ .id = "FUJ02BD" },
	{ .id = "FUJ02BF" },
	{ .id = "" }
};

struct fujitsu_config {
	bool invert_tablet_mode_bit;
	unsigned short keymap[16];
};

static struct fujitsu_config config_Lifebook_Tseries __initconst = {
	.invert_tablet_mode_bit = true,
	.keymap = {
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_SCROLLDOWN,
		KEY_SCROLLUP,
		KEY_DIRECTION,
		KEY_FN,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_BRIGHTNESS_ZERO,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_SCREENLOCK
	}
};

static struct fujitsu_config config_Lifebook_U810 __initconst = {
	.invert_tablet_mode_bit = true,
	.keymap = {
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_PROG1,
		KEY_PROG2,
		KEY_DIRECTION,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_UP,
		KEY_DOWN,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_FN,
		KEY_SCREENLOCK
	}
};

static struct fujitsu_config config_Stylistic_Tseries __initconst = {
	.invert_tablet_mode_bit = false,
	.keymap = {
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_PRINT,
		KEY_BACKSPACE,
		KEY_SPACE,
		KEY_ENTER,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_DOWN,
		KEY_UP,
		KEY_SCROLLUP,
		KEY_SCROLLDOWN,
		KEY_FN
	}
};

static struct fujitsu_config config_Stylistic_ST5xxx __initconst = {
	.invert_tablet_mode_bit = false,
	.keymap = {
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_MAIL,
		KEY_DIRECTION,
		KEY_ESC,
		KEY_ENTER,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_DOWN,
		KEY_UP,
		KEY_SCROLLUP,
		KEY_SCROLLDOWN,
		KEY_FN,
		KEY_SCREENLOCK
	}
};

static struct {						/* fujitsu_t */
	struct input_dev *idev;
	struct fujitsu_config config;
	int tablet_mode;
	unsigned long prev_keymask;

	int irq;
	int io_base;
	int io_length;
} fujitsu;

static inline u8 fujitsu_ack(void)
{
	return inb(fujitsu.io_base+2);
}

static inline u8 fujitsu_status(void)
{
	return inb(fujitsu.io_base+6);
}

static inline u8 fujitsu_read_register(const u8 addr)
{
	outb(addr, fujitsu.io_base);
	return inb(fujitsu.io_base+4);
}

static int __devinit input_fujitsu_setup(struct device *dev)
{
	struct input_dev *idev;
	int error;
	int x;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	idev->dev.parent = dev;
	idev->phys = KBUILD_MODNAME "/input0";
	idev->name = "Fujitsu tablet buttons";
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* Fujitsu Siemens Computer GmbH */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;

	idev->keycode = fujitsu.config.keymap;
	idev->keycodesize = sizeof(fujitsu.config.keymap[0]);
	idev->keycodemax = ARRAY_SIZE(fujitsu.config.keymap);

	__set_bit(EV_REP, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);

	for (x = 0; x < ARRAY_SIZE(fujitsu.config.keymap); x++)
		if (fujitsu.config.keymap[x])
			__set_bit(fujitsu.config.keymap[x], idev->keybit);

	__set_bit(EV_MSC, idev->evbit);
	__set_bit(MSC_SCAN, idev->mscbit);

	__set_bit(EV_SW, idev->evbit);
	__set_bit(SW_TABLET_MODE, idev->swbit);

	error = input_register_device(idev);
	if (error) {
		input_free_device(idev);
		return error;
	}

	fujitsu.idev = idev;
	return 0;
}

static void input_fujitsu_remove(void)
{
	if (fujitsu.idev)
		input_unregister_device(fujitsu.idev);
}

static void fujitsu_report_orientation(void)
{
	struct input_dev *idev = fujitsu.idev;
	int r = fujitsu_read_register(0xdd);

	if (r & 0x02) {
		bool tablet_mode = (r & 0x01);

		if (fujitsu.config.invert_tablet_mode_bit)
			tablet_mode = !tablet_mode;

		if (tablet_mode != fujitsu.tablet_mode) {
			fujitsu.tablet_mode = tablet_mode;
			input_report_switch(idev, SW_TABLET_MODE, tablet_mode);
			input_sync(idev);
		}
	}
}

static void fujitsu_report_key(void)
{
	unsigned long keymask;
	unsigned long changed;

	keymask  = fujitsu_read_register(0xde);
	keymask |= fujitsu_read_register(0xdf) << 8;
	keymask ^= 0xffff;

	changed = keymask ^ fujitsu.prev_keymask;

	if (changed) {
		int keycode, pressed;
		int x = 0;

		fujitsu.prev_keymask = keymask;

		x = find_first_bit(&changed, 16);
		keycode = fujitsu.config.keymap[x];
		pressed = !!(keymask & changed);

		if (pressed)
			input_event(fujitsu.idev, EV_MSC, MSC_SCAN, x);

		input_report_key(fujitsu.idev, keycode, pressed);
		input_sync(fujitsu.idev);
	}
}

static irqreturn_t fujitsu_isr(int irq, void *dev_id)
{
	if (!(fujitsu_status() & 0x01))
		return IRQ_NONE;

	fujitsu_report_orientation();
	fujitsu_report_key();
	fujitsu_ack();

	return IRQ_HANDLED;
}

static void fujitsu_busywait(void)
{
	int timeout_counter = 50;

	while ((fujitsu_status() & 0x02) && (--timeout_counter))
		msleep(20);
}

static void fujitsu_reset(void)
{
	fujitsu_ack();
	fujitsu_busywait();
}

static int __devinit fujitsu_dmi_matched(const struct dmi_system_id *dmi)
{
	printk(KERN_DEBUG MODULENAME ": %s detected\n", dmi->ident);
	memcpy(&fujitsu.config, dmi->driver_data,
			sizeof(struct fujitsu_config));
	return 1;
}

static struct dmi_system_id dmi_ids[] __initdata = {
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu Siemens P/T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu Lifebook T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook T")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic T")
		},
		.driver_data = &config_Stylistic_Tseries
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu LifeBook U810",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook U810")
		},
		.driver_data = &config_Lifebook_U810
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "STYLISTIC ST5")
		},
		.driver_data = &config_Stylistic_ST5xxx
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic ST5")
		},
		.driver_data = &config_Stylistic_ST5xxx
	},
	{
		.callback = fujitsu_dmi_matched,
		.ident = "Unknown (using defaults)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, ""),
			DMI_MATCH(DMI_PRODUCT_NAME, "")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{ NULL }
};

static acpi_status __devinit fujitsu_walk_resources(struct acpi_resource *res, void *data)
{
	switch(res->type) {
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
	if (ACPI_FAILURE(status))
		return -ENODEV;

	if (!fujitsu.irq || !fujitsu.io_base)
		return -ENODEV;

	error = input_fujitsu_setup(&adev->dev);
	if (error)
		return error;

	if (!request_region(fujitsu.io_base, fujitsu.io_length, MODULENAME)) {
		dev_err(&adev->dev, "region 0x%04x busy\n", fujitsu.io_base);
		input_fujitsu_remove();
		return -EBUSY;
	}

	fujitsu_reset();

	fujitsu_report_orientation();
	input_sync(fujitsu.idev);

	error = request_irq(fujitsu.irq, fujitsu_isr,
			IRQF_SHARED, MODULENAME, fujitsu_isr);
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
	free_irq(fujitsu.irq, fujitsu_isr);
	release_region(fujitsu.io_base, fujitsu.io_length);
	input_fujitsu_remove();
	return 0;
}

static int acpi_fujitsu_resume(struct acpi_device *adev)
{
	fujitsu_reset();
	fujitsu_report_orientation();
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
MODULE_DESCRIPTION("Fujitsu Siemens tablet button driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("git");

MODULE_DEVICE_TABLE(acpi, fujitsu_ids);
