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
#include "xsane-icons.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"
#include "xsane-scan.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
  {"device-settings", required_argument, 0, 'd'},
  {"scan", no_argument, 0, 's'},
  {"copy", no_argument, 0, 'c'},
  {"no-mode-selection", no_argument, 0, 'n'},
  {"fax", no_argument, 0, 'f'},
  {"Fixed", no_argument, 0, 'F'},
  {"Resizeable", no_argument, 0, 'R'},
  {0, }
};

/* ---------------------------------------------------------------------------------------------------------------------- */

int gsg_message_dialog_active = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

const char *prog_name           = 0;
const char *device_text         = 0;
GtkWidget *choose_device_dialog = 0;
GSGDialog *dialog               = 0;
const SANE_Device **devlist     = 0;
gint seldev = -1;        /* The selected device */
gint ndevs;              /* The number of available devices */
struct Xsane xsane;   

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_scanmode_number[] = { XSANE_SCAN, XSANE_COPY, XSANE_FAX };

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

static int xsane_option_defined(char *string);
static int xsane_parse_options(char *options, char *argv[]);
static void xsane_update_param(GSGDialog *dialog, void *arg);
static void xsane_zoom_update(GtkAdjustment *adj_data, double *val);
static void xsane_resolution_scale_update(GtkAdjustment *adj_data, double *val);
static void xsane_gamma_changed(GtkAdjustment *adj_data, double *val);
static void xsane_modus_callback(GtkWidget *xsane_parent, int *num);
static void xsane_filetype_callback(GtkWidget *xsane_parent, int *num);
static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_browse_filename_callback(GtkWidget *widget, gpointer data);
static void xsane_outputfilename_new(GtkWidget *vbox);
static void xsane_faxreceiver_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_faxproject_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_fine_mode_callback(GtkWidget * widget);
static void xsane_enhancement_rgb_default_callback(GtkWidget * widget);
static void xsane_enhancement_negative_callback(GtkWidget * widget);
static void xsane_auto_enhancement_callback(GtkWidget * widget);
static void xsane_show_standard_options_callback(GtkWidget * widget);
static void xsane_show_advanced_options_callback(GtkWidget * widget);
static void xsane_show_histogram_callback(GtkWidget * widget);
static void xsane_printer_callback(GtkWidget *widget, gpointer data);
static void xsane_update_preview(GSGDialog *dialog, void *arg);
void xsane_pref_save(void);
static void xsane_pref_restore(void);
static void xsane_quit(void);
static void xsane_exit(void);
static gint xsane_standard_option_win_delete(GtkWidget *widget, gpointer data);
static gint xsane_advanced_option_win_delete(GtkWidget *widget, gpointer data);
static gint xsane_scan_win_delete(GtkWidget *w, gpointer data);
static void xsane_preview_window_destroyed(GtkWidget *widget, gpointer call_data);
static void xsane_show_preview_callback(GtkWidget * widget, gpointer call_data);
static void xsane_files_exit_callback(GtkWidget *widget, gpointer data);
static GtkWidget *xsane_files_build_menu(void);
static void xsane_set_pref_unit_callback(GtkWidget *widget, gpointer data);
static void xsane_set_update_policy_callback(GtkWidget *widget, gpointer data);
static gint xsane_close_info_callback(GtkWidget *widget, gpointer data);
static void xsane_info_dialog(GtkWidget *widget, gpointer data);
static void xsane_about_dialog(GtkWidget *widget, gpointer data);
static SANE_Status xsane_get_area_value(int option, float *val, SANE_Int *unit);
#ifdef XSANE_TEST
static void xsane_batch_scan_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_batch_scan_add_callback(GtkWidget *widget, gpointer list);
static void xsane_batch_scan_dialog(GtkWidget *widget, gpointer data);
#endif
static void xsane_fax_dialog(void);
static void xsane_fax_dialog_close(void);
static void xsane_fax_project_delete(void);
void xsane_fax_project_save(void);
static void xsane_fax_project_load(void);
static void xsane_pref_device_save(GtkWidget *widget, gpointer data);
static void xsane_pref_device_load_file(char *filename);
static void xsane_pref_device_restore(void);
static void xsane_pref_device_load(void);
static void xsane_pref_toggle_tooltips(GtkWidget *widget, gpointer data);
static void xsane_show_doc(GtkWidget *widget, gpointer data);
static void xsane_fax_entrys_swap(GtkWidget *list_item_1, GtkWidget *list_item_2);
static void xsane_fax_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_show_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_send(void);
static GtkWidget *xsane_view_build_menu(void);
static GtkWidget *xsane_pref_build_menu(void);
static GtkWidget *xsane_help_build_menu(void);
static void xsane_device_dialog(void);
static void xsane_choose_dialog_ok_callback(void);
static void xsane_select_device_by_key_callback(GtkWidget * widget, gpointer data);
static void xsane_select_device_by_mouse_callback(GtkWidget * widget, GdkEventButton *event, gpointer data);
static gint32 xsane_choose_device(void);
static void xsane_usage(void);
static void xsane_init(int argc, char **argv);
void xsane_interface(int argc, char **argv);
int main(int argc, char ** argv);

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_option_defined(char *string)
{
  if (string)
  {
    while (*string == ' ') /* skip spaces */
    {
      string++;
    }
    if (*string != 0)
    {
      return 1;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_parse_options(char *options, char *argv[])
{
 int optpos = 0;
 int bufpos = 0;
 int arg    = 0;
 char buf[256];

  while (options[optpos] != 0)
  {
    switch(options[optpos])
    {
      case ' ':
        buf[bufpos] = 0;
        argv[arg++] = strdup(buf);
        bufpos = 0;
        optpos++;
       break;

      case '\"':
        optpos++; /* skip " */
        while ((options[optpos] != 0) && (options[optpos] != '\"'))
        {
          buf[bufpos++] = options[optpos++];
        }
        optpos++; /* skip " */
       break;

      default:
        buf[bufpos++] = options[optpos++];
       break;
    }
  }
  buf[bufpos] = 0;
  argv[arg++] = strdup(buf);
  return arg;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Update the info line with the latest size information and update histogram.  */
static void xsane_update_param(GSGDialog *dialog, void *arg)
{
 SANE_Parameters params;
 gchar buf[200];

  if (!xsane.info_label)
  {
    return;
  }

  if (xsane.block_update_param) /* if we change more than one value, we only want to update all once */
  {
    return;
  }

  if (sane_get_parameters(gsg_dialog_get_device(dialog), &params) == SANE_STATUS_GOOD)
    {
      u_long size = 10 * params.bytes_per_line * params.lines;
      const char *unit = "B";

      if (params.format >= SANE_FRAME_RED && params.format <= SANE_FRAME_BLUE)
      {
	size *= 3;
      }

      if (size >= 10 * 1024 * 1024)
      {
        size /= 1024 * 1024;
        unit = "MB";
      }
      else if (size >= 10 * 1024)
      {
        size /= 1024;
        unit = "KB";
      }
      snprintf(buf, sizeof(buf), "%dx%d: %lu.%01lu %s", params.pixels_per_line, params.lines, size / 10, size % 10, unit);

      if (params.format == SANE_FRAME_GRAY)
      {
        xsane.xsane_color = 0;
      }
#ifdef SUPPORT_RGBA
      else if (params.format == SANE_FRAME_RGBA)
      {
        xsane.xsane_color = 4;
      }
#endif
      else /* RGB */
      {
        xsane.xsane_color = 3;
      }
    }
  else
  {
    snprintf(buf, sizeof(buf), TEXT_INVALID_PARAMS);
  }

  gtk_label_set(GTK_LABEL(xsane.info_label), buf);


  if (xsane.preview) preview_update_surface(xsane.preview, 0);

  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_zoom_update(GtkAdjustment *adj_data, double *val)
{
  *val=adj_data->value;

  xsane.resolution = xsane.zoom * preferences.printer[preferences.printernr]->resolution;
  xsane_set_resolution(xsane.resolution);
  xsane_update_param(dialog, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_resolution_scale_update(GtkAdjustment *adj_data, double *val)
{
  *val = adj_data->value;

  xsane_set_resolution(xsane.resolution);
  xsane_update_param(dialog, 0);
  xsane.zoom = xsane.resolution / preferences.printer[preferences.printernr]->resolution;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_resolution_list_callback(GtkWidget *widget, gpointer data)
{
  GSGMenuItem *menu_item = data;
  GSGDialogElement *elem = menu_item->elem;
  GSGDialog *dialog = elem->dialog;
  SANE_Word val;

  sscanf(menu_item->label, "%d", &val);
  xsane.resolution = val;
  xsane_set_resolution(xsane.resolution);
  xsane_update_param(dialog, 0);
  xsane.zoom = xsane.resolution / preferences.printer[preferences.printernr]->resolution;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gamma_changed(GtkAdjustment *adj_data, double *val)
{
  *val = adj_data->value;
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_modus_callback(GtkWidget *xsane_parent, int *num)
{
  xsane.xsane_mode = *num;
  xsane_refresh_dialog(dialog);

  if (xsane.xsane_mode != XSANE_FAX)
  {
    xsane_fax_dialog_close();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_filetype_callback(GtkWidget *xsane_parent, int *num)
{
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data)
{
  if (preferences.filename)
  {
    free((void *) preferences.filename);
  }
  preferences.filename = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_browse_filename_callback(GtkWidget *widget, gpointer data)
{
 char filename[1024];
 char windowname[256];

  xsane_set_sensitivity(FALSE);

  if (preferences.filename) /* make sure a correct filename is defined */
  {
    strncpy(filename, preferences.filename, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
  }
  else /* no filename given, take standard filename */
  {
    strcpy(filename, OUTFILENAME);
  }

  snprintf(windowname, sizeof(windowname), "%s %s %s", prog_name, WINDOW_OUTPUT_FILENAME, device_text);
  gsg_get_filename(windowname, filename, sizeof(filename), filename);
  gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), filename);

  if (preferences.filename)
  {
    free((void *) preferences.filename);
  }

  xsane_set_sensitivity(TRUE);

  preferences.filename = strdup(filename);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_outputfilename_new(GtkWidget *vbox)
{
 GtkWidget *hbox;
 GtkWidget *text;
 GtkWidget *button;
 GtkWidget *xsane_filetype_menu, *xsane_filetype_item, *xsane_filetype_option_menu;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = xsane_button_new_with_pixmap(hbox, file_xpm, DESC_BROWSE_FILENAME, (GtkSignalFunc) xsane_browse_filename_callback, 0);

  text = gtk_entry_new_with_max_length(255);
  gtk_widget_set_usize(text, 80, 0); /* set minimum size */
  gsg_set_tooltip(dialog->tooltips, text, DESC_FILENAME);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.filename);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_outputfilename_changed_callback, 0);

  xsane.outputfilename_entry = text;

  xsane_filetype_menu = gtk_menu_new();

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_BY_EXT);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, NULL);
  gtk_widget_show(xsane_filetype_item);

#ifdef HAVE_LIBJPEG
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_JPEG);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_JPEG);
  gtk_widget_show(xsane_filetype_item);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNG);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_PNG);
  gtk_widget_show(xsane_filetype_item);
#endif
#endif

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNM);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_PNM);
  gtk_widget_show(xsane_filetype_item);

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PS);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_PS);
  gtk_widget_show(xsane_filetype_item);

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_RAW);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_RAW);
  gtk_widget_show(xsane_filetype_item);

#ifdef HAVE_LIBTIFF
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TIFF);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  gtk_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate",
                     (GtkSignalFunc) xsane_filetype_callback, MENU_ITEM_FILETYPE_TIFF);
  gtk_widget_show(xsane_filetype_item);
