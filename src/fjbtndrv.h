/*
 * Copyright (C) 2011 - Robert Gerlach
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FJBTNDRV_H
#define _FJBTNDRV_H

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#ifdef DEBUG
#  include <stdio.h>
#  define debug(msg, a...) fprintf(stderr, msg "\n", ##a)
#else
#  define debug(msg, a...) do {} while(0)
#endif

#define FJBTNDRV_PROXY_SERVICE_PATH      "/de/khnz/fjbtndrv"
#define FJBTNDRV_PROXY_SERVICE_NAME      "de.khnz.fjbtndrv"
#define FJBTNDRV_PROXY_SERVICE_INTERFACE FJBTNDRV_PROXY_SERVICE_NAME

#endif
