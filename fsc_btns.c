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
#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#else
#include <linux/dmi.h>
#endif
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/delay.h>

#define MODULENAME "fsc_btns"
#define MODULEDESC "Fujitsu Siemens Application Panel Driver for T-Series Lifebooks"
#define MODULEVERS "0.30a"

#define FJBTNS_BASE		fscbtns.address
#define FJBTNS_ADDRESS_PORT	FJBTNS_BASE
#define FJBTNS_RESET_PORT	FJBTNS_BASE+2
#define FJBTNS_DATA_PORT	FJBTNS_BASE+4
#define FJBTNS_STATUS_PORT	FJBTNS_BASE+6

#ifndef KEY_DISPLAYTOGGLE
#define KEY_DISPLAYTOGGLE	0x1af
#endif

#define default_keymap keymap_Tseries
static unsigned int keymap_Tseries[16] = {
  /* 0x0001 */	KEY_UNKNOWN,
  /* 0x0002 */	KEY_UNKNOWN,
  /* 0x0004 */	KEY_UNKNOWN,
  /* 0x0008 */	KEY_UNKNOWN,
  /* 0x0010 */	KEY_SCROLLDOWN,
  /* 0x0020 */	KEY_SCROLLUP,
  /* 0x0040 */	KEY_DIRECTION,
  /* 0x0080 */	KEY_FN,
  /* 0x0100 */	KEY_BRIGHTNESSUP,
  /* 0x0200 */	KEY_BRIGHTNESSDOWN,
  /* 0x0400 */	KEY_DISPLAYTOGGLE,
  /* 0x0800 */	KEY_UNKNOWN,
  /* 0x1000 */	KEY_UNKNOWN,
  /* 0x2000 */	KEY_UNKNOWN,
  /* 0x4000 */	KEY_UNKNOWN,
  /* 0x8000 */	KEY_MENU
};

static unsigned int keymap_Stylistic[16] = {
  /* 0x0001 */	KEY_UNKNOWN,
  /* 0x0002 */	KEY_UNKNOWN,
  /* 0x0004 */	KEY_UNKNOWN,
  /* 0x0008 */	KEY_UNKNOWN,
  /* 0x0010 */	KEY_PRINT,
  /* 0x0020 */	KEY_BACKSPACE,
  /* 0x0040 */	KEY_SPACE,
  /* 0x0080 */	KEY_ENTER,
  /* 0x0100 */	KEY_BRIGHTNESSUP,
  /* 0x0200 */	KEY_BRIGHTNESSDOWN,
  /* 0x0400 */	KEY_UP,
  /* 0x0800 */	KEY_DOWN,
  /* 0x1000 */	KEY_PAGEUP,
  /* 0x2000 */	KEY_PAGEDOWN,
  /* 0x4000 */	KEY_FN,
  /* 0x8000 */	KEY_MENU
};

#ifndef CONFIG_ACPI
static int __init dmi_matched(struct dmi_system_id *dmi);

static struct dmi_system_id dmi_ids[] __initdata = {
	{
		.callback = dmi_matched,
		.ident = "Fujitsu-Siemens Lifebook T-Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK T"),
		},
		.driver_data = keymap_Tseries
	},
	{
		.callback = dmi_matched,
		.ident = "Fujitsu-Siemens Lifebook P-Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK P"),	/* not sure */
		},
		.driver_data = keymap_Tseries
	},
	{
		.callback = dmi_matched,
		.ident = "Fujitsu-Siemens Stylistic ST-Series",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "STYLISTIC"),	/* not sure */
		},
		.driver_data = keymap_Stylistic
	}
};
#endif


static struct fscbtns_t {				/* fscbtns_t */
	unsigned int interrupt;
	unsigned int address;

	unsigned int *keymap;
	int orientation;

	struct platform_device *pdev;

#ifdef CONFIG_ACPI
	struct acpi_device *adev;
#endif

	u8 *reset_sequence;

	struct input_dev *idev;
	char idev_phys[16];
} fscbtns
#ifndef CONFIG_ACPI
	/* XXX: is this always true ??? */
	= { .interrupt = 5, .address = 0xfd70 }
#endif
;

static unsigned int repeat_rate = 16;
static unsigned int repeat_delay = 500;
static unsigned int user_keymap = 0;

