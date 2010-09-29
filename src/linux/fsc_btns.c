/* Kernel driver for FSC Tablet PC buttons
 *
 * Copyright (C) 2006-2010 Robert Gerlach <khnz@users.sourceforge.net>
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
#include <linux/moduleparam.h>
#include <linux/dmi.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#define MODULENAME "fsc_btns"

#define REPEAT_DELAY 700
#define REPEAT_RATE 16

static const struct acpi_device_id fscbtns_ids[] = {
	{ .id = "FUJ02BD" },
	{ .id = "FUJ02BF" },
	{ .id = "" }
};

struct fscbtns_config {
	int invert_orientation_bit;
	unsigned short keymap[16];
};

static struct fscbtns_config config_Lifebook_Tseries __initconst = {
	.invert_orientation_bit = 1,
	.keymap = {
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_SCROLLDOWN,
		KEY_SCROLLUP,
		KEY_DIRECTION,
		KEY_LEFTCTRL,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_BRIGHTNESS_ZERO,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_RESERVED,
		KEY_LEFTALT
	}
};

static struct fscbtns_config config_Lifebook_U810 __initconst = {
	.invert_orientation_bit = 1,
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
		KEY_SLEEP
	}
};

static struct fscbtns_config config_Stylistic_Tseries __initconst = {
	.invert_orientation_bit = 0,
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

static struct fscbtns_config config_Stylistic_ST5xxx __initconst = {
	.invert_orientation_bit = 0,
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
		KEY_LEFTALT
	}
};

static struct {						/* fscbtns_t */
	struct platform_device *pdev;
	struct input_dev *idev;

	unsigned int interrupt;
	unsigned int address;
	struct fscbtns_config *config;

	int orientation;
} fscbtns;


/*** HELPER *******************************************************************/

static inline u8 fscbtns_ack(void)
{
	return inb(fscbtns.address+2);
}

static inline u8 fscbtns_status(void)
{
	return inb(fscbtns.address+6);
}

static inline u8 fscbtns_read_register(const u8 addr)
{
	outb(addr, fscbtns.address);
	return inb(fscbtns.address+4);
}


/*** INPUT ********************************************************************/

static int __devinit input_fscbtns_setup(struct device *dev)
{
	struct input_dev *idev;
	int error;
	int x;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	idev->dev.parent = dev;
	idev->phys = "fsc/input0";
	idev->name = "fsc tablet buttons";
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* "Fujitsu Siemens Computer GmbH" from pci.ids */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;

	idev->keycode = fscbtns.config->keymap;
	idev->keycodesize = sizeof(fscbtns.config->keymap[0]);
	idev->keycodemax = ARRAY_SIZE(fscbtns.config->keymap);

	__set_bit(EV_REP, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);

	for (x = 0; x < ARRAY_SIZE(fscbtns.config->keymap); x++)
		if (fscbtns.config->keymap[x])
			__set_bit(fscbtns.config->keymap[x], idev->keybit);

	__set_bit(EV_MSC, idev->evbit);
	__set_bit(MSC_SCAN, idev->mscbit);

	__set_bit(EV_SW, idev->evbit);
	__set_bit(SW_TABLET_MODE, idev->swbit);

	error = input_register_device(idev);
	if (error) {
		input_free_device(idev);
		return error;
	}

	idev->rep[REP_DELAY]  = REPEAT_DELAY;
	idev->rep[REP_PERIOD] = 1000 / REPEAT_RATE;

	fscbtns.idev = idev;
	return 0;
}

static void input_fscbtns_remove(void)
{
	if (fscbtns.idev)
		input_unregister_device(fscbtns.idev);
}

static void fscbtns_report_orientation(void)
{
	struct input_dev *idev = fscbtns.idev;

	int orientation = fscbtns_read_register(0xdd);

	if (orientation & 0x02) {
		orientation ^= fscbtns.config->invert_orientation_bit;
		orientation &= 0x01;

		if (orientation != fscbtns.orientation) {
			input_report_switch(idev, SW_TABLET_MODE,
					fscbtns.orientation = orientation);
			input_sync(idev);
		}
	}
}

