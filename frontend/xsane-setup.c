/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-setup.c

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

#include "xsane.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

#ifdef HAVE_LIBTIFF
#include <tiff.h>
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_GTK_NAME_IMAGE_PERMISSIONS     "gtk_toggle_button_image_permissions"
#define XSANE_GTK_NAME_DIRECTORY_PERMISSIONS "gtk_toggle_button_directory_permissions"

/* ---------------------------------------------------------------------------------------------------------------------- */

struct XsaneSetup xsane_setup;

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_new_printer(void);
void xsane_update_int(GtkWidget *widget, int *val);

static void xsane_update_bool(GtkWidget *widget, int *val);
static void xsane_update_scale(GtkWidget *widget, double *val);
static void xsane_update_double(GtkWidget *widget, double *val);
static void xsane_setup_printer_update(void);
static void xsane_setup_printer_callback(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_menu_build(GtkWidget *option_menu);
static void xsane_setup_printer_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_new(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_delete(GtkWidget *widget, gpointer data);
static void xsane_setup_display_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_fax_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data);

static void xsane_printer_notebook(GtkWidget *notebook);
static void xsane_saving_notebook(GtkWidget *notebook);
static void xsane_fax_notebook(GtkWidget *notebook);
static void xsane_display_notebook(GtkWidget *notebook);
static void xsane_device_notebook_sensitivity(int lineart_mode);
static void xsane_setup_lineart_mode_callback(GtkWidget *widget, gpointer data);
static void xsane_device_notebook(GtkWidget *notebook);

void xsane_setup_dialog(GtkWidget *widget, gpointer data);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_new_printer(void)
{
  preferences.printernr = preferences.printerdefinitions++;

  preferences.printer[preferences.printernr] = calloc(sizeof(Preferences_printer_t), 1);

  preferences.printer[preferences.printernr]->name               = strdup(PRINTERNAME);
  preferences.printer[preferences.printernr]->command            = strdup(PRINTERCOMMAND);
  preferences.printer[preferences.printernr]->copy_number_option = strdup(PRINTERCOPYNUMBEROPTION);
  preferences.printer[preferences.printernr]->resolution         = 300;
  preferences.printer[preferences.printernr]->width              = 203.2;
  preferences.printer[preferences.printernr]->height             = 294.6;
  preferences.printer[preferences.printernr]->leftoffset         = 3.5;
  preferences.printer[preferences.printernr]->bottomoffset       = 3.5;
  preferences.printer[preferences.printernr]->gamma              = 1.0;
  preferences.printer[preferences.printernr]->gamma_red          = 1.0;
  preferences.printer[preferences.printernr]->gamma_green        = 1.0;
  preferences.printer[preferences.printernr]->gamma_blue         = 1.0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_int(GtkWidget *widget, int *val)
{
  char *start, *end;
  int v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = (int) strtol(start, &end, 10);
  if (end > start)
  {
    *val = v;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_bool(GtkWidget *widget, int *val)
{
  *val = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_scale(GtkWidget *widget, double *val)
{
  *val = GTK_ADJUSTMENT(widget)->value;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_double(GtkWidget *widget, double *val)
{
  char *start, *end;
  double v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = strtod(start, &end);
  if (end > start)
  {
    *val = v;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_update()
{
 char buf[256];
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_name_entry),   
                     (char *) preferences.printer[preferences.printernr]->name);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_command_entry),
                     (char *) preferences.printer[preferences.printernr]->command);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_copy_number_option_entry),
                     (char *) preferences.printer[preferences.printernr]->copy_number_option);

  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->resolution);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_resolution_entry), buf);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_width_entry), buf);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_height_entry), buf);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_leftoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_bottomoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_red_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_green_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_blue_entry), buf);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_callback(GtkWidget *widget, gpointer data)
{
  preferences.printernr = (int) data;
  xsane_setup_printer_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_menu_build(GtkWidget *option_menu)
{
 GtkWidget *printer_menu, *printer_item;
 int i;

  printer_menu = gtk_menu_new();

  for (i=0; i < preferences.printerdefinitions; i++)
  {
    printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
    gtk_container_add(GTK_CONTAINER(printer_menu), printer_item);
    gtk_signal_connect(GTK_OBJECT(printer_item), "activate", (GtkSignalFunc) xsane_setup_printer_callback, (void *) i);
    gtk_widget_show(printer_item);
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), printer_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), preferences.printernr);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_apply_changes(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;

  if (preferences.printer[preferences.printernr]->name)
  {
    free((void *) preferences.printer[preferences.printernr]->name);
  }
  preferences.printer[preferences.printernr]->name = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_name_entry)));

  if (preferences.printer[preferences.printernr]->command)
  {
    free((void *) preferences.printer[preferences.printernr]->command);
  }
  preferences.printer[preferences.printernr]->command = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_command_entry)));

  if (preferences.printer[preferences.printernr]->copy_number_option)
  {
    free((void *) preferences.printer[preferences.printernr]->copy_number_option);
  }
  preferences.printer[preferences.printernr]->copy_number_option =
              strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_copy_number_option_entry)));

  xsane_update_int(xsane_setup.printer_resolution_entry,   &preferences.printer[preferences.printernr]->resolution);

  xsane_update_double(xsane_setup.printer_width_entry,        &preferences.printer[preferences.printernr]->width);
  xsane_update_double(xsane_setup.printer_height_entry,       &preferences.printer[preferences.printernr]->height);
  xsane_update_double(xsane_setup.printer_leftoffset_entry,   &preferences.printer[preferences.printernr]->leftoffset);
  xsane_update_double(xsane_setup.printer_bottomoffset_entry, &preferences.printer[preferences.printernr]->bottomoffset);

  xsane_update_double(xsane_setup.printer_gamma_entry,       &preferences.printer[preferences.printernr]->gamma);
  xsane_update_double(xsane_setup.printer_gamma_red_entry,   &preferences.printer[preferences.printernr]->gamma_red);
  xsane_update_double(xsane_setup.printer_gamma_green_entry, &preferences.printer[preferences.printernr]->gamma_green);
  xsane_update_double(xsane_setup.printer_gamma_blue_entry,  &preferences.printer[preferences.printernr]->gamma_blue);

  if (option_menu)
  {
    xsane_setup_printer_menu_build(option_menu);
  }

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_new(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;

  xsane_new_printer();
  xsane_setup_printer_update();

  xsane_setup_printer_menu_build(option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_delete(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;
 int i;

  preferences.printerdefinitions--;

  i = preferences.printernr;
  while (i < preferences.printerdefinitions)
  {
    memcpy(preferences.printer[i], preferences.printer[i+1], sizeof(Preferences_printer_t));
    i++;
  }

  if (preferences.printernr >= preferences.printerdefinitions)
  {
    preferences.printernr--;
  } 

  if (preferences.printerdefinitions == 0)
  {
    xsane_new_printer();
    preferences.printernr = 0;
  }

  xsane_setup_printer_update();

  xsane_setup_printer_menu_build(option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBTIFF
static void xsane_setup_tiff_compression_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.tiff_compression_nr = (int) data;
}

/* -------------------------------------- */

static void xsane_setup_tiff_compression_1_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.tiff_compression_1_nr = (int) data;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_display_apply_changes(GtkWidget *widget, gpointer data)
{
  xsane_update_bool(xsane_setup.main_window_fixed_button,         &preferences.main_window_fixed);
  xsane_update_bool(xsane_setup.preview_preserve_button,          &preferences.preserve_preview);
  xsane_update_bool(xsane_setup.preview_own_cmap_button,          &preferences.preview_own_cmap);

  xsane_update_double(xsane_setup.preview_gamma_entry,            &preferences.preview_gamma);
  xsane_update_double(xsane_setup.preview_gamma_red_entry,        &preferences.preview_gamma_red);
  xsane_update_double(xsane_setup.preview_gamma_green_entry,      &preferences.preview_gamma_green);
  xsane_update_double(xsane_setup.preview_gamma_blue_entry,       &preferences.preview_gamma_blue);

  xsane_update_double(xsane_setup.preview_oversampling_entry,     &preferences.preview_oversampling);

  if (preferences.doc_viewer)
  {
    free((void *) preferences.doc_viewer);
  }
  preferences.doc_viewer = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.doc_viewer_entry)));

  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_device_apply_changes(GtkWidget *widget, gpointer data)
{
  xsane.lineart_mode = xsane_setup.lineart_mode;

  xsane_update_double(xsane_setup.preview_threshold_min_entry,    &xsane.threshold_min);
  xsane_update_double(xsane_setup.preview_threshold_max_entry,    &xsane.threshold_max);
  xsane_update_double(xsane_setup.preview_threshold_mul_entry,    &xsane.threshold_mul);
  xsane_update_double(xsane_setup.preview_threshold_off_entry,    &xsane.threshold_off);

  if (xsane.grayscale_scanmode)
  {
    free((void *) xsane.grayscale_scanmode);
  }
  xsane.grayscale_scanmode = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.preview_grayscale_scanmode_entry)));

  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data)
{
#ifdef HAVE_LIBJPEG
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#else
#ifdef HAVE_LIBTIFF
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#endif
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_update_scale(xsane_setup.pnm_image_compression_scale, &preferences.png_compression);
#endif
#endif

#ifdef HAVE_LIBTIFF
  preferences.tiff_compression_nr   = xsane_setup.tiff_compression_nr;
  preferences.tiff_compression_1_nr = xsane_setup.tiff_compression_1_nr;
#endif

  xsane_update_bool(xsane_setup.overwrite_warning_button,         &preferences.overwrite_warning);
  xsane_update_bool(xsane_setup.increase_filename_counter_button, &preferences.increase_filename_counter);
  xsane_update_bool(xsane_setup.skip_existing_numbers_button,     &preferences.skip_existing_numbers);
  preferences.image_umask     = 0777 - xsane_setup.image_permissions;
  preferences.directory_umask = 0777 - xsane_setup.directory_permissions;

  xsane_update_double(xsane_setup.psfile_leftoffset_entry,   &preferences.psfile_leftoffset);
  xsane_update_double(xsane_setup.psfile_bottomoffset_entry, &preferences.psfile_bottomoffset);
  xsane_update_double(xsane_setup.psfile_width_entry,        &preferences.psfile_width);
  xsane_update_double(xsane_setup.psfile_height_entry,       &preferences.psfile_height);

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_fax_apply_changes(GtkWidget *widget, gpointer data)
{
  if (preferences.fax_command)
  {
    free((void *) preferences.fax_command);
  }
  preferences.fax_command           = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_command_entry)));
  preferences.fax_receiver_option   = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_receiver_option_entry)));
  preferences.fax_postscript_option = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_postscript_option_entry)));
  preferences.fax_normal_option     = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_normal_option_entry)));
  preferences.fax_fine_option       = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_fine_option_entry)));
  preferences.fax_viewer            = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_viewer_entry)));

  xsane_update_double(xsane_setup.fax_leftoffset_entry,   &preferences.fax_leftoffset);
  xsane_update_double(xsane_setup.fax_bottomoffset_entry, &preferences.fax_bottomoffset);
  xsane_update_double(xsane_setup.fax_width_entry,        &preferences.fax_width);
  xsane_update_double(xsane_setup.fax_height_entry,       &preferences.fax_height);

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup_printer_apply_changes(0, 0);
  xsane_setup_display_apply_changes(0, 0);
  xsane_setup_device_apply_changes(0, 0);
  xsane_setup_saving_apply_changes(0, 0);
  xsane_setup_fax_apply_changes(0, 0);

  xsane_pref_save();

  gtk_widget_destroy((GtkWidget *)data); /* => xsane_destroy_setup_dialog_callback */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_destroy_setup_dialog_callback(GtkWidget *widget, gpointer data)
{
  xsane_set_sensitivity(TRUE);
  xsane_back_gtk_refresh_dialog(dialog);
}
                
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_close_setup_dialog_callback(GtkWidget *widget, gpointer data)
{
  gtk_widget_destroy((GtkWidget *)data); /* => xsane_destroy_setup_dialog_callback */
}
                
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_permission_toggled(GtkWidget *widget, gpointer data)
{
 int mask = (int) data;
 int *permission = 0;
 gchar *name = gtk_widget_get_name(widget);

  if (!strcmp(name, XSANE_GTK_NAME_IMAGE_PERMISSIONS))
  {
    permission = &xsane_setup.image_permissions;
  }
  else if (!strcmp(name, XSANE_GTK_NAME_DIRECTORY_PERMISSIONS))
  {
    permission = &xsane_setup.directory_permissions;
  }

  if (permission)
  {
    if (GTK_TOGGLE_BUTTON(widget)->active) /* set bit */
    {
      *permission = *permission | mask;
    }
    else /* erase bit */
    {
      *permission = *permission & (0777-mask);
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_permission_box(GtkWidget *parent, gchar *name, gchar *description, int *permission,
                                 int header, int x_sensitivity, int user_sensitivity)
{
 GtkWidget *hbox, *button, *label, *hspace;


  if (header)
  {
    hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 2);

    label = gtk_label_new("user");
    gtk_widget_set_usize(label, 75, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    label = gtk_label_new("group");
    gtk_widget_set_usize(label, 75, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    label = gtk_label_new("all");
    gtk_widget_set_usize(label, 75, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    gtk_widget_show(hbox);
  }


  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 2);

  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 256 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 256);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, user_sensitivity);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 128 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 128);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, user_sensitivity);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 64 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 64);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, x_sensitivity & user_sensitivity);



  hspace = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hspace, FALSE, FALSE, 6);
  gtk_widget_show(hspace);



  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 32 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 32);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 16 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 16);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 8 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 8);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, x_sensitivity);



  hspace = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hspace, FALSE, FALSE, 6);
  gtk_widget_show(hspace);



  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 4 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 4);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 2 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 2);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 1 );
  gtk_widget_set_usize(button, 21, 0);
  gtk_widget_set_name(button, name);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 1);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, x_sensitivity);



  hspace = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hspace, FALSE, FALSE, 5);
  gtk_widget_show(hspace);



  label = gtk_label_new(description);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_widget_show(hbox);

  while (gtk_events_pending())
  {
   gtk_main_iteration();
  }                      
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printer_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame;
 GtkWidget *printer_option_menu;
 char buf[64];

  /* Printer options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 5);

  label = gtk_label_new(NOTEBOOK_COPY_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);



  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_PRINTER_SEL);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  printer_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, printer_option_menu, DESC_PRINTER_SETUP);
  gtk_box_pack_end(GTK_BOX(hbox), printer_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(printer_option_menu);
  gtk_widget_show(hbox);

  xsane_setup_printer_menu_build(printer_option_menu);

  /* printername : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_NAME);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_NAME);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->name);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_name_entry = text;

  /* printcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_CMD);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_command_entry = text;

  /* copy number option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_COPY_NR_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_COPY_NUMBER_OPTION);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->copy_number_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_copy_number_option_entry = text;

  /* printerresolution : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_RES);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_RESOLUTION);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->resolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_resolution_entry = text;


  xsane_separator_new(vbox, 2);


  /* printer width: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_WIDTH);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_WIDTH);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_width_entry = text;

  /* printer height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_HEIGHT);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_height_entry = text;

  /* printer left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_LEFT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_LEFTOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_leftoffset_entry = text;

  /* printer bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_BOTTOM);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_BOTTOMOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_bottomoffset_entry = text;


  xsane_separator_new(vbox, 2);


  /* printer gamma: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_entry = text;

  /* printer gamma red: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_RED);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_red_entry = text;

  /* printer gamma green: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_GREEN);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_green_entry = text;

  /* printer gamma blue: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_BLUE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_blue_entry = text;


  xsane_separator_new(vbox, 4);

  /* "apply" "add printer" "delete printer" */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_apply_changes, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_ADD_PRINTER);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_new, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PRINTER);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_delete, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_saving_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame;
 char buf[64];

