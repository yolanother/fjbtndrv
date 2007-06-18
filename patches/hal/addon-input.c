/***************************************************************************
 * CVSID: $Id$
 *
 * addon-input.c : Listen to key events and modify hal device objects
 *
 * Copyright (C) 2005 David Zeuthen, <david@fubar.dk>
 * Copyright (C) 2005 Ryan Lortie <desrt@desrt.ca>
 * Copyright (C) 2006 Matthew Garrett <mjg59@srcf.ucam.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <linux/input.h>
#include <dbus/dbus.h>

#define TRUE  1
#define FALSE 0

typedef struct LibHalContext_s LibHalContext;
LibHalContext *libhal_ctx_init_direct(DBusError *error);

static char *udi;
static int button_state = FALSE;
static int button_has_state = FALSE;

static char *key_name[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_STOP] = "stop",
	[KEY_AGAIN] = "again",
	[KEY_PROPS] = "props",
	[KEY_UNDO] = "undo",
	[KEY_FRONT] = "front",
	[KEY_COPY] = "copy",
	[KEY_OPEN] = "open",
	[KEY_PASTE] = "paste",
	[KEY_FIND] = "find",
	[KEY_CUT] = "cut",
	[KEY_HELP] = "help",
	[KEY_MENU] = "menu",
	[KEY_CALC] = "calc",
	[KEY_SETUP] = "setup",
	[KEY_SLEEP] = "sleep",
	[KEY_WAKEUP] = "wake-up",
	[KEY_FILE] = "file",
	[KEY_SENDFILE] = "send-file",
	[KEY_DELETEFILE] = "delete-file",
	[KEY_XFER] = "xfer",
	[KEY_PROG1] = "prog1",
	[KEY_PROG2] = "prog2",
	[KEY_WWW] = "www",
	[KEY_MSDOS] = "msdos",
	[KEY_COFFEE] = "coffee",
	[KEY_DIRECTION] = "direction",
	[KEY_CYCLEWINDOWS] = "cycle-windows",
	[KEY_MAIL] = "mail",
	[KEY_BOOKMARKS] = "bookmarks",
	[KEY_COMPUTER] = "computer",
	[KEY_BACK] = "back",
	[KEY_FORWARD] = "forward",
	[KEY_CLOSECD] = "close-cd",
	[KEY_EJECTCD] = "eject-cd",
	[KEY_EJECTCLOSECD] = "eject-close-cd",
	[KEY_NEXTSONG] = "next-song",
	[KEY_PLAYPAUSE] = "play-pause",
	[KEY_PREVIOUSSONG] = "previous-song",
	[KEY_STOPCD] = "stop-cd",
	[KEY_RECORD] = "record",
	[KEY_REWIND] = "rewind",
	[KEY_PHONE] = "phone",
	[KEY_ISO] = "iso",
	[KEY_CONFIG] = "config",
	[KEY_HOMEPAGE] = "homepage",
	[KEY_REFRESH] = "refresh",
	[KEY_EXIT] = "exit",
	[KEY_MOVE] = "move",
	[KEY_EDIT] = "edit",
	[KEY_SCROLLUP] = "scroll-up",
	[KEY_SCROLLDOWN] = "scroll-down",
	[KEY_KPLEFTPAREN] = "kp-left-paren",
	[KEY_KPRIGHTPAREN] = "kp-right-paren",
	[KEY_F13] = "f13",
	[KEY_F14] = "f14",
	[KEY_F15] = "f15",
	[KEY_F16] = "f16",
	[KEY_F17] = "f17",
	[KEY_F18] = "f18",
	[KEY_F19] = "f19",
	[KEY_F20] = "f20",
	[KEY_F21] = "f21",
	[KEY_F22] = "f22",
	[KEY_F23] = "f23",
	[KEY_F24] = "f24",
	[KEY_PLAYCD] = "play-cd",
	[KEY_PAUSECD] = "pause-cd",
	[KEY_PROG3] = "prog3",
	[KEY_PROG4] = "prog4",
	[KEY_SUSPEND] = "hibernate",
	[KEY_CLOSE] = "close",
	[KEY_PLAY] = "play",
	[KEY_FASTFORWARD] = "fast-forward",
	[KEY_BASSBOOST] = "bass-boost",
	[KEY_PRINT] = "print",
	[KEY_HP] = "hp",
	[KEY_CAMERA] = "camera",
	[KEY_SOUND] = "sound",
	[KEY_QUESTION] = "question",
	[KEY_EMAIL] = "email",
	[KEY_CHAT] = "chat",
	[KEY_SEARCH] = "search",
	[KEY_CONNECT] = "connect",
	[KEY_FINANCE] = "finance",
	[KEY_SPORT] = "sport",
	[KEY_SHOP] = "shop",
	[KEY_ALTERASE] = "alt-erase",
	[KEY_CANCEL] = "cancel",
	[KEY_BRIGHTNESSDOWN] = "brightness-down",
	[KEY_BRIGHTNESSUP] = "brightness-up",
	[KEY_MEDIA] = "media",
	[KEY_POWER] = "power",
	[KEY_MUTE] = "mute",
	[KEY_VOLUMEDOWN] = "volume-down",
	[KEY_VOLUMEUP] = "volume-up",
#ifndef KEY_SWITCHVIDEOMODE
#define KEY_SWITCHVIDEOMODE	227
#endif
	[KEY_SWITCHVIDEOMODE] = "switch-videomode",
#ifndef KEY_KBDILLUMTOGGLE
#define KEY_KBDILLUMTOGGLE	228
#endif
	[KEY_KBDILLUMTOGGLE] = "kbd-illum-toggle",
#ifndef KEY_KBDILLUMDOWN
#define KEY_KBDILLUMDOWN	229
#endif
	[KEY_KBDILLUMDOWN] = "kbd-illum-down",
#ifndef KEY_KBDILLUMUP
#define KEY_KBDILLUMUP		230
#endif
	[KEY_KBDILLUMUP] = "kbd-illum-up",
#ifndef KEY_BATTERY
#define KEY_BATTERY 236
#endif
	[KEY_BATTERY] = "battery",
#ifndef KEY_BLUETOOTH
#define KEY_BLUETOOTH 237
#endif
	[KEY_BLUETOOTH] = "bluetooth",
#ifndef KEY_WLAN
#define KEY_WLAN 238
#endif
	[KEY_WLAN] = "wlan",
#ifndef KEY_DISPLAYTOGGLE
#define KEY_DISPLAYTOGGLE 431
#endif
	[KEY_DISPLAYTOGGLE] = "display-toggle",
#ifndef KEY_FN
#define KEY_FN 464
#endif
	[KEY_FN] = "fn",
};


/* we must use this kernel-compatible implementation */
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

