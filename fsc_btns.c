/* Tablet Buttons Driver for Fujitsu T-Series Lifebook and Stylistic Tablet PCs
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

/******************************************************************************/

#define DEBUG
#define DEBUG_IO

/* disabled autodetect
#undef  CONFIG_ACPI
*/


/******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
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
#define MODULEVERS "0.30a"

struct keymap_entry {				/* keymap_entry */
	unsigned int mask;
	unsigned int code;
};

static struct keymap_entry keymap_t4010[] = {
	{ 0x0010, KEY_SCROLLDOWN },
	{ 0x0020, KEY_SCROLLUP },
	{ 0x0040, KEY_DIRECTION },
	{ 0x0080, KEY_FN },
	{ 0x0100, KEY_BRIGHTNESSUP },
	{ 0x0200, KEY_BRIGHTNESSDOWN },
	{ 0x8000, KEY_MENU },
	{ 0x0000, 0},
};

#define default_keymap keymap_t4010

static struct fscbtns_t {				/* fscbtns_t */
	unsigned int interrupt;
	unsigned int address;

	struct keymap_entry *keymap;
	int videomode;

	struct platform_device *pdev;

#ifdef CONFIG_ACPI
	struct acpi_device *adev;
#endif

	struct input_dev *idev;
	char idev_phys[16];
} fscbtns = {

#ifndef CONFIG_ACPI
	/* XXX: is this always true ??? */
	.interrupt = 5,
	.address = 0xfd70,
#endif

       	.keymap = default_keymap
};

static unsigned int repeat_rate = 16;
static unsigned int repeat_delay = 500;


#define IOREADB(offset)		inb(fscbtns.address+(offset));
#define IOWRITEB(data, offset)	outb((data), fscbtns.address+(offset));

#ifdef DEBUG
#  define debug(m, a...)	printk( KERN_DEBUG   MODULENAME ": " m "\n", ##a)
#else
#  define debug(m, a...)	do {} while(0)
#endif

#define info(m, a...)	printk( KERN_INFO    MODULENAME ": " m "\n", ##a)
#define warn(m, a...)	printk( KERN_WARNING MODULENAME ": " m "\n", ##a)
#define error(m, a...)	printk( KERN_ERR     MODULENAME ": " m "\n", ##a)


/*** DEBUG HELPER *************************************************************/
#ifdef DEBUG
#ifdef DEBUG_IO

u8 ioreadb(int offset) {
	u8 data = inb(fscbtns.address + offset);
	debug("IOREADB(%d): 0x%02x", offset, data);
	debug("IOREADB(6): 0x%02x", inb(fscbtns.address + 6));
	return data;
}
#undef  IOREADB
#define IOREADB(offset)	ioreadb(offset)


void iowriteb(u8 data, int offset)
{
	outb(data, fscbtns.address + offset);
	debug("IOWRITEB(%d): 0x%02x", offset, data);
	debug("IOREADB(6): 0x%02x", inb(fscbtns.address + 6));
}
#undef  IOWRITEB
#define IOWRITEB(data, offset) iowriteb(data, offset)

static void dump_regs(void)
{
	unsigned char x, y;

	debug("register dump:");
	for(y = 0; y < 8; y++) {
		printk(KERN_DEBUG MODULENAME ": 0x%02x: ", 0xc0+(y*8));
		for(x = 0; x < 8; x++) {
			outb(0xc0+(y*8)+x, fscbtns.address);
			printk(" 0x%02x", inb(fscbtns.address + 4));
		}
		printk("\n");
	}
}

#endif /* DEBUG_IO */
#endif /* DEBUG */


/*** INPUT ********************************************************************/

static int input_fscbtns_setup(void)
{
	struct keymap_entry *key;
	struct input_dev *idev;
	int error;

	snprintf(fscbtns.idev_phys, sizeof(fscbtns.idev_phys),
			"%s/input0", 
#ifdef CONFIG_ACPI
			acpi_device_hid(fscbtns.adev)
#else
			MODULENAME
#endif
			);

	fscbtns.idev = idev = input_allocate_device();
	if(!idev)
		return -ENOMEM;

	idev->phys = fscbtns.idev_phys;
	idev->name = MODULEDESC;
	idev->id.bustype = BUS_HOST;
	idev->id.vendor  = 0x1734;	/* "Fujitsu Siemens Computer GmbH" from pci.ids */
	idev->id.product = 0x0001;
	idev->id.version = 0x0101;
	idev->cdev.dev = &(fscbtns.pdev->dev);

	set_bit(EV_REP, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	for(key = fscbtns.keymap; key->mask; key++)
		set_bit(key->code, idev->keybit);

	set_bit(EV_SW, idev->evbit);
	set_bit(SW_TABLET_MODE, idev->swbit);

	error = input_register_device(idev);
	if(error) {
		input_free_device(idev);
		return error;
	}

	return 0;
}

static void input_fscbtns_remove(void)
{
	input_unregister_device(fscbtns.idev);
}

static void fscbtns_set_repeat_rate(int delay, int period)
{
	fscbtns.idev->rep[REP_DELAY]  = delay;
	fscbtns.idev->rep[REP_PERIOD] = period;
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
		debug("videomode change (%d)", i);
		fscbtns.videomode = i;
		input_report_switch(fscbtns.idev, SW_TABLET_MODE, i);
	}

	IOWRITEB(0xde, 0);
	keymask = IOREADB(4) ^ 0xff;
	IOWRITEB(0xdf, 0);
	keymask |= (IOREADB(4) ^ 0xff) << 8;

	changed = keymask ^ prev_keymask;
	debug("keymask: 0x%04x (0x%04x)", keymask, changed);

	if(changed) {
		for(key = fscbtns.keymap; key->mask; key++)
			if(key->mask == changed) {
				debug("send %d %s", key->code, (keymask & changed ? "pressed" : "released"));
				input_report_key(fscbtns.idev, key->code, !!(keymask & changed));
				break;
			}

		prev_keymask = keymask;
	}

	input_sync(fscbtns.idev);
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
	debug("INTERRUPT (0:%d)", irq_me);

	if(!irq_me)
		return IRQ_NONE;

	schedule_work(&isr_wq);
	return IRQ_HANDLED;
}