#ifdef HAVE_LIBTIFF
 GtkWidget *tiff_compression_option_menu, *tiff_compression_menu, *tiff_compression_item;
 int i, select = 1;

 typedef struct tiff_compression_t
 {
  char *name;
  int number;
 } tiff_compression;

#define TIFF_COMPRESSION_NUMBER 3
#define TIFF_COMPRESSION1_NUMBER 6

 tiff_compression tiff_compression_strings[TIFF_COMPRESSION_NUMBER];
 tiff_compression tiff_compression1_strings[TIFF_COMPRESSION1_NUMBER];
 
 tiff_compression_strings[0].name   = MENU_ITEM_TIFF_COMP_NONE;
 tiff_compression_strings[0].number = COMPRESSION_NONE;
 tiff_compression_strings[1].name   = MENU_ITEM_TIFF_COMP_JPEG;
 tiff_compression_strings[1].number = COMPRESSION_JPEG;
 tiff_compression_strings[2].name   = MENU_ITEM_TIFF_COMP_PACKBITS;
 tiff_compression_strings[2].number = COMPRESSION_PACKBITS;

 tiff_compression1_strings[0].name   = MENU_ITEM_TIFF_COMP_NONE;
 tiff_compression1_strings[0].number = COMPRESSION_NONE;
 tiff_compression1_strings[1].name   = MENU_ITEM_TIFF_COMP_CCITTRLE;
 tiff_compression1_strings[1].number = COMPRESSION_CCITTRLE;
 tiff_compression1_strings[2].name   = MENU_ITEM_TIFF_COMP_CCITFAX3;
 tiff_compression1_strings[2].number = COMPRESSION_CCITTFAX3;
 tiff_compression1_strings[3].name   = MENU_ITEM_TIFF_COMP_CCITFAX4;
 tiff_compression1_strings[3].number = COMPRESSION_CCITTFAX4;
 tiff_compression1_strings[4].name   = MENU_ITEM_TIFF_COMP_JPEG;
 tiff_compression1_strings[4].number = COMPRESSION_JPEG;
 tiff_compression1_strings[5].name   = MENU_ITEM_TIFF_COMP_PACKBITS;
 tiff_compression1_strings[5].number = COMPRESSION_PACKBITS;