#ifdef __ONLY_A_COLLECTION_OF_CODES__
typedef u8 reset_sequence[3];
reset_sequence reset_t4010 = { 0x02, 0x04, 0x05 };
reset_sequence reset_p1610 = { 0x02, 0x00, 0x05 };
reset_sequence reset_jan   = { 0x02, 0x0c, 0x05 };
#endif

#define IOREADB(offset)		inb(offset)
#define IOWRITEB(data, offset)	outb(data, offset)


/*** DEBUG HELPER *************************************************************/
#ifdef DEBUG
#ifdef DEBUG_IO

u8 ioreadb(int offset) {
	u8 data = inb(offset);
	pr_debug("IOREADB(%d): 0x%02x, Status=0x%02x\n",
			offset, data, inb(FJBTNS_STATUS_PORT));
	return data;
}
#undef  IOREADB
#define IOREADB(offset)	ioreadb(offset)


void iowriteb(u8 data, int offset)
{
	outb(data, offset);
	pr_debug("IOWRITEB(%d): 0x%02x, Status=0x%02x\n",
			offset, data, inb(FJBTNS_STATUS_PORT));
}
#undef  IOWRITEB
#define IOWRITEB(data, offset) iowriteb(data, offset)

static void dump_regs(void)
{
	unsigned char x, y;

	pr_debug("register dump:\n");
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

inline u8 fscbtns_read_register(const u8 addr)
{
	IOWRITEB(addr, FJBTNS_ADDRESS_PORT);
	return IOREADB(FJBTNS_DATA_PORT);
}

#if 0 /* avoid warning */
inline void fscbtns_write_register(const u8 addr, const u8 data)
{
	IOWRITEB(addr, FJBTNS_ADDRESS_PORT);
	IOWRITEB(data, FJBTNS_DATA_PORT);
}
#endif


/*** INPUT ********************************************************************/

static int input_fscbtns_setup(void)
{
	struct input_dev *idev;
	int error;
	int x;

	snprintf(fscbtns.idev_phys, sizeof(fscbtns.idev_phys),
			"fsc/input0");

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

	if(!fscbtns.keymap) {
		pr_info("A: NO KEYMAP!");
		fscbtns.keymap = default_keymap;
	}

	idev->keycode = fscbtns.keymap;
	idev->keycodesize = sizeof(unsigned int);
	idev->keycodemax  = ARRAY_SIZE(default_keymap);

	set_bit(EV_REP, idev->evbit);
	set_bit(EV_MSC, idev->evbit);
	set_bit(MSC_RAW, idev->mscbit);
	set_bit(MSC_SCAN, idev->mscbit);
	set_bit(EV_KEY, idev->evbit);
	for(x = 0; x < ARRAY_SIZE(default_keymap); x++)
		set_bit(fscbtns.keymap[x], idev->keybit);

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

inline void fscbtns_handle_rotating(void)
{
	int r = fscbtns_read_register(0xdd) ^ 0xff;
	if(r != fscbtns.orientation) {
		pr_debug("orientation change (%d)\n", r);
		input_report_switch(fscbtns.idev, SW_TABLET_MODE,
				fscbtns.orientation = r);
	}
}


/*** INTERRUPT ****************************************************************/

static void fscbtns_isr_do(struct work_struct *work)
{
	unsigned long keymask;
	unsigned long changed;
	static unsigned long prev_keymask = 0;

	fscbtns_handle_rotating();

	keymask  = fscbtns_read_register(0xde);
	keymask |= fscbtns_read_register(0xdf) << 8;
	keymask ^= 0xffff;

	changed = keymask ^ prev_keymask;
	dev_dbg(&(fscbtns.pdev->dev), "keymask: 0x%04lx (0x%04lx)\n",
			keymask, changed);

	if(changed) {
		int key = 0;
		int pressed = !!(keymask & changed);

		/* save current state and filter not changed bits */
		prev_keymask = keymask;

		/* get number of changed bit */
		while(key < ARRAY_SIZE(default_keymap) && !test_bit(key, &changed))
			key++;

		if(key < ARRAY_SIZE(default_keymap)) {
			/*XXX: useful? right code? both??? */
			input_event(fscbtns.idev, EV_MSC, MSC_RAW, key);
			input_event(fscbtns.idev, EV_MSC, MSC_SCAN, key);

			pr_debug("send %d %s\n", fscbtns.keymap[key],
					(pressed ? "pressed" : "released"));
			input_report_key(fscbtns.idev, fscbtns.keymap[key],
					pressed);
		} else
			printk(KERN_ERR MODULENAME ": BUG! key > %d\n",
					ARRAY_SIZE(default_keymap));

	}

	input_sync(fscbtns.idev);

	inb(FJBTNS_RESET_PORT);
}

static DECLARE_WORK(isr_wq, fscbtns_isr_do);

static irqreturn_t fscbtns_isr(int irq, void *dev_id)
{
	if(IOREADB(FJBTNS_STATUS_PORT) & 0x01) {
		schedule_work(&isr_wq);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}


/*** DEVICE *******************************************************************/

static int fscbtns_busywait(void)
{
	int timeout_counter = 255;

	while(IOREADB(FJBTNS_STATUS_PORT) & 0x02 && --timeout_counter)
		udelay(100);

	pr_debug("busywait rest: %d\n", timeout_counter);
	return !timeout_counter;
}

static int fscbtns_reset(void)
{
	int timeout = 1024;

	/* TODO: write machine specific reset seq. to regs 0xe8..0xea */
	/* TODO: write 0 to status port */

	do {
		inb(FJBTNS_RESET_PORT);
		if(!inb(FJBTNS_STATUS_PORT) & 1)
			return 0;

		udelay(100);
	} while(--timeout);

	printk(KERN_WARNING MODULENAME ": timeout during reset, this should not happen\n");
	return -1;
}


static int __devinit fscbtns_probe(struct platform_device *pdev)
{
	int error;
	struct resource *resource;

	error = input_fscbtns_setup();
	if(error)
		return error;

	fscbtns_set_repeat_rate(repeat_delay, 1000 / repeat_rate);

	resource = request_region(fscbtns.address, 8, MODULENAME);
	if(!resource) {
		dev_err(&(pdev->dev), "request_region failed! (0x%04x)\n",
				fscbtns.address);
		error = -EBUSY;
		goto err_input;
	}

#ifdef DEBUG_IO
	dump_regs();
#endif


	inb(FJBTNS_RESET_PORT);
	error = fscbtns_busywait();
	if(error) {
		pr_debug("reseting...\n");
		error = fscbtns_reset();
		if(error)
			goto err_io;
	} else
		pr_debug("device ready!\n");

	fscbtns_handle_rotating();
	input_sync(fscbtns.idev);

	error = request_irq(fscbtns.interrupt, fscbtns_isr,
			IRQF_SHARED, MODULENAME, fscbtns_isr);
	if(error) {
		dev_err(&(pdev->dev), "unable to use irq %d\n",
				fscbtns.interrupt);
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

static int fscbtns_suspend(struct platform_device *pdev, pm_message_t state)
{
	dev_dbg(&(pdev->dev), "suspend (%d)\n", state.event);
	return 0;
}

static int fscbtns_resume(struct platform_device *pdev)
{
	pr_debug("resume:\n");
	IOREADB(FJBTNS_RESET_PORT);
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

	pr_debug("register platform driver\n");
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

	pr_debug("platform driver registered\n");
	return 0;

err_pdev:
	platform_device_put(fscbtns.pdev);
err_pdrv:
	platform_driver_unregister(&fscbtns_platform_driver);
err:
	printk(KERN_ERR MODULENAME ": failed to register platform driver\n");
	return error;
}


/*** ACPI *********************************************************************/
#ifdef CONFIG_ACPI

static acpi_status fscbtns_walk_resources(struct acpi_resource *res, void *data)
{
	switch(res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			if(fscbtns.interrupt)
				return_ACPI_STATUS(AE_OK);

			pr_debug("acpi walk: res: interrupt (nr=%d)\n",
					res->data.irq.interrupts[0]);

			fscbtns.interrupt = res->data.irq.interrupts[0];
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_IO:
			if(fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			pr_debug("acpi walk: res: ioports (min=0x%08x max=0x%08x len=0x%08x)\n",
					res->data.io.minimum,
					res->data.io.maximum,
					res->data.io.address_length);

			fscbtns.address = res->data.io.minimum;
			return_ACPI_STATUS(AE_OK);

		case ACPI_RESOURCE_TYPE_END_TAG:
			pr_debug("acpi walk: end\n");
			if(fscbtns.interrupt && fscbtns.address)
				return_ACPI_STATUS(AE_OK);

			pr_debug("acpi walk: incomplete\n");
			return_ACPI_STATUS(AE_NOT_FOUND);

		default:
			pr_debug("acpi walk: other (type=0x%08x)\n", res->type);
			return_ACPI_STATUS(AE_ERROR);
	}
}

static int acpi_fscbtns_add(struct acpi_device *adev)
{
	acpi_status status;

	if(!adev) {
		pr_debug("acpi device not found\n");
		return -EINVAL;
	}

	if(!fscbtns.keymap) {
		if(!strcmp("FUJ02BD", acpi_device_hid(adev)))
			fscbtns.keymap = keymap_Stylistic;
		else if(!strcmp("FUJ02BF", acpi_device_hid(adev)))
			fscbtns.keymap = keymap_Tseries;
	}

	status = acpi_walk_resources(adev->handle, METHOD_NAME__CRS, fscbtns_walk_resources, NULL);
	if(ACPI_FAILURE(status))
		return -ENODEV;

	return 0;
}

static int acpi_fscbtns_remove(struct acpi_device *adev, int type)
{
	return 0;
}

static struct acpi_driver acpi_fscbtns_driver = {
	.name  = MODULEDESC,
	.class = "hotkey",
	.ids   = "FUJ02BD,FUJ02BF",
	.ops   = {
		.add    = acpi_fscbtns_add,
		.remove = acpi_fscbtns_remove
	}
};

#endif /* CONFIG_ACPI */


/*** DMI **********************************************************************/
#ifndef CONFIG_ACPI

static int __init dmi_matched(struct dmi_system_id *dmi)
{
	pr_debug("DMI: %s\n", dmi->ident);

	if(!fscbtns.keymap)
		fscbtns.keymap = dmi->driver_data;

	return 1;
}

#endif /* !CONFIG_ACPI */


/*** LOADING ******************************************************************/

static int __init fscbtns_module_init(void)
{
	int error = -EINVAL;

	switch(user_keymap) {
		case 1:
			pr_info("force keymap for Stylistic Tablets");
			fscbtns.keymap = keymap_Stylistic;
			break;
		case 2:
			pr_info("force keymap for T-Series Tablet Lifebooks");
			fscbtns.keymap = keymap_Tseries;
			break;
	}

#ifdef CONFIG_ACPI
	pr_debug("register acpi driver\n");
	error = acpi_bus_register_driver(&acpi_fscbtns_driver);
	if(ACPI_FAILURE(error)) {
		printk(KERN_ERR MODULENAME ": failed to register platform driver\n");
		return error;
	}

	error = -ENODEV;
#endif
#ifndef CONFIG_ACPI
	dmi_check_system(dmi_ids);
#endif

	if(!fscbtns.interrupt || !fscbtns.address)
		goto err;

	pr_debug("register platform driver\n");
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

	pr_debug("module loaded\n");
	return 0;

err_pdev:
	platform_device_put(fscbtns.pdev);
err_pdrv:
	platform_driver_unregister(&fscbtns_platform_driver);
err:
#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif
	pr_debug("failed to register driver\n");
	return error;
}

static void __exit fscbtns_module_exit(void)
{
	platform_device_unregister(fscbtns.pdev);
	platform_driver_unregister(&fscbtns_platform_driver);

#ifdef CONFIG_ACPI
	acpi_bus_unregister_driver(&acpi_fscbtns_driver);
#endif

	pr_debug("module removed\n");
}


/*** MODULE *******************************************************************/

MODULE_AUTHOR("Robert Gerlach <khnz@users.sourceforge.net>");
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

/*TODO: test to load a keymap by hal */
module_param_named(keymap, user_keymap, uint, 0);
MODULE_PARM_DESC(keymap, "keymap (1 = Stylistic, 2 = T- and P-Series)");


static struct pnp_device_id pnp_ids[] __initdata = {
	{ .id = "FUJ02bd" },
	{ .id = "FUJ02bf" },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(pnp, pnp_ids);

module_init(fscbtns_module_init);
module_exit(fscbtns_module_exit);

