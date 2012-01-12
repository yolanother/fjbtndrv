/*
 * Copyright (C) 2012 Robert Gerlach <khnz@users.sourceforge.net>
 * 
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * fjbtndrv3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FJBTNDRV_OSD_H_
#define _FJBTNDRV_OSD_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define FJBTNDRV_TYPE_OSD             (fjbtndrv_osd_get_type ())
#define FJBTNDRV_OSD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FJBTNDRV_TYPE_OSD, FjbtndrvOSD))
#define FJBTNDRV_OSD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FJBTNDRV_TYPE_OSD, FjbtndrvOSDClass))
#define FJBTNDRV_IS_OSD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FJBTNDRV_TYPE_OSD))
#define FJBTNDRV_IS_OSD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), FJBTNDRV_TYPE_OSD))
#define FJBTNDRV_OSD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FJBTNDRV_TYPE_OSD, FjbtndrvOSDClass))

typedef struct _FjbtndrvOSDClass FjbtndrvOSDClass;
typedef struct _FjbtndrvOSD FjbtndrvOSD;

struct _FjbtndrvOSDClass
{
	GObjectClass parent_class;
};

struct _FjbtndrvOSD
{
	GObject parent_instance;
};

GType fjbtndrv_osd_get_type (void) G_GNUC_CONST;

FjbtndrvOSD* fjbtndrv_osd_new (Display *display);

void fjbtndrv_osd_info(FjbtndrvOSD*, gchar *text);
void fjbtndrv_osd_vinfo(FjbtndrvOSD*, gchar *format, ...);
void fjbtndrv_osd_percentage(FjbtndrvOSD*, guint percent, gchar *title, guint timeout);
void fjbtndrv_osd_slider(FjbtndrvOSD*, guint percent, gchar *title, guint timeout);
void fjbtndrv_osd_hide(FjbtndrvOSD*);

G_END_DECLS

#endif /* _FJBTNDRV_OSD_H_ */