static void fscbtns_report_key(void)
{
	static unsigned long prev_keymask = 0;

	unsigned long keymask;
	unsigned long changed;

	keymask  = fscbtns_read_register(0xde);
	keymask |= fscbtns_read_register(0xdf) << 8;
	keymask ^= 0xffff;

	changed = keymask ^ prev_keymask;

	if (changed) {
		int keycode, pressed;
		int x = 0;

		/* save current state and filter not changed bits */
		prev_keymask = keymask;

		/* get number of changed bit */
		while(!test_bit(x, &changed))
			x++;

		keycode = fscbtns.config->keymap[x];
		pressed = !!(keymask & changed);

		if (keycode != KEY_RESERVED) {
			if (pressed)
				input_event(fscbtns.idev, EV_MSC, MSC_SCAN, x);

			input_report_key(fscbtns.idev, keycode, pressed);
		}
	}
}

static void fscbtns_event(void)
{

	fscbtns_report_orientation();
	fscbtns_report_key();
	input_sync(fscbtns.idev);
}


/*** INTERRUPT ****************************************************************/

static void fscbtns_isr_do(struct work_struct *work)
{
	fscbtns_event();
	fscbtns_ack();
}

static DECLARE_WORK(isr_wq, fscbtns_isr_do);

static irqreturn_t fscbtns_isr(int irq, void *dev_id)
{
	if (!(fscbtns_status() & 0x01))
		return IRQ_NONE;

	schedule_work(&isr_wq);
	return IRQ_HANDLED;
}


/*** DEVICE *******************************************************************/

static int fscbtns_busywait(void)
{
	int timeout_counter = 100;

	while(fscbtns_status() & 0x02 && --timeout_counter)
		msleep(10);

	return !timeout_counter;
}

static void fscbtns_reset(void)
{
	fscbtns_ack();
	if (fscbtns_busywait())
		printk(KERN_WARNING MODULENAME ": timeout, real reset needed!\n");
}

static int __devinit fscbtns_probe(struct platform_device *pdev)
{
	int error;

	error = input_fscbtns_setup(&pdev->dev);
	if (error)
		goto err_input;

	if (!request_region(fscbtns.address, 8, MODULENAME)) {
		printk(KERN_ERR MODULENAME ": region 0x%04x busy\n", fscbtns.address);
		error = -EBUSY;
		goto err_input;
	}

	fscbtns_reset();

	fscbtns_report_orientation();
	input_sync(fscbtns.idev);

	error = request_irq(fscbtns.interrupt, fscbtns_isr,
			IRQF_SHARED, MODULENAME, fscbtns_isr);
	if (error) {
		printk(KERN_ERR MODULENAME ": unable to get irq %d\n", fscbtns.interrupt);
		goto err_io;
	}

	return 0;

err_io:
	release_region(fscbtns.address, 8);
err_input:
	input_fscbtns_remove();
	return error;
}

static int __devexit fscbtns_remove(struct platform_device *pdev)
{
	free_irq(fscbtns.interrupt, fscbtns_isr);
	release_region(fscbtns.address, 8);
	input_fscbtns_remove();
	return 0;
}

#ifdef CONFIG_PM
static int fscbtns_resume(struct platform_device *pdev)
{
	fscbtns_reset();
#if 0 // because Xorg Bug #9623 (SEGV at resume if display was rotated)
	fscbtns_report_orientation();
	input_sync(fscbtns.idev);
#endif
	return 0;
}
#else
#define fscbtns_resume NULL
#endif