#endif

  xsane_filetype_option_menu = gtk_option_menu_new();
  gsg_set_tooltip(dialog->tooltips, xsane_filetype_option_menu, DESC_FILETYPE);
  gtk_box_pack_end(GTK_BOX(hbox), xsane_filetype_option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_filetype_option_menu), xsane_filetype_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_filetype_option_menu), 0);
  gtk_widget_show(xsane_filetype_option_menu);
  gtk_widget_set_sensitive(xsane_filetype_option_menu, FALSE);

  gtk_widget_show(text);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_faxreceiver_changed_callback(GtkWidget *widget, gpointer data)
{
  if (xsane.fax_receiver)
  {
    free((void *) xsane.fax_receiver);
  }
  xsane.fax_receiver = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_fax_project_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_faxproject_changed_callback(GtkWidget *widget, gpointer data)
{
  if (preferences.fax_project)
  {
    free((void *) preferences.fax_project);
  }
  preferences.fax_project = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_fax_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_fine_mode_callback(GtkWidget * widget)
{
  xsane.fax_fine_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  if (xsane.fax_fine_mode)
  {
    xsane.resolution = 196;
  }
  else
  {
    xsane.resolution = 98;
  }

  xsane_set_resolution(xsane.resolution);
  xsane_update_param(dialog, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
static void xsane_enhancement_rgb_default_callback(GtkWidget * widget)
{
  xsane.enhancement_rgb_default = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  if (xsane.enhancement_rgb_default)
  {
    xsane.gamma_red        = 1.0;
    xsane.gamma_green      = 1.0;
    xsane.gamma_blue       = 1.0;

    xsane.brightness_red   = 0.0;
    xsane.brightness_green = 0.0;
    xsane.brightness_blue  = 0.0;

    xsane.contrast_red     = 0.0;
    xsane.contrast_green   = 0.0;
    xsane.contrast_blue    = 0.0;

    xsane.slider_red.value[0] =   0.0;
    xsane.slider_red.value[1] =  50.0;
    xsane.slider_red.value[2] = 100.0;

    xsane.slider_green.value[0] =   0.0;
    xsane.slider_green.value[1] =  50.0;
    xsane.slider_green.value[2] = 100.0;

    xsane.slider_blue.value[0] =   0.0;
    xsane.slider_blue.value[1] =  50.0;
    xsane.slider_blue.value[2] = 100.0;
  }
  else
  {
    xsane.slider_red.value[0] = xsane.slider_gray.value[0];
    xsane.slider_red.value[1] = xsane.slider_gray.value[1];
    xsane.slider_red.value[2] = xsane.slider_gray.value[2];

    xsane.slider_green.value[0] = xsane.slider_gray.value[0];
    xsane.slider_green.value[1] = xsane.slider_gray.value[1];
    xsane.slider_green.value[2] = xsane.slider_gray.value[2];

    xsane.slider_blue.value[0] = xsane.slider_gray.value[0];
    xsane.slider_blue.value[1] = xsane.slider_gray.value[1];
    xsane.slider_blue.value[2] = xsane.slider_gray.value[2];
  }

  xsane_update_sliders();
  xsane_update_gamma();
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_negative_callback(GtkWidget * widget)
{
  xsane.negative = (GTK_TOGGLE_BUTTON(widget)->active != 0);
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_auto_enhancement_callback(GtkWidget * widget)
{
  xsane_calculate_histogram();

  xsane.slider_gray.value[0] = xsane.auto_black;
  xsane.slider_gray.value[1] = xsane.auto_gray;
  xsane.slider_gray.value[2] = xsane.auto_white;

  if (xsane.enhancement_rgb_default) /* set same values for color components */
  {
    xsane.slider_red.value[0] = xsane.auto_black;
    xsane.slider_red.value[1] = xsane.auto_gray;
    xsane.slider_red.value[2] = xsane.auto_white;
  
    xsane.slider_green.value[0] = xsane.auto_black;
    xsane.slider_green.value[1] = xsane.auto_gray;
    xsane.slider_green.value[2] = xsane.auto_white;
  
    xsane.slider_blue.value[0] = xsane.auto_black;
    xsane.slider_blue.value[1] = xsane.auto_gray;
    xsane.slider_blue.value[2] = xsane.auto_white;
  }
  else /* set different values for each color component */
  {
    xsane.slider_red.value[0] = xsane.auto_black_red;
    xsane.slider_red.value[1] = xsane.auto_gray_red;
    xsane.slider_red.value[2] = xsane.auto_white_red;
  
    xsane.slider_green.value[0] = xsane.auto_black_green;
    xsane.slider_green.value[1] = xsane.auto_gray_green;
    xsane.slider_green.value[2] = xsane.auto_white_green;
  
    xsane.slider_blue.value[0] = xsane.auto_black_blue;
    xsane.slider_blue.value[1] = xsane.auto_gray_blue;
    xsane.slider_blue.value[2] = xsane.auto_white_blue;
  }

  xsane_enhancement_by_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_standard_options_callback(GtkWidget * widget)
{
  preferences.show_standard_options = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_standard_options)
  {
    gtk_widget_show(xsane.standard_options_shell);
  }
  else
  {
    gtk_widget_hide(xsane.standard_options_shell);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_advanced_options_callback(GtkWidget * widget)
{
  preferences.show_advanced_options = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_advanced_options)
  {
    gtk_widget_show(xsane.advanced_options_shell);
  }
  else
  {
    gtk_widget_hide(xsane.advanced_options_shell);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_resolution_list_callback(GtkWidget * widget)
{
  preferences.show_resolution_list = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  xsane_refresh_dialog(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_histogram_callback(GtkWidget * widget)
{
  preferences.show_histogram = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_histogram)
  {
    xsane_update_histogram();
    gtk_widget_show(xsane.histogram_dialog);
  }
  else
  {
    gtk_widget_hide(xsane.histogram_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printer_callback(GtkWidget *widget, gpointer data)
{
  preferences.printernr = (int) data;
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_update_xsane_callback()
{
  /* creates the XSane option window */

  GtkWidget *xsane_vbox, *xsane_hbox;
  GtkWidget *xsane_modus_menu;
  GtkWidget *xsane_modus_item;
  GtkWidget *xsane_modus_option_menu;
  GtkWidget *xsane_vbox_xsane_modus;
  GtkWidget *xsane_hbox_xsane_modus;
  GtkWidget *xsane_label;
  GtkWidget *xsane_text;
  GtkWidget *xsane_hbox_xsane_enhancement;
  GtkWidget *xsane_frame;
  GtkWidget *xsane_button;
  GSGDialogElement *elem;
  gchar buf[200];

  /* xsane main options */

  xsane_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(xsane_hbox);
  xsane_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(xsane_vbox);
/*  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, FALSE, FALSE, 0); */ /* make scales fixed */
  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, TRUE, TRUE, 0); /* make scales sizeable */

  /* XSane Frame */

  xsane_frame = gtk_frame_new(TEXT_XSANE_OPTIONS);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(xsane_frame), GTK_SHADOW_ETCHED_IN);
/*  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_frame, FALSE, FALSE, 0); */ /* fixed frameheight */
  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(xsane_frame);

/*  xsane_vbox_xsane_modus = gtk_vbox_new(FALSE, 5); */
   xsane_vbox_xsane_modus = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(xsane_frame), xsane_vbox_xsane_modus);
  gtk_widget_show(xsane_vbox_xsane_modus);

/* scan copy fax selection */

  if ( (xsane.mode == XSANE_STANDALONE) && (xsane.mode_selection) ) /* display xsane mode selection menu */
  {
    xsane_hbox_xsane_modus = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(xsane_hbox_xsane_modus), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_modus, FALSE, FALSE, 0);

    xsane_label = gtk_label_new(TEXT_XSANE_MODE);
    gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_modus), xsane_label, FALSE, FALSE, 2);
    gtk_widget_show(xsane_label);

    xsane_modus_menu = gtk_menu_new();

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_SCAN);
    gtk_widget_set_usize(xsane_modus_item, 60, 0);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_SCAN]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_COPY);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_COPY]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_FAX);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_FAX]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_option_menu = gtk_option_menu_new();
    gsg_set_tooltip(dialog->tooltips, xsane_modus_option_menu, DESC_XSANE_MODE);
    gtk_box_pack_end(GTK_BOX(xsane_hbox_xsane_modus), xsane_modus_option_menu, FALSE, FALSE, 2);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_modus_option_menu), xsane_modus_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_modus_option_menu), xsane.xsane_mode);
    gtk_widget_show(xsane_modus_option_menu);
    gtk_widget_show(xsane_hbox_xsane_modus);

    dialog->xsanemode_widget = xsane_modus_option_menu;
  }

  {
   GtkWidget *pixmapwidget;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   GtkWidget *hbox;
   const SANE_Option_Descriptor *opt;


    /* colormode */
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.scanmode);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        hbox = gtk_hbox_new(FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) colormode_xpm);
        pixmapwidget = gtk_pixmap_new(pixmap, mask);
        gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
        gdk_pixmap_unref(pixmap);
        gtk_widget_show(pixmapwidget);

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_STRING_LIST:
          {
            char *set;
            SANE_Status status;

            /* use a "list-selection" widget */
            set = malloc(opt->size);
            status = sane_control_option(dialog->dev, dialog->well_known.scanmode, SANE_ACTION_GET_VALUE, set, 0);

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scanmode,
                                  _BGT(opt->desc), 0, SANE_OPTION_IS_SETTABLE(opt->cap));
          }
           break;

           default:
            fprintf(stderr, "scanmode_selection: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
        }
        gtk_widget_show(hbox);
      }
    }


    /* input selection */
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.scansource);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        hbox = gtk_hbox_new(FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) scanner_xpm);
        pixmapwidget = gtk_pixmap_new(pixmap, mask);
        gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
        gdk_pixmap_unref(pixmap);
        gtk_widget_show(pixmapwidget);

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_STRING_LIST:
          {
           char *set;
           SANE_Status status;

            /* use a "list-selection" widget */
            set = malloc(opt->size);
            status = sane_control_option(dialog->dev, dialog->well_known.scansource, SANE_ACTION_GET_VALUE, set, 0);

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scansource,
                                  _BGT(opt->desc), 0, SANE_OPTION_IS_SETTABLE(opt->cap));
           }
           break;

          default:
            fprintf(stderr, "scansource_selection: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
        }
        gtk_widget_show(hbox);
      }
    }

  }

  if (xsane.xsane_mode == XSANE_SCAN)
  {
   const SANE_Option_Descriptor *opt;

    xsane.copy_number_entry = 0;

    if (xsane.mode == XSANE_STANDALONE)
    {
      xsane_outputfilename_new(xsane_vbox_xsane_modus);
    }

    /* resolution selection */
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_RANGE:
          {
           SANE_Word quant=0;
           SANE_Word min=0;
           SANE_Word max=0;
           SANE_Word val=0;

            gtk_widget_set_sensitive(xsane.show_resolution_list_widget, TRUE); 
            sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &val, 0); 

            switch (opt->type)
            {
              case SANE_TYPE_INT:
                min   = opt->constraint.range->min;
                max   = opt->constraint.range->max;
                quant = opt->constraint.range->quant;
              break;

              case SANE_TYPE_FIXED:
                min   = SANE_UNFIX(opt->constraint.range->min);
                max   = SANE_UNFIX(opt->constraint.range->max);
                quant = SANE_UNFIX(opt->constraint.range->quant);
                val   = SANE_UNFIX(val);
              break;

              default:
                fprintf(stderr, "zoom_scale_update: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
/* xxxxxxxxxxxxxxxxxxxxxxxxxxx */
            }

            if (quant == 0)
            {
              quant = 1;
            }

            if (!xsane.resolution) /* no prefered value */
            {
              xsane.resolution = val; /* set backend predefined value */
            }

            if (!preferences.show_resolution_list) /* user wants slider */ 
            {
              xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), resolution_xpm, DESC_RESOLUTION,
                                          min, max, quant, quant, 0.0, 0, &xsane.resolution, &xsane.resolution_widget,
                                          dialog->well_known.dpi, xsane_resolution_scale_update, SANE_OPTION_IS_SETTABLE(opt->cap));
            }
            else /* user wants list instead of slider */
            {
             SANE_Int max_items = 20;
             char **str_list;
             char str[16];
             int i;
             int j = 0;
             SANE_Word wanted_res;
             SANE_Word val = max;
             int res = max;
             double mul;

              sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &wanted_res, 0); 
              if (opt->type == SANE_TYPE_FIXED)
              {
                wanted_res = (int) SANE_UNFIX(wanted_res);
              }

              if (xsane.resolution) /* prefered value */
              {
                wanted_res = xsane.resolution; /* set frontend prefered value */
              }
 
              str_list = malloc((max_items + 1) * sizeof(str_list[0]));

              sprintf(str, "%d", max);
              str_list[j++] = strdup(str);

              i=9;
              while ((j < max_items) && (res > 50) && (res > min) && (i > 0))
              {
                mul = ((double) i) / (i+1);
                res = (int) (max * mul);
                if  (res/mul == max)
                {
                  sprintf(str, "%d", res);
                  str_list[j++] = strdup(str);
                  if (res >= wanted_res)
                  {
                    val = res;
                  }
                }
                i--;
              }

              i = 3;
              while ((j < max_items) && (res > 50) && (res > min))
              {
                mul = 1.0/i;
                res = max * mul;
                if (res/mul  == max)
                {
                  sprintf(str, "%d", res);
                  str_list[j++] = strdup(str);
                  if (res >= wanted_res)
                  {
                    val = res;
                  }
                }
                i++;
              }

              str_list[j] = 0;
              sprintf(str, "%d", (int) val);

              xsane_option_menu_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), resolution_xpm, DESC_RESOLUTION,
                                                str_list, str, &xsane.resolution_widget, dialog->well_known.dpi,
                                                xsane_resolution_list_callback, SANE_OPTION_IS_SETTABLE(opt->cap));
              free(str_list);
              xsane.resolution = val;
              xsane_set_resolution(xsane.resolution);
            }

            elem = dialog->element + dialog->well_known.dpi;
            elem->data = xsane.resolution_widget;
          }
          break;

          case SANE_CONSTRAINT_WORD_LIST:
          {
           /* use a "list-selection" widget */
           SANE_Int items;
           char **str_list;
           char str[16];
           int j;
           SANE_Word val=0;

            gtk_widget_set_sensitive(xsane.show_resolution_list_widget, FALSE); 

            items = opt->constraint.word_list[0];
            str_list = malloc((items + 1) * sizeof(str_list[0]));
            switch (opt->type)
            {
              case SANE_TYPE_INT:
                for (j = 0; j < items; ++j)
                {
                  sprintf(str, "%d", opt->constraint.word_list[j + 1]);
                  str_list[j] = strdup(str);
                }
                str_list[j] = 0;
                sane_control_option (dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &val, 0); 
                sprintf(str, "%d", (int) val);
              break;

              case SANE_TYPE_FIXED:
                for (j = 0; j < items; ++j)
                {
                  sprintf(str, "%d", (int) SANE_UNFIX(opt->constraint.word_list[j + 1]));
                  str_list[j] = strdup(str);
                 }
                str_list[j] = 0;
                sane_control_option (dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &val, 0); 
                sprintf(str, "%d", (int) SANE_UNFIX(val));
              break;

              default:
                fprintf(stderr, "resolution_word_list_creation: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
            }


            xsane_option_menu_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), resolution_xpm, DESC_RESOLUTION,
                                              str_list, str, &xsane.resolution_widget, dialog->well_known.dpi,
                                              xsane_resolution_list_callback, SANE_OPTION_IS_SETTABLE(opt->cap));
            free(str_list);
            elem = dialog->element + dialog->well_known.dpi;
            elem->data = xsane.resolution_widget;
          }
          break;

          default:
           break;
        }	/* constraint type */
      }		/* if resolution option active */
    } /* if (opt) */
  }
  else if (xsane.xsane_mode == XSANE_COPY)
  {
   const SANE_Option_Descriptor *opt;
   GtkWidget *pixmapwidget, *hbox, *xsane_printer_option_menu, *xsane_printer_menu, *xsane_printer_item;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   int i;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

    pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) printer_xpm);
    pixmapwidget = gtk_pixmap_new(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
    gdk_pixmap_unref(pixmap);
    gtk_widget_show(pixmapwidget);

    xsane_printer_menu = gtk_menu_new();

    for (i=0; i < preferences.printerdefinitions; i++)
    {
      xsane_printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
      gtk_container_add(GTK_CONTAINER(xsane_printer_menu), xsane_printer_item);
      gtk_signal_connect(GTK_OBJECT(xsane_printer_item), "activate", (GtkSignalFunc) xsane_printer_callback, (void *) i);
      gtk_widget_show(xsane_printer_item);
    }

    xsane_printer_option_menu = gtk_option_menu_new();
    gsg_set_tooltip(dialog->tooltips, xsane_printer_option_menu, DESC_PRINTER_SELECT);
    gtk_box_pack_end(GTK_BOX(hbox), xsane_printer_option_menu, FALSE, FALSE, 2);
    gtk_widget_show(xsane_printer_option_menu);
    gtk_widget_show(hbox);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_printer_option_menu), xsane_printer_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_printer_option_menu), preferences.printernr);

    /* number of copies */
    xsane_text = gtk_entry_new();
    gsg_set_tooltip(dialog->tooltips, xsane_text, DESC_COPY_NUMBER);
    gtk_widget_set_usize(xsane_text, 25, 0);
    snprintf(buf, sizeof(buf), "%d", xsane.copy_number);    
    gtk_entry_set_text(GTK_ENTRY(xsane_text), (char *) buf);
    gtk_box_pack_end(GTK_BOX(hbox), xsane_text, FALSE, FALSE, 10);
    gtk_widget_show(xsane_text);
    gtk_widget_show(hbox);
    xsane.copy_number_entry = xsane_text;

    /* zoom selection */
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
       double quant=0.0;
       double min=0.0;
       double max=0.0;

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_RANGE:
            switch (opt->type)
            {
              case SANE_TYPE_INT:
                min   = ((double) opt->constraint.range->min) / preferences.printer[preferences.printernr]->resolution;
                max   = ((double) opt->constraint.range->max) / preferences.printer[preferences.printernr]->resolution;
                quant = ((double) opt->constraint.range->quant) / preferences.printer[preferences.printernr]->resolution;
              break;

              case SANE_TYPE_FIXED:
                min   = SANE_UNFIX(opt->constraint.range->min) / preferences.printer[preferences.printernr]->resolution;
                max   = SANE_UNFIX(opt->constraint.range->max) / preferences.printer[preferences.printernr]->resolution;
                quant = SANE_UNFIX(opt->constraint.range->quant) / preferences.printer[preferences.printernr]->resolution;
              break;

              default:
                fprintf(stderr, "zoom_scale_update: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
            }
          break;

          case SANE_CONSTRAINT_WORD_LIST:
            xsane_get_bounds(opt, &min, &max);
            min   = min / preferences.printer[preferences.printernr]->resolution;
            max   = max / preferences.printer[preferences.printernr]->resolution;
            quant = 0.0;
          break;

          default:
            fprintf(stderr, "zoom_scale_update: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
        }

        if (quant == 0)
        {
          quant = 0.01;
        }

        xsane.zoom = xsane.resolution / preferences.printer[preferences.printernr]->resolution;

        xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), zoom_xpm, DESC_ZOOM, min, max, quant, quant, 0.0, 2,
                                    &xsane.zoom, &xsane.zoom_widget, dialog->well_known.dpi, xsane_zoom_update,
                                    SANE_OPTION_IS_SETTABLE(opt->cap));
      }
    }
  }
  else /* XSANE_FAX */
  {
   const SANE_Option_Descriptor *opt;

    xsane.copy_number_entry = 0;
    xsane.resolution = 98;

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        xsane_button = gtk_check_button_new_with_label(RADIO_BUTTON_FINE_MODE);
        gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_FAX_FINE_MODE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane_button), xsane.fax_fine_mode);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_button, FALSE, FALSE, 2);
        gtk_widget_show(xsane_button);
        gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_fax_fine_mode_callback, 0);

        if (xsane.fax_fine_mode)
        {
          xsane.resolution = 196;
        }
        xsane_set_resolution(xsane.resolution);
      }
    }
    xsane_fax_dialog();
  }

   /* test if scanner gamma table is selected */

   xsane.scanner_gamma_gray  = FALSE;
   if (dialog->well_known.gamma_vector >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_gray  = TRUE;
     }
   }

   xsane.scanner_gamma_color  = FALSE;
   if (dialog->well_known.gamma_vector_r >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_color  = TRUE;
     }
   }



  /* XSane Frame Enhancement */

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  if (xsane.param.depth == 1)
  {
    return(xsane_hbox);
  }

  xsane.slider_gray.active   = XSANE_SLIDER_ACTIVE; /* mark slider active */

  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_xpm, DESC_GAMMA,
                              XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.01, 0.0, 2,
                              &xsane.gamma, &xsane.gamma_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_red_xpm, DESC_GAMMA_R,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_red  , &xsane.gamma_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_green_xpm, DESC_GAMMA_G,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_green, &xsane.gamma_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_blue_xpm, DESC_GAMMA_B,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_blue , &xsane.gamma_blue_widget, 0, xsane_gamma_changed, TRUE);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_xpm, DESC_BRIGHTNESS,
                              XSANE_BRIGHTNESS_MIN, XSANE_BRIGHTNESS_MAX, 1.0, 1.0, 0.0, 0,
                              &xsane.brightness, &xsane.brightness_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_red_xpm, DESC_BRIGHTNESS_R,
                                XSANE_BRIGHTNESS_MIN, XSANE_BRIGHTNESS_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_red  , &xsane.brightness_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_green_xpm, DESC_BRIGHTNESS_G,
                                XSANE_BRIGHTNESS_MIN, XSANE_BRIGHTNESS_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_green, &xsane.brightness_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_blue_xpm, DESC_BRIGHTNESS_B,
                                XSANE_BRIGHTNESS_MIN, XSANE_BRIGHTNESS_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_blue,  &xsane.brightness_blue_widget, 0, xsane_gamma_changed, TRUE);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_xpm, DESC_CONTRAST,
                              XSANE_CONTRAST_GRAY_MIN, XSANE_CONTRAST_MAX, 1.0, 1.0, 0.0, 0,
                              &xsane.contrast, &xsane.contrast_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_red_xpm, DESC_CONTRAST_R,
                                XSANE_CONTRAST_MIN, XSANE_CONTRAST_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_red  , &xsane.contrast_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_green_xpm, DESC_CONTRAST_G,
                                XSANE_CONTRAST_MIN, XSANE_CONTRAST_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_green, &xsane.contrast_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_blue_xpm, DESC_CONTRAST_B,
                                XSANE_CONTRAST_MIN, XSANE_CONTRAST_MAX, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_blue,  &xsane.contrast_blue_widget, 0, xsane_gamma_changed, TRUE);
  }

    xsane_separator_new(xsane_vbox_xsane_modus, 2);

  /* create lower button box (rgb default, negative ,... */
  xsane_hbox_xsane_enhancement = gtk_hbox_new(TRUE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_hbox_xsane_enhancement), 4);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_enhancement, FALSE, FALSE, 0);
  gtk_widget_show(xsane_hbox_xsane_enhancement);

  if (xsane.xsane_color)
  {
    xsane_toggle_button_new_with_pixmap(xsane_hbox_xsane_enhancement, rgb_default_xpm, DESC_RGB_DEFAULT,
                                        &xsane.enhancement_rgb_default, xsane_enhancement_rgb_default_callback);
  }

  xsane_toggle_button_new_with_pixmap(xsane_hbox_xsane_enhancement, negative_xpm, DESC_NEGATIVE,
                                      &xsane.negative, xsane_enhancement_negative_callback);

  xsane_button_new_with_pixmap(xsane_hbox_xsane_enhancement, enhance_xpm, DESC_ENH_AUTO,
                               xsane_auto_enhancement_callback, 0);

  xsane_button_new_with_pixmap(xsane_hbox_xsane_enhancement, default_enhancement_xpm, DESC_ENH_DEFAULT,
                               xsane_enhancement_restore_default, 0);

  xsane_button_new_with_pixmap(xsane_hbox_xsane_enhancement, restore_enhancement_xpm, DESC_ENH_RESTORE,
                               xsane_enhancement_restore, 0);

  xsane_button_new_with_pixmap(xsane_hbox_xsane_enhancement, store_enhancement_xpm, DESC_ENH_STORE,
                               xsane_enhancement_store, 0);

  xsane_update_histogram();

 return(xsane_hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_preview(GSGDialog *dialog, void *arg)
{
  if (xsane.preview)
  {
    preview_update_surface(xsane.preview, 0);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_pref_save(void)
{
  char filename[PATH_MAX];
  int fd;

  /* first save xsane-specific preferences: */
  gsg_make_path(sizeof(filename), filename, "xsane", 0, "xsane", 0, ".rc");
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    gsg_error(buf, TRUE);
    return;
  }
  preferences_save(fd);
  close(fd);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_restore(void)
{
  char filename[PATH_MAX];
  int fd;

  gsg_make_path(sizeof(filename), filename, "xsane", 0, "xsane", 0, ".rc");
  fd = open(filename, O_RDONLY);
  if (fd >= 0)
  {
    preferences_restore(fd);
    close(fd);
  }

  if (!preferences.filename)
  {
    preferences.filename = strdup(OUTFILENAME);
  }

  if (preferences.printerdefinitions == 0)
  {
    xsane_new_printer();
  }

  if (!preferences.fax_project)
  {
    preferences.fax_project = strdup(FAXPROJECT);
  }

  if (!preferences.fax_command)
  {
    preferences.fax_command = strdup(FAXCOMMAND);
  }

  if (!preferences.fax_receiver_option)
  {
    preferences.fax_receiver_option = strdup(FAXRECEIVEROPT);
  }

  if (!preferences.fax_postscript_option)
  {
    preferences.fax_postscript_option = strdup(FAXPOSTSCRIPTOPT);
  }

  if (!preferences.fax_normal_option)
  {
    preferences.fax_normal_option = strdup(FAXNORMALOPT);
  }

  if (!preferences.fax_fine_option)
  {
    preferences.fax_fine_option = strdup(FAXFINEOPT);
  }

  if (!preferences.fax_viewer)
  {
    preferences.fax_viewer = strdup(FAXVIEWER);
  }

  if (!preferences.doc_viewer)
  {
    preferences.doc_viewer = strdup(DOCVIEWER);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_quit(void)
{
  if (xsane.preview)
  {
    Preview *preview = xsane.preview;
    xsane.preview = 0;
    preview_destroy(preview);
  }

  while (gsg_message_dialog_active)
  {
    gtk_main_iteration();
  }

  if (dialog && gsg_dialog_get_device(dialog))
  {
    sane_close(gsg_dialog_get_device(dialog));
  }

  sane_exit();
  gtk_main_quit();

  if (xsane.preview_gamma_data_red)
  {
    free(xsane.preview_gamma_data_red);
    free(xsane.preview_gamma_data_green);
    free(xsane.preview_gamma_data_blue);

    xsane.preview_gamma_data_red   = 0;
    xsane.preview_gamma_data_green = 0;
    xsane.preview_gamma_data_blue  = 0;
  }

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == XSANE_GIMP_EXTENSION)
  {
    gimp_quit();
  }
#endif
  exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_exit(void) /* this is called when xsane exits before gtk_main is called */
{
  while (gsg_message_dialog_active)
  {
    gtk_main_iteration();
  }

  sane_exit();

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == XSANE_GIMP_EXTENSION)
  {
    gimp_quit();
  }
#endif
  exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_standard_option_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_standard_options = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_advanced_option_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_advanced_options = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when window manager's "delete" (or "close") function is invoked.  */
static gint xsane_scan_win_delete(GtkWidget *w, gpointer data)
{
  xsane_scan_done(-1); /* stop scanner when still scanning */
  xsane_pref_save();
  xsane_quit();
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_preview_window_destroyed(GtkWidget *widget, gpointer call_data)
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(call_data), FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the preview button is pressed. */

static void xsane_show_preview_callback(GtkWidget * widget, gpointer call_data)
{
  if (GTK_CHECK_MENU_ITEM(widget)->active)
  {
    if (!xsane.preview)
    {
      xsane.preview = preview_new(dialog);
      if (xsane.preview && xsane.preview->top)
      {
        gtk_signal_connect(GTK_OBJECT(xsane.preview->top), "destroy", GTK_SIGNAL_FUNC(xsane_preview_window_destroyed), widget);
        xsane.show_preview = TRUE;
      }
      else
      {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), FALSE);
      }
    }
  }
  else if (xsane.preview)
  {
    preview_destroy(xsane.preview);
    xsane.preview = 0;
    xsane.show_preview = FALSE;
  }
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_files_exit_callback(GtkWidget *widget, gpointer data)
{
  xsane_pref_save();
  xsane_quit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_files_build_menu(void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new();

  item = gtk_menu_item_new();
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_widget_show(item);


  /* XSane about dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_ABOUT);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_about_dialog, 0);
  gtk_widget_show(item);


  /* XSane info dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_INFO);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_info_dialog, 0);
  gtk_widget_show(item);

  
  /* Exit */

  item = gtk_menu_item_new_with_label(MENU_ITEM_EXIT);
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_files_exit_callback, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_pref_unit_callback(GtkWidget *widget, gpointer data)
{
 const char *unit = data;
 double unit_conversion_factor = 1.0;

  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.length_unit_mm), (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.length_unit_cm), (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.length_unit_in), (GtkSignalFunc) xsane_set_pref_unit_callback, "in");

  if (strcmp(unit, "mm") == 0)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_mm), TRUE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_cm), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_in), FALSE);
  }
  else if (strcmp(unit, "cm") == 0)
  {
    unit_conversion_factor = 10.0;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_mm), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_cm), TRUE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_in), FALSE);
  }
  else if (strcmp(unit, "in") == 0)
  {
    unit_conversion_factor = 25.4;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_mm), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_cm), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.length_unit_in), TRUE);
  }

  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.length_unit_mm), (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.length_unit_cm), (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.length_unit_in), (GtkSignalFunc) xsane_set_pref_unit_callback, "in");

  preferences.length_unit = unit_conversion_factor;

  xsane_refresh_dialog(dialog);
  if (xsane.preview)
  {
    preview_area_resize(xsane.preview->window);
  }

  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_update_policy_callback(GtkWidget *widget, gpointer data)
{
 GtkUpdateType policy = (GtkUpdateType) data;

  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.update_policy_continu), (GtkSignalFunc) xsane_set_update_policy_callback,
                                   (void *) GTK_UPDATE_CONTINUOUS);
  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.update_policy_discont), (GtkSignalFunc) xsane_set_update_policy_callback,
                                   (void *) GTK_UPDATE_DISCONTINUOUS);
  gtk_signal_handler_block_by_func(GTK_OBJECT(xsane.update_policy_delayed), (GtkSignalFunc) xsane_set_update_policy_callback,
                                   (void *) GTK_UPDATE_DELAYED);

  if (policy == GTK_UPDATE_CONTINUOUS)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_continu), TRUE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_discont), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_delayed), FALSE);
  }
  else if (policy == GTK_UPDATE_DISCONTINUOUS)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_continu), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_discont), TRUE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_delayed), FALSE);
  }
  else if (policy == GTK_UPDATE_DELAYED)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_continu), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_discont), FALSE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.update_policy_delayed), TRUE);
  }

  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.update_policy_continu), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_CONTINUOUS);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.update_policy_discont), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_DISCONTINUOUS);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(xsane.update_policy_delayed), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_DELAYED);

  preferences.gtk_update_policy = policy;
  xsane_pref_save();

  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_close_info_callback(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = data;

  gtk_widget_destroy(dialog);

  xsane_set_sensitivity(TRUE);

  xsane_update_histogram();

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_info_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *info_dialog, *vbox, *button, *label, *frame, *framebox, *hbox, *table;
 char buf[256];
 char *bufptr;

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  info_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_position(GTK_WINDOW(info_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(info_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(info_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_info_callback), info_dialog);
  snprintf(buf, sizeof(buf), "%s %s %s", prog_name, WINDOW_INFO, device_text);
  gtk_window_set_title(GTK_WINDOW(info_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_container_add(GTK_CONTAINER(info_dialog), vbox);
  gtk_widget_show(vbox);

  frame = gtk_frame_new(TEXT_SCANNER_BACKEND);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(6, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  snprintf(buf, sizeof(buf), TEXT_VENDOR);
  label = xsane_info_table_text_new(table, buf, 0, 0);
  snprintf(buf, sizeof(buf), "%s", devlist[seldev]->vendor);
  label = xsane_info_table_text_new(table, buf, 1, 0);

  snprintf(buf, sizeof(buf), TEXT_MODEL);
  label = xsane_info_table_text_new(table, buf, 0, 1);
  snprintf(buf, sizeof(buf), "%s", devlist[seldev]->model);
  label = xsane_info_table_text_new(table, buf, 1, 1);

  snprintf(buf, sizeof(buf), TEXT_TYPE);
  label = xsane_info_table_text_new(table, buf, 0, 2);
  snprintf(buf, sizeof(buf), "%s", devlist[seldev]->type);
  label = xsane_info_table_text_new(table, buf, 1, 2);

  snprintf(buf, sizeof(buf), TEXT_DEVICE);
  label = xsane_info_table_text_new(table, buf, 0, 3);
  bufptr = strrchr(devlist[seldev]->name, ':');
  if (bufptr)
  {
    snprintf(buf, sizeof(buf), "%s", bufptr+1);
  }
  else
  {
    snprintf(buf, sizeof(buf), devlist[seldev]->name);
  }
  label = xsane_info_table_text_new(table, buf, 1, 3);

  snprintf(buf, sizeof(buf), "%s", devlist[seldev]->name);
  bufptr = strrchr(buf, ':');
  if (bufptr)
  {
    *bufptr = 0; 
    label = xsane_info_table_text_new(table, buf, 1, 4);
    snprintf(buf, sizeof(buf), TEXT_LOADED_BACKEND);
    label = xsane_info_table_text_new(table, buf, 0, 4);
  }

  snprintf(buf, sizeof(buf), TEXT_SANE_VERSION);
  label = xsane_info_table_text_new(table, buf, 0, 5);
  snprintf(buf, sizeof(buf), "%d.%d build %d",SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                SANE_VERSION_MINOR(xsane.sane_backend_versioncode),
                                SANE_VERSION_BUILD(xsane.sane_backend_versioncode));
  label = xsane_info_table_text_new(table, buf, 1, 5);


  frame = gtk_frame_new(TEXT_RECENT_VALUES);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(4, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  if ((xsane.xsane_color) && (xsane.scanner_gamma_color)) /* color gamma correction by scanner */
  {
   const SANE_Option_Descriptor *opt;

    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_SCANNER);
    label = xsane_info_table_text_new(table, buf, 1, 0);

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_INPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 1);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log((double)opt->size / sizeof(opt->type)) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 1);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_OUTPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 2);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log(opt->constraint.range->max+1.0) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else if ((!xsane.xsane_color) && (xsane.scanner_gamma_gray)) /* gray gamma correction by scanner */
  {
   const SANE_Option_Descriptor *opt;

    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_SCANNER);
    label = xsane_info_table_text_new(table, buf, 1, 0);
    
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_INPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 1);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log((double)opt->size / sizeof(opt->type)) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 1);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_OUTPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 2);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log(opt->constraint.range->max+1.0) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else if (xsane.param.depth != 1) /* gamma correction by xsane */
  {
    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_SOFTWARE_XSANE);
    label = xsane_info_table_text_new(table, buf, 1, 0);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_INPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 1);
    snprintf(buf, sizeof(buf), "%d bit", xsane.param.depth);
    label = xsane_info_table_text_new(table, buf, 1, 1);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_OUTPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 2);