#endif /* HAVE_LIBTIFF */


  /* Saving options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 5);

  label = gtk_label_new(NOTEBOOK_SAVING_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  xsane_setup.image_permissions     = 0777-preferences.image_umask;
  xsane_permission_box(vbox, XSANE_GTK_NAME_IMAGE_PERMISSIONS, TEXT_SETUP_IMAGE_PERMISSION, &xsane_setup.image_permissions,
                       TRUE /* header */, FALSE /* x sens */, FALSE /* user sens */);

  xsane_setup.directory_permissions = 0777-preferences.directory_umask;
  xsane_permission_box(vbox, XSANE_GTK_NAME_DIRECTORY_PERMISSIONS, TEXT_SETUP_DIR_PERMISSION, &xsane_setup.directory_permissions,
                       FALSE /* header */, TRUE /* x sens */, FALSE /* user sens */);

  xsane_separator_new(vbox, 4);


  /* overwrite warning */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_OVERWRITE_WARNING);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_OVERWRITE_WARNING);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.overwrite_warning);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.overwrite_warning_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_INCREASE_COUNTER);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_INCREASE_COUNTER);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.increase_filename_counter);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.increase_filename_counter_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SKIP_EXISTING_NRS);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_SKIP_EXISTING);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.skip_existing_numbers);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.skip_existing_numbers_button = button;

