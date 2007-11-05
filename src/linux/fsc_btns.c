/* Kernel driver for FSC Tablet PC buttons
 *
 * Copyright (C) 2006-2007 Robert Gerlach <khnz@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include "../../config.h"
#else
#  define DEBUG
#  define REPEAT_DELAY 700
#  define REPEAT_RATE 16
#  define STICKY_TIMEOUT 1400
#  undef  ANNOYING_FEATURES
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/dmi.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/ioport.h>
#ifdef CONFIG_ACPI
#  include <linux/acpi.h>
#endif
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#define MODULENAME "fsc_btns"

MODULE_AUTHOR("Robert Gerlach <khnz@users.sourceforge.net>");
MODULE_DESCRIPTION("Fujitsu Siemens tablet button driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.90");

#if defined CONFIG_ACPI && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
static const struct acpi_device_id fscbtns_ids[] = {
	{ .id = "FUJ02BD" },
	{ .id = "FUJ02BF" },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(acpi, fscbtns_ids);
#else
static struct pnp_device_id fscbtns_ids[] __initdata = {
	{ .id = "FUJ02bd" },
	{ .id = "FUJ02bf" },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(pnp, fscbtns_ids);
#endif


#ifndef KEY_BRIGHTNESS_ZERO
#define KEY_BRIGHTNESS_ZERO 244
#endif

#if defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)
static const unsigned long modification_mask[NBITS(KEY_MAX)] = {
		[LONG(KEY_LEFTSHIFT)]	= BIT(KEY_LEFTSHIFT),
		[LONG(KEY_RIGHTSHIFT)]	= BIT(KEY_RIGHTSHIFT),
		[LONG(KEY_LEFTCTRL)]	= BIT(KEY_LEFTCTRL),
		[LONG(KEY_RIGHTCTRL)]	= BIT(KEY_RIGHTCTRL),
		[LONG(KEY_LEFTALT)]	= BIT(KEY_LEFTALT),
		[LONG(KEY_RIGHTALT)]	= BIT(KEY_RIGHTALT),
		[LONG(KEY_LEFTMETA)]	= BIT(KEY_LEFTMETA),
		[LONG(KEY_RIGHTMETA)]	= BIT(KEY_RIGHTMETA),
		[LONG(KEY_COMPOSE)]	= BIT(KEY_COMPOSE),
		[LONG(KEY_LEFTALT)]	= BIT(KEY_LEFTALT),
		[LONG(KEY_FN)]		= BIT(KEY_FN)};
#endif

struct fscbtns_config {
	int invert_orientation_bit;
#ifdef ANNOYING_FEATURES
	unsigned int keymap[32];
#else
	unsigned int keymap[16];
#endif
};

static struct fscbtns_config config_Lifebook_Tseries __initdata = {
	.invert_orientation_bit = 1,
	.keymap = {
		0,
		0,
		0,
		0,
		KEY_SCROLLDOWN,
		KEY_SCROLLUP,
		KEY_DIRECTION,
		KEY_FN,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_BRIGHTNESS_ZERO,
		0,
		0,
		0,
		0,
		KEY_LEFTALT,
#ifdef ANNOYING_FEATURES
		0,
		0,
		0,
		0,
		KEY_PROG1,
		KEY_PROG2,
		KEY_PROG3,
		KEY_PROG4,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		KEY_ENTER
#endif
	}
};

static struct fscbtns_config config_Stylistic_Tseries __initdata = {
	.invert_orientation_bit = 0,
	.keymap = {
		0,
		0,
		0,
		0,
		KEY_PRINT,
		KEY_BACKSPACE,
		KEY_SPACE,
		KEY_ENTER,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_DOWN,
		KEY_UP,
		KEY_PAGEUP,
		KEY_PAGEDOWN,
		KEY_FN,
		KEY_LEFTALT,
#ifdef ANNOYING_FEATURES
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0
#endif
	}
};

static struct fscbtns_config config_Stylistic_ST5xxx __initdata = {
	.invert_orientation_bit = 0,
	.keymap = {
		0,
		0,
		0,
		0,
		KEY_MAIL,
		KEY_DIRECTION,
		KEY_ESC,
		KEY_ENTER,
		KEY_BRIGHTNESSUP,
		KEY_BRIGHTNESSDOWN,
		KEY_DOWN,
		KEY_UP,
		KEY_PAGEUP,
		KEY_PAGEDOWN,
		KEY_FN,
		KEY_LEFTALT,
#ifdef ANNOYING_FEATURES
		0,
		0,
		0,
		0,
		KEY_PROG1,
		KEY_PROG2,
		KEY_PROG3,
		KEY_PROG4,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
#endif
	}
};

static struct {						/* fscbtns_t */
	struct platform_device *pdev;
	struct input_dev *idev;