/*    snprintf(buf, sizeof(buf), "%d bit", 8); */
    snprintf(buf, sizeof(buf), "%d bit", xsane.param.depth); 
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else /* no gamma enhancement */
  {
    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_NONE);
    label = xsane_info_table_text_new(table, buf, 1, 0);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_INPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 1);
    snprintf(buf, sizeof(buf), TEXT_NONE);
    label = xsane_info_table_text_new(table, buf, 1, 1);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_OUTPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 2);
    snprintf(buf, sizeof(buf), TEXT_NONE);
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }

  snprintf(buf, sizeof(buf), TEXT_SCANNER_OUTPUT_DEPTH);
  label = xsane_info_table_text_new(table, buf, 0, 3);
  snprintf(buf, sizeof(buf), "%d bit", xsane.param.depth);
  label = xsane_info_table_text_new(table, buf, 1, 3);

  frame = gtk_frame_new(TEXT_OUTPUT_FORMATS);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(2, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  snprintf(buf, sizeof(buf), TEXT_8BIT_FORMATS);
  label = xsane_info_table_text_new(table, buf, 0, 0);

  bufptr=buf;

#ifdef HAVE_LIBJPEG
  sprintf(bufptr, "JPEG, ");
  bufptr += strlen(bufptr);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  sprintf(bufptr, "PNG, ");
  bufptr += strlen(bufptr);
#endif
#endif

  sprintf(bufptr, "PNM, ");
  bufptr += strlen(bufptr);

  sprintf(bufptr, "PS, ");
  bufptr += strlen(bufptr);

#ifdef SUPPORT_RGBA
  sprintf(bufptr, "RGBA, ");
  bufptr += strlen(bufptr);
#endif

#ifdef HAVE_LIBTIFF
  sprintf(bufptr, "TIFF, ");
  bufptr += strlen(bufptr);
#endif

  bufptr--;
  bufptr--;
  *bufptr = 0; /* erase last comma */

  label = xsane_info_table_text_new(table, buf, 1, 0);

  snprintf(buf, sizeof(buf), TEXT_16BIT_FORMATS);
  label = xsane_info_table_text_new(table, buf, 0, 1);

  bufptr=buf;

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  sprintf(bufptr, "PNG, ");
  bufptr += strlen(bufptr);
#endif
#endif

  sprintf(bufptr, "PNM, ");
  bufptr += strlen(bufptr);

  sprintf(bufptr, "RAW, ");
  bufptr += strlen(bufptr);

#ifdef SUPPORT_RGBA
  sprintf(bufptr, "RGBA, ");
  bufptr += strlen(bufptr);
#endif

  bufptr--;
  bufptr--;
  *bufptr = 0; /* erase last comma */

  label = xsane_info_table_text_new(table, buf, 1, 1);

/*  gtk_label_set((GtkLabel *)label, "HALLO"); */

  button = gtk_button_new_with_label(BUTTON_CLOSE);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_info_callback, info_dialog);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(info_dialog);

  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh); 

  xsane_set_sensitivity(FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_about_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *about_dialog, *vbox, *button, *label;
 char buf[256];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;


  about_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_position(GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(about_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(about_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_info_callback), about_dialog);
  snprintf(buf, sizeof(buf), "%s %s", WINDOW_ABOUT, prog_name);
  gtk_window_set_title(GTK_WINDOW(about_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_container_add(GTK_CONTAINER(about_dialog), vbox);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(about_dialog);

  style = gtk_widget_get_style(about_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  snprintf(buf, sizeof(buf), "%s/xsane-logo.xpm", STRINGIFY(PATH_SANE_DATA_DIR));  
  pixmap = gdk_pixmap_create_from_xpm(about_dialog->window, &mask, bg_trans, buf);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);

  xsane_separator_new(vbox, 5);

  snprintf(buf, sizeof(buf), "XSane %s %s\n", TEXT_VERSION, XSANE_VERSION);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "(c) %s %s\n", XSANE_DATE, XSANE_COPYRIGHT);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  snprintf(buf, sizeof(buf), "%s %s\n", TEXT_EMAIL, XSANE_EMAIL);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_CLOSE);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_info_callback, about_dialog);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(about_dialog);

  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh); 

  xsane_set_sensitivity(FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static SANE_Status xsane_get_area_value(int option, float *val, SANE_Int *unit)
{
 const SANE_Option_Descriptor *opt;
 SANE_Handle dev;
 SANE_Word word;

  if (option <= 0)
  {
    return -1;
  }

  if (sane_control_option(dialog->dev, option, SANE_ACTION_GET_VALUE, &word, 0) == SANE_STATUS_GOOD)
  {
    dev = dialog->dev;
    opt = sane_get_option_descriptor(dev, option);

    if (unit)
    {
      *unit = opt->unit;
    }

    if (val)
    {
      if (opt->type == SANE_TYPE_FIXED)
      {
        *val = (float) word / 65536.0;
      }
      else
      {
        *val = (float) word;
      }
    }

   return 0;
  }
  else if (val)
  {
    *val = 0;
  }
  return -2;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_TEST
static void xsane_batch_scan_delete_callback(GtkWidget *widget, gpointer list)
{
  gtk_list_remove_items(GTK_LIST(list), GTK_LIST(list)->selection);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_add_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 float tlx, tly, brx, bry;
 SANE_Int unit;
 char buf[255];
 

  xsane_get_area_value(dialog->well_known.coord[0], &tlx, &unit);
  xsane_get_area_value(dialog->well_known.coord[1], &tly, &unit);
  xsane_get_area_value(dialog->well_known.coord[2], &brx, &unit);
  xsane_get_area_value(dialog->well_known.coord[3], &bry, &unit);

  if (unit == SANE_UNIT_MM)
  {
    snprintf(buf, sizeof(buf), " top left (%7.2fmm, %7.2fmm), bottom right (%7.2fmm, %7.2fmm)", tlx, tly, brx, bry);
  }
  else
  {
    snprintf(buf, sizeof(buf), " top left (%5.0fpx, %5.0fpx), bottom right (%5.0fpx, %5.0fpx)", tlx, tly, brx, bry);
  }

  list_item = gtk_list_item_new_with_label(buf);
  gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(buf));
  gtk_container_add(GTK_CONTAINER(list), list_item);
  gtk_widget_show(list_item);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_dialog(GtkWidget *widget, gpointer data)
{
  GtkWidget *batch_scan_dialog, *batch_scan_vbox, *hbox, *button, *scrolled_window, *list;
  char buf[64];

  batch_scan_dialog = gtk_dialog_new();
  snprintf(buf, sizeof(buf), "%s %s", prog_name, WINDOW_BATCH_SCAN);
  gtk_window_set_title(GTK_WINDOW(batch_scan_dialog), buf);

  batch_scan_vbox = GTK_DIALOG(batch_scan_dialog)->vbox;

  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_usize(scrolled_window, 400, 200);
  gtk_container_add(GTK_CONTAINER(batch_scan_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);

  gtk_widget_show(list);


  /* fill in action area: */
  hbox = GTK_DIALOG(batch_scan_dialog)->action_area;

  button = gtk_button_new_with_label(BUTTON_OK);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_dialog_callback, batch_scan_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_ADD_AREA);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_add_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(batch_scan_dialog);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_dialog_delete()
{
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_dialog()
{
 GtkWidget *fax_dialog, *fax_scan_vbox, *hbox, *button, *scrolled_window, *list;
 char buf[64];
 GtkWidget *pixmapwidget, *text;
 GdkBitmap *mask;
 GdkPixmap *pixmap;


  if (xsane.fax_dialog) 
  {
    return; /* window already is open */
  }

  fax_dialog = gtk_dialog_new();
  snprintf(buf, sizeof(buf), "%s %s", prog_name, WINDOW_FAX_PROJECT);
  gtk_window_set_title(GTK_WINDOW(fax_dialog), buf);
  gtk_signal_connect(GTK_OBJECT(fax_dialog), "delete_event", (GtkSignalFunc) xsane_fax_dialog_delete, 0);

  fax_scan_vbox = GTK_DIALOG(fax_dialog)->vbox;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) fax_xpm);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_pixmap_unref(pixmap);

  text = gtk_entry_new_with_max_length(128);
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAXPROJECT);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_faxproject_changed_callback, 0);

  xsane.fax_project_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 2);

  gtk_widget_realize(fax_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(fax_dialog->window, &mask, xsane.bg_trans, (gchar **) faxreceiver_xpm);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_pixmap_unref(pixmap);

  text = gtk_entry_new_with_max_length(128);
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAXRECEIVER);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_faxreceiver_changed_callback, 0);

  xsane.fax_receiver_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_usize(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(fax_scan_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);

  gtk_widget_show(list);

  xsane.fax_list = list;


  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_SHOW);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_RENAME);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_fax_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_fax_entry_move_down_callback, list);

  gtk_widget_show(hbox);


  /* fill in action area: */
  hbox = GTK_DIALOG(fax_dialog)->action_area;

  button = gtk_button_new_with_label(BUTTON_SEND_PROJECT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_send, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_project_delete, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane.fax_dialog = fax_dialog;

  xsane_fax_project_load();

  gtk_widget_show(fax_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_dialog_close()
{
  if (xsane.fax_dialog == 0)
  {
    return;
  }

  gtk_widget_destroy(xsane.fax_dialog);

  xsane.fax_dialog = 0;
  xsane.fax_list = 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_load()
{
 FILE *projectfile;
 char page[256];
 char buf[256];
 GtkWidget *list_item;
 int i;
 char c;

  gtk_signal_disconnect_by_func(GTK_OBJECT(xsane.fax_receiver_entry), GTK_SIGNAL_FUNC(xsane_faxreceiver_changed_callback), 0);
  gtk_list_remove_items(GTK_LIST(xsane.fax_list), GTK_LIST(xsane.fax_list)->children);

  snprintf(buf, sizeof(buf), "%s/project-list", preferences.fax_project);
  projectfile = fopen(buf, "r");

  if ((!projectfile) || (feof(projectfile)))
  {
    snprintf(buf, sizeof(buf), "%s/page-001.ps", preferences.fax_project);
    xsane.fax_filename=strdup(buf);

    xsane.fax_receiver=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.fax_receiver_entry), (char *) xsane.fax_receiver);
  }
  else
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is receiver phone number or address */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.fax_receiver=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.fax_receiver_entry), (char *) xsane.fax_receiver);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* second line is next fax filename */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    snprintf(buf, sizeof(buf), "%s/%s", preferences.fax_project, page);
    xsane.fax_filename=strdup(buf);

    while (!feof(projectfile))
    {
      i=0;
      c=0;

      while ((i<255) && (c != 10) && (c != EOF))
      {
        c = fgetc(projectfile);
        page[i++] = c;
      }
      page[i-1]=0;

      if (c > 1)
      {
        list_item = gtk_list_item_new_with_label(page);
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
        gtk_container_add(GTK_CONTAINER(xsane.fax_list), list_item);
        gtk_widget_show(list_item);
      }
    }
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  gtk_signal_connect(GTK_OBJECT(xsane.fax_receiver_entry), "changed", (GtkSignalFunc) xsane_faxreceiver_changed_callback, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_delete()
{
 char *page;
 char file[256];
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s.ps", preferences.fax_project, page);
    free(page);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/project-list", preferences.fax_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.fax_project);
  rmdir(file);

  xsane_fax_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_fax_project_save()
{
 FILE *projectfile;
 char *page;
 char buf[256];
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;

  mkdir(preferences.fax_project, 7*64 + 0*8 + 0); /* make sure directory exists */

  snprintf(buf, sizeof(buf), "%s/project-list", preferences.fax_project);
  projectfile = fopen(buf, "w");

  if (xsane.fax_receiver)
  {
    fprintf(projectfile, "%s\n", xsane.fax_receiver); /* first line is receiver phone number or address */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.fax_filename)
  {
    fprintf(projectfile, "%s\n", strrchr(xsane.fax_filename, '/')+1); /* second line is next fax filename */
  }
  else
  {
    fprintf(projectfile, "\n");
  }


  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = (char *) gtk_object_get_data(list_item, "list_item_data");
    fprintf(projectfile, "%s\n", page);
    list = list->next;
  }
  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_device_save(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[256];
 int fd;
 FILE *file;
 int posx, posy, width, height;

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", prog_name, WINDOW_SAVE_SETTINGS, device_text);
  gsg_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  gsg_get_filename(windowname, filename, sizeof(filename), filename);

  file = fopen(filename, "w");
  if (file == 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    gsg_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }

  fprintf(file, "\"XSANE_DEVICE_RC\"\n");
  fprintf(file, "\"%s\"\n", xsane.device_set_filename);
  fclose(file);

  fd = open(filename, O_WRONLY | O_APPEND , 0666);
  if (fd < 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    gsg_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }
  sanei_save_values(fd, dialog->dev);
  close(fd);


  file = fopen(filename, "a");
  if (file == 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    gsg_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }

  fprintf(file, "\"xsane-version\"\n");
  fprintf(file, "\"" XSANE_VERSION "\"\n");
  fprintf(file, "\"xsane-gamma\"\n");
  fprintf(file, "%f\n", xsane.gamma);
  fprintf(file, "\"xsane-gamma-red\"\n");
  fprintf(file, "%f\n", xsane.gamma_red);
  fprintf(file, "\"xsane-gamma-green\"\n");
  fprintf(file, "%f\n", xsane.gamma_green);
  fprintf(file, "\"xsane-gamma-blue\"\n");
  fprintf(file, "%f\n", xsane.gamma_blue);

  fprintf(file, "\"xsane-brightness\"\n");
  fprintf(file, "%f\n", xsane.brightness);
  fprintf(file, "\"xsane-brightness-red\"\n");
  fprintf(file, "%f\n", xsane.brightness_red);
  fprintf(file, "\"xsane-brightness-green\"\n");
  fprintf(file, "%f\n", xsane.brightness_green);
  fprintf(file, "\"xsane-brightness-blue\"\n");
  fprintf(file, "%f\n", xsane.brightness_blue);

  fprintf(file, "\"xsane-contrast\"\n");
  fprintf(file, "%f\n", xsane.contrast);
  fprintf(file, "\"xsane-contrast-red\"\n");
  fprintf(file, "%f\n", xsane.contrast_red);
  fprintf(file, "\"xsane-contrast-green\"\n");
  fprintf(file, "%f\n", xsane.contrast_green);
  fprintf(file, "\"xsane-contrast-blue\"\n");
  fprintf(file, "%f\n", xsane.contrast_blue);

  fprintf(file, "\"xsane-enhancement-rgb-default\"\n");
  fprintf(file, "%d\n", xsane.enhancement_rgb_default);

  fprintf(file, "\"xsane-negative\"\n");
  fprintf(file, "%d\n", xsane.negative);

  gdk_window_get_root_origin(xsane.shell->window, &posx, &posy);
  gdk_window_get_size(xsane.shell->window, &width, &height);
  fprintf(file, "\"xsane-main-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-main-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  fprintf(file, "\"xsane-main-window-width\"\n");
  fprintf(file, "%d\n", width);
  fprintf(file, "\"xsane-main-window-height\"\n");
  fprintf(file, "%d\n", height);
  gtk_widget_set_uposition(xsane.shell, posx, posy); /* set default geometry used when window is closed and opened again */
  gtk_window_set_default_size(GTK_WINDOW(xsane.shell), width, height);

  gdk_window_get_root_origin(xsane.standard_options_shell->window, &posx, &posy);
  fprintf(file, "\"xsane-standard-options-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-standard-options-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.standard_options_shell, posx, posy);

  gdk_window_get_root_origin(xsane.advanced_options_shell->window, &posx, &posy);
  fprintf(file, "\"xsane-advanced-options-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-advanced-options-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.advanced_options_shell, posx, posy);

  gdk_window_get_root_origin(xsane.histogram_dialog->window, &posx, &posy);
  fprintf(file, "\"xsane-histogram-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-histogram-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.histogram_dialog, posx, posy);

  fprintf(file, "\"xsane-show-preview\"\n");
  fprintf(file, "%d\n", xsane.show_preview);

  if (xsane.preview)
  {
    gdk_window_get_root_origin(xsane.preview->top->window, &posx, &posy);
    gdk_window_get_size(xsane.preview->top->window, &width, &height);
    fprintf(file, "\"xsane-preview-window-x-position\"\n");
    fprintf(file, "%d\n", posx);
    fprintf(file, "\"xsane-preview-window-y-position\"\n");
    fprintf(file, "%d\n", posy);
    fprintf(file, "\"xsane-preview-window-width\"\n");
    fprintf(file, "%d\n", width);
    fprintf(file, "\"xsane-preview-window-height\"\n");
    fprintf(file, "%d\n", height);
    gtk_widget_set_uposition(xsane.preview->top, posx, posy);
    gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), width, height);
  }

  fclose(file);

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_device_load_file(char *filename)
{
 int fd;
 FILE *file;
 char buf[256];
 char option[256];
 char *optionp;
 char *version = 0;
 int len;

 int main_posx   = XSANE_DIALOG_POS_X;
 int main_posy   = XSANE_DIALOG_POS_Y;
 int main_width  = XSANE_DIALOG_WIDTH;
 int main_height = XSANE_DIALOG_HEIGHT;

 int standard_options_posx   = XSANE_DIALOG_POS_X;
 int standard_options_posy   = XSANE_DIALOG_POS_Y2;

 int advanced_options_posx   = XSANE_DIALOG_POS_X2;
 int advanced_options_posy   = XSANE_DIALOG_POS_Y2;

 int histogram_posx = XSANE_DIALOG_POS_X2;
 int histogram_posy = XSANE_DIALOG_POS_Y;

 int preview_posx   = 0;
 int preview_posy   = 0;
 int preview_width  = 0;
 int preview_height = 0;

  file = fopen(filename, "r");
  if (file == 0) /* error ? */
  {
    return;
  }

  if (!feof(file))
  {
    fgets(option, sizeof(option), file); /* get first line */
    option[strlen(option)-1] = 0; /* remove cr */

    if (strcmp(option, "\"XSANE_DEVICE_RC\"") != 0) /* wrong file format ? */
    {
      char buf[256];

      snprintf(buf, sizeof(buf), "%s\n%s %s", ERR_LOAD_DEVICE_SETTINGS, filename, ERR_NO_DRC_FILE);
      gsg_error(buf, TRUE);
      return;
    }

    if (!feof(file))
    {
      fgets(option, sizeof(option), file); /* get version */
      option[strlen(option)-1] = 0; /* remove cr */
      len = strlen(option);
      if (len)
      {
        if (option[len-1] == 34)
        {
          option[len-1] = 0; /* remove " */
        }
      }
      optionp = option+1;

      if (strcmp(optionp, xsane.device_set_filename))
      {
        snprintf(buf, sizeof(buf), "%s \"%s\"\n"
                                   "%s \"%s\",\n"
                                   "%s \"%s\",\n"
                                   "%s",
                                   TEXT_FILE, filename,
                                   ERR_CREATED_FOR_DEVICE, optionp,
                                   ERR_USED_FOR_DEVICE, xsane.device_set_filename,
                                   ERR_MAY_CAUSE_PROBLEMS);
        if (gsg_decision(ERR_HEADER_WARNING, buf, ERR_BUTTON_OK, BUTTON_CANCEL, TRUE) == FALSE)
        { /* cancel */
          fclose(file);
          return; 
        }
      }
    }
  }

  while (!feof(file))
  {
    fgets(option, sizeof(option), file); /* get option name */
    option[strlen(option)-1] = 0; /* remove cr */
    if (strcmp(option, "\"xsane-version\"") == 0)
    {
      fgets(option, sizeof(option), file); /* get version */
      option[strlen(option)-1] = 0; /* remove cr */
      len = strlen(option);
      if (len)
      {
        if (option[len-1] == 34)
        {
          option[len-1] = 0; /* remove " */
        }
      }
      version = strdup(option+1);
    }
    else if (strcmp(option, "\"xsane-gamma\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma);
    }
    else if (strcmp(option, "\"xsane-gamma-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_red);
    }
    else if (strcmp(option, "\"xsane-gamma-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_green);
    }
    else if (strcmp(option, "\"xsane-gamma-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_blue);
    }
    else if (strcmp(option, "\"xsane-brightness\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness);
    }
    else if (strcmp(option, "\"xsane-brightness-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_red);
    }
    else if (strcmp(option, "\"xsane-brightness-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_green);
    }
    else if (strcmp(option, "\"xsane-brightness-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_blue);
    }
    else if (strcmp(option, "\"xsane-contrast\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast);
    }
    else if (strcmp(option, "\"xsane-contrast-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_red);
    }
    else if (strcmp(option, "\"xsane-contrast-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_green);
    }
    else if (strcmp(option, "\"xsane-contrast-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_blue);
    }
    else if (strcmp(option, "\"xsane-enhancement-rgb-default\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.enhancement_rgb_default);
    }
    else if (strcmp(option, "\"xsane-negative\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.negative);
    }
    else if (strcmp(option, "\"xsane-show-preview\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.show_preview);
    }
    else if (strcmp(option, "\"xsane-main-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &main_posx);
    }
    else if (strcmp(option, "\"xsane-main-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &main_posy);
    }
    else if (strcmp(option, "\"xsane-main-window-width\"") == 0)
    {
      fscanf(file, "%d\n", &main_width);
    }
    else if (strcmp(option, "\"xsane-main-window-height\"") == 0)
    {
      fscanf(file, "%d\n", &main_height);
    }
    else if (strcmp(option, "\"xsane-standard-options-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &standard_options_posx);
    }
    else if (strcmp(option, "\"xsane-standard-options-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &standard_options_posy);
    }
    else if (strcmp(option, "\"xsane-advanced-options-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &advanced_options_posx);
    }
    else if (strcmp(option, "\"xsane-advanced-options-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &advanced_options_posy);
    }
    else if (strcmp(option, "\"xsane-histogram-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &histogram_posx);
    }
    else if (strcmp(option, "\"xsane-histogram-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &histogram_posy);
    }
    else if (strcmp(option, "\"xsane-preview-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &preview_posx);
    }
    else if (strcmp(option, "\"xsane-preview-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &preview_posy);
    }
    else if (strcmp(option, "\"xsane-preview-window-width\"") == 0)
    {
      fscanf(file, "%d\n", &preview_width);
    }
    else if (strcmp(option, "\"xsane-preview-window-height\"") == 0)
    {
      fscanf(file, "%d\n", &preview_height);
    }
    else
    {
      fgets(option, sizeof(option), file); /* skip option */
    }
  }
  fclose(file);

#if 0
  if (version)
  {
    if (strcmp(version, XSANE_VERSION))
    {
      snprintf(buf, sizeof(buf), "File: \"%s\"\n"
                                 "has been saved with xsane-%s,\n"
                                 "this may cause problems!", filename, version);
      gsg_warning(buf, TRUE);
    }
    free(version);
  }
  else
  {
    snprintf(buf, sizeof(buf), "File: \"%s\"\n"
                               "has been saved with xsane before version 0.40,\n"
                               "this may cause problems!", filename);
    gsg_warning(buf, TRUE);
  }
#endif


  gtk_widget_set_uposition(xsane.shell, main_posx, main_posy);
  gtk_widget_set_uposition(xsane.standard_options_shell, standard_options_posx, standard_options_posy);
  gtk_widget_set_uposition(xsane.advanced_options_shell, advanced_options_posx, advanced_options_posy);
  gtk_widget_set_uposition(xsane.histogram_dialog, histogram_posx, histogram_posy);

  if (xsane.preview)
  {
    gtk_widget_set_uposition(xsane.preview->top, preview_posx, preview_posy);
  }

  gtk_window_set_default_size(GTK_WINDOW(xsane.shell), main_width, main_height);

  if (xsane.preview)
  {
    gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), preview_width, preview_height);
  }


  fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    return;
  }
  sanei_load_values(fd, dialog->dev);
  close(fd);

  if (dialog->well_known.dpi > 0)
  {
   const SANE_Option_Descriptor *opt;

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
  
    switch (opt->type)
    {
      case SANE_TYPE_INT:
      {
       SANE_Int dpi;
        sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
        xsane.resolution = dpi;
      }
      break;

      case SANE_TYPE_FIXED:
      {
       SANE_Fixed dpi;
        sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
        xsane.resolution = (int) SANE_UNFIX(dpi);
      }
      break;
 
      default:
       fprintf(stderr, "xsane_pref_load_file: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
      return;
    }
  }

  xsane_refresh_dialog(dialog);
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_device_restore(void)
{
  char filename[PATH_MAX];

  gsg_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  xsane_pref_device_load_file(filename);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_device_load(void)
{
  char filename[PATH_MAX];
  char windowname[256];

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", prog_name, WINDOW_LOAD_SETTINGS, device_text);
  gsg_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  gsg_get_filename(windowname, filename, sizeof(filename), filename);
  xsane_pref_device_load_file(filename);
  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_toggle_tooltips(GtkWidget *widget, gpointer data)
{
  preferences.tooltips_enabled = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  gsg_set_tooltips(dialog, preferences.tooltips_enabled);
  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_doc_via_nsr(GtkWidget *widget, gpointer data) /* show via netscape remote */
{
 char *name = (char *) data;
 char buf[256];
 FILE *ns_pipe;

  snprintf(buf, sizeof(buf), "netscape -no-about-splash -remote \"openFile(%s/%s-doc.html)\" 2>&1",
               STRINGIFY(PATH_SANE_DATA_DIR), name);  
  ns_pipe = popen(buf, "r");

  if (ns_pipe)
  {
    while (!feof(ns_pipe))
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
      fgetc(ns_pipe); /* remove char from pipe */
    }

    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    if (pclose(ns_pipe))
    {
      snprintf(buf, sizeof(buf), "%s", ERR_NETSCAPE_NOT_RUNNING);
      gsg_error(buf, TRUE);
    }
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s", ERR_NETSCAPE_EXECUTE_FAIL);
    gsg_error(buf, TRUE);
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_doc(GtkWidget *widget, gpointer data)
{
 char *name = (char *) data;
 char buf[256];
 pid_t pid;
 char *arg[3];

  if (!strcmp(preferences.doc_viewer, DOCVIEWERNETSCAPEREMOTE))
  {
    xsane_show_doc_via_nsr(widget, data);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/%s-doc.html", STRINGIFY(PATH_SANE_DATA_DIR), name);  
    arg[0] = preferences.doc_viewer;
    arg[1] = buf;
    arg[2] = 0;

    pid = fork();

    if (pid == 0) /* new process */
    {
      execvp(arg[0], arg); /* does not return if successfully */
      fprintf(stderr, "%s %s\n", ERR_FAILD_EXEC_DOC_VIEWER, preferences.doc_viewer);
      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entrys_swap(GtkWidget *list_item_1, GtkWidget *list_item_2)
{
 char *text1;
 char *text2;

  text1 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_1), "list_item_data");
  text2 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_2), "list_item_data");

  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_1))->data), text2);
  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_2))->data), text1);
  gtk_object_set_data(GTK_OBJECT(list_item_1), "list_item_data", text2);
  gtk_object_set_data(GTK_OBJECT(list_item_2), "list_item_data", text1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_move_up_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item_1 = select->data;

    position = gtk_list_child_position(GTK_LIST(list), list_item_1);
    position--; /* move up */
    newpos = position;

    if (position >= 0)
    {
      while (position>0)
      {
        item = item->next;
        position--;
      }

      list_item_2 = item->data;
      if (list_item_2)
      {
        xsane_fax_entrys_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);
        xsane_fax_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_move_down_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item_1 = select->data;

    position = gtk_list_child_position(GTK_LIST(list), list_item_1);
    position++; /* move down */
    newpos = position;

    while ((position>0) && (item))
    {
      item = item->next;
      position--;
    }

    if (item)
    {
      list_item_2 = item->data;
      if (list_item_2)
      {
        xsane_fax_entrys_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);
        xsane_fax_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_fax_entry_rename;

static void xsane_fax_entry_rename_button_callback(GtkWidget *widget, gpointer data)
{
 xsane_fax_entry_rename = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 GList *select;
 char *oldpage;
 char *newpage;
 char oldfile[256];
 char newfile[256];

  select = GTK_LIST(list)->selection;
  if (select)
  {
   GtkWidget *rename_dialog;
   GtkWidget *text;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   char buf[256]; 

    list_item = select->data;
    oldpage = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_data"));

    xsane_set_sensitivity(FALSE);

    rename_dialog = gtk_dialog_new();
    gtk_window_set_position(GTK_WINDOW(rename_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_policy(GTK_WINDOW(rename_dialog), FALSE, FALSE, FALSE);
    snprintf(buf, sizeof(buf), "%s %s", prog_name, WINDOW_FAX_RENAME);
    gtk_window_set_title(GTK_WINDOW(rename_dialog), buf);
    gtk_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) -1);
    gtk_widget_show(rename_dialog);

    vbox = GTK_DIALOG(rename_dialog)->vbox;

    text = gtk_entry_new_with_max_length(64);
    gsg_set_tooltip(dialog->tooltips, text, DESC_FAXPAGENAME);
    gtk_entry_set_text(GTK_ENTRY(text), oldpage);
    gtk_widget_set_usize(text, 300, 0);
    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
    gtk_widget_show(text);


    hbox = GTK_DIALOG(rename_dialog)->action_area;

    button = gtk_button_new_with_label("OK");
    gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);

    button = gtk_button_new_with_label("Cancel");
    gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);


    xsane_fax_entry_rename = 0;

    while (xsane_fax_entry_rename == 0)
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    newpage = strdup(gtk_entry_get_text(GTK_ENTRY(text)));

    if (xsane_fax_entry_rename == 1)
    {
      gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item))->data), newpage);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(newpage));

      xsane_convert_text_to_filename(&oldpage);
      xsane_convert_text_to_filename(&newpage);
      snprintf(oldfile, sizeof(oldfile), "%s/%s.ps", preferences.fax_project, oldpage);
      snprintf(newfile, sizeof(newfile), "%s/%s.ps", preferences.fax_project, newpage);

      rename(oldfile, newfile);

      xsane_fax_project_save();
    }

    free(oldpage);
    free(newpage);

    gtk_widget_destroy(rename_dialog);

    xsane_set_sensitivity(TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char file[256];

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s.ps", preferences.fax_project, page);
    free(page);
    remove(file);
    gtk_widget_destroy(GTK_WIDGET(list_item));
    xsane_fax_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 pid_t pid;
 char *arg[100];
 char *page;
 char buf[256];
 int argnr;

  select = GTK_LIST(list)->selection;
  if (select)
  {
    argnr = xsane_parse_options(preferences.fax_viewer, arg);

    list_item = GTK_OBJECT(select->data);
    page = (char *) gtk_object_get_data(list_item, "list_item_data");
    page = strdup(page);
    xsane_convert_text_to_filename(&page);
    snprintf(buf, sizeof(buf), "%s/%s.ps", preferences.fax_project, page);
    free(page);
    arg[argnr++] = buf;
    arg[argnr] = 0;

    pid = fork();

    if (pid == 0) /* new process */
    {
      execvp(arg[0], arg); /* does not return if successfully */
      fprintf(stderr, "%s %s\n", ERR_FAILD_EXEC_FAX_VIEWER, preferences.fax_viewer);
      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_send()
{
 char *page;
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;
 pid_t pid;
 char *arg[1000];
 char buf[256];
 int argnr = 0;
 int i;

  if (list)
  {
    if (!xsane_option_defined(xsane.fax_receiver))
    {
      snprintf(buf, sizeof(buf), "%s\n", ERR_SENDFAX_RECEIVER_MISSING);
      gsg_error(buf, TRUE);
      return;
    }

    argnr = xsane_parse_options(preferences.fax_command, arg);

    if (xsane.fax_fine_mode) /* fine mode */
    {
      if (xsane_option_defined(preferences.fax_fine_option))
      {
        arg[argnr++] = strdup(preferences.fax_fine_option);
      }
    }
    else /* normal mode */
    {
      if (xsane_option_defined(preferences.fax_normal_option))
      {
        arg[argnr++] = strdup(preferences.fax_normal_option);
      }
    }

    if (xsane_option_defined(preferences.fax_receiver_option))
    {
      arg[argnr++] = strdup(preferences.fax_receiver_option);
    }
    arg[argnr++] = strdup(xsane.fax_receiver);

    if (xsane_option_defined(preferences.fax_postscript_option))
    {
      arg[argnr++] = strdup(preferences.fax_postscript_option);
    }

    while ((list) && (argnr<999))	/* add pages to options */
    {
      list_item = GTK_OBJECT(list->data);
      page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
      xsane_convert_text_to_filename(&page);
      snprintf(buf, sizeof(buf), "%s/%s.ps", preferences.fax_project, page);
      free(page);
      arg[argnr++] = strdup(buf);
      list = list->next;
    }

    arg[argnr] = 0;

    pid = fork();

    if (pid == 0) /* new process */
    {
      execvp(arg[0], arg); /* does not return if successfully */
      fprintf(stderr, "%s %s\n", ERR_FAILED_EXEC_FAX_CMD, preferences.fax_command);
      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }

    for (i=0; i<argnr; i++)
    {
      free(arg[i]);
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_view_build_menu(void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new();


  /* show tooltips */

  item = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_TOOLTIPS);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), preferences.tooltips_enabled);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", (GtkSignalFunc) xsane_pref_toggle_tooltips, 0);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* show preview */

  xsane.show_preview_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_PREVIEW);
  gtk_menu_append(GTK_MENU(menu), xsane.show_preview_widget);
  gtk_widget_show(xsane.show_preview_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_preview_widget), "toggled", (GtkSignalFunc) xsane_show_preview_callback, 0);
 
  /* show histogram */

  xsane.show_histogram_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_HISTOGRAM);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  gtk_menu_append(GTK_MENU(menu), xsane.show_histogram_widget);
  gtk_widget_show(xsane.show_histogram_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_histogram_widget), "toggled", (GtkSignalFunc) xsane_show_histogram_callback, 0);

  
  /* show standard options */

  xsane.show_standard_options_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_STANDARDOPTIONS);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_standard_options_widget);
  gtk_widget_show(xsane.show_standard_options_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_standard_options_widget), "toggled", (GtkSignalFunc) xsane_show_standard_options_callback, 0);


  /* show advanced options */

  xsane.show_advanced_options_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_ADVANCEDOPTIONS);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_advanced_options_widget);
  gtk_widget_show(xsane.show_advanced_options_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_advanced_options_widget), "toggled",
                     (GtkSignalFunc) xsane_show_advanced_options_callback, 0);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_pref_build_menu(void)
{
  GtkWidget *menu, *item, *submenu, *subitem;

  menu = gtk_menu_new();


  /* XSane setup dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_SETUP);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_setup_dialog, 0);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


#ifdef XSANE_TEST
  /* XSane batch scan dialog */

  item = gtk_menu_item_new_with_label("Batch scan");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_batch_scan_dialog, 0);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
#endif


  /* length unit */

  item = gtk_menu_item_new_with_label(MENU_ITEM_LENGTH_UNIT);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  submenu = gtk_menu_new();

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_MILLIMETERS);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.length_unit == 1.0)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  gtk_widget_show(subitem);
  xsane.length_unit_mm = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_CENTIMETERS);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.length_unit == 10.0)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  gtk_widget_show(subitem);
  xsane.length_unit_cm = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_INCHES);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.length_unit == 25.4)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_pref_unit_callback, "in");
  gtk_widget_show(subitem);
  xsane.length_unit_in = subitem;

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  xsane.length_unit_widget = item;


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* update policy */

  item = gtk_menu_item_new_with_label(MENU_ITEM_UPDATE_POLICY);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  submenu = gtk_menu_new();

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_POLICY_CONTINUOUS);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_CONTINUOUS);
  gtk_widget_show(subitem);
  xsane.update_policy_continu = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_POLICY_DISCONTINU);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.gtk_update_policy == GTK_UPDATE_DISCONTINUOUS)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_DISCONTINUOUS);
  gtk_widget_show(subitem);
  xsane.update_policy_discont = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_POLICY_DELAYED);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_DELAYED);
  gtk_widget_show(subitem);
  xsane.update_policy_delayed = subitem;

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* show resolution list */

  xsane.show_resolution_list_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_RESOLUTIONLIST);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_resolution_list_widget), preferences.show_resolution_list);
  gtk_menu_append(GTK_MENU(menu), xsane.show_resolution_list_widget);
  gtk_widget_show(xsane.show_resolution_list_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_resolution_list_widget), "toggled",
                     (GtkSignalFunc) xsane_show_resolution_list_callback, 0);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* Save device setting */

  item = gtk_menu_item_new_with_label(MENU_ITEM_SAVE_DEVICE_SETTINGS);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_pref_device_save, 0);
  gtk_widget_show(item);

  /* Load device setting */

  item = gtk_menu_item_new_with_label(MENU_ITEM_LOAD_DEVICE_SETTINGS);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_pref_device_load, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_help_build_menu(void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new();

  item = gtk_menu_item_new_with_label(MENU_ITEM_XSANE_DOC);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-xsane");
  gtk_widget_show(item);

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  if (xsane.backend)
  {
    item = gtk_menu_item_new_with_label(MENU_ITEM_BACKEND_DOC);
    gtk_menu_append(GTK_MENU(menu), item);
    gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) xsane.backend);
    gtk_widget_show(item);
  }

  item = gtk_menu_item_new_with_label(MENU_ITEM_AVAILABLE_BACKENDS);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-backends");
  gtk_widget_show(item);

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label(MENU_ITEM_SCANTIPS);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-scantips");
  gtk_widget_show(item);


  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void panel_build(GSGDialog *dialog)
{
  GtkWidget *xsane_hbox;
  GtkWidget *standard_hbox, *standard_vbox;
  GtkWidget *advanced_hbox, *advanced_vbox;
  GtkWidget *parent, *vbox, *button, *label;
  const SANE_Option_Descriptor *opt;
  SANE_Handle dev = dialog->dev;
  double dval, dmin, dmax, dquant;
  char *buf, str[16], title[256];
  GSGDialogElement *elem;
  SANE_Word quant, val;
  SANE_Status status;
  SANE_Int num_words;
  char **str_list;
  int i, j;
  int num_vector_opts = 0;
  int *vector_opts;

   xsane_hbox = 0;

  /* reset well-known options: */
  dialog->well_known.scanmode        = -1;
  dialog->well_known.scansource      = -1;
  dialog->well_known.preview         = -1;
  dialog->well_known.dpi             = -1;
  dialog->well_known.coord[GSG_TL_X] = -1;
  dialog->well_known.coord[GSG_TL_Y] = -1;
  dialog->well_known.coord[GSG_BR_X] = -1;
  dialog->well_known.coord[GSG_BR_Y] = -1;
  dialog->well_known.gamma_vector    = -1;
  dialog->well_known.gamma_vector_r  = -1;
  dialog->well_known.gamma_vector_g  = -1;
  dialog->well_known.gamma_vector_b  = -1;
  dialog->well_known.bit_depth       = -1;


  /* standard options */
  standard_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(standard_hbox);
  standard_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(standard_vbox);
/*  gtk_box_pack_start(GTK_BOX(standard_hbox), standard_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(standard_hbox), standard_vbox, TRUE, TRUE, 0); /* make frame sizeable */

  /* advanced options */
  advanced_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(advanced_hbox);
  advanced_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(advanced_vbox);
/*   gtk_box_pack_start(GTK_BOX(advanced_hbox), advanced_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(advanced_hbox), advanced_vbox, TRUE, TRUE, 0); /* make frame sizeable */


  vector_opts = alloca(dialog->num_elements * sizeof (int));

  parent = standard_vbox;
  for (i = 1; i < dialog->num_elements; ++i)
    {
      opt = sane_get_option_descriptor (dev, i);
      if (!SANE_OPTION_IS_ACTIVE(opt->cap))
        continue;

      /* pick up well-known options as we go: */
      if (opt->name)
        {
          if (strcmp(opt->name, SANE_NAME_PREVIEW) == 0
              && opt->type == SANE_TYPE_BOOL)
            {
              dialog->well_known.preview = i;
              continue;
            }
          else if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) == 0
                   && opt->unit == SANE_UNIT_DPI && (opt->type == SANE_TYPE_INT || opt->type == SANE_TYPE_FIXED))
            dialog->well_known.dpi = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_MODE) == 0)
            dialog->well_known.scanmode = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) == 0)
            dialog->well_known.scansource = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_TL_X) == 0)
            dialog->well_known.coord[GSG_TL_X] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_TL_Y) == 0)
            dialog->well_known.coord[GSG_TL_Y] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_BR_X) == 0)
            dialog->well_known.coord[GSG_BR_X] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_BR_Y) == 0)
            dialog->well_known.coord[GSG_BR_Y] = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR) == 0)
            dialog->well_known.gamma_vector = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_R) == 0)
            dialog->well_known.gamma_vector_r = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_G) == 0)
            dialog->well_known.gamma_vector_g = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_B) == 0)
            dialog->well_known.gamma_vector_b = i;
          else if (strcmp (opt->name, SANE_NAME_BIT_DEPTH) == 0)
            dialog->well_known.bit_depth = i;
        }

      elem = dialog->element + i;
      elem->dialog = dialog;


      if (opt->unit == SANE_UNIT_NONE)
      {
        snprintf(title, sizeof(title), "%s", _BGT(opt->title));
      }
      else
      {
        snprintf(title, sizeof(title), "%s [%s]", _BGT(opt->title), gsg_unit_string(opt->unit));
      }

      switch (opt->type)
      {
        case SANE_TYPE_GROUP:
          /* group a set of options */
          vbox = standard_vbox;
          if (opt->cap & SANE_CAP_ADVANCED)
          {
            vbox = advanced_vbox;
          }
          parent = gsg_group_new(vbox, title);
          elem->widget = parent;
          break;

        case SANE_TYPE_BOOL:
          assert(opt->size == sizeof(SANE_Word));
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
          {
            goto get_value_failed;
          }
          gsg_button_new(parent, title, val, elem, dialog->tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
          gtk_widget_show(parent->parent);
          break;

        case SANE_TYPE_INT:
          if (opt->size != sizeof(SANE_Word))
          {
            vector_opts[num_vector_opts++] = i;
            break;
          }
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
          {
            goto get_value_failed;
          }

          switch (opt->constraint_type)
          {
            case SANE_CONSTRAINT_RANGE:
              if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a scale */
                quant = opt->constraint.range->quant;
                if (quant == 0)
                  quant = 1;
                gsg_scale_new(parent, title, val, opt->constraint.range->min, opt->constraint.range->max, quant,
                          (opt->cap & SANE_CAP_AUTOMATIC), elem, dialog->tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
                gtk_widget_show(parent->parent);
              }
              break;

            case SANE_CONSTRAINT_WORD_LIST:
              if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a "list-selection" widget */
                num_words = opt->constraint.word_list[0];
                str_list = malloc((num_words + 1) * sizeof(str_list[0]));
                for (j = 0; j < num_words; ++j)
                {
                  sprintf(str, "%d", opt->constraint.word_list[j + 1]);
                  str_list[j] = strdup(str);
                }
                str_list[j] = 0;
                sprintf(str, "%d", val);
                gsg_option_menu_new(parent, title, str_list, str, elem, dialog->tooltips, _BGT(opt->desc),
                                    SANE_OPTION_IS_SETTABLE(opt->cap));
                free(str_list);
                gtk_widget_show(parent->parent);
              }
              break;

            default:
              fprintf(stderr, "panel_build: %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
              break;
          }
          break;

        case SANE_TYPE_FIXED:
          if (opt->size != sizeof (SANE_Word))
          {
            vector_opts[num_vector_opts++] = i;
            break;
          }
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
          {
            goto get_value_failed;
          }

          switch (opt->constraint_type)
          {
            case SANE_CONSTRAINT_RANGE:
              if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a scale */
                quant = opt->constraint.range->quant;
                if (quant == 0)
                quant = 1;
                dval = SANE_UNFIX (val);
                dmin = SANE_UNFIX (opt->constraint.range->min);
                dmax = SANE_UNFIX (opt->constraint.range->max);
                dquant = SANE_UNFIX (quant);
                if (opt->unit == SANE_UNIT_MM)
                {
                  dval /= preferences.length_unit;
                  dmin /= preferences.length_unit;
                  dmax /= preferences.length_unit;
                  dquant /= preferences.length_unit;
                }
                gsg_scale_new(parent, title, dval, dmin, dmax, dquant, (opt->cap & SANE_CAP_AUTOMATIC), elem,
                              dialog->tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
                gtk_widget_show(parent->parent);
              }
              break;

            case SANE_CONSTRAINT_WORD_LIST:
              if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a "list-selection" widget */
                num_words = opt->constraint.word_list[0];
                str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
                for (j = 0; j < num_words; ++j)
                {
                  sprintf(str, "%g", SANE_UNFIX (opt->constraint.word_list[j + 1]));
                  str_list[j] = strdup (str);
                }
                str_list[j] = 0;
                sprintf(str, "%g", SANE_UNFIX (val));
                gsg_option_menu_new(parent, title, str_list, str, elem, dialog->tooltips, _BGT(opt->desc),
                                    SANE_OPTION_IS_SETTABLE(opt->cap));
                free (str_list);
                gtk_widget_show(parent->parent);
              }
              break;

            default:
              fprintf(stderr, "panel_build: %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
              break;
          }
          break;

        case SANE_TYPE_STRING:
          buf = malloc (opt->size);
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, buf, 0);
          if (status != SANE_STATUS_GOOD)
          {
            free (buf);
            goto get_value_failed;
          }

          switch (opt->constraint_type)
          {
            case SANE_CONSTRAINT_STRING_LIST:
              if ( (strcmp (opt->name, SANE_NAME_SCAN_MODE) != 0) &&  /* do not show scanmode */
                   (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) != 0) ) /* do not show scansource */
              {
                /* use a "list-selection" widget */
                gsg_option_menu_new(parent, title, (char **) opt->constraint.string_list, buf,
                                    elem, dialog->tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
                gtk_widget_show (parent->parent);
              }
              break;

            case SANE_CONSTRAINT_NONE:
              gsg_text_entry_new(parent, title, buf, elem, dialog->tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
              gtk_widget_show (parent->parent);
              break;

            default:
              fprintf(stderr, "panel_build: %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
              break;
          }
          free (buf);
          break;

        case SANE_TYPE_BUTTON:
          button = gtk_button_new();
          gtk_signal_connect(GTK_OBJECT (button), "clicked", (GtkSignalFunc) gsg_push_button_callback, elem);
          gsg_set_tooltip(dialog->tooltips, button, _BGT(opt->desc));

          label = gtk_label_new(title);
          gtk_container_add(GTK_CONTAINER (button), label);

          gtk_box_pack_start(GTK_BOX (parent), button, FALSE, TRUE, 0);

          gtk_widget_show(label);
          gtk_widget_show(button);

          gtk_widget_set_sensitive(button, SANE_OPTION_IS_SETTABLE(opt->cap));

          elem->widget = button;
          gtk_widget_show(parent->parent);
          break;

        default:
          fprintf(stderr, "panel_build: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
          break;
      }
      continue;

    get_value_failed:
      {
        char msg[256];

        sprintf(msg, "%s %s: %s.", ERR_GET_OPTION, opt->name, XSANE_STRSTATUS(status));
        gsg_error(msg, TRUE);
      }
    }
  xsane_hbox = xsane_update_xsane_callback();

  gtk_container_add(GTK_CONTAINER(dialog->xsane_window), xsane_hbox);
  gtk_container_add(GTK_CONTAINER(dialog->standard_window), standard_hbox);
  gtk_container_add(GTK_CONTAINER(dialog->advanced_window), advanced_hbox);

  dialog->xsane_hbox    = xsane_hbox;
  dialog->standard_hbox = standard_hbox;
  dialog->advanced_hbox = advanced_hbox;

  xsane_update_histogram();
/*
  xsane_draw_slider_level(&xsane.slider_gray);
  xsane_draw_slider_level(&xsane.slider_red);
  xsane_draw_slider_level(&xsane.slider_green);
  xsane_draw_slider_level(&xsane.slider_blue);
*/
  xsane_update_sliders();

  if (xsane.length_unit_widget)
  {
   int unit;

    status = xsane_get_area_value(dialog->well_known.coord[0], 0, &unit);

    if ( (unit == SANE_UNIT_PIXEL) || (status) )
    {
      gtk_widget_set_sensitive(xsane.length_unit_widget, FALSE);
    }
    else
    {
      gtk_widget_set_sensitive(xsane.length_unit_widget, TRUE);
    }
  }

  /* now add in vector editor, if necessary: */
/*
  if (num_vector_opts)
    vector_new (dialog, custom_gamma_vbox, num_vector_opts, vector_opts);
*/
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Create the main dialog box.  */

static void xsane_device_dialog(void)
{
  GtkWidget *hbox, *button, *frame, *infobox;
  GtkWidget *main_dialog_window, *standard_dialog_window, *advanced_dialog_window;
  GtkWidget *menubar, *menubar_item;
  const gchar *devname;
  char buf[256];
  char windowname[255];
  char devicetext[255];
  char *textptr;
  GtkWidget *xsane_window;
  GtkWidget *xsane_vbox_main;
  GtkWidget *xsane_vbox_standard;
  GtkWidget *xsane_vbox_advanced;
  GdkColormap *colormap;
  SANE_Int num_elements;
  SANE_Status status;
  SANE_Handle dev;



  devname = devlist[seldev]->name;

  status = sane_open(devname, &dev);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s `%s':\n %s.", ERR_DEVICE_OPEN_FAILED, devname, XSANE_STRSTATUS(status));
    gsg_error(buf, TRUE);
    return;
  }

  if (sane_control_option(dev, 0, SANE_ACTION_GET_VALUE, &num_elements, 0) != SANE_STATUS_GOOD)
  {
    gsg_error(ERR_OPTION_COUNT, TRUE);
    sane_close(dev);
    return;
  }

  snprintf(buf, sizeof(buf), "%s", devlist[seldev]->name);	/* generate "sane-BACKENDNAME" */
  textptr = strrchr(buf, ':'); /* format is midend:midend:midend:backend:device or backend:device */
  if (textptr)
  {
    *textptr = 0; /* erase ":device" at end of text */
    textptr = strrchr(buf, ':');
    if (textptr) /* midend:backend:device */
    {
      textptr++;
    }
    else /* backend:device */
    {
      textptr = buf;
    }

    xsane.backend = malloc(strlen(textptr)+6);
    sprintf(xsane.backend, "sane-%s", textptr); /* add "sane-" */

    bindtextdomain(xsane.backend, LOCALEDIR); /* set path for backend translation texts */
  }

  /* create device-text for window titles */

  snprintf(devicetext, sizeof(devicetext), "%s", devlist[seldev]->model);
  textptr = devicetext + strlen(devicetext);
  while (*(textptr-1) == ' ') /* erase spaces at end of text */
  {
    textptr--;
  }

  *textptr = ':';
  textptr++;
  *textptr = 0;

  if (!strncmp(devname, "net:", 4)) /* network device ? */
  {
    sprintf(textptr, "net:");
    textptr = devicetext + strlen(devicetext);
  }

  snprintf(buf, sizeof(buf), ":%s", devname);
  snprintf(buf, sizeof(buf), "/%s", (strrchr(buf, ':')+1));
  sprintf(textptr, (strrchr(buf, '/')+1));

  device_text = strdup(devicetext);


  /* if no preferences filename is given on commandline create one from devicenaname */
 
  if (!xsane.device_set_filename)
  {
    if (!strcmp(devlist[seldev]->vendor, TEXT_UNKNOWN))
    {
      snprintf(buf, sizeof(buf), "%s",  devlist[seldev]->name);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%s:%s", devlist[seldev]->vendor, devlist[seldev]->model);
    }
    xsane.device_set_filename = strdup(buf); /* set preferences filename */
  }


  xsane_pref_restore(); /* restore preferences */

  if (xsane.main_window_fixed == -1) /* no command line option given */
  {
    xsane.main_window_fixed = preferences.main_window_fixed;
  }


  /* create the xsane dialog box */

  xsane.shell = gtk_dialog_new();
  gtk_widget_set_uposition(xsane.shell, XSANE_DIALOG_POS_X, XSANE_DIALOG_POS_Y);
  sprintf(windowname, "%s %s %s", prog_name, XSANE_VERSION, device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.shell), (char *) windowname);
  gtk_signal_connect(GTK_OBJECT(xsane.shell), "delete_event", GTK_SIGNAL_FUNC(xsane_scan_win_delete), 0);

  /* set the main vbox */

  xsane_window = GTK_DIALOG(xsane.shell)->vbox;
  gtk_widget_show(xsane_window); /* normally not necessary, but to be sure */

  /* create the menubar */

  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(xsane_window), menubar, FALSE, FALSE, 0);

  /* "Files" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_FILE);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_files_build_menu());
  gtk_widget_show(menubar_item);

  /* "Preferences" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_PREFERENCES);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_pref_build_menu());
  gtk_widget_show(menubar_item);

  /* "View" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_VIEW);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_view_build_menu());
  gtk_widget_show(menubar_item);


  /* "Help" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_HELP);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_right_justify((GtkMenuItem *) menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_help_build_menu());
  gtk_widget_show(menubar_item);

  gtk_widget_show(menubar);

  if (xsane.main_window_fixed) /* fixed window: use it like it is */
  {
                                              /* shrink grow auto_shrink */
    gtk_window_set_policy(GTK_WINDOW(xsane.shell), FALSE, FALSE, TRUE); /* auto size */

    xsane_vbox_main = gtk_vbox_new(TRUE, 5); /* we need this to set the wanted borders */
    gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
    gtk_container_add(GTK_CONTAINER(xsane_window), xsane_vbox_main);          
  }
  else /* scrolled window: create a scrolled window and put it into the xsane dialog box */
  {
    gtk_window_set_default_size(GTK_WINDOW(xsane.shell), XSANE_DIALOG_WIDTH, XSANE_DIALOG_HEIGHT); /* set default size */
#if 0
    gtk_widget_set_usize(xsane.shell, XSANE_DIALOG_WIDTH/2, XSANE_DIALOG_HEIGHT/2); /* set default size */
#endif

                                              /* shrink grow auto_shrink */
    gtk_window_set_policy(GTK_WINDOW(xsane.shell), TRUE, TRUE, FALSE); /* allow resizing */

    xsane.main_dialog_scrolled = gtk_scrolled_window_new(0, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(xsane_window), xsane.main_dialog_scrolled);
    gtk_widget_show(xsane.main_dialog_scrolled);

    xsane_vbox_main = gtk_vbox_new(TRUE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled), xsane_vbox_main);
  }

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  main_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_main), main_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(main_dialog_window);

  gtk_widget_show(xsane_vbox_main);



  /* create the scanner standard options dialog box */

  xsane.standard_options_shell = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_widget_set_uposition(xsane.standard_options_shell, XSANE_DIALOG_POS_X, XSANE_DIALOG_POS_Y2);
  sprintf(windowname, "%s %s", WINDOW_STANDARD_OPTIONS, device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.standard_options_shell), (char *) windowname);

                                                               /* shrink grow  auto_shrink */
  gtk_window_set_policy(GTK_WINDOW(xsane.standard_options_shell), FALSE, FALSE, TRUE);
  gtk_signal_connect(GTK_OBJECT(xsane.standard_options_shell), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_standard_option_win_delete), 0);

  xsane_vbox_standard = gtk_vbox_new(TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_standard), 5);
  gtk_container_add(GTK_CONTAINER(xsane.standard_options_shell), xsane_vbox_standard);
  gtk_widget_show(xsane_vbox_standard);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  standard_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_standard), standard_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(standard_dialog_window);



  /* create the scanner advanced options dialog box */

  xsane.advanced_options_shell = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_widget_set_uposition(xsane.advanced_options_shell, XSANE_DIALOG_POS_X2, XSANE_DIALOG_POS_Y2);
  sprintf(windowname, "%s %s",WINDOW_ADVANCED_OPTIONS, device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.advanced_options_shell), (char *) windowname);

                                                               /* shrink grow  auto_shrink */
  gtk_window_set_policy(GTK_WINDOW(xsane.advanced_options_shell), FALSE, FALSE, TRUE);
  gtk_signal_connect(GTK_OBJECT(xsane.advanced_options_shell), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_advanced_option_win_delete), 0);

  xsane_vbox_advanced = gtk_vbox_new(TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_advanced), 5);
  gtk_container_add(GTK_CONTAINER(xsane.advanced_options_shell), xsane_vbox_advanced);
  gtk_widget_show(xsane_vbox_advanced);

  /* create  a subwindow so the advanced dialog keeps its position on rebuilds: */
  advanced_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_advanced), advanced_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(advanced_dialog_window);


  dialog = malloc(sizeof (*dialog));
  if (!dialog)
  {
    printf("Could not create dialog\n");
    return;
  }

  /* fill in dialog structure */

  memset(dialog, 0, sizeof(*dialog));

  dialog->xsane_window           = main_dialog_window;
  dialog->standard_window        = standard_dialog_window;
  dialog->advanced_window        = advanced_dialog_window;
  dialog->dev                    = dev;
  dialog->dev_name               = strdup(devname);
  dialog->num_elements           = num_elements;
  dialog->option_reload_callback = xsane_update_preview;
  dialog->option_reload_arg      = 0;
  dialog->param_change_callback  = xsane_update_param;
  dialog->param_change_arg       = 0;

  dialog->element = malloc(num_elements * sizeof(dialog->element[0]));
  memset(dialog->element, 0, num_elements * sizeof(dialog->element[0]));




  /* realize xsane main dialog */
  /* normally a realize should be ok, but then
     the default size of the scrollwed window is ignored
     so we use a widget_show in that case */

  if (xsane.main_window_fixed)
  {
    gtk_widget_realize(xsane.shell);
  }
  else
  {
    gtk_widget_show(xsane.shell); 
    /* the disadavantage of this is that the main window does
       not have the focus when every window is shown */
  }


  /* define tooltips colors */

  dialog->tooltips = gtk_tooltips_new();
  colormap = gdk_window_get_colormap(xsane.shell->window);

/* I don`t know why the following does not work with gtk-1.2.x */
/* but the gimp has the same problems ;-) */
  /* use black as foreground: */
  dialog->tooltips_fg.red   = 0;
  dialog->tooltips_fg.green = 0;
  dialog->tooltips_fg.blue  = 0;
  gdk_color_alloc(colormap, &dialog->tooltips_fg);

  /* postit yellow (khaki) as background: */
  dialog->tooltips_bg.red   = 61669;
  dialog->tooltips_bg.green = 59113;
  dialog->tooltips_bg.blue  = 35979;
  gdk_color_alloc(colormap, &dialog->tooltips_bg);

  gtk_tooltips_set_colors(dialog->tooltips, &dialog->tooltips_bg, &dialog->tooltips_fg);
  gsg_set_tooltips(dialog, preferences.tooltips_enabled);



  /* create histogram dialog and set colors */
  xsane_create_histogram_dialog(device_text); /* create the histogram dialog */


  /* create backend dependend options */
  panel_build(dialog);



  /* The bottom row of info and start button */

  hbox = GTK_DIALOG(xsane.shell)->action_area;

  frame = gtk_frame_new(0);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show(frame);

  infobox = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(infobox), 2);
  gtk_container_add(GTK_CONTAINER(frame), infobox);
  gtk_widget_show(infobox);

  xsane.info_label = gtk_label_new(TEXT_INFO_BOX);
  gtk_box_pack_start(GTK_BOX(infobox), xsane.info_label, FALSE, FALSE, 0);
  gtk_widget_show(xsane.info_label);



  /* The bottom row of buttons */
  hbox = GTK_DIALOG(xsane.shell)->action_area;


  /* The Scan button */
  button = gtk_button_new_with_label(BUTTON_START);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_scan_dialog, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  xsane.start_button = GTK_OBJECT(button);

  xsane_pref_device_restore();	/* restore device-settings */

  xsane_update_param(dialog, 0);

  gtk_widget_realize(xsane.standard_options_shell); /* is needed for saving window geometry */
  gtk_widget_realize(xsane.advanced_options_shell);

  if (preferences.show_standard_options)
  {
    gtk_widget_show(xsane.standard_options_shell);
  }

  if (preferences.show_advanced_options)
  {
    gtk_widget_show(xsane.advanced_options_shell);
  }

  gtk_widget_show(xsane.shell); /* call as last so focus is on it */

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_update_sliders();

  if ((xsane.show_preview) && (!xsane.preview))
  {
    gtk_signal_emit_by_name(GTK_OBJECT(xsane.show_preview_widget), "toggled");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_preview_widget), xsane.show_preview);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_choose_dialog_ok_callback(void)
{
  gtk_signal_disconnect_by_func(GTK_OBJECT(choose_device_dialog), GTK_SIGNAL_FUNC(xsane_quit), 0);
  gtk_widget_destroy(choose_device_dialog);
  xsane_device_dialog();

  if (!dialog)
  {
    xsane_quit();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_select_device_by_key_callback(GtkWidget * widget, gpointer data)
{
  seldev = (long) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_select_device_by_mouse_callback(GtkWidget * widget, GdkEventButton *event, gpointer data)
{
  seldev = (long) data;
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    xsane_choose_dialog_ok_callback();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint32 xsane_choose_device(void)
{
 GtkWidget *main_vbox, *vbox, *hbox, *button, *device_frame, *device_vbox;
 GSList *owner;
 const SANE_Device *adev;
 gint i;
 char buf[256];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;
 char vendor[9];
 char model[17];
 char type[20];
 int j;

  choose_device_dialog = gtk_dialog_new();
  gtk_window_set_position(GTK_WINDOW(choose_device_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(choose_device_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(choose_device_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_quit), 0);
  snprintf(buf, sizeof(buf), "%s %s", prog_name, WINDOW_DEVICE_SELECTION);
  gtk_window_set_title(GTK_WINDOW(choose_device_dialog), buf);

  main_vbox = GTK_DIALOG(choose_device_dialog)->vbox;

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
  gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(choose_device_dialog);

  style = gtk_widget_get_style(choose_device_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  snprintf(buf, sizeof(buf), "%s/xsane-logo.xpm", STRINGIFY(PATH_SANE_DATA_DIR));  
  pixmap = gdk_pixmap_create_from_xpm(choose_device_dialog->window, &mask, bg_trans, buf);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);


  xsane_separator_new(vbox, 5);


  /* list the drivers with radiobuttons */
  device_frame = gtk_frame_new(TEXT_AVAILABLE_DEVICES);
  gtk_box_pack_start(GTK_BOX(vbox), device_frame, FALSE, FALSE, 2);
  gtk_widget_show(device_frame);

  device_vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(device_vbox), 3);
  gtk_container_add(GTK_CONTAINER(device_frame), device_vbox);

  owner = 0;
  for (i = 0; i < ndevs; i++)
  {
    adev = devlist[i];

    strncpy(vendor, adev->vendor, sizeof(vendor)-1);
    for (j = strlen(vendor); j < sizeof(vendor)-1; j++)
    { vendor[j]=' '; }
    vendor[j]=0;

    strncpy(model, adev->model, sizeof(model)-1);
    for (j = strlen(model); j < sizeof(model)-1; j++)
    { model[j]=' '; }
    model[j]=0;

    strncpy(type, _(adev->type), sizeof(type)-1); /* allow translation of device type */
    for (j = strlen(type); j < sizeof(type)-1; j++)
    { type[j]=' '; }
    type[j]=0;

    snprintf(buf, sizeof(buf), "%s %s %s [%s]", vendor, model, type, adev->name);
    button = gtk_radio_button_new_with_label(owner, (char *) buf);
    gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
                       (GtkSignalFunc) xsane_select_device_by_mouse_callback, (void *) (long) i);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       (GtkSignalFunc) xsane_select_device_by_key_callback, (void *) (long) i);
    gtk_box_pack_start(GTK_BOX(device_vbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);
    owner = gtk_radio_button_group(GTK_RADIO_BUTTON(button));;
  }
  gtk_widget_show(device_vbox);

  /* The bottom row of buttons */
  hbox = GTK_DIALOG(choose_device_dialog)->action_area;

  /* The OK button */
  button = gtk_button_new_with_label(BUTTON_OK);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_choose_dialog_ok_callback, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  /* The Cancel button */
  button = gtk_button_new_with_label(BUTTON_CANCEL);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_quit, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(choose_device_dialog);

  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_usage(void)
{
  printf("%s %s %s\n", TEXT_USAGE, prog_name, TEXT_USAGE_OPTIONS);
  printf("\n%s\n\n", TEXT_HELP);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_init(int argc, char **argv)
{
  char filename[PATH_MAX];
  struct stat st;

  gtk_init(&argc, &argv);
#ifdef HAVE_LIBGIMP_GIMP_H
  gtk_rc_parse(gimp_gtkrc());

  gdk_set_use_xshm(gimp_use_xshm());
#endif

  gsg_make_path(sizeof(filename), filename, 0, 0, "xsane-style", 0, ".rc");
  if (stat(filename, &st) >= 0)
  {
    gtk_rc_parse(filename);
  }
  else
  {
    strncpy(filename, STRINGIFY(PATH_SANE_DATA_DIR) "/xsane-style.rc", sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
    if (stat(filename, &st) >= 0)
    {
      gtk_rc_parse(filename);
    }
  }

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);
  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    fprintf(stderr, "\n\n"
                    "%s %s:\n"
                    "  %s\n"
                    "  %s %d\n"
                    "  %s %d\n"
                    "%s\n\n",
                    prog_name, ERR_ERROR,
                    ERR_MAJOR_VERSION_NR_CONFLICT,
                    ERR_XSANE_MAJOR_VERSION, SANE_V_MAJOR,
                    ERR_BACKEND_MAJOR_VERSION, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                    ERR_PROGRAM_ABORTED);
    return;
  }

  if (argc > 1)
  {
    int ch;

    while((ch = getopt_long(argc, argv, "cd:fghnsvFR", long_options, 0)) != EOF)
    {
      switch(ch)
      {
        case 'g': /* This options is set when xsane is called from the */
                  /* GIMP. If xsane is compiled without GIMP support */
                  /* then you get the error message when GIMP does */
                  /* query or tries to start the xsane plugin! */
#ifndef HAVE_LIBGIMP_GIMP_H
          printf("%s: %s\n", argv[0], ERR_GIMP_SUPPORT_MISSING);
          exit(0);
#endif
         break;

        case 'v': /* --version */
#ifdef HAVE_LIBGIMP_GIMP_H
          printf("%s-%s, %s \"%s\", SANE-%d.%d, %s, %s%s\n",
                 prog_name,
                 XSANE_VERSION, 
                 TEXT_PACKAGE,
                 PACKAGE_VERSION,
                 SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                 SANE_VERSION_MINOR(xsane.sane_backend_versioncode),
                 TEXT_WITH_GIMP_SUPPORT,
                 TEXT_GIMP_VERSION,
                 GIMP_VERSION);
#else
          printf("%s-%s, %s \"%s\", SANE-%d.%d, %s\n",
                 prog_name,
                 XSANE_VERSION, 
                 TEXT_PACKAGE,
                 PACKAGE_VERSION,
                 SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                 SANE_VERSION_MINOR(xsane.sane_backend_versioncode),
                 TEXT_WITHOUT_GIMP_SUPPORT);
#endif
          exit(0);

        case 'd': /* --device-settings */
          xsane.device_set_filename = strdup(optarg);
         break;

        case 's': /* --scan */
          xsane.xsane_mode = XSANE_SCAN;
         break;

        case 'c': /* --copy */
          xsane.xsane_mode = XSANE_COPY;
         break;

        case 'n': /* --No-modes-election */
           xsane.mode_selection = 0;
         break;

        case 'f': /* --fax */
          xsane.xsane_mode = XSANE_FAX;
         break;

        case 'F': /* --Fixed */
           xsane.main_window_fixed = 1;
         break;

        case 'R': /* --Resizeable */
           xsane.main_window_fixed = 0;
         break;

        case 'h': /* --help */
        default:
          xsane_usage();
          exit(0);
      }
    }
  }

  sane_get_devices(&devlist, SANE_FALSE /* local and network devices */);

  /* if devicename is given try to identify it, if not found, open device list */
  if (optind < argc) 
  {
   int ndevs;

    for (ndevs = 0; devlist[ndevs]; ++ndevs)
    {
      if (!strncmp(devlist[ndevs]->name, argv[argc - 1], strlen(argv[argc - 1])))
      {
        seldev = ndevs;
        break; 
      }
    }

    if ((seldev < 0) && (argc > 1))
    {
     static SANE_Device dev;
     static const SANE_Device *device_list[] = { &dev, 0 };

      memset(&dev, 0, sizeof(dev));
      dev.name   = argv[argc - 1];
      dev.vendor = TEXT_UNKNOWN;
      dev.type   = TEXT_UNKNOWN;
      dev.model  = TEXT_UNKNOWN;

      devlist = device_list;
      seldev = 0;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_interface(int argc, char **argv)
{
  xsane.info_label = 0;

  xsane_init(argc, argv); /* initialize xsane variables if command line option is given, set seldev */

  for (ndevs = 0; devlist[ndevs]; ++ndevs); /* count available devices */

  if (seldev >= 0) /* device name is given on cammand line */
  {
    xsane_device_dialog(); /* open device seldev */

    if (!dialog)
    {
      xsane_exit();
    }
  }
  else /* no device name given on command line */
  {
    if (ndevs > 0) /* devices available */
    {
      seldev = 0;
      if (ndevs == 1)
      {
        xsane_device_dialog(); /* open device seldev */
        if (!dialog)
	{
	  xsane_exit();
	}
      }
      else
      {
        xsane_choose_device(); /* open device selection window and get device */
      }
    }
    else /* ndevs == 0, no devices available */
    {
     char buf[256];

      snprintf(buf, sizeof(buf), "%s: %s\n", prog_name, ERR_NO_DEVICES);
      gsg_error(buf, TRUE);
      xsane_exit();
    }
  }

  gtk_main();
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  dialog = 0;
  memset(&xsane, 0, sizeof(xsane)); /* set all values in xsane to 0 */

  xsane.sensitivity         = TRUE;

  xsane.main_window_fixed   = -1; /* no command line option given, use preferences or fixed */

  xsane.mode                = XSANE_STANDALONE;
  xsane.xsane_mode          = XSANE_SCAN;
  xsane.xsane_output_format = XSANE_PNM;
  xsane.mode_selection      = 1; /* enable selection of xsane mode */

  xsane.input_tag           = -1; /* no input tag */

  xsane.histogram_lines = 1;

  xsane.zoom             = 1.0;
  xsane.resolution       = 0.0;
  xsane.copy_number      = 1;

  xsane.gamma            = 1.0;
  xsane.gamma_red        = 1.0;
  xsane.gamma_green      = 1.0;
  xsane.gamma_blue       = 1.0;
  xsane.brightness       = 0.0;
  xsane.brightness_red   = 0.0;
  xsane.brightness_green = 0.0;
  xsane.brightness_blue  = 0.0;
  xsane.contrast         = 0.0;
  xsane.contrast_red     = 0.0;
  xsane.contrast_green   = 0.0;
  xsane.contrast_blue    = 0.0;
  xsane.slider_gray.value[2]  = 100.0;
  xsane.slider_gray.value[1]  = 50.0;
  xsane.slider_gray.value[0]  = 0.0;
  xsane.slider_red.value[2]   = 100.0;
  xsane.slider_red.value[1]   = 50.0;
  xsane.slider_red.value[0]   = 0.0;
  xsane.slider_green.value[2] = 100.0;
  xsane.slider_green.value[1] = 50.0;
  xsane.slider_green.value[0] = 0.0;
  xsane.slider_blue.value[2]  = 100.0;
  xsane.slider_blue.value[1]  = 50.0;
  xsane.slider_blue.value[0]  = 0.0;
  xsane.auto_white       = 100.0;
  xsane.auto_gray        = 50.0;
  xsane.auto_black       = 0.0;

  xsane.histogram_red    = 1;
  xsane.histogram_green  = 1;
  xsane.histogram_blue   = 1;
  xsane.histogram_int    = 1;
  xsane.histogram_log    = 1;

  xsane.xsane_color                = TRUE;
  xsane.scanner_gamma_color        = FALSE;
  xsane.scanner_gamma_gray         = FALSE;
  xsane.enhancement_rgb_default    = TRUE;

  prog_name = strrchr(argv[0], '/');
  if (prog_name)
  {
    ++prog_name;
  }
  else
  {
    prog_name = argv[0];
  }
#if 0
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);         
#endif
  bindtextdomain(prog_name, LOCALEDIR);
  textdomain(prog_name);         

#ifdef HAVE_LIBGIMP_GIMP_H
  {
    GPrintFunc old_print_func;
    int result;

    /* Temporarily install a print function that discards all output.
       This is to avoid annoying "you must run this program under
       gimp" messages when xsane gets invoked in stand-alone
       mode.  */
    old_print_func = g_set_print_handler((GPrintFunc) null_print_func);

    /* gimp_main() returns 1 if xsane wasn't invoked by GIMP */
    result = gimp_main(argc, argv);
    g_set_message_handler(old_print_func);
    if (result)
    {
      xsane_interface(argc, argv);
    }
  }
#else
  xsane_interface(argc, argv);
#endif
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
