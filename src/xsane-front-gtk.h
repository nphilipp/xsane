/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-front-gtk.h

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

#include <sane/sane.h>
#include "xsane.h"

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef xsane_front_gtk_h
#define xsane_front_gtk_h

/* ---------------------------------------------------------------------------------------------------------------------- */

extern int xsane_parse_options(char *options, char *argv[]);
extern void xsane_get_bounds(const SANE_Option_Descriptor *opt, double *minp, double *maxp);
extern double xsane_find_best_resolution(int well_known_option, double dpi);
extern double xsane_set_resolution(int well_known_option, double resolution);
extern void xsane_set_all_resolutions(void);
extern void xsane_define_maximum_output_size(); 
extern void xsane_close_dialog_callback(GtkWidget *widget, gpointer data);
extern void xsane_authorization_button_callback(GtkWidget *widget, gpointer data);
extern gint xsane_authorization_callback(SANE_String_Const resource,
                                         SANE_Char username[SANE_MAX_USERNAME_LEN],
                                         SANE_Char password[SANE_MAX_PASSWORD_LEN]);
extern void xsane_progress_bar_set_fraction(GtkProgressBar *progress_bar, gdouble fraction);
extern void xsane_progress_cancel(GtkWidget *widget, gpointer data);
extern void xsane_progress_new(char *bar_text, char *info, GtkSignalFunc callback, int *cancel_data_pointer);
extern void xsane_progress_update(gfloat newval);
extern void xsane_progress_clear();
extern GtkWidget *xsane_vendor_pixmap_new(GdkWindow *window, GtkWidget *parent); 
extern GtkWidget *xsane_toggle_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                                int *state, void *xsane_toggle_button_callback);
extern GtkWidget *xsane_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc, 
                                               void *xsane_button_callback, gpointer data);
extern void xsane_pixmap_new(GtkWidget *parent, char *title, int width, int height, XsanePixmap *hist);
extern void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc,
                                  void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
extern void xsane_option_menu_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                              char *str_list[], const char *val,
                                              GtkWidget **data, int option,
                                              void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
extern void xsane_range_new(GtkBox *parent, char *labeltext, const char *desc,
                            float min, float max, float quant, float page_step,
                            int digits, double *val, GtkWidget **data, void *xsane_range_callback, SANE_Int settable);
extern void xsane_range_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                        float min, float max, float quant, float page_step, int digits,
                                        double *val, GtkWidget **data, int option, void *xsane_range_callback, SANE_Int settable);
extern void xsane_set_filename(gchar *filename);
extern void xsane_outputfilename_new(GtkWidget *vbox);
extern void xsane_separator_new(GtkWidget *xsane_parent, int dist);
extern void xsane_vseparator_new(GtkWidget *xsane_parent, int dist);
extern GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn);
extern GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text);
extern void xsane_refresh_dialog(void);
extern void xsane_update_param(void *arg);
extern void xsane_define_output_filename(void);
extern int xsane_identify_output_format(char *filename, char *filetype, char **ext);
extern void xsane_change_working_directory(void);
extern int xsane_display_eula(int ask_for_accept);
extern void xsane_display_gpl(void);
extern void xsane_window_get_position(GtkWidget *gtk_window, gint *x, gint *y);
extern void xsane_widget_test_uposition(GtkWidget *gtk_window);
extern int xsane_front_gtk_getname_dialog(const char *dialog_title, const char *desc_text, char *oldname, char **newname);
extern void xsane_front_gtk_list_entries_swap(GtkWidget *list_item_1, GtkWidget *list_item_2);
extern void xsane_front_gtk_add_process_to_list(pid_t pid);
extern int xsane_front_gtk_option_defined(char *string);
extern void xsane_front_gtk_email_project_update_lockfile_status(void);
extern void xsane_project_dialog_close(void);


/* ---------------------------------------------------------------------------------------------------------------------- */

#endif