#if (defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)) || defined(ANNOYING_FEATURES)
	struct timer_list timer;
#endif
#ifdef ANNOYING_FEATURES
	unsigned long lp_timer_start;
#endif

	unsigned int interrupt;
	unsigned int address;

	struct fscbtns_config config;
	int orientation;
} fscbtns = {
#ifndef CONFIG_ACPI
	/* XXX: is this always true ??? */
	.interrupt = 5,
	.address = 0xfd70
#endif
};

module_param_named(irq, fscbtns.interrupt, uint, 0);
MODULE_PARM_DESC(irq, "interrupt");

module_param_named(io, fscbtns.address, uint, 0);
MODULE_PARM_DESC(io, "io base address");

static unsigned int user_model;
module_param_named(model, user_model, uint, 0);
MODULE_PARM_DESC(model, "model (1 = Stylistic, 2 = T- and P-Series, 3 = Stylistic ST5xxx)");


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

static inline void fscbtns_use_config(struct fscbtns_config *config)
{
	memcpy(&fscbtns.config, config, sizeof(struct fscbtns_config));
}


/*** INPUT ********************************************************************/

static int __devinit input_fscbtns_setup(struct device *dev)
{
	struct input_dev *idev;
	int error;
	int x;

	fscbtns.idev = idev = input_allocate_device();
	if(!idev)
		return -ENOMEM;

	idev->dev.parent = dev;
	idev->cdev.dev = dev;

	idev->phys = "fsc/input0";
	idev->name = "fsc tablet buttons";
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* "Fujitsu Siemens Computer GmbH" from pci.ids */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;

	idev->keycode = fscbtns.config.keymap;
	idev->keycodesize = sizeof(unsigned int);
	idev->keycodemax = ARRAY_SIZE(fscbtns.config.keymap);

#ifdef REPEAT_RATE
	set_bit(EV_REP, idev->evbit);
#endif
	set_bit(EV_KEY, idev->evbit);
	for(x = 0; x < ARRAY_SIZE(fscbtns.config.keymap); x++)
		set_bit(fscbtns.config.keymap[x], idev->keybit);

	set_bit(EV_MSC, idev->evbit);
	set_bit(MSC_SCAN, idev->mscbit);

	set_bit(EV_SW, idev->evbit);
	set_bit(SW_TABLET_MODE, idev->swbit);

	error = input_register_device(idev);
	if(error) {
		input_free_device(idev);
		return error;
	}

#ifdef REPEAT_RATE
	idev->rep[REP_DELAY]  = REPEAT_DELAY;
	idev->rep[REP_PERIOD] = 1000 / REPEAT_RATE;
#endif

	return 0;
}

static void input_fscbtns_remove(void)
{
	input_unregister_device(fscbtns.idev);
}

static void fscbtns_report_orientation(void)
{
	int orientation = fscbtns_read_register(0xdd);

	if(orientation & 0x02) {
		orientation ^= fscbtns.config.invert_orientation_bit;
		orientation &= 0x01;

		if(orientation != fscbtns.orientation)
			input_report_switch(fscbtns.idev, SW_TABLET_MODE,
					fscbtns.orientation = orientation);
	}
}

#ifdef ANNOYING_FEATURES
static void fscbtns_lp_timeout(unsigned long data)
{
	fscbtns.lp_timer_start = 0;

	fscbtns.idev->rep[REP_DELAY] = 1;
	input_report_key(fscbtns.idev, data, 1);
	fscbtns.timer.data = 0;

	fscbtns.idev->rep[REP_DELAY] = REPEAT_DELAY;
	input_sync(fscbtns.idev);
}