/*** DEVICE *******************************************************************/

static int fscbtns_busywait(void)
{
	int timeout_counter = 100;

	while(IOREADB(6) & 0x02 && --timeout_counter)
		msleep(10);

	debug("busywait done (rest: %d)", timeout_counter);
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
	fscbtns_set_repeat_rate(repeat_delay, 1000 / repeat_rate);

	resource = request_region(fscbtns.address, 8, MODULENAME);
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
	if(!fscbtns_busywait())
		debug("device ready");
	else
		debug("timeout, need a reset?");

	return 0;

//err_irq:
//	free_irq(fscbtns.interrupt, fscbtns_isr);
err_io:
	release_region(fscbtns.address, 8);
err_input:
	input_fscbtns_remove();
	return error;
}

static int __devexit fscbtns_remove(struct platform_device *dev)
{
	free_irq(fscbtns.interrupt, fscbtns_isr);
	release_region(fscbtns.address, 8);
	input_fscbtns_remove();
	return 0;
}

static int fscbtns_suspend(struct platform_device *dev, pm_message_t state)
{
	debug("suspend (%d)", state.event);
	return 0;
}

static int fscbtns_resume(struct platform_device *dev)
{
	debug("resume:");
	IOREADB(2);
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

static inline int fscbtns_register_platfrom_driver(void)
{
	int error;

	debug("register platform driver");
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

	debug("platform driver registered");
	return 0;

err_pdev:
	platform_device_put(fscbtns.pdev);
err_pdrv:
	platform_driver_unregister(&fscbtns_platform_driver);
err:
	debug("failed to register driver");
	return error;
}


/*** ACPI *********************************************************************/
#ifdef CONFIG_ACPI

static acpi_status fscbtns_walk_resources(struct acpi_resource *res, void *data)
{
	debug("acpi walk: %d", res->type);

	switch(res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			if(fscbtns.interrupt)
				return_ACPI_STATUS(AE_OK);

			debug("acpi walk: res: interrupt (nr=%d)",
					res->data.irq.interrupts[0]);

			fscbtns.interrupt = res->data.irq.interrupts[0];
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_IO:
			if(fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			debug("acpi walk: res: ioports (min=0x%08x max=0x%08x len=0x%08x)",
					res->data.io.minimum,
					res->data.io.maximum,
					res->data.io.address_length);

			fscbtns.address = res->data.io.minimum;
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_END_TAG:
			debug("acpi walk: end");
			if(fscbtns.interrupt && fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			warn("acpi walk: incomplete");
			return_ACPI_STATUS(AE_NOT_FOUND);

		default:
			debug("acpi walk: other (type=0x%08x)", res->type);
			return_ACPI_STATUS(AE_ERROR);
	}
}

static int acpi_fscbtns_add(struct acpi_device *device)
{
	acpi_status status;

	if(!device) {
		error("acpi device not found");
		return -EINVAL;
	}

	fscbtns.adev = device;

	debug("acpi: walking...");
	status = acpi_walk_resources(device->handle, METHOD_NAME__CRS, fscbtns_walk_resources, NULL);
	if(ACPI_FAILURE(status)) {
		error("acpi walk failed");
		return -ENODEV;
	}

	return 0;
}

static int acpi_fscbtns_remove(struct acpi_device *device, int type)
{
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

#endif /* CONFIG_ACPI */


/*** LOADING ******************************************************************/

static int __init fscbtns_module_init(void)
{
	int error = -EINVAL;

#ifdef CONFIG_ACPI
	debug("register acpi driver");
	error = acpi_bus_register_driver(&acpi_fscbtns_driver);
	if(ACPI_FAILURE(error)) {
		error("acpi_bus_register_driver failed");
		return error;
	}

	error = -ENODEV;
#endif

	if(!fscbtns.interrupt || !fscbtns.address)
		goto err;

	debug("register platform driver");
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

	debug("module loaded");
	return 0;

err_pdev:
	platform_device_put(fscbtns.pdev);
err_pdrv:
	platform_driver_unregister(&fscbtns_platform_driver);
err:
#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif
	debug("failed to register driver");
	return error;
}

static void __exit fscbtns_module_exit(void)
{
	platform_device_unregister(fscbtns.pdev);
	platform_driver_unregister(&fscbtns_platform_driver);

#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif

	debug("module removed");
}


/*** MODULE *******************************************************************/

MODULE_AUTHOR("Robert Gerlach <r.gerlach@snafu.de>");
MODULE_DESCRIPTION(MODULEDESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(MODULEVERS);

module_param_named(irq, fscbtns.interrupt, uint, 0);
MODULE_PARM_DESC(irq, "interrupt");

module_param_named(io, fscbtns.address, uint, 0);
MODULE_PARM_DESC(io, "io address");

module_param_named(rate, repeat_rate, uint, 0);
MODULE_PARM_DESC(rate, "repeat rate");

module_param_named(delay, repeat_delay, uint, 0);
MODULE_PARM_DESC(delay, "repeat delay");


static struct pnp_device_id pnp_ids[] = {
	{ .id = "FUJ02bf" },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(pnp, pnp_ids);

module_init(fscbtns_module_init);
module_exit(fscbtns_module_exit);

