/* Kernel driver for FSC Tablet PC buttons
 *
 * Copyright (C) 2006-2010 Robert Gerlach <khnz@gmx.de>
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/dmi.h>

#define MODULENAME "fujitsu-tablet"

#define INTERRUPT 5
#define IO_BASE 0xfd70

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
		KEY_F13
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
		KEY_SLEEP
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
		KEY_F13
	}
};

static struct {						/* fujitsu_t */
	struct platform_device *pdev;
	struct input_dev *idev;
	struct fujitsu_config config;
	int tablet_mode;
	unsigned long prev_keymask;
} fujitsu;

/*** HELPER *******************************************************************/

static inline u8 fujitsu_ack(void)
{
	return inb(IO_BASE+2);
}

static inline u8 fujitsu_status(void)
{
	return inb(IO_BASE+6);
}

static inline u8 fujitsu_read_register(const u8 addr)
{
	outb(addr, IO_BASE);
	return inb(IO_BASE+4);
}


/*** INPUT ********************************************************************/

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

		/* save current state and filter not changed bits */
		fujitsu.prev_keymask = keymask;

		/* looking for the location of the first bit which is set */
		x = find_first_bit(&changed, 16);

		keycode = fujitsu.config.keymap[x];
		pressed = !!(keymask & changed);

		if (pressed)
			input_event(fujitsu.idev, EV_MSC, MSC_SCAN, x);

		input_report_key(fujitsu.idev, keycode, pressed);
		input_sync(fujitsu.idev);
	}
}


/*** INTERRUPT ****************************************************************/

static irqreturn_t fujitsu_isr(int irq, void *dev_id)
{
	if (!(fujitsu_status() & 0x01))
		return IRQ_NONE;

	fujitsu_report_orientation();
	fujitsu_report_key();
	fujitsu_ack();

	return IRQ_HANDLED;
}


/*** DEVICE *******************************************************************/

static int fujitsu_busywait(void)
{
	int timeout_counter = 50;

	while (fujitsu_status() & 0x02 && --timeout_counter)
		msleep(20);

	return !timeout_counter;
}

static void fujitsu_reset(void)
{
	fujitsu_ack();
	if (fujitsu_busywait())
		printk(KERN_WARNING MODULENAME ": timeout, real reset needed!\n");
}

static int __devinit fujitsu_probe(struct platform_device *pdev)
{
	int error;

	error = input_fujitsu_setup(&pdev->dev);
	if (error)
		goto err_input;

	if (!request_region(IO_BASE, 8, MODULENAME)) {
		dev_err(&pdev->dev, "region 0x%04x busy\n", IO_BASE);
		error = -EBUSY;
		goto err_input;
	}

	fujitsu_reset();

	fujitsu_report_orientation();
	input_sync(fujitsu.idev);

	error = request_irq(INTERRUPT, fujitsu_isr,
			IRQF_SHARED, MODULENAME, fujitsu_isr);
	if (error) {
		dev_err(&pdev->dev, "unable to get irq %d\n", INTERRUPT);
		goto err_io;
	}

	return 0;

err_io:
	release_region(IO_BASE, 8);
err_input:
	input_fujitsu_remove();
	return error;
}

static int __devexit fujitsu_remove(struct platform_device *pdev)
{
	free_irq(INTERRUPT, fujitsu_isr);
	release_region(IO_BASE, 8);
	input_fujitsu_remove();
	return 0;
}

#ifdef CONFIG_PM
static int fujitsu_resume(struct platform_device *pdev)
{
	fujitsu_reset();
	fujitsu_report_orientation();
	return 0;
}
#else
#define fujitsu_resume NULL
#endif

static struct platform_driver fujitsu_platform_driver = {
	.driver		= {
		.name	= MODULENAME,
		.owner	= THIS_MODULE,
	},
	.probe		= fujitsu_probe,
	.remove		= __devexit_p(fujitsu_remove),
	.resume		= fujitsu_resume,
};


/*** DMI **********************************************************************/

static int __init fujitsu_dmi_matched(const struct dmi_system_id *dmi)
{
	printk(KERN_INFO MODULENAME ": %s detected\n", dmi->ident);
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


/*** MODULE *******************************************************************/

static int __init fujitsu_module_init(void)
{
	int error;

	dmi_check_system(dmi_ids);

	error = platform_driver_register(&fujitsu_platform_driver);
	if (error)
		return error;

	fujitsu.pdev = platform_device_register_simple(MODULENAME, -1, NULL, 0);
	if (IS_ERR(fujitsu.pdev)) {
		error = PTR_ERR(fujitsu.pdev);
		platform_driver_unregister(&fujitsu_platform_driver);
		return error;
	}

	return 0;
}

static void __exit fujitsu_module_exit(void)
{
	platform_device_unregister(fujitsu.pdev);
	platform_driver_unregister(&fujitsu_platform_driver);
}

module_init(fujitsu_module_init);
module_exit(fujitsu_module_exit);

MODULE_AUTHOR("Robert Gerlach <khnz@gmx.de>");
MODULE_DESCRIPTION("Fujitsu Siemens tablet button driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("git");

MODULE_DEVICE_TABLE(acpi, fujitsu_ids);
