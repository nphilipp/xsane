/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998,1999 Oliver Rauch
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

#ifdef _AIX
# include <lalloca.h>   /* MUST come first for AIX! */
#endif

#include "sane/config.h"

#include <lalloca.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/stat.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>

#include "sane/sanei_signal.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-text.h"
#include "xsane-gamma.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

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
  preferences.printer[preferences.printernr]->width              = 576;
  preferences.printer[preferences.printernr]->height             = 835;
  preferences.printer[preferences.printernr]->leftoffset         = 10;
  preferences.printer[preferences.printernr]->bottomoffset       = 10;
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
  if (end > start && v > 0)
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
  if (end > start && v > 0.0)
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
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_width_entry), buf);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_height_entry), buf);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_leftoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_bottomoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_entry), buf);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_red_entry), buf);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_green_entry), buf);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_blue);
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
  xsane_update_int(xsane_setup.printer_width_entry,        &preferences.printer[preferences.printernr]->width);
  xsane_update_int(xsane_setup.printer_height_entry,       &preferences.printer[preferences.printernr]->height);
  xsane_update_int(xsane_setup.printer_leftoffset_entry,   &preferences.printer[preferences.printernr]->leftoffset);
  xsane_update_int(xsane_setup.printer_bottomoffset_entry, &preferences.printer[preferences.printernr]->bottomoffset);

  xsane_update_double(xsane_setup.printer_gamma_entry,       &preferences.printer[preferences.printernr]->gamma);
  xsane_update_double(xsane_setup.printer_gamma_red_entry,   &preferences.printer[preferences.printernr]->gamma_red);
  xsane_update_double(xsane_setup.printer_gamma_green_entry, &preferences.printer[preferences.printernr]->gamma_green);
  xsane_update_double(xsane_setup.printer_gamma_blue_entry,  &preferences.printer[preferences.printernr]->gamma_blue);

  if (option_menu)
  {
    xsane_setup_printer_menu_build(option_menu);
  }
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

static void xsane_setup_display_apply_changes(GtkWidget *widget, gpointer data)
{
  xsane_update_double(xsane_setup.preview_gamma_entry,            &preferences.preview_gamma);
  xsane_update_double(xsane_setup.preview_gamma_red_entry,        &preferences.preview_gamma_red);
  xsane_update_double(xsane_setup.preview_gamma_green_entry,      &preferences.preview_gamma_green);
  xsane_update_double(xsane_setup.preview_gamma_blue_entry,       &preferences.preview_gamma_blue);
  xsane_update_bool(xsane_setup.preview_preserve_button,          &preferences.preserve_preview);
  xsane_update_bool(xsane_setup.preview_own_cmap_button,          &preferences.preview_own_cmap);
  preferences.doc_viewer = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.doc_viewer_entry)));
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data)
{
#ifdef HAVE_LIBJPEG
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_update_scale(xsane_setup.pnm_image_compression_scale, &preferences.png_compression);
#endif
#endif

  xsane_update_bool(xsane_setup.overwrite_warning_button,         &preferences.overwrite_warning);
  xsane_update_bool(xsane_setup.increase_filename_counter_button, &preferences.increase_filename_counter);
  xsane_update_bool(xsane_setup.skip_existing_numbers_button,     &preferences.skip_existing_numbers);
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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup_printer_apply_changes(0, 0);
  xsane_setup_display_apply_changes(0, 0);
  xsane_setup_saving_apply_changes(0, 0);
  xsane_setup_fax_apply_changes(0, 0);


  gtk_widget_destroy((GtkWidget *)data);
  xsane_pref_save();

//  xsane_update_gamma();
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_setup_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *setup_dialog, *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame, *notebook;
 GtkWidget *printer_option_menu;
 char buf[64];

  setup_dialog = gtk_dialog_new();
  snprintf(buf, sizeof(buf), "%s setup", prog_name);
  gtk_window_set_title(GTK_WINDOW(setup_dialog), buf);

  setup_vbox = GTK_DIALOG(setup_dialog)->vbox;

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(setup_vbox), notebook, FALSE, FALSE, 0);
  gtk_widget_show(notebook);




  /* Printer options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Printer options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);



  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new("Printer selection:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  printer_option_menu = gtk_option_menu_new();
  gsg_set_tooltip(dialog->tooltips, printer_option_menu, DESC_PRINTER_SETUP);
  gtk_box_pack_end(GTK_BOX(hbox), printer_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(printer_option_menu);
  gtk_widget_show(hbox);

  xsane_setup_printer_menu_build(printer_option_menu);

  /* printername : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Name:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_NAME);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->name);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_name_entry = text;

  /* printcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Command:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_command_entry = text;

  /* copy number option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Copy number option:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_COPY_NUMBER_OPTION);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->copy_number_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_copy_number_option_entry = text;

  /* printerresolution : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Resolution (dpi):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_RESOLUTION);
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

  label = gtk_label_new("Width (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_WIDTH);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_width_entry = text;

  /* printer height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Height (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_HEIGHT);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_height_entry = text;

  /* printer left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Left offset (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_LEFTOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_leftoffset_entry = text;

  /* printer bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Bottom offset (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_BOTTOMOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_bottomoffset_entry = text;


  xsane_separator_new(vbox, 2);


  /* printer gamma: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma value:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_entry = text;

  /* printer gamma red: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma red:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_red_entry = text;

  /* printer gamma green: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma green:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_green_entry = text;

  /* printer gamma blue: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma blue:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  snprintf(buf, sizeof(buf), "%1.3g", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_blue_entry = text;


  xsane_separator_new(vbox, 4);

  /* "apply" "add printer" "delete printer" */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_apply_changes, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Add printer");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_new, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Delete printer");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_delete, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* Saving options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Saving options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* overwrite warning */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Overwrite warning");
  gsg_set_tooltip(dialog->tooltips, button, DESC_OVERWRITE_WARNING);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.overwrite_warning);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.overwrite_warning_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Increase filename counter");
  gsg_set_tooltip(dialog->tooltips, button, DESC_INCREASE_COUNTER);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.increase_filename_counter);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.increase_filename_counter_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Skip existing numbers");
  gsg_set_tooltip(dialog->tooltips, button, DESC_SKIP_EXISTING);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.skip_existing_numbers);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.skip_existing_numbers_button = button;


