/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-back-gtk.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2007 Oliver Rauch
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

#ifndef XSANE_BACK_GTK_H
#define XSANE_BACK_GTK_H

/* ---------------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-batch-scan.h"

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef enum
  {
    xsane_back_gtk_TL_X,	/* top-left x */
    xsane_back_gtk_TL_Y,	/* top-left y */
    xsane_back_gtk_BR_X,	/* bottom-right x */
    xsane_back_gtk_BR_Y	/* bottom-right y */
  }
CornerCoordinates;

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_FILE_FILTER_ALL		1
#define XSANE_FILE_FILTER_IMAGES	2
#define XSANE_FILE_FILTER_DRC		4
#define XSANE_FILE_FILTER_BATCHLIST	8
#define XSANE_FILE_FILTER_ICM		16

#define XSANE_GET_FILENAME_SHOW_NOTHING 	0
#define XSANE_GET_FILENAME_SHOW_FILETYPE 	1
#define XSANE_GET_FILENAME_SHOW_CMS_FUNCTION	2


typedef enum
  {
    XSANE_FILE_CHOOSER_ACTION_OPEN = 1,
    XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN,
    XSANE_FILE_CHOOSER_ACTION_SAVE,
    XSANE_FILE_CHOOSER_ACTION_SELECT_SAVE,
    XSANE_FILE_CHOOSER_ACTION_SELECT_FOLDER,
    XSANE_FILE_CHOOSER_ACTION_SELECT_PROJECT
  }
XsaneFileChooserAction;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern int xsane_back_gtk_message_dialog_active;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_bound_int(int *value, int min, int max);
extern void xsane_bound_float(float *value, float min, float max);
extern void xsane_bound_double(double *value, double min, double max);
extern int xsane_check_bound_double(double value, double min, double max);
extern const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option);
extern SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info); 
extern int xsane_back_gtk_make_path(size_t max_len, char *filename_ret, const char *prog_name, const char *dir_name,
                                    const char *prefix, const char *dev_name, const char *postfix, int local);
extern gint xsane_back_gtk_decision(gchar *title, gchar **icon_xpm, gchar *message, gchar *oktext, gchar *rejecttext, int wait);
extern void xsane_back_gtk_ipc_dialog_callback(gpointer data, gint source, GdkInputCondition cond);
extern void xsane_back_gtk_message(gchar *title, gchar **icon_xpm, gchar *message, int wait);
extern void xsane_back_gtk_error(gchar *error_message, int wait);
extern void xsane_back_gtk_warning(gchar *warning_message, int wait);
extern void xsane_back_gtk_info(gchar *info_message, int wait);
extern void xsane_back_gtk_filetype_menu_set_history(GtkWidget *xsane_filetype_option_menu, char *filetype);
extern GtkWidget *xsane_back_gtk_filetype_menu_new(char *filetype, GtkSignalFunc filetype_callback);
extern GtkWidget *xsane_back_gtk_cms_function_menu_new(int cms_select_function, GtkSignalFunc cms_function_menu_callback);
extern int xsane_back_gtk_get_filename(const char *label, const char *default_name, size_t max_len, char *filename, char **filetype, int *cms_function,
                                       XsaneFileChooserAction action, int show_filetype_menu, int enable_filters, int activate_filter);

extern void xsane_back_gtk_update_vector(int opt_num, SANE_Int *vector);
extern void xsane_back_gtk_refresh_dialog(void);
/* extern void xsane_back_gtk_vector_new(GtkWidget *box, int num_vopts, int *vopts); */
extern void xsane_back_gtk_update_scan_window(void);
extern void xsane_back_gtk_set_advanced(int advanced);
extern void xsane_back_gtk_set_tooltips(int enable);
extern void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const char *desc);
extern void xsane_back_gtk_set_sensitivity(int sensitive);
extern void xsane_set_sensitivity(SANE_Int sensitivity);
extern void xsane_back_gtk_destroy_dialog(void);
extern void xsane_back_gtk_set_option(int opt_num, void *val, SANE_Action action);
extern int xsane_back_gtk_set_option_double(int option, double value);
extern int xsane_back_gtk_get_option_double(int option, double *val, SANE_Int *unit);
extern void xsane_back_gtk_set_option(int opt_num, void *val, SANE_Action action);
extern void xsane_back_gtk_get_batch_parameters(Batch_Scan_Parameters *parameters, SANE_Int *unit);
extern void xsane_back_gtk_establish_batch_parameters(Batch_Scan_Parameters *parameters, int panel_rebuild, int set_scanmode, int set_resolution);
extern GtkWidget *xsane_back_gtk_group_new (GtkWidget *parent, const char *title);
extern void xsane_back_gtk_button_new(GtkWidget *parent, const char *name, SANE_Word val,
            DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_range_new(GtkWidget *parent, const char *name, gfloat val,
           gfloat min, gfloat max, gfloat quant, int automatic,
	   DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_value_new(GtkWidget *parent, const char *name, gfloat val,
	   gfloat quant, int automatic,
	   DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_option_menu_new(GtkWidget *parent, const char *name, char *str_list[],
           const char *val, DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_text_entry_new(GtkWidget *parent, const char *name, const char *val,
                DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable);
extern void xsane_back_gtk_push_button_callback(GtkWidget *widget, gpointer data);
extern const char *xsane_back_gtk_unit_string(SANE_Unit unit);
extern void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d);

#endif