static inline int fscbtns_lp_report_key(unsigned int keycode,
		unsigned int lp_keycode, int pressed)
{
	if(!lp_keycode)
		return 0;

	if(fscbtns.lp_timer_start) {
		int lp = jiffies - fscbtns.lp_timer_start;
		fscbtns.lp_timer_start = 0;

		del_timer(&fscbtns.timer);
		fscbtns.timer.data = 0;

		if(lp > HZ/3) {
			input_report_key(fscbtns.idev, lp_keycode, 1);
			input_sync(fscbtns.idev);
			input_report_key(fscbtns.idev, lp_keycode, 0);
			return 1;
		}

		input_report_key(fscbtns.idev, keycode, 1);
		input_sync(fscbtns.idev);
		return 0;
	}

	if(pressed && !timer_pending(&fscbtns.timer)) {
		fscbtns.lp_timer_start = jiffies;

		fscbtns.timer.data = keycode;
		fscbtns.timer.function = fscbtns_lp_timeout;
		fscbtns.timer.expires = jiffies + (REPEAT_DELAY*HZ)/1000;
		add_timer(&fscbtns.timer);
		return 1;
	}

	return 0;
}
#endif

#if defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)
static void fscbtns_sticky_timeout(unsigned long data)
{
	input_report_key(fscbtns.idev, data, 0);
	fscbtns.timer.data = 0;

	input_sync(fscbtns.idev);
}

static inline int fscbtns_sticky_report_key(unsigned int keycode, int pressed)
{
	if(pressed) {
		del_timer(&fscbtns.timer);
		return 0;
	}

	if(fscbtns.timer.data && (fscbtns.timer.data != keycode)) {
		input_report_key(fscbtns.idev, fscbtns.timer.data, 0);
		fscbtns.timer.data = 0;
		return 0;
	}

	if(test_bit(keycode, modification_mask)) {
		fscbtns.timer.data = keycode;
		fscbtns.timer.function = fscbtns_sticky_timeout;
		fscbtns.timer.expires = jiffies + (STICKY_TIMEOUT*HZ)/1000;
		add_timer(&fscbtns.timer);
		return 1;
	}

	return 0;
}
#endif

static void fscbtns_report_key(int key, int pressed)
{
	int handled;
	unsigned int *kcptr = &(fscbtns.config.keymap[key]);

#ifdef ANNOYING_FEATURES
	handled = fscbtns_lp_report_key(*kcptr, *(kcptr+16), pressed);
	if(handled)
		return;
#endif

#if defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)
	handled = fscbtns_sticky_report_key(*kcptr, pressed);
	if(handled)
		return;
#endif

	input_report_key(fscbtns.idev, *kcptr, pressed);
}

static void fscbtns_event(void)
{
	unsigned long keymask;
	unsigned long changed;
	static unsigned long prev_keymask = 0;

	fscbtns_report_orientation();

	keymask  = fscbtns_read_register(0xde);
	keymask |= fscbtns_read_register(0xdf) << 8;
	keymask ^= 0xffff;

	changed = keymask ^ prev_keymask;

	if(changed) {
		int x = 0;
		int pressed = !!(keymask & changed);

		/* save current state and filter not changed bits */
		prev_keymask = keymask;

		/* get number of changed bit */
		while(!test_bit(x, &changed))
			x++;

#ifndef ANNOYING_FEATURES
		input_event(fscbtns.idev, EV_MSC, MSC_SCAN, x);
#endif
		fscbtns_report_key(x, pressed);
	}

	input_sync(fscbtns.idev);
}


/*** INTERRUPT ****************************************************************/