#ifdef HAVE_LIBJPEG
//  xsane_scale_new(GTK_BOX(vbox), "JPEG image quality", DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 1.0, 0.0, 0,
//                  &preferences.jpeg_quality, &scale, xsane_quality_changed);
  xsane_scale_new(GTK_BOX(vbox), "JPEG image quality", DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 1.0, 0.0, 0,
                  &preferences.jpeg_quality, (GtkObject **) &xsane_setup.jpeg_image_quality_scale, 0);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_scale_new(GTK_BOX(vbox), "PNG image compression", DESC_PNG_COMPRESSION, 0.0, Z_BEST_COMPRESSION, 1.0, 1.0, 0.0, 0,
                  &preferences.png_compression, (GtkObject **) &xsane_setup.pnm_image_compression_scale, 0);
#endif
#endif

  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_saving_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);





  /* Display options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Display options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* preserve preview image: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Preserve preview image");
  gsg_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_PRESERVE);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preserve_preview);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_preserve_button = button;


  /* private colormap: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Use private colormap");
  gsg_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_COLORMAP);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preview_own_cmap);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_own_cmap_button = button;


  xsane_separator_new(vbox, 2);


  /* preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.3g", preferences.preview_gamma);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_entry = text;

  /* red preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma red:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.3g", preferences.preview_gamma_red);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_red_entry = text;

  /* green preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma green:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.3g", preferences.preview_gamma_green);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_green_entry = text;

  /* blue preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma blue:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%1.3g", preferences.preview_gamma_blue);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_blue_entry = text;


  xsane_separator_new(vbox, 2);


  /* docviewer */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Helpfile viewer (HTML):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_DOC_VIEWER);
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

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_display_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* Fax options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Fax options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* faxcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Command:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_command_entry = text;


  /* fax receiver option: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Receiver option:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_RECEIVER_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_receiver_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_receiver_option_entry = text;

  
/* fax postscript option: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Postscriptfile option:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_POSTSCRIPT_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_postscript_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_postscript_option_entry = text;


  /* fax normal mode option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Normal mode option:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_NORMAL_OPT);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_normal_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_normal_option_entry = text;


  /* fax fine mode option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Fine mode option:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_FINE_OPT);
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

  label = gtk_label_new("Viewer:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_VIEWER);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_viewer);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_viewer_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_fax_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* fill in action area: */
  hbox = GTK_DIALOG(setup_dialog)->action_area;

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_options_ok_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_dialog_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(setup_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
