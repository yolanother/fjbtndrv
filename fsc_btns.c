/*
 * SMBus client for the Fujitsu Lifebook Application Panel
 * Copyright (C) 2001-2004 Jochen Eisinger <jochen@penguin-breeder.org>
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

/******************************************************************************/

#define DEBUG
#define DEBUG_IO

#undef  SPLIT_INPUT_DEVICE

/* TODO: mod parameters? */
#define REPEAT_DELAY 500
#define REPEAT_RATE  8

/******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/delay.h>

#define MODULENAME "fsc_btns"
#define MODULEDESC "Fujitsu Siemens Application Panel Driver for T-Series Lifebooks"
#define MODULEVERS "0.20"

MODULE_AUTHOR("Robert Gerlach <r.gerlach@snafu.de>");
MODULE_DESCRIPTION(MODULEDESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(MODULEVERS);

static struct pnp_device_id pnp_ids[] = {
	{ .id = "FUJ02bf" },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(pnp, pnp_ids);

struct keymap_entry {				/* keymap_entry */
	unsigned int mask;
	unsigned int code;
};

static struct keymap_entry keymap[] = {
	{ 0x0010, KEY_SCROLLDOWN },
	{ 0x0020, KEY_SCROLLUP },
	{ 0x0040, KEY_DIRECTION },
	{ 0x0080, KEY_FN },
	{ 0x0100, KEY_BRIGHTNESSUP },
	{ 0x0200, KEY_BRIGHTNESSDOWN },
	{ 0x8000, KEY_MENU },
	{ 0x0000, 0},
};

static struct {					/* fscbtns */
	u8 interrupt;
	struct {
		u16 address;
		u8  length;
	} io;

	struct keymap_entry *keymap;
	int videomode;

	struct acpi_device *adev;
	struct platform_device *pdev;
#ifndef SPLIT_INPUT_DEVICE
	struct input_dev *idev;
	char idev_phys[16];
#else
	struct input_dev *idev_keys;
	char idev_keys_phys[16];
	struct input_dev *idev_sw;
	char idev_sw_phys[16];
#endif
} fscbtns = { .keymap = keymap };


#define IOREADB(offset)	ioreadb(offset)
#define IOWRITEB(data, offset) iowriteb(data, offset)

#ifdef DEBUG
#  define dbg(m, a...)	printk( KERN_DEBUG   MODULENAME ": " m "\n", ##a)
#else
#  define dbg(m, a...)	do {} while(0)
#endif

#define info(m, a...)	printk( KERN_INFO    MODULENAME ": " m "\n", ##a)
#define warn(m, a...)	printk( KERN_WARNING MODULENAME ": " m "\n", ##a)
#define error(m, a...)	printk( KERN_ERR     MODULENAME ": " m "\n", ##a)


/*** DEBUG HELPER *************************************************************/

#ifdef DEBUG_IO