static void fscbtns_isr_do(struct work_struct *work)
{
	fscbtns_event();
	fscbtns_ack();
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static DECLARE_WORK(isr_wq, fscbtns_isr_do);
#else
static DECLARE_WORK(isr_wq, fscbtns_isr_do, NULL);
#endif

static irqreturn_t fscbtns_isr(int irq, void *dev_id)
{
	if(!(fscbtns_status() & 0x01))
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

static int fscbtns_reset(void)
{
	fscbtns_ack();
	if(fscbtns_busywait())
		printk(KERN_WARNING MODULENAME ": timeout, reset needed!\n");

	return 0;
}

static int __devinit fscbtns_probe(struct platform_device *pdev)
{
	int error;

	error = input_fscbtns_setup(&pdev->dev);
	if(error)
		return error;

	if(!request_region(fscbtns.address, 8, MODULENAME)) {
		printk(KERN_ERR MODULENAME ": region 0x%04x busy\n", fscbtns.address);
		error = -EBUSY;
		goto err_input;
	}

	fscbtns_reset();

	fscbtns_report_orientation();
	input_sync(fscbtns.idev);

	error = request_irq(fscbtns.interrupt, fscbtns_isr,
			IRQF_SHARED, MODULENAME, fscbtns_isr);
	if(error) {
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
	return fscbtns_reset();
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
#ifdef CONFIG_ACPI

static acpi_status fscbtns_walk_resources(struct acpi_resource *res, void *data)
{
	switch(res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			if(fscbtns.interrupt)
				return_ACPI_STATUS(AE_OK);

			fscbtns.interrupt = res->data.irq.interrupts[0];
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_IO:
			if(fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			fscbtns.address = res->data.io.minimum;
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_END_TAG:
			if(fscbtns.interrupt && fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			return_ACPI_STATUS(AE_NOT_FOUND);

		default:
			return_ACPI_STATUS(AE_ERROR);
	}
}

static int acpi_fscbtns_add(struct acpi_device *adev)
{
	acpi_status status;

	if(!adev)
		return -EINVAL;

	status = acpi_walk_resources(adev->handle, METHOD_NAME__CRS,
			fscbtns_walk_resources, NULL);
	if(ACPI_FAILURE(status))
		return -ENODEV;

	return 0;
}

static struct acpi_driver acpi_fscbtns_driver = {
	.name  = MODULENAME,
	.class = "hotkey",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
	.ids   = fscbtns_ids,
#else
	.ids   = "FUJ02BD,FUJ02BF",
#endif
	.ops   = {
		.add    = acpi_fscbtns_add
	}
};

#endif /* CONFIG_ACPI */


/*** DMI **********************************************************************/

static int __init fscbtns_dmi_matched(struct dmi_system_id *dmi)
{
	printk(KERN_INFO MODULENAME ": found: %s\n", dmi->ident);
	fscbtns_use_config(dmi->driver_data);
	return 1;
}

static struct dmi_system_id dmi_ids[] __initdata = {
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens P/T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK")
		},
		.driver_data = &config_Lifebook_Tseries
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic T Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Stylistic T")
		},
		.driver_data = &config_Stylistic_Tseries
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "STYLISTIC ST5")
		},
		.driver_data = &config_Stylistic_ST5xxx
	},
	{
		.callback = fscbtns_dmi_matched,
		.ident = "Fujitsu Siemens Stylistic ST5xxx Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
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
	int error = -EINVAL;

	switch(user_model) {
		case 1:
			fscbtns_use_config(&config_Stylistic_Tseries);
			break;
		case 2:
			fscbtns_use_config(&config_Lifebook_Tseries);
			break;
		case 3:
			fscbtns_use_config(&config_Stylistic_ST5xxx);
			break;
		default:
			dmi_check_system(dmi_ids);
	}

#ifdef CONFIG_ACPI
	error = acpi_bus_register_driver(&acpi_fscbtns_driver);
	if(ACPI_FAILURE(error))
		return error;

	error = -ENODEV;
#endif

#if defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)
	init_timer(&fscbtns.timer);
#endif

	if(!fscbtns.interrupt || !fscbtns.address)
		goto err;

	error = platform_driver_register(&fscbtns_platform_driver);
	if(error)
		goto err;

	fscbtns.pdev = platform_device_alloc(MODULENAME, -1);
	if(!fscbtns.pdev) {
		error = -ENOMEM;
		goto err_pdrv;
	}

	error = platform_device_add(fscbtns.pdev);
	if(error)
		goto err_pdev;

	return 0;

err_pdev:
	platform_device_put(fscbtns.pdev);
err_pdrv:
	platform_driver_unregister(&fscbtns_platform_driver);
err:
#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif
#if (defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)) || defined(ANNOYING_FEATURES)
	del_timer_sync(&fscbtns.timer);
#endif
	return error;
}

static void __exit fscbtns_module_exit(void)
{
	platform_device_unregister(fscbtns.pdev);
	platform_driver_unregister(&fscbtns_platform_driver);

#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif
#if (defined(STICKY_TIMEOUT) && (STICKY_TIMEOUT > 0)) || defined(ANNOYING_FEATURES)
	del_timer_sync(&fscbtns.timer);
#endif
}

module_init(fscbtns_module_init);
module_exit(fscbtns_module_exit);