#ifdef HAVE_LIBJPEG 
  xsane_separator_new(vbox, 4);
#else
#ifdef HAVE_LIBTIFF
  xsane_separator_new(vbox, 4);
#else
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_separator_new(vbox, 4);
#endif
#endif
#endif
#endif

#ifdef HAVE_LIBJPEG 
  xsane_scale_new(GTK_BOX(vbox), TEXT_SETUP_JPEG_QUALITY, DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 10.0, 0.0, 0,
                  &preferences.jpeg_quality, (GtkObject **) &xsane_setup.jpeg_image_quality_scale, 0, TRUE);
#else
#ifdef HAVE_LIBTIFF
  xsane_scale_new(GTK_BOX(vbox), TEXT_SETUP_JPEG_QUALITY, DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 10.0, 0.0, 0,
                  &preferences.jpeg_quality, (GtkObject **) &xsane_setup.jpeg_image_quality_scale, 0, TRUE);
#endif
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_scale_new(GTK_BOX(vbox), TEXT_SETUP_PNG_COMPRESSION, DESC_PNG_COMPRESSION, 0.0, Z_BEST_COMPRESSION, 1.0, 10.0, 0.0, 0,
                  &preferences.png_compression, (GtkObject **) &xsane_setup.pnm_image_compression_scale, 0, TRUE);