#if 0
/* regdump after windows xp */
static unsigned char fscbtns_t4010d_regdump[] = {
	0x00, 0x81, 0x3f, 0xff, 0x3f, 0x2b, 0x6d, 0x00,
	0x00, 0x22, 0x00, 0x0a, 0x18, 0x00, 0x00, 0x00,
	0x3b, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x81, 0x00, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x04, 0x05, 0x00, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void reset_regs(void)
{
	unsigned char i;

	for(i = 0x00; i < 0x40; i++) {
		outb(0xc0+i, fscbtns.io.address);
		outb(fscbtns_t4010d_regdump[i], fscbtns.io.address+4);
	}
}
#endif

u8 ioreadb(int offset) {
	u8 data = inb(fscbtns.io.address + offset);
	dbg("IOREADB(%d): 0x%02x", offset, data);
	dbg("IOREADB(6): 0x%02x", inb(fscbtns.io.address + 6));
	return data;
}

void iowriteb(u8 data, int offset)
{
	outb(data, fscbtns.io.address + offset);
	dbg("IOWRITEB(%d): 0x%02x", offset, data);
	dbg("IOREADB(6): 0x%02x", inb(fscbtns.io.address + 6));
}

static void dump_regs(void)
{
	unsigned char x, y;

	dbg("register dump:");
	for(y = 0; y < 8; y++) {
		printk(KERN_DEBUG MODULENAME ": 0x%02x: ", 0xc0+(y*8));
		for(x = 0; x < 8; x++) {
			outb(0xc0+(y*8)+x, fscbtns.io.address);
			printk(" 0x%02x", inb(fscbtns.io.address + 4));
		}
		printk("\n");
	}
}

#endif /* DEBUG_IO */

/*** INPUT ********************************************************************/

static int input_fscbtns_setup(void)
{
	struct keymap_entry *key;
	struct input_dev *idev;
	int error;

#ifndef SPLIT_INPUT_DEVICE
	snprintf(fscbtns.idev_phys, sizeof(fscbtns.idev_phys),
			"%s/input0", acpi_device_hid(fscbtns.adev));

	fscbtns.idev = idev = input_allocate_device();
#else
	snprintf(fscbtns.idev_keys_phys, sizeof(fscbtns.idev_keys_phys),
			"%s/input0", acpi_device_hid(fscbtns.adev));

	fscbtns.idev_keys = idev = input_allocate_device();
#endif
	if(!idev)
		return -ENOMEM;

#ifndef SPLIT_INPUT_DEVICE
	idev->phys = fscbtns.idev_phys;
	idev->name = MODULEDESC;
#else
	idev->phys = fscbtns.idev_keys_phys;
	idev->name = "Tablet Buttons";
#endif
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* "Fujitsu Siemens Computer GmbH" from pci.ids */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;
	idev->cdev.dev = &(fscbtns.pdev->dev);

	set_bit(EV_REP, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	for(key = fscbtns.keymap; key->mask; key++)
		set_bit(key->code, idev->keybit);

#ifndef SPLIT_INPUT_DEVICE
	set_bit(EV_SW, idev->evbit);
	set_bit(SW_TABLET_MODE, idev->swbit);
#endif

	error = input_register_device(idev);
	if(error) {
		input_free_device(idev);
		return error;
	}

#ifdef SPLIT_INPUT_DEVICE
	snprintf(fscbtns.idev_sw_phys, sizeof(fscbtns.idev_sw_phys),
			"%s/input1", acpi_device_hid(fscbtns.adev));

	fscbtns.idev_sw = idev = input_allocate_device();
	if(!idev)
		goto err_unregister_keys;

	idev->name = "Tablet Mode Switch";
	idev->phys = fscbtns.idev_sw_phys;
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* "Fujitsu Siemens Computer GmbH" from pci.ids */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;
	idev->cdev.dev = &(fscbtns.pdev->dev);

	set_bit(EV_SW, idev->evbit);
	set_bit(SW_TABLET_MODE, idev->swbit);

	input_register_device(idev);
	if(error)
		goto err_free_sw;
#endif

	return 0;

#ifdef SPLIT_INPUT_DEVICE
err_free_sw:
	input_free_device(fscbtns.idev_sw);
err_unregister_keys:
	input_unregister_device(fscbtns.idev_keys);
	return error;
#endif
}

static void input_fscbtns_remove(void)
{
#ifndef SPLIT_INPUT_DEVICE
	input_unregister_device(fscbtns.idev);
#else
	input_unregister_device(fscbtns.idev_sw);
	input_unregister_device(fscbtns.idev_keys);
#endif
}

static void fscbtns_set_repeat_rate(int delay, int period)
{
#ifndef SPLIT_INPUT_DEVICE
	fscbtns.idev->rep[REP_DELAY]  = delay;
	fscbtns.idev->rep[REP_PERIOD] = period;
#else
	fscbtns.idev_keys->rep[REP_DELAY]  = delay;
	fscbtns.idev_keys->rep[REP_PERIOD] = period;
#endif
}

static void fscbtns_event(void)
{
	u8 i;
	unsigned int keymask;
	unsigned int changed;
	static unsigned int prev_keymask = 0;
	struct keymap_entry *key;

	IOWRITEB(0xdd, 0);
	i = IOREADB(4) ^ 0xff;
	if(i != fscbtns.videomode) {
		dbg("videomode change (%d)", i);
		fscbtns.videomode = i;
#ifndef SPLIT_INPUT_DEVICE
		input_report_switch(fscbtns.idev, SW_TABLET_MODE, i);
#else
		input_report_switch(fscbtns.idev_sw, SW_TABLET_MODE, i);
		input_sync(fscbtns.idev_sw);
#endif
	}

	IOWRITEB(0xde, 0);
	keymask = IOREADB(4) ^ 0xff;
	IOWRITEB(0xdf, 0);
	keymask |= (IOREADB(4) ^ 0xff) << 8;

	changed = keymask ^ prev_keymask;
	dbg("keymask: 0x%04x (0x%04x)", keymask, changed);

	if(changed) {
		for(key = fscbtns.keymap; key->mask; key++)
			if(key->mask == changed) {
				dbg("send %d %s", key->code, (keymask & changed ? "pressed" : "released"));
#ifndef SPLIT_INPUT_DEVICE
				input_report_key(fscbtns.idev, key->code, !!(keymask & changed));
#else
				input_report_key(fscbtns.idev_keys, key->code, !!(keymask & changed));
				input_sync(fscbtns.idev_keys);
#endif
				break;
			}

		prev_keymask = keymask;
	}


#ifndef SPLIT_INPUT_DEVICE
	input_sync(fscbtns.idev);
#endif
}


/*** INTERRUPT ****************************************************************/

static void fscbtns_isr_do(struct work_struct *work)
{
	fscbtns_event();
	IOREADB(2);	
}
static DECLARE_WORK(isr_wq, fscbtns_isr_do);

static irqreturn_t fscbtns_isr(int irq, void *dev_id)
{
	int irq_me = IOREADB(6) & 0x01;
	dbg("INTERRUPT (0:%d)", irq_me);

	if(!irq_me)
		return IRQ_NONE;

	schedule_work(&isr_wq);
	return IRQ_HANDLED;
}



/*** DEVICE *******************************************************************/

static int fscbtns_busywait(void)
{
	int timeout_counter = 100;

	while(IOREADB(6) & 0x02 && --timeout_counter) {
		msleep(10);
		dbg("status bit 2 set");
	}

	dbg("busywait done (rest time: %d)", timeout_counter);
	return !timeout_counter;
}

static int __devinit fscbtns_probe(struct platform_device *dev)
{
	int error;
	struct resource *resource;

	error = input_fscbtns_setup();
	if(error)
		return error;

	/* TODO: mod parameters? */
	fscbtns_set_repeat_rate(REPEAT_DELAY, 1000 / REPEAT_RATE);

	resource = request_region(fscbtns.io.address,
			fscbtns.io.length,
			MODULENAME);
	if(!resource) {
		error("request_region failed!");
		error = 1;
		goto err_input;
	}

#ifdef DEBUG_IO
	dump_regs();
#endif

	error = request_irq(fscbtns.interrupt,
			fscbtns_isr, SA_INTERRUPT, MODULENAME, fscbtns_isr);
	if(error) {
		error("request_irq failed!");
		goto err_io;
	}

	IOREADB(2);
	IOREADB(6);

	if(!fscbtns_busywait())
		dbg("device ready");
	else
		dbg("timeout, need a reset?");

	return 0;

//err_irq:
//	free_irq(fscbtns.interrupt, fscbtns_isr);
err_io:
	release_region(fscbtns.io.address, fscbtns.io.length);
err_input:
	input_fscbtns_remove();
	return error;
}

static int __devexit fscbtns_remove(struct platform_device *dev)
{
//	if(fscbtns.interrupt)
		free_irq(fscbtns.interrupt, fscbtns_isr);
//	if(fscbtns.io.address)
		release_region(fscbtns.io.address, fscbtns.io.length);
//	if(fscbtns.idev_keys)
		input_fscbtns_remove();

	return 0;
}

static int fscbtns_suspend(struct platform_device *dev, pm_message_t state)
{
//	if(fscbtns.interrupt)
//		free_irq(fscbtns.interrupt, fscbtns_isr);
	dbg("suspend: %d", state.event);
	return 0;
}

static int fscbtns_resume(struct platform_device *dev)
{
	int x;
	x = IOREADB(2);
	dbg("resume: io+2=%d", x);
//	return request_irq(fscbtns.interrupt,
//			fscbtns_isr, SA_INTERRUPT, MODULENAME, fscbtns_isr);
	return 0;
}

static struct platform_driver fscbtns_platform_driver = {
	.driver		= {
		.name	= MODULENAME,
		.owner	= THIS_MODULE,
	},
	.probe		= fscbtns_probe,
	.remove		= __devexit_p(fscbtns_remove),
	.suspend	= fscbtns_suspend,
	.resume		= fscbtns_resume,
};


/*** ACPI *********************************************************************/

static acpi_status fscbtns_walk_resources(struct acpi_resource *res, void *data)
{
	switch(res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			dbg("acpi walk: res: interrupt (nr=%d)",
					res->data.irq.interrupts[0]);

			fscbtns.interrupt = res->data.irq.interrupts[0];
			break;

		case ACPI_RESOURCE_TYPE_IO:
			dbg("acpi walk: res: ioports (min=0x%08x max=0x%08x len=0x%08x)",
					res->data.io.minimum,
					res->data.io.maximum,
					res->data.io.address_length);

			fscbtns.io.address = res->data.io.minimum;
			fscbtns.io.length  = res->data.io.address_length;
			break;

		case ACPI_RESOURCE_TYPE_END_TAG:
			dbg("acpi walk: end");
			if(fscbtns.interrupt && fscbtns.io.address)
				return_ACPI_STATUS(AE_OK);
			else
				return_ACPI_STATUS(AE_NOT_FOUND);

		default:
			dbg("acpi walk: other (type=0x%08x)", res->type);
			return_ACPI_STATUS(AE_ERROR);
	}

	return_ACPI_STATUS(AE_OK);
}

static int acpi_fscbtns_add(struct acpi_device *device)
{
	acpi_status status;
	int error;

	if(!device) {
		error("acpi device not found");
		return -EINVAL;
	}

	fscbtns.adev = device;

	status = acpi_walk_resources(device->handle, METHOD_NAME__CRS, fscbtns_walk_resources, NULL);
	if(ACPI_FAILURE(status)) {
		error("acpi walk failed");
		return -ENODEV;
	}

	dbg("register platform driver");
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
	dbg("failed to register driver");
	return error;
}

static int acpi_fscbtns_remove(struct acpi_device *device, int type)
{
	platform_device_unregister(fscbtns.pdev);
	platform_driver_unregister(&fscbtns_platform_driver);
	return 0;
}

static struct acpi_driver acpi_fscbtns_driver = {
	.name  = MODULEDESC,
	.class = "hotkey",
	.ids   = "FUJ02BF",
	.ops   = {
		.add    = acpi_fscbtns_add,
		.remove = acpi_fscbtns_remove
	}
};


/*** LOADING ******************************************************************/

static int __init fscbtns_module_init(void)
{
	acpi_status status;

	dbg("register acpi driver");
	status = acpi_bus_register_driver(&acpi_fscbtns_driver);
	if(ACPI_FAILURE(status)) {
		error("acpi_bus_register_driver failed");
		return -ENODEV;
	}

	dbg("module loaded");
	return 0;
}

static void __exit fscbtns_module_exit(void)
{
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
	dbg("module removed");
}

module_init(fscbtns_module_init);
module_exit(fscbtns_module_exit);

