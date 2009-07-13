/*
 * Copyright (C) 2009 Robert Gerlach
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "fjbtndrv.h"
#include "rotation.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/extensions/Xrandr.h>

typedef struct _scriptlist {
	char name[PATH_MAX];
	struct _scriptlist * next;
} scriptlist;

static int is_regular_file(const char *filename)
{
	struct stat s;
	char buffer[PATH_MAX];
	int error, len;

	error = stat(filename, &s);
	if(error)
		return 0;

	if((s.st_mode & S_IFMT) == S_IFREG)
		return 1;

	else if((s.st_mode & S_IFMT) == S_IFLNK) {
		len = readlink(filename, buffer, PATH_MAX-1);
		if(len > 0) {
			buffer[len] = '\0';
			return is_regular_file(buffer);
		} else
			perror(filename);
	}

	return 0;
}

// TODO: better name
static int is_script(const char *filename)
{
	int len = strlen(filename);

	return ((filename[0] != '.') &&
		(filename[len-1] != '~') &&
		(strcasecmp(&(filename[len-4]), ".bak") != 0));
}

static scriptlist* find_scripts(void)
{
	DIR *dh;
	struct dirent *de;
	int len;
	char *homedir, buffer[PATH_MAX];
	scriptlist *paths, **next;

	paths = NULL;
	next = &paths;

	homedir = getenv("HOME");
	if(homedir) {
		len = snprintf(buffer, PATH_MAX, "%s/." PACKAGE "/scripts", homedir);
		if(len > 0) {
			dh = opendir(buffer);
			if(dh) {
				buffer[len++] = '/';

				while((de = readdir(dh))) {
					if((!de->d_name) || (de->d_name[0] == '.'))
						continue;

					strncpy(&(buffer[len]), de->d_name, PATH_MAX - len);
					if(is_regular_file(buffer) &&
					   is_script(buffer)) {
						*next = malloc(sizeof(scriptlist));
						strcpy((*next)->name, buffer);
						next = &((*next)->next);
					}
				}

				closedir(dh);
			}
		}
	}

	len = snprintf(buffer, PATH_MAX, "%s", SCRIPTDIR);
	if(len > 0) {
		dh = opendir(SCRIPTDIR);
		if(dh) {
			buffer[len++] = '/';

			while((de = readdir(dh))) {
				if((!de->d_name) || (de->d_name[0] == '.'))
					continue;

				strncpy(&(buffer[len]), de->d_name, PATH_MAX - len);
				if(is_regular_file(buffer) &&
				   is_script(buffer)) {
					*next = malloc(sizeof(scriptlist));
					strcpy((*next)->name, buffer);
					next = &((*next)->next);
				}
			}

			closedir(dh);
		}
	}

	(*next) = NULL;
	return paths;
}

static void free_scriptlist(scriptlist* list)
{
	if(!list)
		return;

	if(list->next)
		free_scriptlist(list->next);

	free(list);
}

static int run_scripts(const char *arg)
{
	int error;
       	scriptlist *paths, *path;

	debug("HOOKS: %s", arg);

	paths = find_scripts();
	if(!paths)
		return 0;

	path = paths;
	do {
		int pid = fork();
		if(pid == 0) {
			debug("EXEC: %s", path->name);
			execl(path->name, path->name, arg, NULL);
		} else {
			waitpid(pid, &error, 0);
			debug("  waitpid: error=%d", error);
		}
	} while((!error) && (path = path->next));

	free_scriptlist(paths);
	return error;
}

char* r2s(Rotation r)
{
	switch(r) {
		case RR_Rotate_0:
			return "normal";
		
		case RR_Rotate_90:
			return "left";

		case RR_Rotate_180:
			return "inverted";

		case RR_Rotate_270:
			return "right";

		default:
			return "";
	}
}

void rotate_display(Display *display, Rotation rr)
{
	Window rw;
	XRRScreenConfiguration *sc;
	Rotation cr;
	SizeID sz;
	int error;

	rw = DefaultRootWindow(display);
	sc = XRRGetScreenInfo(display, rw);
	if(!sc) return;

	sz = XRRConfigCurrentConfiguration(sc, &cr);
	if(!rr) {
		rr = (cr & 0x7) << 1;
		if(!rr)
			rr = RR_Rotate_0;
	}

	if(rr != (cr & 0xf)) {
/* TODO:
		error = run_scripts((rr & RR_Rotate_0)
				? "pre-normal"
				: "pre-tablet", cr);
		if(error)
			goto err;
*/

		error = XRRSetScreenConfig(display, sc, rw, sz,
				rr | (cr & ~0xf),
				CurrentTime);
		if(error)
			goto err;

		error = run_scripts(r2s(rr));
		if(error)
			goto err;

		//TODO: screen_rotated();
	}

  err:
	XRRFreeScreenConfigInfo(sc);
}