#endif
#endif

#ifdef HAVE_LIBTIFF
  /* TIFF MULTI BIT IMAGES COMPRESSION */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_TIFF_COMPRESSION);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  tiff_compression_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, tiff_compression_option_menu, DESC_TIFF_COMPRESSION);
  gtk_box_pack_end(GTK_BOX(hbox), tiff_compression_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(tiff_compression_option_menu);
  gtk_widget_show(hbox);

  tiff_compression_menu = gtk_menu_new();

  for (i=1; i <= TIFF_COMPRESSION_NUMBER; i++)
  {
    tiff_compression_item = gtk_menu_item_new_with_label(tiff_compression_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(tiff_compression_menu), tiff_compression_item);
    gtk_signal_connect(GTK_OBJECT(tiff_compression_item), "activate",
       (GtkSignalFunc) xsane_setup_tiff_compression_callback, (void *) tiff_compression_strings[i-1].number);
    gtk_widget_show(tiff_compression_item);
    if (tiff_compression_strings[i-1].number == preferences.tiff_compression_nr)
    {
      select = i-1;
    }
  }


  gtk_option_menu_set_menu(GTK_OPTION_MENU(tiff_compression_option_menu), tiff_compression_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(tiff_compression_option_menu), select);
  xsane_setup.tiff_compression_nr = preferences.tiff_compression_nr;


  /* TIFF ONE BIT IMAGES COMPRESSION */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_TIFF_COMPRESSION_1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  tiff_compression_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, tiff_compression_option_menu, DESC_TIFF_COMPRESSION_1);
  gtk_box_pack_end(GTK_BOX(hbox), tiff_compression_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(tiff_compression_option_menu);
  gtk_widget_show(hbox);

  tiff_compression_menu = gtk_menu_new();

  for (i=1; i <= TIFF_COMPRESSION1_NUMBER; i++)
  {
    tiff_compression_item = gtk_menu_item_new_with_label(tiff_compression1_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(tiff_compression_menu), tiff_compression_item);
    gtk_signal_connect(GTK_OBJECT(tiff_compression_item), "activate",
        (GtkSignalFunc) xsane_setup_tiff_compression_1_callback, (void *) tiff_compression1_strings[i-1].number);
    gtk_widget_show(tiff_compression_item);
    if (tiff_compression1_strings[i-1].number == preferences.tiff_compression_1_nr)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(tiff_compression_option_menu), tiff_compression_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(tiff_compression_option_menu), select);

  xsane_setup.tiff_compression_1_nr = preferences.tiff_compression_1_nr;

#endif

  xsane_separator_new(vbox, 4);

  /* psfile width: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PSFILE_WIDTH);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PSFILE_WIDTH);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.psfile_width);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.psfile_width_entry = text;

  /* psfile height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PSFILE_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PSFILE_HEIGHT);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.psfile_height);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.psfile_height_entry = text;

  /* psfile left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PSFILE_LEFT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PSFILE_LEFTOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.psfile_leftoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.psfile_leftoffset_entry = text;

  /* psfile bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PSFILE_BOTTOM);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PSFILE_BOTTOMOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.psfile_bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.psfile_bottomoffset_entry = text;

  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_saving_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame;
 char buf[64];


  /* Fax options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 5);

  label = gtk_label_new(NOTEBOOK_FAX_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* faxcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_COMMAND);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_command_entry = text;


  /* fax receiver option: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_RECEIVER_OPTION);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_RECEIVER_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_receiver_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_receiver_option_entry = text;

  
/* fax postscript option: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_POSTSCRIPT_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_POSTSCRIPT_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_postscript_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_postscript_option_entry = text;


  /* fax normal mode option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_NORMAL_MODE_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_NORMAL_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_normal_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_normal_option_entry = text;


  /* fax fine mode option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_FINE_MODE_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_FINE_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_fine_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_fine_option_entry = text;


  xsane_separator_new(vbox, 2);


  /* faxviewer */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_VIEWER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_VIEWER);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_viewer);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_viewer_entry = text;


  xsane_separator_new(vbox, 4);

  /* fax width: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_WIDTH);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_WIDTH);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.fax_width);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_width_entry = text;

  /* fax height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_HEIGHT);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.fax_height);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_height_entry = text;

  /* fax left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_LEFT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_LEFTOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.fax_leftoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_leftoffset_entry = text;

  /* fax bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_BOTTOM);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_FAX_BOTTOMOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%3.2f", preferences.fax_bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_bottomoffset_entry = text;

  xsane_separator_new(vbox, 4);

  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_fax_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_display_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame;
 char buf[64];

  /* Display options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 5);

  label = gtk_label_new(NOTEBOOK_DISPLAY_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* main window fixed: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_WINDOW_FIXED);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_MAIN_WINDOW_FIXED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.main_window_fixed);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.main_window_fixed_button = button;


  /* preserve preview image: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_PRESERVE_PRVIEW);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_PRESERVE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.preserve_preview);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_preserve_button = button;


  /* private colormap: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_PRIVATE_COLORMAP);
  xsane_back_gtk_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_COLORMAP);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.preview_own_cmap);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_own_cmap_button = button;


  xsane_separator_new(vbox, 2);

  
/* preview oversampling value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_OVERSAMPLING);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", preferences.preview_oversampling);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_OVERSAMPLING);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_oversampling_entry = text;


  xsane_separator_new(vbox, 2);


  /* preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_GAMMA);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", preferences.preview_gamma);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_entry = text;

  /* red preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_GAMMA_RED);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", preferences.preview_gamma_red);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_red_entry = text;

  /* green preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_GAMMA_GREEN);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", preferences.preview_gamma_green);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_green_entry = text;

  /* blue preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_GAMMA_BLUE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", preferences.preview_gamma_blue);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_blue_entry = text;


  xsane_separator_new(vbox, 2);


  /* docviewer */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_HELPFILE_VIEWER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_DOC_VIEWER);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.doc_viewer);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.doc_viewer_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_display_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_device_notebook_sensitivity(int lineart_mode)
{
 int sensitivity_val = FALSE;
 int sensitivity_mode = FALSE;

  if (lineart_mode == XSANE_LINEART_XSANE)
  {
    sensitivity_val = TRUE;
  }

  if (lineart_mode == XSANE_LINEART_GRAYSCALE)
  {
    sensitivity_val  = TRUE;
    sensitivity_mode = TRUE;
  }

  gtk_widget_set_sensitive(xsane_setup.preview_threshold_min_entry, sensitivity_val);
  gtk_widget_set_sensitive(xsane_setup.preview_threshold_max_entry, sensitivity_val);
  gtk_widget_set_sensitive(xsane_setup.preview_threshold_mul_entry, sensitivity_val);
  gtk_widget_set_sensitive(xsane_setup.preview_threshold_off_entry, sensitivity_val);
  gtk_widget_set_sensitive(xsane_setup.preview_grayscale_scanmode_entry, sensitivity_mode);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_lineart_mode_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.lineart_mode = (int) data;
  xsane_device_notebook_sensitivity(xsane_setup.lineart_mode);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_device_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame;
 GtkWidget *lineart_mode_option_menu, *lineart_mode_menu, *lineart_mode_item;
 char buf[64];
 int i, select = 1;

 typedef struct lineart_mode_t
 {
  char *name;
  int number;
 } lineart_mode;

#define LINEART_MODE_NUMBER 3
 lineart_mode lineart_mode_strings[LINEART_MODE_NUMBER];
 

  /* Device options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 5);

  label = gtk_label_new(NOTEBOOK_DEVICE_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* lineart modus menu  */
  lineart_mode_strings[0].name   = MENU_ITEM_LINEART_MODE_STANDARD;
  lineart_mode_strings[0].number = XSANE_LINEART_STANDARD;
  lineart_mode_strings[1].name   = MENU_ITEM_LINEART_MODE_XSANE;
  lineart_mode_strings[1].number = XSANE_LINEART_XSANE;
  lineart_mode_strings[2].name   = MENU_ITEM_LINEART_MODE_GRAY;
  lineart_mode_strings[2].number = XSANE_LINEART_GRAYSCALE;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_LINEART_MODE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  lineart_mode_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, lineart_mode_option_menu, DESC_LINEART_MODE);
  gtk_box_pack_end(GTK_BOX(hbox), lineart_mode_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(lineart_mode_option_menu);
  gtk_widget_show(hbox);

  lineart_mode_menu = gtk_menu_new();

  for (i=1; i <= LINEART_MODE_NUMBER; i++)
  {
    lineart_mode_item = gtk_menu_item_new_with_label(lineart_mode_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(lineart_mode_menu), lineart_mode_item);
    gtk_signal_connect(GTK_OBJECT(lineart_mode_item), "activate",
       (GtkSignalFunc) xsane_setup_lineart_mode_callback, (void *) lineart_mode_strings[i-1].number);
    gtk_widget_show(lineart_mode_item);
    if (lineart_mode_strings[i-1].number == xsane.lineart_mode)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(lineart_mode_option_menu), lineart_mode_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(lineart_mode_option_menu), select);
  xsane_setup.lineart_mode = xsane.lineart_mode;


  /* threshold minimum value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_THRESHOLD_MIN);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", xsane.threshold_min);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_THRESHOLD_MIN);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_threshold_min_entry = text;

  
  /* threshold maximum value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_THRESHOLD_MAX);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", xsane.threshold_max);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_THRESHOLD_MAX);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_threshold_max_entry = text;

  
  /* threshold multiplier value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_THRESHOLD_MUL);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", xsane.threshold_mul);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_THRESHOLD_MUL);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_threshold_mul_entry = text;

  
  /* threshold offset value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_SETUP_THRESHOLD_OFF);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.2f", xsane.threshold_off);
  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_THRESHOLD_OFF);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_threshold_off_entry = text;


  /* grayscale scanmode name */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_GRAYSCALE_SCANMODE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(dialog->tooltips, text, DESC_GRAYSCALE_SCANMODE);
  gtk_widget_set_usize(text, 250, 0);
  if (xsane.grayscale_scanmode)
  {  
    gtk_entry_set_text(GTK_ENTRY(text), (char *) xsane.grayscale_scanmode);
  }
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.preview_grayscale_scanmode_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_device_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_device_notebook_sensitivity(xsane_setup.lineart_mode);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_setup_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *setup_dialog, *setup_vbox, *hbox, *button, *notebook;
 char buf[64];

  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh);
  xsane_set_sensitivity(FALSE);

  setup_dialog = gtk_dialog_new();
  snprintf(buf, sizeof(buf), "%s %s", prog_name, WINDOW_SETUP);
  gtk_window_set_title(GTK_WINDOW(setup_dialog), buf);
  gtk_signal_connect(GTK_OBJECT(setup_dialog), "destroy", (GtkSignalFunc) xsane_destroy_setup_dialog_callback, setup_dialog);
  xsane_set_window_icon(setup_dialog, 0);

  setup_vbox = GTK_DIALOG(setup_dialog)->vbox;

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(setup_vbox), notebook, FALSE, FALSE, 0);
  gtk_widget_show(notebook);


  xsane_saving_notebook(notebook);
  xsane_printer_notebook(notebook);
  xsane_fax_notebook(notebook);
  xsane_display_notebook(notebook);
  xsane_device_notebook(notebook);


  /* fill in action area: */
  hbox = GTK_DIALOG(setup_dialog)->action_area;

  button = gtk_button_new_with_label(BUTTON_OK);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_options_ok_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_CANCEL);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_setup_dialog_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(setup_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