static void
main_loop (LibHalContext *ctx, FILE* eventfp)
{
	DBusError error;
	struct input_event event;
	char *event_name;


	while (fread (&event, sizeof(event), 1, eventfp)) {

		if (button_has_state && event.type == EV_SW) {
			char *name = NULL;

			switch (event.code) {
			case SW_LID:
				name = "lid";
				break;
			case SW_TABLET_MODE:
				name = "tablet_mode";
				break;
			case SW_HEADPHONE_INSERT:
				name = "headphone_insert";
				break;
			}
			if (name != NULL) {
				long bitmask[NBITS(SW_MAX)];

				/* check switch state - cuz apparently we get spurious events (or I don't know
				 * how to use the input layer correctly)
				 *
				 * Lid close:
				 * 19:08:22.911 [I] event.value=1 ; event.code=0 (0x00)
				 * 19:08:22.914 [I] event.value=0 ; event.code=0 (0x00)
				 *
				 * Lid open:
				 * 19:08:26.772 [I] event.value=0 ; event.code=0 (0x00)
				 * 19:08:26.776 [I] event.value=0 ; event.code=0 (0x00)
				 * 19:08:26.863 [I] event.value=1 ; event.code=0 (0x00)
				 * 19:08:26.868 [I] event.value=0 ; event.code=0 (0x00)
				 * 19:08:26.955 [I] event.value=0 ; event.code=0 (0x00)
				 * 19:08:26.960 [I] event.value=0 ; event.code=0 (0x00)
				 */

				if (ioctl (fileno (eventfp), EVIOCGSW(sizeof (bitmask)), bitmask) >= 0) {
					int new_state = test_bit (event.code, bitmask);
					if (new_state != button_state) {
						button_state = new_state;
						
						dbus_error_init (&error);
						libhal_device_set_property_bool (ctx, udi, "button.state.value", 
										 button_state, &error);
						
						dbus_error_init (&error);
						libhal_device_emit_condition (ctx, udi, 
									      "ButtonPressed",
									      name,
									      &error);
					}
				}
			}
		} else if (event.type == EV_KEY && key_name[event.code] != NULL) {
			switch(event.value) {
				case 0:
					event_name = "ButtonReleased";
					break;
				case 1:
					event_name = "ButtonPressed";
					break;
				case 2:
					event_name = "ButtonRepeat";
					break;
			}
			dbus_error_init (&error);
			libhal_device_emit_condition (ctx, udi,
						      event_name,
						      key_name[event.code],
						      &error);
		}
	}
	
	dbus_error_free (&error);
}

int
main (int argc, char **argv)
{
	LibHalContext *ctx = NULL;
	DBusError error;
	char *device_file;
	FILE *eventfp;
	char *s;

	dbus_error_init (&error);

	if ((udi = getenv ("UDI")) == NULL)
		goto out;
	
	if ((device_file = getenv ("HAL_PROP_INPUT_DEVICE")) == NULL)
		goto out;

	s = getenv ("HAL_PROP_BUTTON_HAS_STATE");
	if (s != NULL && strcmp (s, "true") == 0) {
		button_has_state = TRUE;
		if ((s = getenv ("HAL_PROP_BUTTON_STATE_VALUE")) == NULL)
			goto out;
		button_state = (strcmp (s, "true") == 0);
	}

	if ((ctx = libhal_ctx_init_direct (&error)) == NULL)
                goto out;

	dbus_error_init (&error);
	if (!libhal_device_addon_is_ready (ctx, udi, &error)) {
		goto out;
	}

	eventfp = fopen(device_file, "r");	

	if (!eventfp)
		goto out;

	setgid (HAL_USER);
	setuid (HAL_GROUP);

	while (1)
	{
		main_loop (ctx, eventfp);
		
		/* If main_loop exits sleep for 5s and try to reconnect (again). */
		sleep (5);
	}
	
	return 0;

 out:
	if (ctx != NULL) {
                dbus_error_init (&error);
                libhal_ctx_shutdown (ctx, &error);
                libhal_ctx_free (ctx);
        }
	
	return 0;
}

/* vim:set sw=8 noet: */