static struct platform_driver fscbtns_platform_driver = {
	.driver		= {
		.name	= MODULENAME,
		.owner	= THIS_MODULE,
	},
	.probe		= fscbtns_probe,
	.remove		= __devexit_p(fscbtns_remove),
	.resume		= fscbtns_resume,
};


/*** ACPI *********************************************************************/

static acpi_status fscbtns_walk_resources(struct acpi_resource *res, void *data)
{
	switch(res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			fscbtns.interrupt = res->data.irq.interrupts[0];
			return AE_OK;

		case ACPI_RESOURCE_TYPE_IO:
			fscbtns.address = res->data.io.minimum;
			return AE_OK;

		case ACPI_RESOURCE_TYPE_END_TAG:
			if (fscbtns.interrupt && fscbtns.address)
				return AE_OK;
			else
				return AE_NOT_FOUND;

		default:
			return AE_ERROR;
	}
}

static int acpi_fscbtns_add(struct acpi_device *adev)
{
	acpi_status status;
	int error;

	if (!adev)
		return -EINVAL;

	status = acpi_walk_resources(adev->handle, METHOD_NAME__CRS,
			fscbtns_walk_resources, NULL);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	if (!fscbtns.interrupt || !fscbtns.address) {
		return -ENODEV;
	}

	printk(KERN_INFO MODULENAME ": found %s at io 0x%04x irq %d\n",
			acpi_device_hid(adev),
			fscbtns.address, fscbtns.interrupt);

	error = platform_driver_register(&fscbtns_platform_driver);
	if (error)
		return error;

	fscbtns.pdev = platform_device_register_simple(MODULENAME, -1, NULL, 0);
	if (IS_ERR(fscbtns.pdev)) {
		error = PTR_ERR(fscbtns.pdev);
		platform_driver_unregister(&fscbtns_platform_driver);
		return error;
	}

	return 0;
}

static int acpi_fscbtns_remove(struct acpi_device *adev, int type)
{
	platform_device_unregister(fscbtns.pdev);
	platform_driver_unregister(&fscbtns_platform_driver);
	return 0;
}

static struct acpi_driver acpi_fscbtns_driver = {
	.name  = MODULENAME,
	.class = "hotkey",
	.ids   = fscbtns_ids,
	.ops   = {
		.add    = acpi_fscbtns_add,
		.remove = acpi_fscbtns_remove
	}
};


/*** DMI **********************************************************************/

static int __init fscbtns_dmi_matched(const struct dmi_system_id *dmi)
{
	printk(KERN_INFO MODULENAME ": %s detected\n", dmi->ident);
	fscbtns.config = (struct fscbtns_config*) dmi->driver_data;
	return 1;
}

static struct dmi_system_id dmi_ids[] __initdata = {
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens P/T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Lifebook T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook T")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic T")
		},
		.driver_data = &config_Stylistic_Tseries
	},
 	{
 		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu LifeBook U810",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook U810")
		},
		.driver_data = &config_Lifebook_U810
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "STYLISTIC ST5")
		},
		.driver_data = &config_Stylistic_ST5xxx
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic ST5")
		},
		.driver_data = &config_Stylistic_ST5xxx
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Unknown (using defaults)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, ""),
			DMI_MATCH(DMI_PRODUCT_NAME, "")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{ NULL }
};


/*** MODULE *******************************************************************/

static int __init fscbtns_module_init(void)
{
	int error;

	dmi_check_system(dmi_ids);

	error = acpi_bus_register_driver(&acpi_fscbtns_driver);
	if (ACPI_FAILURE(error)) {
		return error;
	}

	return 0;
}

static void __exit fscbtns_module_exit(void)
{
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
}

module_init(fscbtns_module_init);
module_exit(fscbtns_module_exit);

MODULE_AUTHOR("Robert Gerlach <khnz@users.sourceforge.net>");
MODULE_DESCRIPTION("Fujitsu Siemens tablet button driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.1.2");

MODULE_DEVICE_TABLE(acpi, fscbtns_ids);
