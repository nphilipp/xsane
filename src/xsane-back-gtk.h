/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-back-gtk.h

   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998-2000 Oliver Rauch
   This file is part of the XSANE package.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef xsane_back_gtk_h
#define xsane_back_gtk_h

/* ---------------------------------------------------------------------------------------------------------------------- */
 
#include <sys/types.h>

#include <gtk/gtk.h>

#include <sane/config.h>
#include <sane/sane.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
  XSANE_PATH_LOCAL_SANE = 0,
  XSANE_PATH_SYSTEM,
  XSANE_PATH_TMP
};

/* ---------------------------------------------------------------------------------------------------------------------- */

struct GSGDialog;

typedef void (*GSGCallback) (struct GSGDialog *dialog, void *arg);
typedef GtkWidget *(*XSANECallback) (void);

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef enum
  {
    xsane_back_gtk_TL_X,	/* top-left x */
    xsane_back_gtk_TL_Y,	/* top-left y */
    xsane_back_gtk_BR_X,	/* bottom-right x */
    xsane_back_gtk_BR_Y	/* bottom-right y */
  }
GSGCornerCoordinates;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    /* The option number of the well-known options.  Each of these may
       be -1 in case the backend doesn't define the respective option.  */
    int scanmode;
    int scansource;
    int preview;
    int dpi;
    int dpi_x;
    int dpi_y;
    int coord[4];
    int gamma_vector;
    int gamma_vector_r;
    int gamma_vector_g;
    int gamma_vector_b;
    int bit_depth;
    int threshold;
  }
GSGWellKnownOptions;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    gchar *label;
    struct GSGDialogElement *elem;
    gint index;
  }
GSGMenuItem;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct GSGDialogElement
  {
    struct GSGDialog *dialog;	/* wasteful, but is there a better solution? */
    GtkWidget *automatic;	/* auto button for options that support this */
    GtkWidget *widget;
    GtkObject *data;
    int menu_size;		/* # of items in menu (if any) */
    GSGMenuItem *menu;
  }
GSGDialogElement;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct GSGDialog
  {
    GtkWidget *xsane_window;
    GtkWidget *standard_window;
    GtkWidget *advanced_window;
    GtkWidget *xsane_hbox;
    GtkWidget *standard_hbox;
    GtkWidget *advanced_hbox;
    GtkWidget *xsanemode_widget;
    GtkTooltips *tooltips;
    GdkColor tooltips_fg;
    GdkColor tooltips_bg;
    SANE_Handle *dev;
    const char *dev_name;
    GSGWellKnownOptions well_known;
    int num_elements;
    GSGDialogElement *element;
    gint idle_id;
    u_int rebuild : 1;
    /* This callback gets invoked whenever the backend notifies us
       that the option descriptors have changed.  */
    GSGCallback option_reload_callback;
    void *option_reload_arg;
    /* This callback gets invoked whenever the backend notifies us
       that the parameters have changed.  */
    GSGCallback param_change_callback;
    void *param_change_arg;
    XSANECallback update_xsane_callback;
    void *update_xsane_arg;
    int pixelcolor;
  }
GSGDialog;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern int xsane_back_gtk_message_dialog_active;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option);
extern SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info); 
extern int xsane_back_gtk_make_path(size_t max_len, char *filename_ret, const char *prog_name, const char *dir_name,
                                    const char *prefix, const char *dev_name, const char *postfix, int local);
extern gint xsane_back_gtk_decision(gchar *title, gchar** icon_xpm, gchar *message, gchar *oktext, gchar *rejecttext, gint wait);
extern void xsane_back_gtk_message(gchar *title, gchar** icon_xpm, gchar *message, gint wait);
extern void xsane_back_gtk_error(gchar *error_message, gint wait);
extern void xsane_back_gtk_warning(gchar *warning_message, gint wait);
extern int xsane_back_gtk_get_filename(const char *label, const char *default_name,
			    size_t max_len, char *filename, int show_fileopts);

extern void xsane_back_gtk_sync(GSGDialog *dialog);
extern void xsane_back_gtk_update_vector(GSGDialog *dialog, int opt_num, SANE_Int *vector);
extern void xsane_back_gtk_refresh_dialog(GSGDialog *dialog);
extern void xsane_back_gtk_update_scan_window(GSGDialog *dialog);
extern void xsane_back_gtk_set_advanced(GSGDialog *dialog, int advanced);
extern void xsane_back_gtk_set_tooltips(GSGDialog *dialog, int enable);
extern void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const char *desc);
extern void xsane_back_gtk_set_sensitivity(GSGDialog *dialog, int sensitive);
extern void xsane_set_sensitivity(SANE_Int sensitivity);
extern void xsane_back_gtk_destroy_dialog(GSGDialog *dialog);
extern void xsane_back_gtk_set_option(GSGDialog * dialog, int opt_num, void *val, SANE_Action action);
extern GtkWidget *xsane_back_gtk_group_new (GtkWidget *parent, const char * title);
extern void xsane_back_gtk_button_new(GtkWidget * parent, const char *name, SANE_Word val,
            GSGDialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_scale_new(GtkWidget * parent, const char *name, gfloat val,
           gfloat min, gfloat max, gfloat quant, int automatic,
	   GSGDialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_option_menu_new(GtkWidget *parent, const char *name, char *str_list[],
           const char *val, GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_text_entry_new(GtkWidget *parent, const char *name, const char *val,
                GSGDialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_push_button_callback(GtkWidget * widget, gpointer data);
extern const char *xsane_back_gtk_unit_string(SANE_Unit unit);
void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d);

#define xsane_back_gtk_dialog_get_device(dialog)	((dialog)->dev)

#endif /* gtkglue_h */
