/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2005 Oliver Rauch
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
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"
#include "xsane-scan.h"
#include "xsane-rc-io.h"
#include "xsane-device-preferences.h"
#include "xsane-preferences.h"
#include "xsane-icons.h"
#include "xsane-batch-scan.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

#include <sys/wait.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
  {"license", no_argument, 0, 'l'},
  {"device-settings", required_argument, 0, 'd'},
  {"save", no_argument, 0, 's'},
  {"viewer", no_argument, 0, 'V'},
  {"copy", no_argument, 0, 'c'},
  {"fax", no_argument, 0, 'f'},
#ifdef XSANE_ACTIVATE_MAIL
  {"mail", no_argument, 0, 'm'},
#endif
  {"no-mode-selection", no_argument, 0, 'n'},
  {"Fixed", no_argument, 0, 'F'},
  {"Resizeable", no_argument, 0, 'R'},
  {"print-filenames", no_argument, 0, 'p'},
  {"force-filename", required_argument, 0, 'N'},
  {0, }
};

/* ---------------------------------------------------------------------------------------------------------------------- */

static const pref_default_preset_area_t pref_default_preset_area[] =
{
 { MENU_ITEM_SURFACE_FULL_SIZE, 0,      0,      INF,    INF },
 { MENU_ITEM_SURFACE_DIN_A3P,   0,      0,      296.98, 420.0 },
 { MENU_ITEM_SURFACE_DIN_A3L,   0,      0,      420.0,  296.98 },
 { MENU_ITEM_SURFACE_DIN_A4P,   0,      0,      210.0,  296.98 },
 { MENU_ITEM_SURFACE_DIN_A4L,   0,      0,      296.98, 210.0 },
 { MENU_ITEM_SURFACE_DIN_A5P,   0,      0,      148.5,  210.0 },
 { MENU_ITEM_SURFACE_DIN_A5L,   0,      0,      210.0,  148.5 },
 { MENU_ITEM_SURFACE_13cmx18cm, 0,      0,      130.0,  180.0 },
 { MENU_ITEM_SURFACE_18cmx13cm, 0,      0,      180.0,  130.0 },
 { MENU_ITEM_SURFACE_10cmx15cm, 0,      0,      100.0,  150.0 },
 { MENU_ITEM_SURFACE_15cmx10cm, 0,      0,      150.0,  100.0 },
 { MENU_ITEM_SURFACE_9cmx13cm,  0,      0,      90.0,   130.0 },
 { MENU_ITEM_SURFACE_13cmx9cm,  0,      0,      130.0,  90.0 },
 { MENU_ITEM_SURFACE_legal_P,   0,      0,      215.9,  355.6 },
 { MENU_ITEM_SURFACE_legal_L,   0,      0,      355.6,  215.9 },
 { MENU_ITEM_SURFACE_letter_P,  0,      0,      215.9,  279.4 },
 { MENU_ITEM_SURFACE_letter_L,  0,      0,      279.4,  215.9 },
};

/* ---------------------------------------------------------------------------------------------------------------------- */

static const Preferences_medium_t pref_default_medium[]=
{
/* medium				shadow                  highlight                gamma                negative */
/* name					gray  red   green blue  gray  red   green blue   gray  red   gren  blue */
 { MENU_ITEM_MEDIUM_FULL_COLOR_RANGE,	 0.0,  0.0,  0.0,  0.0, 100.0,100.0,100.0,100.0, 1.00, 1.00, 1.00, 1.00 , 0},
 { MENU_ITEM_MEDIUM_SLIDE,		 0.0,  0.0,  0.0,  0.0,  40.0, 40.0, 40.0, 40.0, 1.00, 1.00, 1.00, 1.00 , 0},
 { MENU_ITEM_MEDIUM_STANDARD_NEG,	 0.0,  7.0,  1.0,  0.0,  66.0, 66.0, 33.0, 16.0, 1.00, 1.00, 1.00, 1.00 , 1},
 { MENU_ITEM_MEDIUM_AGFA_NEG,		 0.0,  6.0,  2.0,  0.0,  31.0, 61.0, 24.0, 13.0, 1.00, 1.00, 1.00, 1.00 , 1},
 { MENU_ITEM_MEDIUM_AGFA_NEG_XRG200_4,	 0.0, 12.0,  2.0,  1.6,  35.0, 61.5, 21.5, 14.5, 1.00, 0.80, 0.67, 0.60 , 1},
 { MENU_ITEM_MEDIUM_AGFA_NEG_HDC_100,	 0.0,  3.5,  1.0,  0.5,  26.5, 53.5, 22.0, 17.0, 1.00, 0.79, 0.65, 0.60 , 1},
 { MENU_ITEM_MEDIUM_FUJI_NEG,		 0.0,  7.0,  1.0,  0.0,  32.0, 64.0, 33.0, 16.0, 1.00, 1.00, 1.00, 1.00 , 1},
 { MENU_ITEM_MEDIUM_KODAK_NEG,		 0.0,  9.0,  2.0,  0.0,  27.0, 54.0, 18.0, 12.0, 1.00, 1.00, 1.00, 1.00 , 1},
 { MENU_ITEM_MEDIUM_KONICA_NEG,		 0.0,  3.0,  0.0,  0.0,  25.0, 38.0, 21.0, 14.0, 1.00, 1.00, 1.00, 1.00 , 1},
 { MENU_ITEM_MEDIUM_KONICA_NEG_VX_100,	 0.0,  2.0,  0.0,  0.0,  25.0, 46.0, 22.0, 13.5, 1.00, 0.74, 0.56, 0.53 , 1},
 { MENU_ITEM_MEDIUM_ROSSMANN_NEG_HR_100, 0.0,  7.0,  1.0,  1.6,  26.5, 58.0, 25.5, 19.0, 1.00, 0.54, 0.43, 0.41 , 1}
};

/* ---------------------------------------------------------------------------------------------------------------------- */

int DBG_LEVEL = 0;
static guint xsane_resolution_timer = 0;
static guint xsane_mail_send_timer = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

struct Xsane xsane;				/* most xsane dependant values */

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_scanmode_number[] = { XSANE_VIEWER, XSANE_SAVE, XSANE_COPY, XSANE_FAX, XSANE_MAIL };

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_GTK_NAME_RESOLUTION   "GtkMenuResolution"
#define XSANE_GTK_NAME_X_RESOLUTION "GtkMenuXResolution"
#define XSANE_GTK_NAME_Y_RESOLUTION "GtkMenuYResolution"

#define XSANE_GTK_NAME_ZOOM   "GtkMenuZoom"
#define XSANE_GTK_NAME_X_ZOOM "GtkMenuXZoom"
#define XSANE_GTK_NAME_Y_ZOOM "GtkMenuYZoom"

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

static int xsane_option_defined(char *string);
static void xsane_zoom_update(GtkAdjustment *adj_data, double *val);
static void xsane_resolution_scale_update(GtkAdjustment *adj_data, double *val);
static void xsane_threshold_changed(void);
static void xsane_gamma_changed(GtkAdjustment *adj_data, double *val);
static void xsane_set_modus_defaults(void);
static void xsane_modus_callback(GtkWidget *xsane_parent, int *num);
static void xsane_enhancement_rgb_default_callback(GtkWidget *widget);
static void xsane_enhancement_negative_callback(GtkWidget *widget);
static void xsane_auto_enhancement_callback(GtkWidget *widget);
static void xsane_show_standard_options_callback(GtkWidget *widget);
static void xsane_show_advanced_options_callback(GtkWidget *widget);
static void xsane_show_histogram_callback(GtkWidget *widget);
#ifdef HAVE_WORKING_GTK_GAMMACURVE
static void xsane_show_gamma_callback(GtkWidget *widget);
#endif
static void xsane_show_batch_scan_callback(GtkWidget *widget);
static void xsane_printer_callback(GtkWidget *widget, gpointer data);
void xsane_pref_save(void);
static int xsane_pref_restore(void);
static void xsane_pref_save_media(void);
static void xsane_pref_restore_media(void);
static RETSIGTYPE xsane_quit_handler(int signal);
static RETSIGTYPE xsane_sigchld_handler(int signal);
static void xsane_quit(void);
static void xsane_exit(void);
static gint xsane_standard_option_win_delete(GtkWidget *widget, gpointer data);
static gint xsane_advanced_option_win_delete(GtkWidget *widget, gpointer data);
static gint xsane_scan_win_delete(GtkWidget *w, gpointer data);
static gint xsane_preview_window_destroyed(GtkWidget *widget, gpointer call_data);
static void xsane_show_preview_callback(GtkWidget * widget, gpointer call_data);
static GtkWidget *xsane_files_build_menu(void);
static gint xsane_medium_context_menu_callback(GtkWidget *widget, GdkEvent *event);
static void xsane_set_medium_callback(GtkWidget *widget, gpointer data);
static void xsane_set_pref_unit_callback(GtkWidget *widget, gpointer data);
static void xsane_edit_medium_definition_callback(GtkWidget *widget, gpointer data);
static void xsane_set_update_policy_callback(GtkWidget *widget, gpointer data);
static gint xsane_close_info_callback(GtkWidget *widget, gpointer data);
static void xsane_info_dialog(GtkWidget *widget, gpointer data);
static void xsane_about_dialog(GtkWidget *widget, gpointer data);
static void xsane_about_translation_dialog(GtkWidget *widget, gpointer data);
static gint xsane_fax_dialog_delete();
static void xsane_fax_dialog(void);
static void xsane_fax_dialog_close(void);
static void xsane_fax_receiver_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_project_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_fine_mode_callback(GtkWidget *widget);
static void xsane_fax_project_update_project_status();
void xsane_fax_project_save(void);
static void xsane_fax_project_load(void);
static void xsane_fax_project_delete(void);
static void xsane_fax_project_create(void);
static void xsane_list_entrys_swap(GtkWidget *list_item_1, GtkWidget *list_item_2);
static void xsane_fax_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_insert_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_show_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_send(void);
#ifdef XSANE_ACTIVATE_MAIL
static gint xsane_mail_dialog_delete();
static void xsane_mail_filetype_callback(GtkWidget *filetype_option_menu, char *filetype);
static void xsane_mail_dialog(void);
static void xsane_mail_dialog_close(void);
static void xsane_mail_receiver_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_mail_subject_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_mail_project_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_mail_html_mode_callback(GtkWidget *widget);
void xsane_mail_project_save(void);
static void xsane_mail_project_display_status(void);
static void xsane_mail_project_load(void);
static void xsane_mail_project_delete(void);
static void xsane_mail_project_update_project_status();
static void xsane_mail_project_create(void);
static void xsane_mail_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_mail_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_mail_entry_rename_callback(GtkWidget *widget, gpointer list);
static void xsane_mail_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_mail_show_callback(GtkWidget *widget, gpointer data);
#if 0
static void xsane_mail_edit_callback(GtkWidget *widget, gpointer data);
#endif
static void xsane_mail_send_process(void);
static void xsane_mail_send(void);
#endif
static void xsane_pref_toggle_tooltips(GtkWidget *widget, gpointer data);
static void xsane_show_eula(GtkWidget *widget, gpointer data);
static void xsane_show_gpl(GtkWidget *widget, gpointer data);
static void xsane_show_doc(GtkWidget *widget, gpointer data);
static GtkWidget *xsane_view_build_menu(void);
static GtkWidget *xsane_window_build_menu(void);
static GtkWidget *xsane_preferences_build_menu(void);
static GtkWidget *xsane_help_build_menu(void);
static void xsane_device_dialog(void);
static void xsane_choose_dialog_ok_callback(void);
static void xsane_select_device_by_key_callback(GtkWidget * widget, gpointer data);
static int xsane_select_device_by_mouse_callback(GtkWidget * widget, GdkEventButton *event, gpointer data);
static void xsane_choose_device(void);
static void xsane_usage(void);
static int xsane_init(int argc, char **argv);
void xsane_interface(int argc, char **argv);
int main(int argc, char ** argv);

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef __GNUC__
void xsane_debug_message(int level, const char *fmt, ...)
{
  if (DBG_LEVEL >= level)
  {
    va_list ap;
    fprintf(stderr, "[xsane] ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }
}
#endif

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

static void xsane_threshold_changed()
{
  DBG(DBG_proc, "xsane_threshold_changed\n");

  if (xsane.param.depth == 1) /* lineart mode */
  {
    if ( (xsane.lineart_mode == XSANE_LINEART_GRAYSCALE) || (xsane.lineart_mode == XSANE_LINEART_XSANE) )
    {
     const SANE_Option_Descriptor *opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.threshold);

      if (opt)
      {
       SANE_Word threshold_value;
       double threshold = xsane.threshold * xsane.threshold_mul + xsane.threshold_off;

        if (opt->type == SANE_TYPE_FIXED)
        {
          threshold_value = SANE_FIX(threshold);
        }
        else
        {
          threshold_value = (int) threshold;
        }

        xsane_back_gtk_set_option(xsane.well_known.threshold, &threshold_value, SANE_ACTION_SET_VALUE);
      }
    }
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gamma_changed(GtkAdjustment *adj_data, double *val)
{
  DBG(DBG_proc, "xsane_gamma_changed\n");

  *val = adj_data->value;

  if (!xsane.block_enhancement_update)
  {
    xsane_enhancement_by_gamma();
    xsane_threshold_changed();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_modus_defaults(void)
{
  DBG(DBG_proc, "xsane_set_modus_defaults\n");

  switch(xsane.xsane_mode)
  {
    case XSANE_VIEWER:
      xsane_define_maximum_output_size();
     break;

    case XSANE_SAVE:
      xsane_define_maximum_output_size();
     break;

    case XSANE_COPY: /* set zoomfactor to 1.0 and select full preview area */
      {
       int printer_resolution;

        switch (xsane.param.format)
        {
          case SANE_FRAME_GRAY:
            if (xsane.param.depth == 1)
            {
              printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
            }
            else
            {
              printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
            }
          break;

          case SANE_FRAME_RGB:
          case SANE_FRAME_RED:
          case SANE_FRAME_GREEN:
          case SANE_FRAME_BLUE:
          default:
            printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
          break;
        }    
        xsane.zoom   = 1.0;
        xsane.zoom_x = 1.0;
        xsane.zoom_y = 1.0;

        xsane.resolution   = xsane.zoom   * printer_resolution;
        xsane.resolution_x = xsane.zoom_x * printer_resolution;
        xsane.resolution_y = xsane.zoom_y * printer_resolution;

        xsane_set_all_resolutions();
        xsane_define_maximum_output_size(); /* must come before select_full_preview_area */
        preview_select_full_preview_area(xsane.preview);
      }
     break;

    case XSANE_FAX:
      /* select full preview area */
      xsane_define_maximum_output_size(); /* must come before select_full_preview_area */
      preview_select_full_preview_area(xsane.preview);
     break;

    case XSANE_MAIL:
      xsane_define_maximum_output_size();
     break;

    default:
      xsane_define_maximum_output_size();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_modus_callback(GtkWidget *xsane_parent, int *num)
{
  DBG(DBG_proc, "xsane_modus_callback\n");

  xsane.xsane_mode = *num;
  preferences.xsane_mode = *num;

  xsane_set_modus_defaults(); /* set defaults and maximum output size */
  xsane_refresh_dialog();

  if ((xsane.xsane_mode == XSANE_SAVE) || (xsane.xsane_mode == XSANE_VIEWER) || (xsane.xsane_mode == XSANE_COPY))
  {
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
  }

  if (xsane.xsane_mode != XSANE_FAX)
  {
    xsane_fax_dialog_close();
  }

#ifdef XSANE_ACTIVATE_MAIL
  if (xsane.xsane_mode != XSANE_MAIL)
  {
    if (xsane.mail_project_save)
    {
      xsane.mail_project_save = 0;
      xsane_mail_project_save();
    }

    xsane_mail_dialog_close();
  }
#endif

  /* make sure dialogs are rebuild, otherwise we can get in trouble */
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_rgb_default_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_enhancement_rgb_default_callback\n");

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
  xsane_update_gamma_curve(FALSE);
  xsane_refresh_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_negative_callback(GtkWidget * widget)
{
 double v0;

  DBG(DBG_proc, "xsane_enhancement_negative_callback\n");

  if (xsane.negative != (GTK_TOGGLE_BUTTON(widget)->active != 0))
  {
    v0 = xsane.slider_gray.value[0];
    xsane.slider_gray.value[0] = 100.0 - xsane.slider_gray.value[2];
    xsane.slider_gray.value[1] = 100.0 - xsane.slider_gray.value[1];
    xsane.slider_gray.value[2] = 100.0 - v0;

    if (!xsane.enhancement_rgb_default)
    {
      v0 = xsane.slider_red.value[0];
      xsane.slider_red.value[0] = 100.0 - xsane.slider_red.value[2];
      xsane.slider_red.value[1] = 100.0 - xsane.slider_red.value[1];
      xsane.slider_red.value[2] = 100.0 - v0;

      v0 = xsane.slider_green.value[0];
      xsane.slider_green.value[0] = 100.0 - xsane.slider_green.value[2];
      xsane.slider_green.value[1] = 100.0 - xsane.slider_green.value[1];
      xsane.slider_green.value[2] = 100.0 - v0;

      v0 = xsane.slider_blue.value[0];
      xsane.slider_blue.value[0] = 100.0 - xsane.slider_blue.value[2];
      xsane.slider_blue.value[1] = 100.0 - xsane.slider_blue.value[1];
      xsane.slider_blue.value[2] = 100.0 - v0;
    }
  }

  xsane.negative = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  xsane_update_sliders();
  xsane_enhancement_by_histogram(TRUE);
  xsane_update_gamma_curve(TRUE /* update raw */);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_auto_enhancement_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_auto_enhancement_callback\n");

  xsane_calculate_raw_histogram();

  xsane_set_auto_enhancement();

  xsane_enhancement_by_histogram(preferences.auto_enhance_gamma);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_standard_options_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_show_standard_options_callback\n");

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
  DBG(DBG_proc, "xsane_show_advanced_options_callback\n");

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

static void xsane_show_resolution_list_callback(GtkWidget *widget)
{
  DBG(DBG_proc, "xsane_show_resolution_list_callback\n");

  preferences.show_resolution_list = (GTK_CHECK_MENU_ITEM(widget)->active != 0);

  xsane_refresh_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_histogram_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_show_histogram_callback\n");

  preferences.show_histogram = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_histogram)
  {
    xsane_update_histogram(TRUE /* update raw */);
    gtk_widget_show(xsane.histogram_dialog);
  }
  else
  {
    gtk_widget_hide(xsane.histogram_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_WORKING_GTK_GAMMACURVE
static void xsane_show_gamma_callback(GtkWidget *widget)
{
  DBG(DBG_proc, "xsane_show_gamma_callback\n");

  preferences.show_gamma = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_gamma)
  {
    xsane_update_gamma_dialog();
    gtk_widget_show(xsane.gamma_dialog);
  }
  else
  {
    gtk_widget_hide(xsane.gamma_dialog);
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_batch_scan_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_show_batch_scan_callback\n");

  preferences.show_batch_scan = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_batch_scan)
  {
    gtk_widget_show(xsane.batch_scan_dialog);
  }
  else
  {
    gtk_widget_hide(xsane.batch_scan_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_paper_orientation_callback(GtkWidget *widget, gpointer data)
{
 int pos = (int) data;

  DBG(DBG_proc, "xsane_paper_orientation_callback\n");

  preferences.paper_orientation = pos;
  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printer_callback(GtkWidget *widget, gpointer data)
{
 int printer_resolution; 

  DBG(DBG_proc, "xsane_printer_callback\n");

  preferences.printernr = (int) data;

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }       

  xsane.resolution   = xsane.zoom   * printer_resolution;
  xsane.resolution_x = xsane.zoom_x * printer_resolution;
  xsane.resolution_y = xsane.zoom_y * printer_resolution;

  xsane_set_all_resolutions();
  xsane_define_maximum_output_size();
  xsane_back_gtk_refresh_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_resolution_timer_callback(GtkAdjustment *adj)
{
  if ((adj) && (!preferences.show_resolution_list)) /* make sure adjustment is valid */
  {
   float val = adj->value;

    adj->value += 1.0; /* we need this to make sure that set_value really redraws the widgets */
    gtk_adjustment_set_value(adj, val);
  }

  gtk_timeout_remove(xsane_resolution_timer);
  xsane_resolution_timer = 0;

 return 0; /* stop timeout */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_resolution_scale_update(GtkAdjustment *adj, double *val)
{
 int printer_resolution; 

/* gtk does not make sure that the value is quantisized correct */
  float diff, old, new, quant;

  DBG(DBG_proc, "xsane_resolution_scale_update\n");

  quant = adj->step_increment;

  if (quant != 0)
  {
    new = adj->value;
    old = *val;
    diff = quant*((int) ((new - old)/quant));

    *val = old + diff;
    adj->value = *val;

#ifndef _WIN32
    /* the resolution slider gets almost unusable when we do this with win32 */
    if (xsane_resolution_timer)
    {
      gtk_timeout_remove(xsane_resolution_timer);
      xsane_resolution_timer = 0;
    }
    xsane_resolution_timer = gtk_timeout_add(XSANE_HOLD_TIME, (GtkFunction) xsane_resolution_timer_callback, (gpointer) adj);
#endif
  }
  else
  {
    *val = adj->value;
  }

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }       

  xsane_set_all_resolutions();

  xsane_update_param(0);
  xsane.zoom   = xsane.resolution   / printer_resolution;
  xsane.zoom_x = xsane.resolution_x / printer_resolution;
  xsane.zoom_y = xsane.resolution_y / printer_resolution;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_resolution_list_callback(GtkWidget *widget, gpointer data)
{
 MenuItem *menu_item = data;
 SANE_Word val;
 const gchar *name = gtk_widget_get_name(widget->parent);
 int printer_resolution; 

  DBG(DBG_proc, "xsane_resolution_list_callback\n");

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }       

  sscanf(menu_item->label, "%d", &val);

  if (!strcmp(name, XSANE_GTK_NAME_RESOLUTION))
  {
    xsane.resolution   = val;
    xsane.resolution_x = val;
    xsane.resolution_y = val;

    xsane_set_resolution(xsane.well_known.dpi,   xsane.resolution);
    xsane_set_resolution(xsane.well_known.dpi_x, xsane.resolution_x);
    xsane_set_resolution(xsane.well_known.dpi_y, xsane.resolution_y);

    xsane.zoom   = xsane.resolution   / printer_resolution;
    xsane.zoom_x = xsane.resolution_x / printer_resolution;
    xsane.zoom_y = xsane.resolution_y / printer_resolution;
  }
  else if (!strcmp(name, XSANE_GTK_NAME_X_RESOLUTION))
  {
    xsane.resolution   = val;
    xsane.resolution_x = val;
    xsane_set_resolution(xsane.well_known.dpi_x, xsane.resolution_x);
    xsane.zoom = xsane.resolution / printer_resolution;
  }
  else if (!strcmp(name, XSANE_GTK_NAME_Y_RESOLUTION))
  {
    xsane.resolution_y = val;
    xsane_set_resolution(xsane.well_known.dpi_y, xsane.resolution_y);
    xsane.zoom = xsane.resolution / printer_resolution;
  }

  xsane_update_param(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_resolution_widget_new(GtkWidget *parent, int well_known_option, double *resolution, const char *image_xpm[],
                                       const gchar *desc, const gchar *widget_name)
{
 GtkObject *resolution_widget;
 const SANE_Option_Descriptor *opt;

  DBG(DBG_proc, "xsane_resolution_widget_new\n");

  opt = xsane_get_option_descriptor(xsane.dev, well_known_option);

  if (!opt)
  {
    return -1; /* options does not exist */
  }
  else
  {
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      switch (opt->constraint_type)
      {
        case SANE_CONSTRAINT_RANGE:
        {
         double quant= 0.0;
         double min  = 0.0;
         double max  = 0.0;
         double val  = 0.0;
         SANE_Word value;

          gtk_widget_set_sensitive(xsane.show_resolution_list_widget, TRUE); 
          xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_GET_VALUE, &value, 0); 

          switch (opt->type)
          {
            case SANE_TYPE_INT:
              min   = opt->constraint.range->min;
              max   = opt->constraint.range->max;
              quant = opt->constraint.range->quant;
              val   = (int) value;
            break;

            case SANE_TYPE_FIXED:
              min   = SANE_UNFIX(opt->constraint.range->min);
              max   = SANE_UNFIX(opt->constraint.range->max);
              quant = SANE_UNFIX(opt->constraint.range->quant);
              val   = SANE_UNFIX(value);
            break;

            default:
              DBG(DBG_error, "resolution_widget_new: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
          }

          if (quant == 0)
          {
            quant = 1.0;
          }

          *resolution = val; /* set backend predefined value */

          if (!preferences.show_resolution_list) /* user wants slider */ 
          {
            xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(parent), image_xpm, desc,
                                        min, max, quant, quant*10, 0, resolution, &resolution_widget,
                                        well_known_option, xsane_resolution_scale_update, SANE_OPTION_IS_SETTABLE(opt->cap));
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

            xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_GET_VALUE, &wanted_res, 0); 
            if (opt->type == SANE_TYPE_FIXED)
            {
              wanted_res = (int) SANE_UNFIX(wanted_res);
            }

            str_list = malloc((max_items + 1) * sizeof(str_list[0]));

            sprintf(str, "%d", (int) max);
            str_list[j++] = strdup(str);

            i=9;
            while ((j < max_items) && (res > 50) && (res > min) && (i > 0))
            {
              mul = ((double) i) / (i+1);
              res = (int) (max * mul);
              if  (res/mul == max)
              {
                res = xsane_find_best_resolution(well_known_option, res);
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
                res = xsane_find_best_resolution(well_known_option, res);
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

            xsane_option_menu_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(parent), image_xpm, desc, str_list, str, &resolution_widget, well_known_option,
                                              xsane_resolution_list_callback, SANE_OPTION_IS_SETTABLE(opt->cap), widget_name);

            free(str_list);
            *resolution = val;
            xsane_set_resolution(well_known_option, *resolution);
          }
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
              xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_GET_VALUE, &val, 0); 
              val = xsane_find_best_resolution(well_known_option, val); /* when backends uses default value not in list or range */
              sprintf(str, "%d", (int) val);
            break;

            case SANE_TYPE_FIXED:
              for (j = 0; j < items; ++j)
              {
                sprintf(str, "%d", (int) SANE_UNFIX(opt->constraint.word_list[j + 1]));
                str_list[j] = strdup(str);
               }
              str_list[j] = 0;
              xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_GET_VALUE, &val, 0); 
              val = xsane_find_best_resolution(well_known_option, val); /* when backends uses default value not in list or range */
              sprintf(str, "%d", (int) SANE_UNFIX(val));
            break;

            default:
              DBG(DBG_error, "resolution_word_list_creation: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
          }


          xsane_option_menu_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(parent), image_xpm, desc,
                                            str_list, str, &resolution_widget, well_known_option,
                                            xsane_resolution_list_callback, SANE_OPTION_IS_SETTABLE(opt->cap), widget_name);
          free(str_list);
        }
        break;

        default:
         break;
      }	/* constraint type */

      return 0; /* everything is ok */

    } /* if resolution option active */

    return 1; /* not active */

  } /* if (opt) */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_zoom_update(GtkAdjustment *adj, double *val)
{
 int printer_resolution;

  DBG(DBG_proc, "xsane_zoom_update\n");

  *val=adj->value;

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }       

  /* update all resolutions */
  xsane.resolution   = xsane.zoom   * printer_resolution;
  xsane.resolution_x = xsane.zoom_x * printer_resolution;
  xsane.resolution_y = xsane.zoom_y * printer_resolution;

  xsane_set_all_resolutions();

  xsane_update_param(0);

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_zoom_widget_new(GtkWidget *parent, int well_known_option, double *zoom, double resolution,
                                 const char *image_xpm[], const gchar *desc)
{
 const SANE_Option_Descriptor *opt;
 int printer_resolution;

  DBG(DBG_proc, "xsane_zoom_widget_new\n");

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }       

  opt = xsane_get_option_descriptor(xsane.dev, well_known_option);
  if (!opt)
  {
    return -1; /* option not available */
  }
  else
  {
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
     double min = 0.0;
     double max = 0.0;
     SANE_Word val = 0.0;

      xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_GET_VALUE, &val, 0); 

      switch (opt->constraint_type)
      {
        case SANE_CONSTRAINT_RANGE:
          switch (opt->type)
          {
            case SANE_TYPE_INT:
              min   = ((double) opt->constraint.range->min) / printer_resolution;
              max   = ((double) opt->constraint.range->max) / printer_resolution;
            break;

            case SANE_TYPE_FIXED:
              min   = SANE_UNFIX(opt->constraint.range->min) / printer_resolution;
              max   = SANE_UNFIX(opt->constraint.range->max) / printer_resolution;
              val   = SANE_UNFIX(val);
            break;

            default:
              DBG(DBG_error, "zoom_scale_update: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
          }
        break;

        case SANE_CONSTRAINT_WORD_LIST:
          xsane_get_bounds(opt, &min, &max);
          min   = min / printer_resolution;
          max   = max / printer_resolution;
        break;

        default:
          DBG(DBG_error, "zoom_scale_update: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
      }

      if (resolution == 0) /* no prefered value */
      {
        resolution = val; /* set backend predefined value */
      }

      *zoom = resolution / printer_resolution;

      xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(parent), image_xpm, desc, min, max, 0.01, 0.1, 2,
                                  zoom, &xsane.zoom_widget, well_known_option, xsane_zoom_update,
                                  SANE_OPTION_IS_SETTABLE(opt->cap));

      return 0; /* everything is ok */
    }
    return 1; /* option not active */
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scanmode_menu_callback(GtkWidget *widget, gpointer data)
{
 MenuItem *menu_item = data;
 DialogElement *elem = menu_item->elem;
 const SANE_Option_Descriptor *opt;
 int opt_num;
 int printer_resolution;
 double zoom, zoom_x, zoom_y;

  DBG(DBG_proc, "xsane_scanmode_menu_callback\n");

  zoom   = xsane.zoom;
  zoom_x = xsane.zoom_x;
  zoom_y = xsane.zoom_y;

  opt_num = elem - xsane.element;
  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
  xsane_back_gtk_set_option(opt_num, menu_item->label, SANE_ACTION_SET_VALUE);

  if (xsane.xsane_mode == XSANE_COPY)
  {
    switch (xsane.param.format)
    {
      case SANE_FRAME_GRAY:
        if (xsane.param.depth == 1)
        {
          printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
        }
        else
        {
          printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
        }
      break;

      case SANE_FRAME_RGB:
      case SANE_FRAME_RED:
      case SANE_FRAME_GREEN:
      case SANE_FRAME_BLUE:
      default:
        printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
      break;
    }

    xsane.resolution   = xsane_find_best_resolution(xsane.well_known.dpi,   zoom   * printer_resolution);
    xsane.resolution_x = xsane_find_best_resolution(xsane.well_known.dpi_x, zoom_x * printer_resolution);
    xsane.resolution_y = xsane_find_best_resolution(xsane.well_known.dpi_y, zoom_y * printer_resolution);

    xsane_set_all_resolutions(); /* make sure resolution, resolution_x and resolution_y are up to date */
    xsane_back_gtk_refresh_dialog(); /* update resolution - a bit overkill, any better idea? */
    xsane_define_maximum_output_size(); /* draw maximum output frame in preview window if necessary */
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_update_xsane_callback() /* creates the XSane option window */
{
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
 GtkWidget *button;
 gchar buf[256];

  DBG(DBG_proc, "xsane_update_xsane_callback\n");

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
    gtk_menu_set_accel_group(GTK_MENU(xsane_modus_menu), xsane.accelerator_group);


    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_VIEWER);
    gtk_widget_add_accelerator(xsane_modus_item, "activate", xsane.accelerator_group, GDK_V, GDK_CONTROL_MASK, DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_widget_set_size_request(xsane_modus_item, 60, -1);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    g_signal_connect(GTK_OBJECT(xsane_modus_item), "activate", (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_VIEWER]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_SAVE);
    gtk_widget_add_accelerator(xsane_modus_item, "activate", xsane.accelerator_group, GDK_S, GDK_CONTROL_MASK, DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    g_signal_connect(GTK_OBJECT(xsane_modus_item), "activate", (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_SAVE]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_COPY);
    gtk_widget_add_accelerator(xsane_modus_item, "activate", xsane.accelerator_group, GDK_C, GDK_CONTROL_MASK, DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    g_signal_connect(GTK_OBJECT(xsane_modus_item), "activate", (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_COPY]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_FAX);
    gtk_widget_add_accelerator(xsane_modus_item, "activate", xsane.accelerator_group, GDK_F, GDK_CONTROL_MASK, DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    g_signal_connect(GTK_OBJECT(xsane_modus_item), "activate", (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_FAX]);
    gtk_widget_show(xsane_modus_item);

#ifdef XSANE_ACTIVATE_MAIL
    xsane_modus_item = gtk_menu_item_new_with_label(MENU_ITEM_MAIL);
    gtk_widget_add_accelerator(xsane_modus_item, "activate", xsane.accelerator_group, GDK_M, GDK_CONTROL_MASK, DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    g_signal_connect(GTK_OBJECT(xsane_modus_item), "activate", (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_MAIL]);
    gtk_widget_show(xsane_modus_item);
#endif

    xsane_modus_option_menu = gtk_option_menu_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_modus_option_menu, DESC_XSANE_MODE);
    gtk_box_pack_end(GTK_BOX(xsane_hbox_xsane_modus), xsane_modus_option_menu, FALSE, FALSE, 2);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_modus_option_menu), xsane_modus_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_modus_option_menu), xsane.xsane_mode);
    gtk_widget_show(xsane_modus_option_menu);
    gtk_widget_show(xsane_hbox_xsane_modus);

    xsane.xsanemode_widget = xsane_modus_option_menu;
  }

  {
   GtkWidget *pixmapwidget;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   GtkWidget *hbox;
   GtkWidget *xsane_medium_option_menu, *xsane_medium_menu, *xsane_medium_item;
   const SANE_Option_Descriptor *opt;
   int i;


    /* scanmode */
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.scanmode);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        hbox = gtk_hbox_new(FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) colormode_xpm);
        pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
        gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
        gdk_drawable_unref(pixmap);
        gtk_widget_show(pixmapwidget);

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_STRING_LIST:
          {
            char *set;
            SANE_Status status;

            /* use a "list-selection" widget */
            set = malloc(opt->size);
            status = xsane_control_option(xsane.dev, xsane.well_known.scanmode, SANE_ACTION_GET_VALUE, set, 0);

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, xsane.well_known.scanmode,
                                  _BGT(opt->desc), xsane_scanmode_menu_callback, SANE_OPTION_IS_SETTABLE(opt->cap), 0);
          }
           break;

           default:
            DBG(DBG_error, "scanmode_selection: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
        }
        gtk_widget_show(hbox);
      }
    }


    /* input selection */
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.scansource);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        hbox = gtk_hbox_new(FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) scanner_xpm);
        pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
        gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
        gdk_drawable_unref(pixmap);
        gtk_widget_show(pixmapwidget);

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_STRING_LIST:
          {
           char *set;
           SANE_Status status;

            /* use a "list-selection" widget */
            set = malloc(opt->size);
            status = xsane_control_option(xsane.dev, xsane.well_known.scansource, SANE_ACTION_GET_VALUE, set, 0);

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, xsane.well_known.scansource,
                                  _BGT(opt->desc), 0, SANE_OPTION_IS_SETTABLE(opt->cap), 0);
           }
           break;

          default:
            DBG(DBG_error, "scansource_selection: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
        }
        gtk_widget_show(hbox);
      }
    }


    if (xsane.param.depth != 1) /* show medium selection of not lineart mode */
    {
      /* medium selection */
      hbox = gtk_hbox_new(FALSE, 2);
      gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
      gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

      if (xsane.medium_calibration)
      {
        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) medium_edit_xpm);
      }
      else
      {
        pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) medium_xpm);
      }
      pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
      gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
      gdk_drawable_unref(pixmap);
      gtk_widget_show(pixmapwidget);

      xsane_medium_menu = gtk_menu_new();

      for (i=0; i<preferences.medium_definitions; i++)
      {
        xsane_medium_item = gtk_menu_item_new_with_label(preferences.medium[i]->name);
        gtk_menu_append(GTK_MENU(xsane_medium_menu), xsane_medium_item);
        g_signal_connect(GTK_OBJECT(xsane_medium_item), "button_press_event", (GtkSignalFunc) xsane_medium_context_menu_callback, (void *) i);
        g_signal_connect(GTK_OBJECT(xsane_medium_item), "activate", (GtkSignalFunc) xsane_set_medium_callback, (void *) i);
        gtk_object_set_data(GTK_OBJECT(xsane_medium_item), "Selection", (void *) i);

        gtk_widget_show(xsane_medium_item);
      }

      gtk_widget_show(xsane_medium_menu);

      xsane_medium_option_menu = gtk_option_menu_new();
      xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_medium_option_menu, DESC_XSANE_MEDIUM);
      gtk_box_pack_end(GTK_BOX(hbox), xsane_medium_option_menu, FALSE, FALSE, 2);
      gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_medium_option_menu), xsane_medium_menu);
      gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_medium_option_menu), preferences.medium_nr);
      gtk_widget_show(xsane_medium_option_menu);
      gtk_widget_show(hbox);

      xsane.medium_widget = xsane_medium_option_menu;

      if (xsane.medium_calibration) /* are we running in medium calibration mode? */
      {
        xsane_apply_medium_definition_as_enhancement(preferences.medium[preferences.medium_nr]);
        xsane_set_medium(NULL);
      }
      else
      {
        xsane_set_medium(preferences.medium[preferences.medium_nr]);
      }
    }
    else /* no medium selextion for lineart mode: use Full range gamma curve */
    {
      xsane_set_medium(preferences.medium[0]); /* make sure Full range is active */
    }
  }


  if (xsane.xsane_mode == XSANE_SAVE)
  {
    xsane.copy_number_entry = NULL;

    if ( (xsane.mode == XSANE_STANDALONE) && (!xsane.force_filename) )
    {
      xsane_outputfilename_new(xsane_vbox_xsane_modus); /* create filename box, step and type menu */
    }
  }

  if ( (xsane.xsane_mode == XSANE_SAVE) || (xsane.xsane_mode == XSANE_VIEWER) )
  {
    /* resolution selection */
    if (!xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_x, &xsane.resolution_x, resolution_x_xpm,
                                     DESC_RESOLUTION_X, XSANE_GTK_NAME_X_RESOLUTION)) /* draw x resolution widget if possible */
    {
      xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_y, &xsane.resolution_y, resolution_y_xpm,
                                  DESC_RESOLUTION_Y, XSANE_GTK_NAME_Y_RESOLUTION); /* ok, also draw y resolution widget */
    }
    else /* no x resolution, so lets draw common resolution widget */
    {
      xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi, &xsane.resolution, resolution_xpm,
                                  DESC_RESOLUTION, XSANE_GTK_NAME_RESOLUTION); 
    }
  }
  else if (xsane.xsane_mode == XSANE_COPY)
  {
   GtkWidget *pixmapwidget, *hbox, *xsane_printer_option_menu, *xsane_printer_menu, *xsane_printer_item;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   GtkWidget *paper_orientation_option_menu, *paper_orientation_menu, *paper_orientation_item;
   int i;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

    pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, (gchar **) printer_xpm);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
    gdk_drawable_unref(pixmap);
    gtk_widget_show(pixmapwidget);


    /* printer position */
    paper_orientation_menu = gtk_menu_new();

    for (i = 0; i <= 12; ++i)
    {
     gchar **xpm_d;

      if (i == 5) /* 5, 6, 7 are not used */
      {
        i = 8;
      }

      switch (i)
      {
        /* portrait */
        default:
        case 0:
           xpm_d = (gchar **) paper_orientation_portrait_top_left_xpm;
         break;

        case 1:
           xpm_d = (gchar **) paper_orientation_portrait_top_right_xpm;
         break;

        case 2:
           xpm_d = (gchar **) paper_orientation_portrait_bottom_right_xpm;
         break;

        case 3:
           xpm_d = (gchar **) paper_orientation_portrait_bottom_left_xpm;
         break;

        case 4:
           xpm_d = (gchar **) paper_orientation_portrait_center_xpm;
         break;

        /* landscape */
        case 8:
           xpm_d = (gchar **) paper_orientation_landscape_top_left_xpm;
         break;

        case 9:
           xpm_d = (gchar **) paper_orientation_landscape_top_right_xpm;
         break;

        case 10:
           xpm_d = (gchar **) paper_orientation_landscape_bottom_right_xpm;
         break;

        case 11:
           xpm_d = (gchar **) paper_orientation_landscape_bottom_left_xpm;
         break;

        case 12:
           xpm_d = (gchar **) paper_orientation_landscape_center_xpm;
         break;
      }

      paper_orientation_item = gtk_menu_item_new();
      pixmap = gdk_pixmap_create_from_xpm_d(xsane.histogram_dialog->window, &mask, xsane.bg_trans, xpm_d);
      pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
      gtk_container_add(GTK_CONTAINER(paper_orientation_item), pixmapwidget);
      gtk_widget_show(pixmapwidget);
      gdk_drawable_unref(pixmap);


      gtk_container_add(GTK_CONTAINER(paper_orientation_menu), paper_orientation_item);
      g_signal_connect(GTK_OBJECT(paper_orientation_item), "activate", (GtkSignalFunc) xsane_paper_orientation_callback, (void *) i);

      gtk_widget_show(paper_orientation_item);
    }

    paper_orientation_option_menu = gtk_option_menu_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, paper_orientation_option_menu, DESC_PAPER_ORIENTATION);
    gtk_box_pack_end(GTK_BOX(hbox), paper_orientation_option_menu, FALSE, FALSE, 0);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(paper_orientation_option_menu), paper_orientation_menu);

    /* set default selection */
    if (preferences.paper_orientation < 8) /* portrai number is correct */
    {
      gtk_option_menu_set_history(GTK_OPTION_MENU(paper_orientation_option_menu), preferences.paper_orientation);
    }
    else /* numbers 5, 6, 7 are unused, so we have to substract 3 for landscape mode */
    {
      gtk_option_menu_set_history(GTK_OPTION_MENU(paper_orientation_option_menu), preferences.paper_orientation-3);
    }

    gtk_widget_show(paper_orientation_option_menu);


    xsane_printer_menu = gtk_menu_new();

    for (i=0; i < preferences.printerdefinitions; i++)
    {
      xsane_printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
      if (i<12)
      {
        gtk_widget_add_accelerator(xsane_printer_item, "activate", xsane.accelerator_group,
                                   GDK_F1+i, GDK_SHIFT_MASK,  DEF_GTK_MENU_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
      }
      gtk_container_add(GTK_CONTAINER(xsane_printer_menu), xsane_printer_item);
      g_signal_connect(GTK_OBJECT(xsane_printer_item), "activate", (GtkSignalFunc) xsane_printer_callback, (void *) i);
      gtk_widget_show(xsane_printer_item);
    }

    xsane_printer_option_menu = gtk_option_menu_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_printer_option_menu, DESC_PRINTER_SELECT);
    gtk_box_pack_end(GTK_BOX(hbox), xsane_printer_option_menu, FALSE, FALSE, 2);
    gtk_widget_show(xsane_printer_option_menu);
    gtk_widget_show(hbox);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_printer_option_menu), xsane_printer_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_printer_option_menu), preferences.printernr);

    /* number of copies */
    xsane_text = gtk_entry_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_text, DESC_COPY_NUMBER);
    gtk_widget_set_size_request(xsane_text, 25, -1);
    snprintf(buf, sizeof(buf), "%d", xsane.copy_number);    
    gtk_entry_set_text(GTK_ENTRY(xsane_text), (char *) buf);
    gtk_box_pack_end(GTK_BOX(hbox), xsane_text, FALSE, FALSE, 10);
    gtk_widget_show(xsane_text);
    gtk_widget_show(hbox);
    xsane.copy_number_entry = xsane_text;

    /* zoom selection */
    if (!xsane_zoom_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_x, &xsane.zoom_x,
                               xsane.resolution_x, zoom_x_xpm, DESC_ZOOM_X))
    {
      xsane_zoom_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_y, &xsane.zoom_y,
                            xsane.resolution_y, zoom_y_xpm, DESC_ZOOM_Y);
    }
    else
    {
      xsane_zoom_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi, &xsane.zoom,
                            xsane.resolution, zoom_xpm, DESC_ZOOM);
    }
  }
  else if (xsane.xsane_mode == XSANE_FAX)
  {
    xsane.copy_number_entry = NULL;

    xsane.resolution   = 204;
    xsane.resolution_x = 204;
    xsane.resolution_y = 196;
    xsane_set_all_resolutions();

    xsane_fax_dialog();
  }
#ifdef XSANE_ACTIVATE_MAIL
  else if (xsane.xsane_mode == XSANE_MAIL)
  {
    xsane.copy_number_entry = NULL;

    /* resolution selection */
    if (!xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_x, &xsane.resolution_x, resolution_x_xpm,
                                     DESC_RESOLUTION_X, XSANE_GTK_NAME_X_RESOLUTION)) /* draw x resolution widget if possible */
    {
      xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi_y, &xsane.resolution_y, resolution_y_xpm,
                                  DESC_RESOLUTION_Y, XSANE_GTK_NAME_Y_RESOLUTION); /* ok, also draw y resolution widget */
    }
    else /* no x resolution, so lets draw common resolution widget */
    {
      xsane_resolution_widget_new(xsane_vbox_xsane_modus, xsane.well_known.dpi, &xsane.resolution, resolution_xpm,
                                  DESC_RESOLUTION, XSANE_GTK_NAME_RESOLUTION); 
    }

    xsane_mail_dialog();
  }
#endif

   /* test if scanner gamma table is selected */

   xsane.scanner_gamma_gray  = FALSE;
   if (xsane.well_known.gamma_vector >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_gray  = TRUE;
     }
   }

   xsane.scanner_gamma_color  = FALSE;
   if (xsane.well_known.gamma_vector_r >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_r);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_color  = TRUE;
     }
   }



  /* XSane Frame Enhancement */

  sane_get_parameters(xsane.dev, &xsane.param); /* update xsane.param */

  if (xsane.param.depth == 1)
  {
    switch (xsane.lineart_mode)
    {
      case XSANE_LINEART_STANDARD:
        break;

      case XSANE_LINEART_GRAYSCALE:
      case XSANE_LINEART_XSANE:
        if (xsane.well_known.threshold > 0)
        {
          xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), threshold_xpm, DESC_THRESHOLD,
                                      xsane.threshold_min, xsane.threshold_max, 1.0, 10.0, 0,
                                      &xsane.threshold, &xsane.threshold_widget, 0, xsane_gamma_changed, TRUE);
          xsane_threshold_changed();
        }
        break;

      default:
        break;
    }

    return(xsane_hbox);
  }

  xsane.slider_gray.active   = XSANE_SLIDER_ACTIVE; /* mark slider active */

  if ( (xsane.xsane_colors > 1) && (!xsane.enhancement_rgb_default) )
  {
    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), Gamma_xpm, DESC_GAMMA,
                              XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.1, 2,
                              &xsane.gamma, &xsane.gamma_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_colors > 1) && (!xsane.enhancement_rgb_default) )
  {
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), Gamma_red_xpm, DESC_GAMMA_R,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.1, 2,
                                &xsane.gamma_red  , &xsane.gamma_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), Gamma_green_xpm, DESC_GAMMA_G,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.1, 2,
                                &xsane.gamma_green, &xsane.gamma_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), Gamma_blue_xpm, DESC_GAMMA_B,
                                XSANE_GAMMA_MIN, XSANE_GAMMA_MAX, 0.01, 0.1, 2,
                                &xsane.gamma_blue , &xsane.gamma_blue_widget, 0, xsane_gamma_changed, TRUE);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), brightness_xpm, DESC_BRIGHTNESS,
                              xsane.brightness_min, xsane.brightness_max, 0.1, 1.0, 1,
                              &xsane.brightness, &xsane.brightness_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_colors > 1) && (!xsane.enhancement_rgb_default) )
  {
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), brightness_red_xpm, DESC_BRIGHTNESS_R,
                                xsane.brightness_min, xsane.brightness_max, 0.1, 1.0, 1,
                                &xsane.brightness_red  , &xsane.brightness_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), brightness_green_xpm, DESC_BRIGHTNESS_G,
                                xsane.brightness_min, xsane.brightness_max, 0.1, 1.0, 1,
                                &xsane.brightness_green, &xsane.brightness_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), brightness_blue_xpm, DESC_BRIGHTNESS_B,
                                xsane.brightness_min, xsane.brightness_max, 0.1, 1.0, 1,
                                &xsane.brightness_blue,  &xsane.brightness_blue_widget, 0, xsane_gamma_changed, TRUE);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), contrast_xpm, DESC_CONTRAST,
                              xsane.contrast_gray_min, xsane.contrast_max, 0.1, 1.0, 1,
                              &xsane.contrast, &xsane.contrast_widget, 0, xsane_gamma_changed, TRUE);
  if ( (xsane.xsane_colors > 1) && (!xsane.enhancement_rgb_default) )
  {
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), contrast_red_xpm, DESC_CONTRAST_R,
                                xsane.contrast_min, xsane.contrast_max, 0.1, 1.0, 1,
                                &xsane.contrast_red  , &xsane.contrast_red_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), contrast_green_xpm, DESC_CONTRAST_G,
                                xsane.contrast_min, xsane.contrast_max, 0.1, 1.0, 1,
                                &xsane.contrast_green, &xsane.contrast_green_widget, 0, xsane_gamma_changed, TRUE);
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(xsane_vbox_xsane_modus), contrast_blue_xpm, DESC_CONTRAST_B,
                                xsane.contrast_min, xsane.contrast_max, 0.1, 1.0, 1,
                                &xsane.contrast_blue,  &xsane.contrast_blue_widget, 0, xsane_gamma_changed, TRUE);
  }

    xsane_separator_new(xsane_vbox_xsane_modus, 2);

  /* create lower button box (rgb default, negative ,... */
  xsane_hbox_xsane_enhancement = gtk_hbox_new(TRUE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_hbox_xsane_enhancement), 4);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_enhancement, FALSE, FALSE, 0);
  gtk_widget_show(xsane_hbox_xsane_enhancement);

  if (xsane.xsane_colors > 1)
  {
    button = xsane_toggle_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, rgb_default_xpm, DESC_RGB_DEFAULT,
                                                 &xsane.enhancement_rgb_default, xsane_enhancement_rgb_default_callback);
    gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_B, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);
  }

  button = xsane_toggle_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, negative_xpm, DESC_NEGATIVE,
                                               &xsane.negative, xsane_enhancement_negative_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_N, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, enhance_xpm, DESC_ENH_AUTO,
                                        xsane_auto_enhancement_callback, NULL);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_E, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, default_enhancement_xpm, DESC_ENH_DEFAULT,
                                        xsane_enhancement_restore_default, NULL);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_0, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);


  if (!xsane.medium_calibration) /* do not display "M R" when we are we running in medium calibration mode */
  {
    button = xsane_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, restore_enhancement_xpm, DESC_ENH_RESTORE,
                                         xsane_enhancement_restore, NULL);
    gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_R, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);

    button = xsane_button_new_with_pixmap(xsane.xsane_window->window, xsane_hbox_xsane_enhancement, store_enhancement_xpm, DESC_ENH_STORE,
                                          xsane_enhancement_store, NULL);
    gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_plus, GDK_CONTROL_MASK, DEF_GTK_ACCEL_LOCKED);
  }

  xsane_update_histogram(TRUE /* update raw */);
#ifdef HAVE_WORKING_GTK_GAMMACURVE
  xsane_update_gamma_dialog();
#endif

 return(xsane_hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_pref_save(void)
{
 char filename[PATH_MAX];
 int fd;

  DBG(DBG_proc, "xsane_pref_save\n");

  /* first save xsane-specific preferences: */
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, "xsane", NULL, ".rc", XSANE_PATH_LOCAL_SANE);

  DBG(DBG_info2, "saving preferences to \"%s\"\n", filename);

  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */    
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600); /* rw- --- --- */

  if (fd < 0)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
    return;
  }
  preferences_save(fd);
  close(fd);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_pref_restore(void)
/* returns true if this is the first time this xsane version is called */
{
 char filename[PATH_MAX];
 int fd;
 int result = TRUE;
 int i;

  DBG(DBG_proc, "xsane_pref_restore\n");

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, "xsane", NULL, ".rc", XSANE_PATH_LOCAL_SANE);
  fd = open(filename, O_RDONLY);

  if (fd >= 0)
  {
    preferences_restore(fd);
    close(fd);

    /* the version test only is done for the local xsane.rc file because each user */
    /* shall accept (or not) the license for xsane */
    if (preferences.xsane_version_str)
    {
      if (!strcmp(preferences.xsane_version_str, XSANE_VERSION))
      {
        result = FALSE; /* this version already has been started */
      }
    }
  }
  else /* no local sane file, look for system file */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane", 0, ".rc", XSANE_PATH_SYSTEM);
    fd = open(filename, O_RDONLY);

    if (fd >= 0)
    {
      preferences_restore(fd);
      close(fd);
    }
  }

  if (!preferences.preset_area_definitions)
  {
    DBG(DBG_info, "no preset area definitions in preferences file, using predefined list\n");
 
    preferences.preset_area_definitions = sizeof(pref_default_preset_area)/sizeof(pref_default_preset_area_t);
    preferences.preset_area = calloc(preferences.preset_area_definitions, sizeof(void *));
 
    for (i=0; i<preferences.preset_area_definitions; i++)
    {
      preferences.preset_area[i] = calloc(sizeof(Preferences_preset_area_t), 1);
      preferences.preset_area[i]->name    = strdup(_(pref_default_preset_area[i].name));
      preferences.preset_area[i]->xoffset = pref_default_preset_area[i].xoffset;
      preferences.preset_area[i]->yoffset = pref_default_preset_area[i].yoffset;
      preferences.preset_area[i]->width   = pref_default_preset_area[i].width;
      preferences.preset_area[i]->height  = pref_default_preset_area[i].height;
    }
  }

  if (preferences.xsane_version_str)
  {
    free(preferences.xsane_version_str);
  }
  preferences.xsane_version_str = strdup(XSANE_VERSION); /* store recent xsane-version */

  if (!preferences.tmp_path)
  {
    if (getenv(STRINGIFY(ENVIRONMENT_TEMP_DIR_NAME))) /* if possible get temp path from environment */
    {
      preferences.tmp_path = strdup(getenv(STRINGIFY(ENVIRONMENT_TEMP_DIR_NAME)));
      DBG(DBG_info, "setting temporary directory by environment variable %s: %s\n",
          STRINGIFY(ENVIRONMENT_TEMP_DIR_NAME), preferences.tmp_path);
    }
    else /* otherwise use predefined path */
    {
      preferences.tmp_path = strdup(STRINGIFY(TEMP_PATH));
      DBG(DBG_info, "setting temporary directory to %s\n", preferences.tmp_path);
    }
  }

  if (!preferences.working_directory)
  {
   char filename[PATH_MAX];

    if (getcwd(filename, sizeof(filename)))
    {
      preferences.working_directory = strdup(filename); /* set current working directory */
    }
  }
  else
  {
    chdir(preferences.working_directory); /* set working directory */
  }

  if (!preferences.filename)
  {
    preferences.filename = strdup(OUT_FILENAME);
  }

  if (!preferences.filetype)
  {
    preferences.filetype = strdup(XSANE_FILETYPE_BY_EXT);
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

#ifdef XSANE_ACTIVATE_MAIL
  if (!preferences.mail_smtp_server)
  {
    preferences.mail_smtp_server = strdup("");
  }

  if (!preferences.mail_from)
  {
    preferences.mail_from = strdup("");
  }

  if (!preferences.mail_reply_to)
  {
    preferences.mail_reply_to = strdup("");
  }

  if (!preferences.mail_pop3_server)
  {
    preferences.mail_pop3_server = strdup("");
  }

  if (!preferences.mail_pop3_user)
  {
    preferences.mail_pop3_user = strdup("");
  }

  if (!preferences.mail_pop3_pass)
  {
    preferences.mail_pop3_pass = strdup("");
  }

  if (!preferences.mail_project)
  {
    preferences.mail_project = strdup(MAILPROJECT);
  }

  if (!preferences.mail_viewer)
  {
    preferences.mail_viewer = strdup(MAILVIEWER);
  }

  if (!preferences.mail_filetype)
  {
    preferences.mail_filetype = strdup(XSANE_DEFAULT_MAILTYPE);
  }
#endif

  if (!preferences.ocr_command)
  {
    preferences.ocr_command = strdup(OCRCOMMAND);
  }

  if (!preferences.ocr_inputfile_option)
  {
    preferences.ocr_inputfile_option = strdup(OCRINPUTFILEOPT);
  }

  if (!preferences.ocr_outputfile_option)
  {
    preferences.ocr_outputfile_option = strdup(OCROUTPUTFILEOPT);
  }

  if (!preferences.ocr_gui_outfd_option)
  {
    preferences.ocr_gui_outfd_option = strdup(OCROUTFDOPT);
  }

  if (!preferences.ocr_progress_keyword)
  {
    preferences.ocr_progress_keyword = strdup(OCRPROGRESSKEY);
  }

  if (!preferences.browser)
  {
    if (getenv(STRINGIFY(ENVIRONMENT_BROWSER_NAME)))
    {
      preferences.browser = getenv(STRINGIFY(ENVIRONMENT_BROWSER_NAME));
    }
    else
    {
      preferences.browser = strdup(DEFAULT_BROWSER);
    }
  }

  if (xsane.mode != XSANE_GIMP_EXTENSION)
  {
    if (xsane.xsane_mode < 0)
    {
      xsane.xsane_mode = preferences.xsane_mode;
    }
  }

 return result;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_pref_save_media(void)
{
 char filename[PATH_MAX];
 int fd;

  DBG(DBG_proc, "xsane_pref_save_media\n");

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, "xsane", NULL, ".mdf", XSANE_PATH_LOCAL_SANE);

  DBG(DBG_info2, "saving media to \"%s\"\n", filename);

  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */    
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600); /* rw- --- --- */

  if (fd < 0)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
    return;
  }
  preferences_save_media(fd);
  close(fd);
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_restore_media(void)
{
 char filename[PATH_MAX];
 int fd;
 int i;

  DBG(DBG_proc, "xsane_pref_restore_media\n");

  preferences.medium_definitions = 0;

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, "xsane", NULL, ".mdf", XSANE_PATH_LOCAL_SANE);
  fd = open(filename, O_RDONLY);

  if (fd >= 0)
  {
    preferences_restore_media(fd);
    close(fd);
  }

  if (!preferences.medium_definitions)
  {
    DBG(DBG_info, "no medium definitions found, using predefined list\n");
 
    preferences.medium_definitions = sizeof(pref_default_medium)/sizeof(Preferences_medium_t);
    preferences.medium = calloc(preferences.medium_definitions, sizeof(void *));
 
    for (i=0; i<preferences.medium_definitions; i++)
    {
      preferences.medium[i] = calloc(sizeof(Preferences_medium_t), 1);
      preferences.medium[i]->name            = strdup(_(pref_default_medium[i].name));
      preferences.medium[i]->shadow_gray     = pref_default_medium[i].shadow_gray;
      preferences.medium[i]->shadow_red      = pref_default_medium[i].shadow_red;
      preferences.medium[i]->shadow_green    = pref_default_medium[i].shadow_green;
      preferences.medium[i]->shadow_blue     = pref_default_medium[i].shadow_blue;
      preferences.medium[i]->highlight_gray  = pref_default_medium[i].highlight_gray;
      preferences.medium[i]->highlight_red   = pref_default_medium[i].highlight_red;
      preferences.medium[i]->highlight_green = pref_default_medium[i].highlight_green;
      preferences.medium[i]->highlight_blue  = pref_default_medium[i].highlight_blue;
      preferences.medium[i]->gamma_gray      = pref_default_medium[i].gamma_gray;
      preferences.medium[i]->gamma_red       = pref_default_medium[i].gamma_red;
      preferences.medium[i]->gamma_green     = pref_default_medium[i].gamma_green;
      preferences.medium[i]->gamma_blue      = pref_default_medium[i].gamma_blue;
      preferences.medium[i]->negative        = pref_default_medium[i].negative;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_quit_handler(int signal)
{
  DBG(DBG_proc, "xsane_quit_handler\n");

  xsane_quit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_add_process_to_list(pid_t pid)
{
 XsaneChildprocess *newprocess;
 
  DBG(DBG_proc, "xsane_add_process_to_list(%d)\n", pid);
 
  newprocess = malloc(sizeof(XsaneChildprocess));
  newprocess->pid = pid;
  newprocess->next = xsane.childprocess_list;
  xsane.childprocess_list = newprocess;
} 

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_sigchld_handler(int signal)
{
 int status;
 XsaneChildprocess **childprocess_listptr = &xsane.childprocess_list;
 XsaneChildprocess *childprocess = xsane.childprocess_list;

  DBG(DBG_proc, "xsane_sigchld_handler\n");

  /* normally we would do a wait(&status); here, but some backends fork() a reader
     process and test the return status of waitpid() that returns with an error
     when we get the signal at first and clean up the process with wait().
     A backend should not handle this as error but some do and so we have to handle this */

  while (childprocess)
  {
   XsaneChildprocess *nextprocess;
   pid_t pid;

    pid = waitpid(childprocess->pid, &status, WNOHANG);
    if ( (WIFEXITED(status)) && (pid == childprocess->pid) )
    {
      DBG(DBG_info, "deleteing pid %d from list\n", childprocess->pid);

      nextprocess = childprocess->next;
      free(childprocess); /* free memory of element */
      childprocess = nextprocess; /* go to next element */
      *childprocess_listptr = childprocess; /* remove element from list */
      /* childprocess_listptr keeps the same !!! */
    }
    else
    {
      DBG(DBG_info, "keeping pid %d in list\n", childprocess->pid);

      childprocess_listptr = &childprocess->next; /* set pointer to next element */
      childprocess = childprocess->next; /* go to next element */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_quit(void)
{
  DBG(DBG_proc, "xsane_quit\n");

  while (xsane.viewer_list) /* remove all viewer images */
  {
   Viewer *next_viewer = xsane.viewer_list->next_viewer; /* store pointer to next viewer */

    DBG(DBG_info, "removing viewer image %s\n", xsane.viewer_list->filename);
    remove(xsane.viewer_list->filename); /* remove image file */

    gtk_widget_destroy(xsane.viewer_list->top); /* destroy the viewer window */
    free(xsane.viewer_list); /* free memory of struct Viewer */

    xsane.viewer_list = next_viewer;
  }

  if (xsane.preview)
  {
    Preview *preview = xsane.preview;
    xsane.preview = 0;
    preview_destroy(preview);
  }

#ifdef XSANE_ACTIVATE_MAIL
  if (xsane.mail_project_save)
  {
    xsane.mail_project_save = 0;
    xsane_mail_project_save();
  }
#endif

  while (xsane.back_gtk_message_dialog_active)
  {
    gtk_main_iteration();
  }

  if (xsane.dev)
  {
    sane_close(xsane.dev);
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

#ifdef HAVE_ANY_GIMP
  if (xsane.mode == XSANE_GIMP_EXTENSION)
  {
    gimp_quit();
  }
#endif

  if (preferences.printer)
  {
    free(preferences.printer);
  }

  exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_exit(void) /* this is called when xsane exits before gtk_main is called */
{
  DBG(DBG_proc, "xsane_exit\n");

  while (xsane.back_gtk_message_dialog_active)
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  sane_exit();

#ifdef HAVE_ANY_GIMP
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
  DBG(DBG_proc, "xsane_standard_option_win_delete\n");

  gtk_widget_hide(widget);
  preferences.show_standard_options = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_advanced_option_win_delete(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_advanced_option_win_delete\n");

  gtk_widget_hide(widget);
  preferences.show_advanced_options = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when window manager's "delete" (or "close") function is invoked.  */
static gint xsane_scan_win_delete(GtkWidget *w, gpointer data)
{
 int unsaved_images = 0;
 Viewer *viewer = xsane.viewer_list;

  DBG(DBG_proc, "xsane_scan_win_delete\n");

  xsane_scan_done(-1); /* stop scanner when still scanning */

  while (viewer)
  {
    if (!viewer->image_saved)
    {
      unsaved_images++;
    }
    viewer = viewer->next_viewer;
  }

  if (unsaved_images)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), WARN_UNSAVED_IMAGES, unsaved_images);
    if (!xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_DO_NOT_CLOSE, BUTTON_DISCARD_ALL_IMAGES, TRUE /* wait */) == FALSE)
    {
      return TRUE;
    }
  }

  xsane_pref_save();
  xsane_pref_save_media();

  if (preferences.save_devprefs_at_exit)
  {
    xsane_device_preferences_store();
  }

  xsane_quit();
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_preview_window_destroyed(GtkWidget *widget, gpointer call_data)
{
  DBG(DBG_proc, "xsane_preview_window_destroyed\n");

  gtk_widget_hide(widget);
  xsane.show_preview = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_preview_widget), FALSE);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_preview_callback(GtkWidget * widget, gpointer call_data)
{
  DBG(DBG_proc, "xsane_show_preview_callback\n");

  if (GTK_CHECK_MENU_ITEM(widget)->active)
  {
    gtk_widget_show(xsane.preview->top);
    xsane.show_preview = TRUE;
  }
  else
  {
    gtk_widget_hide(xsane.preview->top);
    xsane.show_preview = FALSE;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_files_build_menu(void)
{
 GtkWidget *menu, *item;

  DBG(DBG_proc, "xsane_files_build_menu\n");

  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);

  /* XSane info dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_INFO);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_info_dialog, NULL);
  gtk_widget_show(item);

  
  /* Quit */

  item = gtk_menu_item_new_with_label(MENU_ITEM_QUIT);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_container_add(GTK_CONTAINER(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate",  (GtkSignalFunc) xsane_scan_win_delete, NULL);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_move_up_callback(GtkWidget *widget, GtkWidget *medium_widget)
{
 int selection;
  
  DBG(DBG_proc, "xsane_medium_move_up_callback\n");
  
  selection = (int) gtk_object_get_data(GTK_OBJECT(medium_widget), "Selection");

  if (selection > 1) /* make sure "full range" stays at top */
  {
   Preferences_medium_t *help_medium;
   
    DBG(DBG_info ,"moving up %s\n", preferences.medium[selection]->name);
  
    help_medium = preferences.medium[selection-1];
    preferences.medium[selection-1] = preferences.medium[selection];
    preferences.medium[selection] = help_medium;
    
    if (preferences.medium_nr == selection)
    {
      preferences.medium_nr--;
    }
    else if (preferences.medium_nr == selection-1)
    {
      preferences.medium_nr++;
    }
  
    xsane_back_gtk_refresh_dialog(); /* brute-force: update menu */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_move_down_callback(GtkWidget *widget, GtkWidget *medium_widget)
{
 int selection;
  
  DBG(DBG_proc, "xsane_medium_move_up_callback\n");
  
  selection = (int) gtk_object_get_data(GTK_OBJECT(medium_widget), "Selection");

  if ((selection) && (selection < preferences.medium_definitions-1))
  {
   Preferences_medium_t *help_medium;
   
    DBG(DBG_info ,"moving down %s\n", preferences.medium[selection]->name);
  
    help_medium = preferences.medium[selection];
    preferences.medium[selection] = preferences.medium[selection+1];
    preferences.medium[selection+1] = help_medium;
    
    if (preferences.medium_nr == selection)
    {
      preferences.medium_nr++;
    }
    else if (preferences.medium_nr == selection+1)
    {
      preferences.medium_nr--;
    }
  
    xsane_back_gtk_refresh_dialog(); /* brute-force: update menu */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_rename_callback(GtkWidget *widget, GtkWidget *medium_widget)
{
 int selection;
 char *oldname;
 char *newname;
 GtkWidget *old_medium_menu;
 int old_selection;

  DBG(DBG_proc, "xsane_medium_rename_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(medium_widget), "Selection");

  oldname = strdup(preferences.medium[selection]->name);

  DBG(DBG_info ,"rename %s\n", oldname);

  /* set menu in correct state, is a bit strange this way but I do not have a better idea */
  old_medium_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(xsane.medium_widget));
  old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_medium_menu))), "Selection");
  gtk_menu_popdown(GTK_MENU(old_medium_menu));
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane.medium_widget), old_selection);

  if (!xsane_front_gtk_getname_dialog(WINDOW_MEDIUM_RENAME, DESC_MEDIUM_RENAME, oldname, &newname))
  {
    free(preferences.medium[selection]->name);
    preferences.medium[selection]->name = strdup(newname);
    DBG(DBG_info, "renaming %s to %s\n", oldname, newname);

    xsane_back_gtk_refresh_dialog(); /* update menu */
  }

  free(oldname);
  free(newname);

  xsane_set_sensitivity(TRUE);

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_add_callback(GtkWidget *widget, GtkWidget *medium_widget)
{
 int selection;
 char *oldname;
 char *newname;
 GtkWidget *old_medium_menu;
 int old_selection;
 int i;

  DBG(DBG_proc, "xsane_medium_add_callback\n");

  /* add new item after selected item */
  selection = 1 + (int) gtk_object_get_data(GTK_OBJECT(medium_widget), "Selection");

  oldname = strdup(TEXT_NEW_MEDIA_NAME);

  /* set menu in correct state, is a bit strange this way but I do not have a better idea */
  old_medium_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(xsane.medium_widget));
  old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_medium_menu))), "Selection");
  gtk_menu_popdown(GTK_MENU(old_medium_menu));
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane.medium_widget), old_selection);

  if (!xsane_front_gtk_getname_dialog(WINDOW_MEDIUM_ADD, DESC_MEDIUM_ADD, oldname, &newname))
  {
    preferences.medium_definitions++;
    preferences.medium = realloc(preferences.medium, preferences.medium_definitions * sizeof(void *));

    for (i = preferences.medium_definitions-1; i > selection; i--)
    {
      preferences.medium[i] = preferences.medium[i-1];
    }

    DBG(DBG_info, "adding new media %s\n", newname);

    if (xsane.negative)
    {
      preferences.medium[selection] = calloc(sizeof(Preferences_medium_t), 1);
      preferences.medium[selection]->name            = strdup(newname);
      preferences.medium[selection]->shadow_gray     = 100.0 - xsane.slider_gray.value[2];
      preferences.medium[selection]->shadow_red      = 100.0 - xsane.slider_red.value[2];
      preferences.medium[selection]->shadow_green    = 100.0 - xsane.slider_green.value[2];
      preferences.medium[selection]->shadow_blue     = 100.0 - xsane.slider_blue.value[2];
      preferences.medium[selection]->highlight_gray  = 100.0 - xsane.slider_gray.value[0];
      preferences.medium[selection]->highlight_red   = 100.0 - xsane.slider_red.value[0];
      preferences.medium[selection]->highlight_green = 100.0 - xsane.slider_green.value[0];
      preferences.medium[selection]->highlight_blue  = 100.0 - xsane.slider_blue.value[0];
      preferences.medium[selection]->gamma_gray      = xsane.gamma;
      preferences.medium[selection]->gamma_red       = xsane.gamma * xsane.gamma_red;
      preferences.medium[selection]->gamma_green     = xsane.gamma * xsane.gamma_green;
      preferences.medium[selection]->gamma_blue      = xsane.gamma * xsane.gamma_blue;
      preferences.medium[selection]->negative        = xsane.negative;
    }
    else
    { 
      preferences.medium[selection] = calloc(sizeof(Preferences_medium_t), 1);
      preferences.medium[selection]->name            = strdup(newname);
      preferences.medium[selection]->shadow_gray     = xsane.slider_gray.value[0];
      preferences.medium[selection]->shadow_red      = xsane.slider_red.value[0];
      preferences.medium[selection]->shadow_green    = xsane.slider_green.value[0];
      preferences.medium[selection]->shadow_blue     = xsane.slider_blue.value[0];
      preferences.medium[selection]->highlight_gray  = xsane.slider_gray.value[2];
      preferences.medium[selection]->highlight_red   = xsane.slider_red.value[2];
      preferences.medium[selection]->highlight_green = xsane.slider_green.value[2];
      preferences.medium[selection]->highlight_blue  = xsane.slider_blue.value[2];
      preferences.medium[selection]->gamma_gray      = xsane.gamma;
      preferences.medium[selection]->gamma_red       = xsane.gamma * xsane.gamma_red;
      preferences.medium[selection]->gamma_green     = xsane.gamma * xsane.gamma_green;
      preferences.medium[selection]->gamma_blue      = xsane.gamma * xsane.gamma_blue;
      preferences.medium[selection]->negative        = xsane.negative;
    }

    preferences.medium_nr = selection; /* activate new created medium */

    xsane_back_gtk_refresh_dialog(); /* update menu */
    xsane_enhancement_by_gamma(); /* update sliders */
  }

  free(oldname);
  free(newname);

  xsane_set_sensitivity(TRUE);

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_delete_callback(GtkWidget *widget, GtkWidget *medium_widget)
{
 int selection, i;

  DBG(DBG_proc, "xsane_medium_delete_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(medium_widget), "Selection");

  if (selection) /* full range can not be deleted */
  {
    DBG(DBG_info ,"deleting %s\n", preferences.medium[selection]->name);

    free(preferences.medium[selection]);

    preferences.medium_definitions--;

    for (i = selection; i < preferences.medium_definitions; i++)
    {
      preferences.medium[i] = preferences.medium[i+1];
    }

    if (preferences.medium_nr == selection)
    {
      preferences.medium_nr--;
      xsane.medium_changed = TRUE;

      if (xsane.medium_calibration) /* are we running in medium calibration mode? */
      {
        xsane_apply_medium_definition_as_enhancement(preferences.medium[preferences.medium_nr]);
        xsane_set_medium(NULL);
      }
      else
      {
        xsane_set_medium(preferences.medium[preferences.medium_nr]);
      }

      xsane_enhancement_by_gamma(); /* update sliders */
    }

    xsane_back_gtk_refresh_dialog(); /* update menu */

    xsane_update_gamma_curve(TRUE); /* if necessary update preview gamma */

    preview_display_valid(xsane.preview); /* update valid status of preview image */
    /* the valid status depends on gamma handling an medium change */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_medium_context_menu_callback(GtkWidget *widget, GdkEvent *event)
{
 GtkWidget *menu;
 GtkWidget *menu_item;
 GdkEventButton *event_button;
 int selection;
                                                                                                
  DBG(DBG_proc, "xsane_medium_context_menu_callback\n");
                                                                                                
  selection = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");
                                                                                                
  if (event->type == GDK_BUTTON_PRESS)
  {
    event_button = (GdkEventButton *) event;
                                                                                                
    if (event_button->button == 3)
    {
     char buf[256];

      menu = gtk_menu_new();

      if (xsane.medium_calibration) /* are we running in medium calibration mode? */
      {
        /* add medium */
        menu_item = gtk_menu_item_new_with_label(MENU_ITEM_MEDIUM_ADD);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) xsane_medium_add_callback, widget);
      }
                                                                                                
      /* rename medium */
      snprintf(buf, sizeof(buf), "%s: %s", preferences.medium[selection]->name, MENU_ITEM_RENAME);
      menu_item = gtk_menu_item_new_with_label(buf);
      gtk_widget_show(menu_item);
      gtk_container_add(GTK_CONTAINER(menu), menu_item);
      g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) xsane_medium_rename_callback, widget);
                                                                                                
      if (selection) /* not available for "full area" */
      {
        /* delete medium */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.medium[selection]->name, MENU_ITEM_DELETE);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) xsane_medium_delete_callback, widget);
      }
                                                                                                
      if (selection>1) /* available from 3rd item */
      {
        /* move up */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.medium[selection]->name, MENU_ITEM_MOVE_UP);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) xsane_medium_move_up_callback, widget);
      }
                                                                                                
      if ((selection) && (selection < preferences.medium_definitions-1))
      {
        /* move down */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.medium[selection]->name, MENU_ITEM_MOVE_DWN);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) xsane_medium_move_down_callback, widget);
      }
                                                                                                
      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
                                                                                                
     return TRUE; /* event is handled */
    }
  }
                                                                                                
 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_medium_callback(GtkWidget *widget, gpointer data)
{
 int medium_nr = (int) data;

  if (medium_nr != preferences.medium_nr)
  {
    xsane.medium_changed = TRUE;
  }

  preferences.medium_nr = medium_nr;

  if (xsane.medium_calibration) /* are we running in medium calibration mode? */
  {
    xsane_apply_medium_definition_as_enhancement(preferences.medium[preferences.medium_nr]);
    xsane_set_medium(NULL);
  }
  else
  {
    xsane_set_medium(preferences.medium[preferences.medium_nr]);
  }

  xsane_enhancement_by_gamma(); /* update sliders */
  xsane_back_gtk_refresh_dialog(); /* update menu */

  xsane_update_gamma_curve(TRUE); /* if necessary update preview gamma */

  preview_display_valid(xsane.preview); /* update valid status of preview image */
  /* the valid status depends on gamma handling an medium change */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_pref_unit_callback(GtkWidget *widget, gpointer data)
{
 const char *unit = data;
 double unit_conversion_factor = 1.0;

  DBG(DBG_proc, "xsane_set_pref_unit_callback\n");

  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.length_unit_mm), (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.length_unit_cm), (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.length_unit_in), (GtkSignalFunc) xsane_set_pref_unit_callback, "in");

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

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.length_unit_mm), (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.length_unit_cm), (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.length_unit_in), (GtkSignalFunc) xsane_set_pref_unit_callback, "in");

  preferences.length_unit = unit_conversion_factor;

  xsane_refresh_dialog();

  if (xsane.preview)
  {
    preview_area_resize(xsane.preview); /* redraw rulers */
  }

  xsane_batch_scan_update_label_list(); /* update units in batch scan list */

  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_edit_medium_definition_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_edit_medium_definition_callback\n");

  if (GTK_CHECK_MENU_ITEM(widget)->active) /* enable edit mode */
  {
    DBG(DBG_info2, "enabling edit mode\n");

    xsane.medium_calibration = TRUE;
    xsane.no_preview_medium_gamma = TRUE;

    xsane.brightness_min    = XSANE_MEDIUM_CALIB_BRIGHTNESS_MIN;
    xsane.brightness_max    = XSANE_MEDIUM_CALIB_BRIGHTNESS_MAX;
    xsane.contrast_gray_min = XSANE_CONTRAST_GRAY_MIN;
    xsane.contrast_min      = XSANE_MEDIUM_CALIB_CONTRAST_MIN;
    xsane.contrast_max      = XSANE_MEDIUM_CALIB_CONTRAST_MAX;

    xsane_apply_medium_definition_as_enhancement(preferences.medium[preferences.medium_nr]);
    xsane_set_medium(NULL);
  }
  else  /* disable edit mode */
  {
    DBG(DBG_info2, "disabling edit mode\n");

    xsane.medium_calibration = FALSE;
    xsane.no_preview_medium_gamma = FALSE;

    xsane.brightness_min    = XSANE_BRIGHTNESS_MIN;
    xsane.brightness_max    = XSANE_BRIGHTNESS_MAX;
    xsane.contrast_gray_min = XSANE_CONTRAST_GRAY_MIN;
    xsane.contrast_min      = XSANE_CONTRAST_MIN;
    xsane.contrast_max      = XSANE_CONTRAST_MAX;

    xsane_apply_medium_definition_as_enhancement(preferences.medium[0]);
    xsane_set_medium(preferences.medium[preferences.medium_nr]);
  }

  xsane_enhancement_by_gamma(); /* update sliders */
  xsane_back_gtk_refresh_dialog(); /* update dialog */

  xsane_update_gamma_curve(TRUE); /* if necessary update preview gamma */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_update_policy_callback(GtkWidget *widget, gpointer data)
{
 GtkUpdateType policy = (GtkUpdateType) data;

  DBG(DBG_proc, "xsane_set_update_policy_callback\n");

  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.update_policy_continu), (GtkSignalFunc) xsane_set_update_policy_callback,
                                   (void *) GTK_UPDATE_CONTINUOUS);
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.update_policy_discont), (GtkSignalFunc) xsane_set_update_policy_callback,
                                   (void *) GTK_UPDATE_DISCONTINUOUS);
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.update_policy_delayed), (GtkSignalFunc) xsane_set_update_policy_callback,
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

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.update_policy_continu), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_CONTINUOUS);
  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.update_policy_discont), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_DISCONTINUOUS);
  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.update_policy_delayed), (GtkSignalFunc) xsane_set_update_policy_callback,
                                     (void *) GTK_UPDATE_DELAYED);

  preferences.gtk_update_policy = policy;
  xsane_pref_save();

  xsane_back_gtk_refresh_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_close_info_callback(GtkWidget *widget, gpointer data)
{
 GtkWidget *dialog_widget = data;

  DBG(DBG_proc, "xsane_close_info_callback\n");

  gtk_widget_destroy(dialog_widget);

  xsane_set_sensitivity(TRUE);

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_info_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *info_dialog, *vbox, *button, *label, *frame, *framebox, *hbox, *table;
 char buf[256];
 char *bufptr;
 GtkAccelGroup *accelerator_group;

  DBG(DBG_proc, "xsane_info_dialog\n");

  sane_get_parameters(xsane.dev, &xsane.param); /* update xsane.param */

  info_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(info_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(info_dialog), FALSE);
  g_signal_connect(GTK_OBJECT(info_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_info_callback), info_dialog);
  snprintf(buf, sizeof(buf), "%s %s %s", xsane.prog_name, WINDOW_INFO, xsane.device_text);
  gtk_window_set_title(GTK_WINDOW(info_dialog), buf);
  gtk_container_set_border_width(GTK_CONTAINER(info_dialog), 5);

  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(info_dialog), accelerator_group); 

  xsane_set_window_icon(info_dialog, 0);

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
  snprintf(buf, sizeof(buf), "%s", xsane.devlist[xsane.selected_dev]->vendor);
  label = xsane_info_table_text_new(table, buf, 1, 0);

  snprintf(buf, sizeof(buf), TEXT_MODEL);
  label = xsane_info_table_text_new(table, buf, 0, 1);
  snprintf(buf, sizeof(buf), "%s", xsane.devlist[xsane.selected_dev]->model);
  label = xsane_info_table_text_new(table, buf, 1, 1);

  snprintf(buf, sizeof(buf), TEXT_TYPE);
  label = xsane_info_table_text_new(table, buf, 0, 2);
  snprintf(buf, sizeof(buf), "%s", _(xsane.devlist[xsane.selected_dev]->type));
  label = xsane_info_table_text_new(table, buf, 1, 2);

  snprintf(buf, sizeof(buf), TEXT_DEVICE);
  label = xsane_info_table_text_new(table, buf, 0, 3);
  bufptr = strrchr(xsane.devlist[xsane.selected_dev]->name, ':');
  if (bufptr)
  {
    snprintf(buf, sizeof(buf), "%s", bufptr+1);
  }
  else
  {
    snprintf(buf, sizeof(buf), xsane.devlist[xsane.selected_dev]->name);
  }
  label = xsane_info_table_text_new(table, buf, 1, 3);

  snprintf(buf, sizeof(buf), "%s", xsane.devlist[xsane.selected_dev]->name);
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
  snprintf(buf, sizeof(buf), "%d.%d.%d",SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
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

  if ((xsane.xsane_colors > 1) && (xsane.scanner_gamma_color)) /* color gamma correction by scanner */
  {
   const SANE_Option_Descriptor *opt;

    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_SCANNER);
    label = xsane_info_table_text_new(table, buf, 1, 0);

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_r);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_INPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 1);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log((double)opt->size / sizeof(opt->type)) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 1);

    snprintf(buf, sizeof(buf), TEXT_GAMMA_OUTPUT_DEPTH);
    label = xsane_info_table_text_new(table, buf, 0, 2);
    snprintf(buf, sizeof(buf), "%d bit", (int) (0.5 + log(opt->constraint.range->max+1.0) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else if ((!xsane.xsane_colors > 1) && (xsane.scanner_gamma_gray)) /* gray gamma correction by scanner */
  {
   const SANE_Option_Descriptor *opt;

    snprintf(buf, sizeof(buf), TEXT_GAMMA_CORR_BY);
    label = xsane_info_table_text_new(table, buf, 0, 0);
    snprintf(buf, sizeof(buf), TEXT_SCANNER);
    label = xsane_info_table_text_new(table, buf, 1, 0);
    
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);

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

  sprintf(bufptr, "PDF, ");
  bufptr += strlen(bufptr);

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

  sprintf(bufptr, "TXT, ");
  bufptr += strlen(bufptr);

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

  label = xsane_info_table_text_new(table, buf, 1, 1);

/*  gtk_label_set((GtkLabel *)label, "HALLO"); */

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
#else
  button = gtk_button_new_with_label(BUTTON_CLOSE);
#endif
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);  
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_info_callback, info_dialog);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  gtk_widget_show(info_dialog);

  xsane_set_sensitivity(FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *about_dialog = NULL;

static int xsane_close_about_dialog_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_close_about_dialog_callback\n");

  gtk_widget_destroy(about_dialog);
  about_dialog = NULL;

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_about_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *vbox, *hbox, *button, *label;
 char buf[1024];
 char filename[PATH_MAX];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;
 GtkAccelGroup *accelerator_group;

  DBG(DBG_proc, "xsane_about_dialog\n");

  if (about_dialog)
  {
    return;
  }

  about_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(about_dialog), FALSE);
  g_signal_connect(GTK_OBJECT(about_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_about_dialog_callback), NULL);
  snprintf(buf, sizeof(buf), "%s %s", WINDOW_ABOUT_XSANE, xsane.prog_name);
  gtk_window_set_title(GTK_WINDOW(about_dialog), buf);
  gtk_container_set_border_width(GTK_CONTAINER(about_dialog), 5);

  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(about_dialog), accelerator_group); 

  xsane_set_window_icon(about_dialog, 0);

  hbox = gtk_hbox_new(/* not homogeneous */ TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_container_add(GTK_CONTAINER(about_dialog), hbox);
  gtk_widget_show(hbox);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 20);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(about_dialog);

  style = gtk_widget_get_style(about_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-logo", 0, ".xpm", XSANE_PATH_SYSTEM);
  pixmap = gdk_pixmap_create_from_xpm(about_dialog->window, &mask, bg_trans, filename);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);


  xsane_separator_new(vbox, 5);


  snprintf(buf, sizeof(buf), "XSane %s %s\n"
                             "%s %s\n"
                             "\n"
                             "%s %s\n"
                             "%s %s\n",
                             TEXT_VERSION, XSANE_VERSION,
                             XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT,
                             TEXT_HOMEPAGE, XSANE_HOMEPAGE,
                             TEXT_EMAIL, XSANE_EMAIL);

  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);


#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
#else
  button = gtk_button_new_with_label(BUTTON_CLOSE);
#endif
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);  
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_about_dialog_callback, NULL);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  gtk_widget_show(about_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *about_translation_dialog = NULL;

static int xsane_close_about_translation_dialog_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_close_about_translation_dialog_callback\n");

  gtk_widget_destroy(about_translation_dialog);
  about_translation_dialog = NULL;

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_about_translation_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *vbox, *hbox, *button, *label;
 char buf[1024];
 char filename[PATH_MAX];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;
 GtkAccelGroup *accelerator_group;

  DBG(DBG_proc, "xsane_about_translation_dialog\n");

  if (about_translation_dialog)
  {
    return;
  }

  about_translation_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(about_translation_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(about_translation_dialog), FALSE);
  g_signal_connect(GTK_OBJECT(about_translation_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_about_translation_dialog_callback), NULL);
  snprintf(buf, sizeof(buf), "%s %s", WINDOW_ABOUT_TRANSLATION, xsane.prog_name);
  gtk_window_set_title(GTK_WINDOW(about_translation_dialog), buf);
  gtk_container_set_border_width(GTK_CONTAINER(about_translation_dialog), 5);

  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(about_translation_dialog), accelerator_group); 

  xsane_set_window_icon(about_translation_dialog, 0);

  hbox = gtk_hbox_new(/* not homogeneous */ TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_container_add(GTK_CONTAINER(about_translation_dialog), hbox);
  gtk_widget_show(hbox);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 20);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(about_translation_dialog);

  style = gtk_widget_get_style(about_translation_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-logo", 0, ".xpm", XSANE_PATH_SYSTEM);
  pixmap = gdk_pixmap_create_from_xpm(about_translation_dialog->window, &mask, bg_trans, filename);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);


  xsane_separator_new(vbox, 5);

  snprintf(buf, sizeof(buf), "%s\n"
                             "%s",
                             TEXT_TRANSLATION,
                             TEXT_TRANSLATION_INFO);

  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);


#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
#else
  button = gtk_button_new_with_label(BUTTON_CLOSE);
#endif
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);  
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_about_translation_dialog_callback, NULL);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  gtk_widget_show(about_translation_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_fax_dialog_delete()
{
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_dialog()
{
 GtkWidget *fax_dialog, *fax_scan_vbox, *fax_project_vbox, *hbox, *fax_project_exists_hbox, *button;
 GtkWidget *scrolled_window, *list;
 char buf[64];
 GtkWidget *pixmapwidget, *text;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_fax_dialog\n");

  if (xsane.fax_dialog) 
  {
    return; /* window already is open */
  }

  /* GTK_WINDOW_TOPLEVEL looks better but does not place it nice*/
  fax_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_FAX_PROJECT);
  gtk_window_set_title(GTK_WINDOW(fax_dialog), buf);
  g_signal_connect(GTK_OBJECT(fax_dialog), "delete_event", (GtkSignalFunc) xsane_fax_dialog_delete, NULL);
  xsane_set_window_icon(fax_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(fax_dialog), xsane.accelerator_group); 

  /* set the main vbox */
  fax_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(fax_scan_vbox), 0);
  gtk_container_add(GTK_CONTAINER(fax_dialog), fax_scan_vbox);
  gtk_widget_show(fax_scan_vbox);

  /* fax project */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) fax_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXPROJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_fax_project_changed_callback, NULL);

  xsane.fax_project_entry = text;
  xsane.fax_project_entry_box = hbox;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);

  fax_project_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), fax_project_vbox, TRUE, TRUE, 0);
  gtk_widget_show(fax_project_vbox);

  /* fax receiver */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), hbox, FALSE, FALSE, 2);

  gtk_widget_realize(fax_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(fax_dialog->window, &mask, xsane.bg_trans, (gchar **) faxreceiver_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXRECEIVER);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_fax_receiver_changed_callback, NULL);

  xsane.fax_receiver_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);

  /* fine mode */
  button = gtk_check_button_new_with_label(RADIO_BUTTON_FINE_MODE);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_FAX_FINE_MODE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.fax_fine_mode);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), button, FALSE, FALSE, 2);
  gtk_widget_show(button);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_fine_mode_callback, NULL);


  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(fax_project_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);

  gtk_widget_show(list);

  xsane.fax_list = list;


  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_FILE_INSERT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_insert_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_SHOW);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_RENAME);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_DELETE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(fax_dialog->window, hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_fax_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(fax_dialog->window, hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_fax_entry_move_down_callback, list);

  gtk_widget_show(hbox);

  xsane.fax_project_box = fax_project_vbox;

  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(fax_project_vbox, 2);
  gtk_box_pack_end(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);     


  fax_project_exists_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), fax_project_exists_hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(BUTTON_SEND_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_send, NULL);
  gtk_box_pack_start(GTK_BOX(fax_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_project_delete, NULL);
  gtk_box_pack_start(GTK_BOX(fax_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(fax_project_exists_hbox);
  xsane.fax_project_exists = fax_project_exists_hbox;

  button = gtk_button_new_with_label(BUTTON_CREATE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_project_create, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  xsane.fax_project_not_exists = button;

  /* progress bar */
  xsane.fax_progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), (GtkWidget *) xsane.fax_progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.fax_progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.fax_progress_bar));


  xsane.fax_dialog = fax_dialog;

  xsane_fax_project_load();

  gtk_widget_show(fax_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_dialog_close()
{
  DBG(DBG_proc, "xsane_fax_dialog_close\n");

  if (xsane.fax_dialog == NULL)
  {
    return;
  }

  gtk_widget_destroy(xsane.fax_dialog);

  xsane.fax_dialog = NULL;
  xsane.fax_list = NULL;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_load()
{
 FILE *projectfile;
 char page[256];
 char filename[PATH_MAX];
 GtkWidget *list_item;
 int i;
 int c;

  DBG(DBG_proc, "xsane_fax_project_load\n");

  if (xsane.fax_status)
  {
    free(xsane.fax_status);
    xsane.fax_status = NULL;
  }

  if (xsane.fax_receiver)
  {
    free(xsane.fax_receiver);
    xsane.fax_receiver = NULL;
  }

  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.fax_receiver_entry), GTK_SIGNAL_FUNC(xsane_fax_receiver_changed_callback), 0);
  gtk_list_remove_items(GTK_LIST(xsane.fax_list), GTK_LIST(xsane.fax_list)->children);

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    xsane.fax_status=strdup(TEXT_FAX_STATUS_NOT_CREATED);

    snprintf(filename, sizeof(filename), "%s/page-1.pnm", preferences.fax_project);
    xsane.fax_filename=strdup(filename);
    xsane_update_counter_in_filename(&xsane.fax_filename, FALSE, 0, preferences.filename_counter_len); /* correct counter len */

    xsane.fax_receiver=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.fax_receiver_entry), (char *) xsane.fax_receiver);

    gtk_widget_set_sensitive(xsane.fax_project_box, FALSE);
    gtk_widget_hide(xsane.fax_project_exists);
    gtk_widget_show(xsane.fax_project_not_exists);
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
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
    if (strchr(page, '@'))
    {
      *strchr(page, '@') = 0;
    }
    xsane.fax_status = strdup(page);

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

    snprintf(filename, sizeof(filename), "%s/%s", preferences.fax_project, page);
    xsane.fax_filename=strdup(filename);

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
       char *type;
       char *extension;

        extension = strrchr(page, '.');
        if (extension)
        {
          type = strdup(extension);
          *extension = 0;
        }
        else
        {
          type = strdup("");
        }

        list_item = gtk_list_item_new_with_label(page);
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
        gtk_container_add(GTK_CONTAINER(xsane.fax_list), list_item);
        gtk_widget_show(list_item);
      }
    }
    gtk_widget_set_sensitive(xsane.fax_project_box, TRUE);
    gtk_widget_show(xsane.fax_project_exists);
    gtk_widget_hide(xsane.fax_project_not_exists);
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  g_signal_connect(GTK_OBJECT(xsane.fax_receiver_entry), "changed", (GtkSignalFunc) xsane_fax_receiver_changed_callback, NULL);

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), _(xsane.fax_status));
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_delete()
{
 char *page;
 char file[256];
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_fax_project_delete\n");

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s.pnm", preferences.fax_project, page);
    free(page);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/xsane-fax-list", preferences.fax_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.fax_project);
  rmdir(file);

  xsane_fax_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_update_project_status()
{
 FILE *projectfile;
 char filename[PATH_MAX];
 char buf[256];

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);
  projectfile = fopen(filename, "r+b"); /* r+ = read and write, position = start of file */

  snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.fax_status); /* fill 32 characters status line */
  fprintf(projectfile, "%s\n", buf); /* first line is status of mail */

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_fax_project_save()
{
 FILE *projectfile;
 char *page;
 char *type;
 char filename[256];
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_fax_project_save\n");

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  mkdir(preferences.fax_project, 0777); /* make sure directory exists */

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);

  if (xsane_create_secure_file(filename)) /* remove possibly existing symbolic links for security
*/
  {
   char buf[256];

    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, filename);
    xsane_back_gtk_error(buf, TRUE);
   return; /* error */
  }
  projectfile = fopen(filename, "wb"); /* write binary (b for win32) */

  if (!projectfile)
  {
    xsane_back_gtk_error(ERR_CREATE_FAX_PROJECT, TRUE);

   return;
  }

  if (xsane.fax_status)
  {
   char buf[256];

    snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.fax_status); /* fill 32 characters status line */
    fprintf(projectfile, "%s\n", buf); /* first line is status of mail */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), _(xsane.fax_status));
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);
  }
  else
  {
    fprintf(projectfile, "                                \n"); /* no mail status */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), "");
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);
  }

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
    type = (char *) gtk_object_get_data(list_item, "list_item_type");
    fprintf(projectfile, "%s%s\n", page, type);
    list = list->next;
  }
  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_create()
{
  DBG(DBG_proc, "xsane_fax_project_create\n");

  if (strlen(preferences.fax_project))
  {
    if (xsane.fax_status)
    {
      free(xsane.fax_status);
    }
    xsane.fax_status = strdup(TEXT_FAX_STATUS_CREATED);
    xsane_fax_project_save();
    xsane_fax_project_load();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_receiver_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_fax_receiver_changed_callback\n");

  if (xsane.fax_status)
  {
    free(xsane.fax_status);
  }
  xsane.fax_status = strdup(TEXT_FAX_STATUS_CHANGED);

  if (xsane.fax_receiver)
  {
    free((void *) xsane.fax_receiver);
  }
  xsane.fax_receiver = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_fax_project_save();

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), _(xsane.fax_status));
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_fax_project_changed_callback\n");

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
  DBG(DBG_proc, "xsane_fax_fine_mode_callback\n");

  preferences.fax_fine_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_list_entrys_swap(GtkWidget *list_item_1, GtkWidget *list_item_2)
{
 char *page1;
 char *page2;
 char *type1;
 char *type2;

  DBG(DBG_proc, "xsane_list_entrys_swap\n");

  page1 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_1), "list_item_data");
  type1 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_1), "list_item_type");
  page2 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_2), "list_item_data");
  type2 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_2), "list_item_type");

  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_1))->data), page2);
  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_2))->data), page1);
  gtk_object_set_data(GTK_OBJECT(list_item_1), "list_item_data", page2);
  gtk_object_set_data(GTK_OBJECT(list_item_1), "list_item_type", type2);
  gtk_object_set_data(GTK_OBJECT(list_item_2), "list_item_data", page1);
  gtk_object_set_data(GTK_OBJECT(list_item_2), "list_item_type", type1);
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

  DBG(DBG_proc, "xsane_fax_entry_move_up\n");

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
        xsane_list_entrys_swap(list_item_1, list_item_2);
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

  DBG(DBG_proc, "xsane_fax_entry_move_down\n");

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
        xsane_list_entrys_swap(list_item_1, list_item_2);
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
  DBG(DBG_proc, "xsane_fax_entry_rename\n");

  xsane_fax_entry_rename = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 GList *select;
 char *oldpage;
 char *newpage;
 char *type;
 char oldfile[256];
 char newfile[256];

  DBG(DBG_proc, "xsane_fax_entry_rename_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
   GtkWidget *rename_dialog;
   GtkWidget *text;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   char filename[PATH_MAX]; 

    list_item = select->data;
    oldpage = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_data"));
    type    = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_type"));

    xsane_set_sensitivity(FALSE);

    rename_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    xsane_set_window_icon(rename_dialog, 0);

    /* set the main vbox */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
    gtk_container_add(GTK_CONTAINER(rename_dialog), vbox);
    gtk_widget_show(vbox);

    /* set the main hbox */
    hbox = gtk_hbox_new(FALSE, 0);
    xsane_separator_new(vbox, 2);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    gtk_widget_show(hbox); 

    gtk_window_set_position(GTK_WINDOW(rename_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(rename_dialog), FALSE);
    snprintf(filename, sizeof(filename), "%s %s", xsane.prog_name, WINDOW_FAX_RENAME);
    gtk_window_set_title(GTK_WINDOW(rename_dialog), filename);
    g_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_fax_entry_rename_button_callback,(void *) -1);
    gtk_widget_show(rename_dialog);

    text = gtk_entry_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXPAGENAME);
    gtk_entry_set_max_length(GTK_ENTRY(text), 64);
    gtk_entry_set_text(GTK_ENTRY(text), oldpage);
    gtk_widget_set_size_request(text, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
    gtk_widget_show(text);


#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
    button = gtk_button_new_with_label(BUTTON_OK);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) -1);
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
      snprintf(oldfile, sizeof(oldfile), "%s/%s%s", preferences.fax_project, oldpage, type);
      snprintf(newfile, sizeof(newfile), "%s/%s%s", preferences.fax_project, newpage, type);

      rename(oldfile, newfile);

      xsane_fax_project_save();
    }

    free(oldpage);
    free(newpage);
    free(type);

    gtk_widget_destroy(rename_dialog);

    xsane_set_sensitivity(TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_insert_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 char filename[PATH_MAX];
 char windowname[255];

  DBG(DBG_proc, "xsane_fax_entry_insert_callback\n");

  xsane_set_sensitivity(FALSE);

  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_FAX_INSERT, preferences.fax_project);
  filename[0] = 0;

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, TRUE, FALSE, FALSE, FALSE)) /* filename is selected */
  {
   FILE *sourcefile;
 
    sourcefile = fopen(filename, "rb"); /* read binary (b for win32) */
    if (sourcefile) /* file exists */
    {
     char buf[1024];

      fgets(buf, sizeof(buf), sourcefile);

      if (!strncmp("%!PS", buf, 4))
      {
       FILE *destfile;
       char destpath[PATH_MAX];
       char *destfilename;
       char *destfiletype;
       char *extension;

        destfilename = strdup(strrchr(filename, '/')+1);
        extension = strrchr(destfilename, '.');
        if (extension)
        {
          destfiletype = strdup(extension);
          *extension = 0;
        }
        else
        {
          destfiletype = strdup("");
        }
        
        snprintf(destpath, sizeof(destpath), "%s/%s%s", preferences.fax_project, destfilename, destfiletype);
        /* copy file to project directory */
        if (xsane_create_secure_file(destpath)) /* remove possibly existing symbolic links for security
*/
        {
          fclose(sourcefile);
          snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, destpath);
          xsane_back_gtk_error(buf, TRUE);
         return; /* error */
        }

        destfile = fopen(destpath, "wb"); /* write binary (b for win32) */

        if (destfile) /* file is created */
        {
          fprintf(destfile, "%s\n", buf);

          while (!feof(sourcefile))
          {
            fgets(buf, sizeof(buf), sourcefile);
            fprintf(destfile, "%s", buf);
          }

          fclose(destfile);


          /* add filename to fax page list */
          list_item = gtk_list_item_new_with_label(destfilename);
          gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(destfilename));
          gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(destfiletype));
          gtk_container_add(GTK_CONTAINER(xsane.fax_list), list_item);
          gtk_widget_show(list_item);

          xsane_update_counter_in_filename(&xsane.fax_filename, TRUE, 1, preferences.filename_counter_len);
          xsane_fax_project_save();
          free(destfilename);
        }
        else /* file could not be created */
        {
          snprintf(buf, sizeof(buf), "%s %s", ERR_OPEN_FAILED, filename);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
        }
      }
      else
      {
        snprintf(buf, sizeof(buf), ERR_FILE_NOT_POSTSCRIPT, filename);
        xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      }

      fclose(sourcefile);
    } 
    else
    {
     char buf[256];
      snprintf(buf, sizeof(buf), ERR_FILE_NOT_EXISTS, filename);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    }
  }

  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */    

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_fax_entry_delete_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.fax_project, page, type);
    free(page);
    free(type);
    remove(filename);
    gtk_widget_destroy(GTK_WIDGET(list_item));
    xsane_fax_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[256];

  DBG(DBG_proc, "xsane_fax_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.fax_project, page, type);

    if (!strncmp(type, ".pnm", 4))
    {
      /* when we do not allow any modification then we can work with the original file */
      /* so we do not have to copy the image into a dummy file here! */

      xsane_viewer_new(filename, FALSE, filename, VIEWER_NO_MODIFICATION);
    }
    else if (!strncmp(type, ".ps", 3))
    {
     char *arg[100];
     int argnr;
     pid_t pid;

      argnr = xsane_parse_options(preferences.fax_viewer, arg);
      arg[argnr++] = filename;
      arg[argnr] = 0;

      pid = fork();

      if (pid == 0) /* new process */
      {
       FILE *ipc_file = NULL;

        if (xsane.ipc_pipefd[0])
        {
          close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
          ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
        }

        DBG(DBG_info, "trying to change user id fo new subprocess:\n");
        DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
        setuid(getuid());
        DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

        execvp(arg[0], arg); /* does not return if successfully */
        DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_FAX_VIEWER, preferences.fax_viewer);

        /* send error message via IPC pipe to parent process */
        if (ipc_file)
        {
          fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_FAX_VIEWER, preferences.fax_viewer, strerror(errno));
          fflush(ipc_file); /* make sure message is displayed */
          fclose(ipc_file);
        }

        _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
      }
      else /* parent process */
      {
        xsane_add_process_to_list(pid); /* add pid to child process list */
      }
    }

    free(page);
    free(type);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_fax_convert_pnm_to_ps(char *source_filename, char *fax_filename)
{
 FILE *outfile;
 FILE *infile;
 Image_info image_info;
 char buf[256];
 int cancel_save;

  /* open progressbar */
  snprintf(buf, sizeof(buf), "%s - %s", PROGRESS_CONVERTING_DATA, source_filename);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), buf);
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  infile = fopen(source_filename, "rb"); /* read binary (b for win32) */
  if (infile != 0)
  {
    xsane_read_pnm_header(infile, &image_info);

    umask((mode_t) preferences.image_umask); /* define image file permissions */   
    outfile = fopen(fax_filename, "wb"); /* b = binary mode for win32 */
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   
    if (outfile != 0)
    {
     float imagewidth, imageheight;

      imagewidth  = 72.0 * image_info.image_width /image_info.resolution_x; /* width in 1/72 inch */
      imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

      DBG(DBG_info, "imagewidth  = %f 1/72 inch\n", imagewidth);
      DBG(DBG_info, "imageheight = %f 1/72 inch\n", imageheight);

      xsane_save_ps(outfile, infile,
                    &image_info,
                    imagewidth, imageheight,
                    preferences.fax_leftoffset   * 72.0/MM_PER_INCH, /* paper_left_margin */
                    preferences.fax_bottomoffset * 72.0/MM_PER_INCH, /* paper_bottom_margin */
                    preferences.fax_width  * 72.0/MM_PER_INCH, /* paper_width */
                    preferences.fax_height * 72.0/MM_PER_INCH, /* paper_height */
                    0 /* portrait top left */,
                    xsane.fax_progress_bar,
                    &cancel_save);
      fclose(outfile);
    }
    else
    {
     char buf[256];

      DBG(DBG_info, "open of faxfile `%s'failed : %s\n", fax_filename, strerror(errno));

      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, fax_filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
    }

    fclose(infile);
  }
  else
  {
   char buf[256];

    DBG(DBG_info, "open of faxfile `%s'failed : %s\n", source_filename, strerror(errno));

    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, source_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_send()
{
 char *page;
 char *type;
 char *fax_type=".ps";
 GList *list = (GList *) GTK_LIST(xsane.fax_list)->children;
 GtkObject *list_item;
 pid_t pid;
 char *arg[1000];
 char buf[256];
 char source_filename[PATH_MAX];
 char fax_filename[PATH_MAX];
 int argnr = 0;
 int i;

  DBG(DBG_proc, "xsane_fax_send\n");

  if (list)
  {
    if (!xsane_option_defined(xsane.fax_receiver))
    {
      snprintf(buf, sizeof(buf), "%s\n", ERR_SENDFAX_RECEIVER_MISSING);
      xsane_back_gtk_error(buf, TRUE);
      return;
    }

    xsane_set_sensitivity(FALSE);
    /* gtk_widget_set_sensitive(xsane.fax_dialog, FALSE); */

    argnr = xsane_parse_options(preferences.fax_command, arg);

    if (preferences.fax_fine_mode) /* fine mode */
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
      type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
      xsane_convert_text_to_filename(&page);
      snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.fax_project, page, type);
      snprintf(fax_filename, sizeof(fax_filename), "%s/%s-fax%s", preferences.fax_project, page, fax_type);
      if (xsane_create_secure_file(fax_filename)) /* remove possibly existing symbolic links for security */
      {
       char buf[256];

        snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, fax_filename);
        xsane_back_gtk_error(buf, TRUE);
       return; /* error */
      }

      if (!strncmp(type, ".pnm", 4))
      {
        DBG(DBG_info, "converting %s to %s\n", source_filename, fax_filename);
        xsane_fax_convert_pnm_to_ps(source_filename, fax_filename);
      }
      else if (!strncmp(type, ".ps", 3))
      {
       int cancel_save = 0;
        xsane_copy_file_by_name(fax_filename, source_filename, xsane.fax_progress_bar, &cancel_save);
      }
      arg[argnr++] = strdup(fax_filename);
      list = list->next;
      free(page);
      free(type);
    }

    arg[argnr] = 0;

    pid = fork();

    if (pid == 0) /* new process */
    {
     FILE *ipc_file = NULL;

      if (xsane.ipc_pipefd[0])
      {
        close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
        ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
      }

      DBG(DBG_info, "trying to change user id for new subprocess:\n");
      DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
      setuid(getuid());
      DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

      execvp(arg[0], arg); /* does not return if successfully */
      DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_FAX_CMD, preferences.fax_command);

      /* send error message via IPC pipe to parent process */
      if (ipc_file)
      {
        fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_FAX_CMD, preferences.fax_command, strerror(errno));
        fflush(ipc_file); /* make sure message is displayed */
        fclose(ipc_file);
      }

      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
    else /* parent process */
    {
      xsane_add_process_to_list(pid); /* add pid to child process list */
    }

    for (i=0; i<argnr; i++)
    {
      free(arg[i]);
    }

    if (xsane.fax_status)
    {
      free(xsane.fax_status);
    }
    xsane.fax_status = strdup(TEXT_FAX_STATUS_QUEUEING_FAX);
    xsane_fax_project_update_project_status();
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), _(xsane.fax_status));
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);

    while (pid)
    {
     int status = 0;
     pid_t pid_status = waitpid(pid, &status, WNOHANG);
  
      if ( (pid_status < 0 ) || (pid == pid_status) )
      {
        pid = 0; /* ok, child process has terminated */
      }

      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    /* delete created fax files */
    list = (GList *) GTK_LIST(xsane.fax_list)->children;
    while (list)
    {
      list_item = GTK_OBJECT(list->data);
      page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
      xsane_convert_text_to_filename(&page);
      snprintf(fax_filename, sizeof(fax_filename), "%s/%s-fax%s", preferences.fax_project, page, fax_type);
      free(page);

      DBG(DBG_info, "removing %s\n", fax_filename);
      remove(fax_filename);

      list = list->next;
    }

    xsane.fax_status = strdup(TEXT_FAX_STATUS_FAX_QUEUED);
    xsane_fax_project_update_project_status();
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.fax_progress_bar), _(xsane.fax_status));
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.fax_progress_bar), 0.0);

    xsane_set_sensitivity(TRUE);

    /* gtk_widget_set_sensitive(xsane.fax_dialog, TRUE); */
  }

  DBG(DBG_info, "xsane_fax_send: done\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#ifdef XSANE_ACTIVATE_MAIL

static gint xsane_mail_dialog_delete()
{
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_filetype_callback(GtkWidget *filetype_option_menu, char *filetype)
{
  DBG(DBG_proc, "xsane_mail_filetype_callback(%s)\n", filetype);

  if (preferences.mail_filetype)
  {
    free(preferences.mail_filetype);
  }
  preferences.mail_filetype = strdup(filetype);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_dialog()
{
 GtkWidget *mail_dialog, *mail_scan_vbox, *mail_project_vbox;
 GtkWidget *mail_project_exists_hbox, *button;
 GtkWidget *hbox;
 GtkWidget *scrolled_window, *list;
 GtkWidget *pixmapwidget, *text;
 GtkWidget *attachment_frame, *text_frame;
 GtkWidget *label;
 GtkWidget *filetype_menu, *filetype_item;
 GtkWidget *filetype_option_menu;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 char buf[64];
 int filetype_nr;
 int select_item;

  DBG(DBG_proc, "xsane_mail_dialog\n");

  if (xsane.mail_dialog) 
  {
    return; /* window already is open */
  }

  /* GTK_WINDOW_TOPLEVEL looks better but does not place it nice*/
  mail_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_MAIL_PROJECT);
  gtk_window_set_title(GTK_WINDOW(mail_dialog), buf);
  g_signal_connect(GTK_OBJECT(mail_dialog), "delete_event", (GtkSignalFunc) xsane_mail_dialog_delete, NULL);
  xsane_set_window_icon(mail_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(mail_dialog), xsane.accelerator_group); 

  /* set the main vbox */
  mail_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(mail_scan_vbox), 0);
  gtk_container_add(GTK_CONTAINER(mail_dialog), mail_scan_vbox);
  gtk_widget_show(mail_scan_vbox);


  /* mail project */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(mail_scan_vbox), hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) mail_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_MAILPROJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.mail_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_mail_project_changed_callback, NULL);

  xsane.mail_project_entry = text;
  xsane.mail_project_entry_box = hbox;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);

  mail_project_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(mail_scan_vbox), mail_project_vbox, TRUE, TRUE, 0);
  gtk_widget_show(mail_project_vbox);


  /* mail receiver */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), hbox, FALSE, FALSE, 2);

  gtk_widget_realize(mail_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(mail_dialog->window, &mask, xsane.bg_trans, (gchar **) mailreceiver_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);

  text = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_MAILRECEIVER);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_mail_receiver_changed_callback, NULL);

  xsane.mail_receiver_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* subject */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), hbox, FALSE, FALSE, 2);

  gtk_widget_realize(mail_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(mail_dialog->window, &mask, xsane.bg_trans, (gchar **) subject_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_MAILSUBJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_mail_subject_changed_callback, NULL);

  xsane.mail_subject_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* email text frame */
  text_frame = gtk_frame_new(TEXT_MAIL_TEXT);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), text_frame, TRUE, TRUE, 2);
  gtk_widget_show(text_frame);

  /* email text box */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_container_add(GTK_CONTAINER(text_frame), hbox);
  gtk_widget_show(hbox);

#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkWidget *scrolled_window, *text_view, *text_buffer;
 
    /* create a scrolled window to get a vertical scrollbar */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(hbox), scrolled_window);
    gtk_widget_show(scrolled_window);
 
    /* create the gtk_text_view widget */
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_widget_show(text_view);
 
    /* get the text_buffer widget and insert the text from file */
    text_buffer = (GtkWidget *) gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    xsane.mail_text_widget = text_buffer;
  }
#else 
  {
    GtkWidget *vscrollbar;

    /* Create the GtkText widget */
    text = gtk_text_new(NULL, NULL);
    gtk_text_set_editable(GTK_TEXT(text), TRUE); /* text is editable */
    gtk_text_set_word_wrap(GTK_TEXT(text), TRUE); /* wrap complete words */
    gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0);
    gtk_widget_show(text);
    xsane.mail_text_widget = text;
 
    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);
    gtk_widget_show(vscrollbar); 
  }
#endif


  /* html mail */
  button = gtk_check_button_new_with_label(RADIO_BUTTON_HTML_MAIL);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_HTML_MAIL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), xsane.mail_html_mode);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), button, FALSE, FALSE, 2);
  gtk_widget_show(button);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_html_mode_callback, NULL);
  xsane.mail_html_mode_widget = button;

  xsane_separator_new(mail_scan_vbox, 2);

  /* FILETYPE MENU */
  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  filetype_menu = gtk_menu_new();

  filetype_nr = -1;
  select_item = 0;

#ifdef HAVE_LIBJPEG
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_JPEG);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_mail_filetype_callback, (void *) XSANE_FILETYPE_JPEG);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.mail_filetype) && (!strcasecmp(preferences.mail_filetype, XSANE_FILETYPE_JPEG)) )
  {
    select_item = filetype_nr;
  }
#endif


  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PDF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_mail_filetype_callback, (void *) XSANE_FILETYPE_PDF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.mail_filetype) && (!strcasecmp(preferences.mail_filetype, XSANE_FILETYPE_PDF)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNG);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_mail_filetype_callback, (void *) XSANE_FILETYPE_PNG);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.mail_filetype) && (!strcasecmp(preferences.mail_filetype, XSANE_FILETYPE_PNG)) )
  {
    select_item = filetype_nr;
  }
#endif
#endif

  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PS);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_mail_filetype_callback, (void *) XSANE_FILETYPE_PS);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.mail_filetype) && (!strcasecmp(preferences.mail_filetype, XSANE_FILETYPE_PS)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBTIFF
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TIFF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_mail_filetype_callback, (void *) XSANE_FILETYPE_TIFF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.mail_filetype) && (!strcasecmp(preferences.mail_filetype, XSANE_FILETYPE_TIFF)) )
  {
    select_item = filetype_nr;
  }
#endif
                                                                                                              
  label = gtk_label_new(TEXT_MAIL_FILETYPE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  filetype_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, filetype_option_menu, DESC_MAIL_FILETYPE);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(filetype_option_menu), filetype_menu);
  if (select_item >= 0)
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(filetype_option_menu), select_item);
  }
  gtk_box_pack_end(GTK_BOX(hbox), filetype_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(filetype_menu);
  gtk_widget_show(filetype_option_menu);


  /* attachment frame */
  attachment_frame = gtk_frame_new(TEXT_ATTACHMENTS);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), attachment_frame, FALSE, FALSE, 2);
  gtk_widget_show(attachment_frame);

  /* attachment list */
  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(attachment_frame), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
  gtk_widget_show(list);
  xsane.mail_list = list;


  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(mail_project_vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label(BUTTON_IMAGE_SHOW);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

#if 0
  /* before we enable the edit function we have to make sure that the rename function
     does also rename the image name of the opened viewer */
  button = gtk_button_new_with_label(BUTTON_IMAGE_EDIT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_edit_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
#endif

  button = gtk_button_new_with_label(BUTTON_IMAGE_RENAME);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_entry_rename_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_IMAGE_DELETE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(mail_dialog->window, hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_mail_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(mail_dialog->window, hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_mail_entry_move_down_callback, list);

  gtk_widget_show(hbox);

  xsane.mail_project_box = mail_project_vbox;

  xsane_separator_new(mail_scan_vbox, 2);


  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(mail_project_vbox, 2);
  gtk_box_pack_end(GTK_BOX(mail_scan_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);     


  mail_project_exists_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), mail_project_exists_hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(BUTTON_SEND_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_send, NULL);
  gtk_box_pack_start(GTK_BOX(mail_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_project_delete, NULL);
  gtk_box_pack_start(GTK_BOX(mail_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(mail_project_exists_hbox);
  xsane.mail_project_exists = mail_project_exists_hbox;

  button = gtk_button_new_with_label(BUTTON_CREATE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_project_create, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  xsane.mail_project_not_exists = button;

  /* progress bar */
  xsane.mail_progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(mail_scan_vbox), (GtkWidget *) xsane.mail_progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.mail_progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.mail_progress_bar));


  xsane.mail_dialog = mail_dialog;

  xsane_mail_project_load();

  gtk_widget_show(mail_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_dialog_close()
{
  DBG(DBG_proc, "xsane_mail_dialog_close\n");

  if (xsane.mail_dialog == NULL)
  {
    return;
  }

  gtk_widget_destroy(xsane.mail_dialog);

  xsane.mail_dialog = NULL;
  xsane.mail_list = NULL;
  xsane.mail_progress_bar = NULL;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_set_sensitive(int sensitive)
{
  gtk_widget_set_sensitive(xsane.mail_project_box, sensitive);
  gtk_widget_set_sensitive(xsane.mail_project_exists, sensitive);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_display_status()
{
 FILE *lockfile;
 char buf[256];
 char filename[PATH_MAX];
 int val;
 int i, c;

  DBG(DBG_proc, "xsane_mail_project_display_status\n");

  snprintf(filename, sizeof(filename), "%s/lockfile", preferences.mail_project);
  lockfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if (lockfile)
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is mail status */
    {
      c = fgetc(lockfile);
      buf[i++] = c;
    }
    buf[i-1] = 0;

    fscanf(lockfile, "%d\n", &val);

    fclose(lockfile);

    if ( (!strcmp(buf, TEXT_MAIL_STATUS_SENDING)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SENT)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_ERR_READ_PROJECT)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_POP3_CONNECTION_FAILED)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_POP3_LOGIN_FAILED)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SMTP_CONNECTION_FAILED)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SMTP_ERR_FROM)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SMTP_ERR_RCPT)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SMTP_ERR_DATA)) ||
         (!strcmp(buf, TEXT_MAIL_STATUS_SENT)) )
    {
      if (strcmp(xsane.mail_status, buf))
      {
        if (xsane.mail_status)
        {
          free(xsane.mail_status);
        }
        xsane.mail_status = strdup(buf);
 
        if (xsane.mail_progress_bar)
        {
          gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), _(xsane.mail_status));
        }
      }

      xsane.mail_progress_val = val / 100.0;
      if (xsane.mail_progress_bar)
      {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), xsane.mail_progress_val);
      }

      DBG(DBG_info, "reading from lockfile: mail_status %s, mail_progress_val %1.3f\n" , xsane.mail_status, xsane.mail_progress_val);

      if (strcmp(xsane.mail_status, TEXT_MAIL_STATUS_SENDING)) /* not sending */
      {
        DBG(DBG_info, "removing %s\n", filename);
        remove(filename); /* remove lockfile */
 
        xsane.mail_progress_val = 0.0;
 
        xsane_mail_project_update_project_status();

        if (xsane.mail_dialog)
        {
          xsane_mail_project_load();

          xsane_mail_project_set_sensitive(TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
        }
      }
    }
  }
  else
  {
    DBG(DBG_info, "no lockfile present\n");
    if (xsane.mail_progress_bar)
    {
      gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), _(xsane.mail_status));
      gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), xsane.mail_progress_val);
    }
  
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_mail_send_timer_callback(gpointer data)
{
  xsane_mail_project_display_status();

  if (strcmp(xsane.mail_status, TEXT_MAIL_STATUS_SENDING)) /* not sending */
  {
    if (xsane_mail_send_timer)
    {
      DBG(DBG_info, "disabling mail send timer\n");
      xsane_mail_send_timer = 0;
    }
  }

 return xsane_mail_send_timer;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_load()
{
 FILE *projectfile;
 char page[256];
 char *type;
 char *extension;
 char buf[256];
 char filename[PATH_MAX];
 GtkWidget *list_item;
 int i;
 int c;

  DBG(DBG_proc, "xsane_mail_project_load\n");

  if (xsane.mail_status)
  {
    free(xsane.mail_status);
    xsane.mail_status = NULL;
  }

  if (xsane.mail_receiver)
  {
    free(xsane.mail_receiver);
    xsane.mail_receiver = NULL;
  }

  if (xsane.mail_filename)
  {
    free(xsane.mail_filename);
    xsane.mail_filename = NULL;
  }

  if (xsane.mail_subject)
  {
    free(xsane.mail_subject);
    xsane.mail_subject = NULL;
  }

  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.mail_receiver_entry), GTK_SIGNAL_FUNC(xsane_mail_receiver_changed_callback), 0);
  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.mail_subject_entry), GTK_SIGNAL_FUNC(xsane_mail_subject_changed_callback), 0);
  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.mail_html_mode_widget), GTK_SIGNAL_FUNC(xsane_mail_html_mode_callback), 0);

#ifdef HAVE_GTK_TEXT_VIEW_H
  gtk_text_buffer_set_text(GTK_TEXT_BUFFER(xsane.mail_text_widget), "", 0);
#else
  gtk_text_set_point(GTK_TEXT(xsane.mail_text_widget), 0);
  gtk_text_forward_delete(GTK_TEXT(xsane.mail_text_widget), gtk_text_get_length(GTK_TEXT(xsane.mail_text_widget)));
#endif
  gtk_list_remove_items(GTK_LIST(xsane.mail_list), GTK_LIST(xsane.mail_list)->children);

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.mail_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    snprintf(filename, sizeof(filename), "%s/image-1.pnm", preferences.mail_project);
    xsane.mail_filename=strdup(filename);
    xsane_update_counter_in_filename(&xsane.mail_filename, FALSE, 0, preferences.filename_counter_len); /* correct counter len */

    xsane.mail_status=strdup(TEXT_MAIL_STATUS_NOT_CREATED);
    xsane.mail_progress_val = 0.0;

    xsane.mail_receiver=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.mail_receiver_entry), (char *) xsane.mail_receiver);

    xsane.mail_subject=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.mail_subject_entry), (char *) xsane.mail_subject);

    gtk_widget_hide(xsane.mail_project_exists);
    gtk_widget_show(xsane.mail_project_not_exists);

    gtk_widget_set_sensitive(xsane.mail_project_box, FALSE);
    gtk_widget_set_sensitive(xsane.mail_project_exists, FALSE);
    /* do not change sensitivity of mail_project_entry_box here !!! */
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 

    xsane.mail_project_save = 0;
  }
  else
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is mail status */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;
    if (strchr(page, '@'))
    {
      *strchr(page, '@') = 0;
    }

    if (xsane.mail_status)
    {
      free(xsane.mail_status);
    }
    xsane.mail_status = strdup(page);
    xsane.mail_progress_val = 0.0;


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* second line is email address */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.mail_receiver=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.mail_receiver_entry), (char *) xsane.mail_receiver);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* third line is next mail filename */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    snprintf(filename, sizeof(filename), "%s/%s", preferences.mail_project, page);
    xsane.mail_filename=strdup(filename);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* fourth line is subject */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.mail_subject=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.mail_subject_entry), (char *) xsane.mail_subject);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* fifth line is html/ascii */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    if (!strcasecmp("html", page))
    {
      xsane.mail_html_mode = 1;
    }
    else
    {
      xsane.mail_html_mode = 0;
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane.mail_html_mode_widget), xsane.mail_html_mode);


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

      if (!strcmp("mailtext:", page))
      {
        break; /* mailtext follows */
      }

      extension = strrchr(page, '.');
      if (extension)
      {
        type = strdup(extension);
        *extension = 0;
      }
      else
      {
        type = strdup("");
      }

      if (c > 1)
      {
        list_item = gtk_list_item_new_with_label(page);
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
        gtk_container_add(GTK_CONTAINER(xsane.mail_list), list_item);
        gtk_widget_show(list_item);
      }
    }

    while (!feof(projectfile))
    {
      i = fread(buf, 1, sizeof(buf), projectfile);
#ifdef HAVE_GTK_TEXT_VIEW_H
      gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(xsane.mail_text_widget), buf, i);
#else
      gtk_text_insert(GTK_TEXT(xsane.mail_text_widget), NULL, NULL, NULL, buf, i);
#endif
    }

    if (!strcmp(xsane.mail_status, TEXT_MAIL_STATUS_SENDING)) /* mail project is locked (sending) */
    {
      xsane_mail_project_set_sensitive(FALSE);
      gtk_widget_set_sensitive(xsane.mail_project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 

      if (xsane_mail_send_timer == 0)
      {
        xsane_mail_send_timer = gtk_timeout_add(100, (GtkFunction) xsane_mail_send_timer_callback, NULL);
        DBG(DBG_info, "enabling mail send timer (%d)\n", xsane_mail_send_timer);
      }
    }
    else
    {
      xsane_mail_project_set_sensitive(TRUE);
      gtk_widget_set_sensitive(xsane.mail_project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
    }

    gtk_widget_show(xsane.mail_project_exists);
    gtk_widget_hide(xsane.mail_project_not_exists);

    xsane.mail_project_save = 1;
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), _(xsane.mail_status));
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), xsane.mail_progress_val);

  xsane_mail_project_display_status();

  g_signal_connect(GTK_OBJECT(xsane.mail_html_mode_widget), "clicked", (GtkSignalFunc) xsane_mail_html_mode_callback, NULL);
  g_signal_connect(GTK_OBJECT(xsane.mail_receiver_entry), "changed", (GtkSignalFunc) xsane_mail_receiver_changed_callback, NULL);
  g_signal_connect(GTK_OBJECT(xsane.mail_subject_entry), "changed", (GtkSignalFunc) xsane_mail_subject_changed_callback, NULL);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_delete()
{
 char *page;
 char *type;
 char file[256];
 GList *list = (GList *) GTK_LIST(xsane.mail_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_mail_project_delete\n");

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.mail_project, page, type);
    free(page);
    free(type);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/xsane-mail-list", preferences.mail_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.mail_project);
  rmdir(file);

  xsane_mail_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_update_project_status()
{
 FILE *projectfile;
 char filename[PATH_MAX];
 char buf[256];

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.mail_project);
  projectfile = fopen(filename, "r+b"); /* r+ = read and write, position = start of file */

  snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.mail_status); /* fill 32 characters status line */
  fprintf(projectfile, "%s\n", buf); /* first line is status of mail */

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_mail_project_save()
{
 FILE *projectfile;
 GList *list = (GList *) GTK_LIST(xsane.mail_list)->children;
 GtkObject *list_item;
 char *page;
 char *type;
 gchar *mail_text;
 char filename[256];

  DBG(DBG_proc, "xsane_mail_project_save\n");

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  mkdir(preferences.mail_project, 0777); /* make sure directory exists */

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.mail_project);

  if (xsane_create_secure_file(filename)) /* remove possibly existing symbolic links for security */
  {
   char buf[256];

    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, filename);
    xsane_back_gtk_error(buf, TRUE);
   return; /* error */
  }

  projectfile = fopen(filename, "wb"); /* write binary (b for win32) */

  if (xsane.mail_status)
  {
   char buf[256];

    snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.mail_status); /* fill 32 characters status line */
    fprintf(projectfile, "%s\n", buf); /* first line is status of mail */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), _(xsane.mail_status));
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), 0.0);
  }
  else
  {
    fprintf(projectfile, "                                \n"); /* no mail status */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), "");
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), 0.0);
  }

  if (xsane.mail_receiver)
  {
    fprintf(projectfile, "%s\n", xsane.mail_receiver); /* second line is receiver phone number or address */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.mail_filename)
  {
    fprintf(projectfile, "%s\n", strrchr(xsane.mail_filename, '/')+1); /* third line is next mail filename */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.mail_subject)
  {
    fprintf(projectfile, "%s\n", xsane.mail_subject); /* fourth line is subject */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.mail_html_mode) /* fith line is mode html/ascii */
  {
    fprintf(projectfile, "html\n");
  }
  else
  {
    fprintf(projectfile, "ascii\n");
  }


  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = (char *) gtk_object_get_data(list_item, "list_item_data");
    type = (char *) gtk_object_get_data(list_item, "list_item_type");
    fprintf(projectfile, "%s%s\n", page, type);
    list = list->next;
  }

  /* save mail text */
  fprintf(projectfile, "mailtext:\n");
#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkTextIter start, end;

    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(xsane.mail_text_widget), &start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(xsane.mail_text_widget), &end);
    mail_text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(xsane.mail_text_widget), &start, &end, FALSE);
  }
#else
  mail_text = gtk_editable_get_chars(GTK_EDITABLE(xsane.mail_text_widget), 0, -1);
#endif
  fprintf(projectfile, "%s", mail_text);

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_create()
{
  DBG(DBG_proc, "xsane_mail_project_create\n");

  if (strlen(preferences.mail_project))
  {
    if (xsane.mail_status)
    {
      free(xsane.mail_status);
    }
    xsane.mail_status = strdup(TEXT_MAIL_STATUS_CREATED);
    xsane_mail_project_save();
    xsane_mail_project_load();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_receiver_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_mail_receiver_changed_callback\n");

  if (xsane.mail_receiver)
  {
    free((void *) xsane.mail_receiver);
  }
  xsane.mail_receiver = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (xsane.mail_status)
  {
    free(xsane.mail_status);
  }
  xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
  xsane.mail_project_save = 1;
  xsane_mail_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_subject_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_mail_subject_changed_callback\n");

  if (xsane.mail_subject)
  {
    free((void *) xsane.mail_subject);
  }
  xsane.mail_subject = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (xsane.mail_status)
  {
    free(xsane.mail_status);
  }
  xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
  xsane.mail_project_save = 1;
  xsane_mail_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_project_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_mail_project_changed_callback\n");

  if (xsane.mail_project_save)
  {
    xsane.mail_project_save = 0;
    xsane_mail_project_save();
  }

  if (preferences.mail_project)
  {
    free((void *) preferences.mail_project);
  }
  preferences.mail_project = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_mail_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_html_mode_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_mail_html_mode_callback\n");

  xsane.mail_html_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  /* we can save it because this routine is only called when the project already exists */
  if (xsane.mail_status)
  {
    free(xsane.mail_status);
  }
  xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
  xsane.mail_project_save = 1;
  xsane_mail_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_entry_move_up_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_mail_entry_move_up\n");

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
        xsane_list_entrys_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);

        if (xsane.mail_status)
        {
          free(xsane.mail_status);
        }
        xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
        xsane_mail_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_entry_move_down_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_mail_entry_move_down\n");

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
        xsane_list_entrys_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);

        if (xsane.mail_status)
        {
          free(xsane.mail_status);
        }
        xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
        xsane_mail_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_mail_entry_rename;

static void xsane_mail_entry_rename_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_mail_entry_rename\n");

  xsane_mail_entry_rename = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_entry_rename_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 GList *select;
 char *oldpage;
 char *newpage;
 char *type;
 char oldfile[256];
 char newfile[256];

  DBG(DBG_proc, "xsane_mail_entry_rename_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
   GtkWidget *rename_dialog;
   GtkWidget *text;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   char filename[PATH_MAX]; 

    list_item = select->data;
    oldpage = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_data"));
    type    = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_type"));

    xsane_set_sensitivity(FALSE);

    rename_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    xsane_set_window_icon(rename_dialog, 0);

    /* set the main vbox */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
    gtk_container_add(GTK_CONTAINER(rename_dialog), vbox);
    gtk_widget_show(vbox);

    /* set the main hbox */
    hbox = gtk_hbox_new(FALSE, 0);
    xsane_separator_new(vbox, 2);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    gtk_widget_show(hbox); 

    gtk_window_set_position(GTK_WINDOW(rename_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(rename_dialog), FALSE);
    snprintf(filename, sizeof(filename), "%s %s", xsane.prog_name, WINDOW_MAIL_RENAME);
    gtk_window_set_title(GTK_WINDOW(rename_dialog), filename);
    g_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_mail_entry_rename_button_callback, (void *) -1);
    gtk_widget_show(rename_dialog);

    text = gtk_entry_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_MAILIMAGENAME);
    gtk_entry_set_max_length(GTK_ENTRY(text), 64);
    gtk_entry_set_text(GTK_ENTRY(text), oldpage);
    gtk_widget_set_size_request(text, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
    gtk_widget_show(text);


#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
    button = gtk_button_new_with_label(BUTTON_OK);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_entry_rename_button_callback, (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_mail_entry_rename_button_callback,(void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);


    xsane_mail_entry_rename = 0;

    while (xsane_mail_entry_rename == 0)
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    newpage = strdup(gtk_entry_get_text(GTK_ENTRY(text)));

    if (xsane_mail_entry_rename == 1)
    {
      gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item))->data), newpage);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(newpage));

      xsane_convert_text_to_filename(&oldpage);
      xsane_convert_text_to_filename(&newpage);
      snprintf(oldfile, sizeof(oldfile), "%s/%s%s", preferences.mail_project, oldpage, type);
      snprintf(newfile, sizeof(newfile), "%s/%s%s", preferences.mail_project, newpage, type);

      rename(oldfile, newfile);

      if (xsane.mail_status)
      {
        free(xsane.mail_status);
      }
      xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
      xsane_mail_project_save();
    }

    free(oldpage);
    free(newpage);

    gtk_widget_destroy(rename_dialog);

    xsane_set_sensitivity(TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char file[256];

  DBG(DBG_proc, "xsane_mail_entry_delete_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.mail_project, page, type);
    free(page);
    free(type);
    remove(file);
    gtk_widget_destroy(GTK_WIDGET(list_item));

    if (xsane.mail_status)
    {
      free(xsane.mail_status);
    }
    xsane.mail_status = strdup(TEXT_MAIL_STATUS_CHANGED);
    xsane_mail_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[256];

  DBG(DBG_proc, "xsane_mail_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.mail_project, page, type);
    free(page);
    free(type);

    xsane_viewer_new(filename, FALSE, filename, VIEWER_NO_MODIFICATION);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0
static void xsane_mail_edit_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[256];
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Image_info image_info;
 int cancel_save;

  DBG(DBG_proc, "xsane_mail_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.mail_project, page, type);
    free(page);
    free(type);


    infile = fopen(filename, "rb");
    if (!infile)
    {
      DBG(DBG_error, "could not load file %s\n", filename);
     return;
    }

    xsane_read_pnm_header(infile, &image_info);

    DBG(DBG_info, "copying image %s with geometry: %d x %d x %d, %d colors\n", filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

    xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".pnm", XSANE_PATH_TMP);

    outfile = fopen(outfilename, "wb");
    if (!outfile)
    {
      DBG(DBG_error, "could not save file %s\n", outfilename);
     return;
    }

    gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), PROGRESS_CLONING_DATA);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), 0.0);

    xsane_save_rotate_image(outfile, infile, &image_info, 0, xsane.mail_progress_bar, &cancel_save);

    fclose(infile);
    fclose(outfile);

    gtk_progress_set_format_string(GTK_PROGRESS(xsane.mail_progress_bar), "");
    gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.mail_progress_bar), 0.0);

    xsane_viewer_new(outfilename, FALSE, filename, VIEWER_NO_NAME_MODIFICATION);
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_create_mail(int fd)
{
 FILE *attachment_file;
 FILE *projectfile;
 char *boundary="-----partseparator";
 char *image_filename;
 char *mail_text = NULL;
 char *mail_text_pos = NULL;
 char **attachment_filename = NULL;
 char *mime_type = NULL;
 char buf[256];
 char filename[256];
 char content_id[256];
 char image[256];
 int i, j;
 int c;
 int attachments = 0;
 int use_attachment = 0;
 int mail_text_size = 0;
 int display_images_inline = FALSE;

  DBG(DBG_proc, "xsane_create_mail\n");

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.mail_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    DBG(DBG_error, "could not open mail project file %s\n", filename);

    if (xsane.mail_status)
    {
      free(xsane.mail_status);
    }
    xsane.mail_status = strdup(TEXT_MAIL_STATUS_ERR_READ_PROJECT);
    xsane.mail_progress_val = 0.0;
    xsane_front_gtk_mail_project_update_lockfile_status();

   return;
  }

  for (i=0; i<5; i++) /* skip 5 lines */
  {
    j=0;
    c=0;
    while ((j<255) && (c != 10) && (c != EOF)) /* first line is mail status */
    {
      c = fgetc(projectfile);
      j++;
    }
  }

  if (!strcmp(preferences.mail_filetype, XSANE_FILETYPE_PNG))
  {
    mime_type = "image/png";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.mail_filetype, XSANE_FILETYPE_JPEG))
  {
    mime_type = "image/jpeg";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.mail_filetype, XSANE_FILETYPE_TIFF))
  {
    mime_type = "image/tiff";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.mail_filetype, XSANE_FILETYPE_PDF))
  {
    mime_type = "doc/pdf";
    display_images_inline = FALSE;
  }
  else if (!strcmp(preferences.mail_filetype, XSANE_FILETYPE_PS))
  {
    mime_type = "doc/postscript";
    display_images_inline = FALSE;
  }
  else
  {
    mime_type = "doc/unknown";
    display_images_inline = FALSE;
  }

  DBG(DBG_info, "reading list of attachments:\n");
  /* read list of attachments */
  while (!feof(projectfile))
  {
    /* read next attachment line */
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF))
    {
      c = fgetc(projectfile);
      image[i++] = c;
    }
    image[i-1]=0;

    if (strcmp("mailtext:", image) && (c > 1))
    {
     char imagename[256];
     char *filename;
     char *extension;

      DBG(DBG_info, " - %s\n", image);

      extension = strrchr(image, '.');
      if (extension)
      {
        *extension = 0;
      }

      snprintf(imagename, sizeof(imagename), "%s%s", image, preferences.mail_filetype);
      filename=strdup(imagename);
      xsane_convert_text_to_filename(&filename);
      attachment_filename = realloc(attachment_filename, (attachments+1)*sizeof(void *));
      attachment_filename[attachments++] = strdup(filename);
      free(filename);
    }
    else
    {
      break;
    }
  }

  /* read mail text */
  while (!feof(projectfile))
  {
    mail_text = realloc(mail_text, mail_text_size+1025); /* increase mail_text by 1KB */
    mail_text_size += fread(mail_text+mail_text_size, 1, 1024, projectfile); /* read next KB */
  }
  DBG(DBG_info, "%d bytes mailtext read\n", mail_text_size);

  *(mail_text + mail_text_size) = 0; /* set end of text marker */
  mail_text_pos = mail_text;

  if (xsane.mail_html_mode) /* create html mail */
  {
    DBG(DBG_info, "sending mail in html format\n");

    write_mail_header(fd, preferences.mail_from, preferences.mail_reply_to, xsane.mail_receiver, xsane.mail_subject, boundary, 1 /* related */);
    write_mail_mime_html(fd, boundary);

    DBG(DBG_info, "sending mail text\n");
    while (*mail_text_pos != 0)
    {
      if (!strncasecmp("<image>", mail_text_pos, 7)) /* insert image */
      {
        mail_text_pos += 6; /* <image> is 7 characters, 6 additional ones */

        if (use_attachment < attachments)
        {
          image_filename = attachment_filename[use_attachment++];
          DBG(DBG_info, "inserting image cid for %s\n", image_filename);
          snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */

          /* doc files like ps and pdf can not be displayed inline in html mail */
          if (display_images_inline)
          {
            snprintf(buf, sizeof(buf), "<p><img SRC=\"cid:%s\">\n", content_id);
          }
          write(fd, buf, strlen(buf));
        }
        else /* more images selected than available */
        {
        }
      }
      else if (*mail_text_pos == 10) /* new line */
      {
        snprintf(buf, sizeof(buf), "<br>\n");
        write(fd, buf, strlen(buf));
      }
      else
      {
        write(fd, mail_text_pos, 1);
      }
      mail_text_pos++;
    }

    while (use_attachment < attachments) /* append not already referenced images */
    {
      image_filename = attachment_filename[use_attachment++];
      DBG(DBG_info, "appending image cid for %s\n", image_filename);
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */

      /* doc files like ps and pdf can not be displayed inline in html mail */
      if (display_images_inline)
      {
        snprintf(buf, sizeof(buf), "<p><img SRC=\"cid:%s\">\n", content_id);
      }
      write(fd, buf, strlen(buf));
    }

    snprintf(buf, sizeof(buf), "</html>\n");
    write(fd, buf, strlen(buf));


    for (i=0; i<attachments; i++)
    {
      image_filename = attachment_filename[i];
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */
      snprintf(filename, sizeof(filename), "%s/mail-%s", preferences.mail_project, image_filename);
      attachment_file = fopen(filename, "rb"); /* read, b=binary for win32 */

      if (attachment_file)
      {
        DBG(DBG_info, "attaching file \"%s\" as \"%s\" with type %s\n", filename, image_filename, preferences.mail_filetype);
        write_mail_attach_image(fd, boundary, content_id, mime_type, attachment_file, image_filename);

        remove(filename);
      }
      else /* could not open attachment file */
      {
        DBG(DBG_error, "could not open attachment file \"%s\"\n", filename);
      }

      free(attachment_filename[i]);
    }
    free(attachment_filename);

    write_mail_footer(fd, boundary);
  }
  else /* ascii mail */
  {
    DBG(DBG_info, "sending mail in ascii format\n");

    write_mail_header(fd, preferences.mail_from, preferences.mail_reply_to, xsane.mail_receiver, xsane.mail_subject, boundary, 0 /* not related */);
    write_mail_mime_ascii(fd, boundary);

    write(fd, mail_text, strlen(mail_text));
    write(fd, "\n\n", 2);

    for (i=0; i<attachments; i++)
    {
      image_filename = strdup(attachment_filename[i]);
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */
      snprintf(filename, sizeof(filename), "%s/mail-%s", preferences.mail_project, image_filename);
      attachment_file = fopen(filename, "rb"); /* read, b=binary for win32 */

      if (attachment_file)
      {
        DBG(DBG_info, "attaching file \"%s\" as \"%s\" with type %s\n", filename, image_filename, preferences.mail_filetype);
        write_mail_attach_image(fd, boundary, content_id, mime_type, attachment_file, image_filename);

        remove(filename);
      }
      else /* could not open attachment file */
      {
        DBG(DBG_error, "could not oppen attachment png file \"%s\"\n", filename);
      }

      free(image_filename);
      free(attachment_filename[i]);
    }
    free(attachment_filename);

    write_mail_footer(fd, boundary);
  }

  free(mail_text);

  if (projectfile)
  {
    fclose(projectfile);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_send_process()
{
 int fd_socket;
 int status;

  DBG(DBG_proc, "xsane_mail_send_process\n");

  /* pop3 authentification */
  if (preferences.mail_pop3_authentification)
  {
   char *password;
   int i;

    fd_socket = open_socket(preferences.mail_pop3_server, preferences.mail_pop3_port);

    if (fd_socket < 0) /* could not open socket */
    {
      if (xsane.mail_status)
      {
        free(xsane.mail_status);
      }
      xsane.mail_status = strdup(TEXT_MAIL_STATUS_POP3_CONNECTION_FAILED);
      xsane.mail_progress_val = 0.0;
      xsane_front_gtk_mail_project_update_lockfile_status();

     return;
    }

    password = strdup(preferences.mail_pop3_pass);

    for (i=0; i<strlen(password); i++)
    {
      password[i] ^= 0x53;
    } 

    status = pop3_login(fd_socket, preferences.mail_pop3_user, password);

    free(password);

    close(fd_socket);

    if (status == -1)
    {
      if (xsane.mail_status)
      {
        free(xsane.mail_status);
      }
      xsane.mail_status = strdup(TEXT_MAIL_STATUS_POP3_LOGIN_FAILED);
      xsane.mail_progress_val = 0.0;
      xsane_front_gtk_mail_project_update_lockfile_status();

     return;
    }
  }

  DBG(DBG_info, "POP3 authentification done\n");


  /* smtp mail */
  fd_socket = open_socket(preferences.mail_smtp_server, preferences.mail_smtp_port);

  if (fd_socket < 0) /* could not open socket */
  {
    if (xsane.mail_status)
    {
      free(xsane.mail_status);
    }
    xsane.mail_status = strdup(TEXT_MAIL_STATUS_SMTP_CONNECTION_FAILED);
    xsane.mail_progress_val = 0.0;
    xsane_front_gtk_mail_project_update_lockfile_status();

   return;
  }

  status = write_smtp_header(fd_socket, preferences.mail_from, xsane.mail_receiver);
  if (status == -1)
  {
   return;
  }

  xsane_create_mail(fd_socket); /* create mail and write to socket */

  write_smtp_footer(fd_socket);

  close(fd_socket);

  if (xsane.mail_status)
  {
    free(xsane.mail_status);
  }
  xsane.mail_status = strdup(TEXT_MAIL_STATUS_SENT);
  xsane.mail_progress_val = 1.0;
  xsane_front_gtk_mail_project_update_lockfile_status();
  _exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_mail_send()
{
 pid_t pid;
 char *image;
 char *type;
 GList *list = (GList *) GTK_LIST(xsane.mail_list)->children;
 GtkObject *list_item;
 char source_filename[PATH_MAX];
 char mail_filename[PATH_MAX];
 int output_format;
 int cancel_save = 0;

  DBG(DBG_proc, "xsane_mail_send\n");

  xsane_set_sensitivity(FALSE); /* do not allow changing xsane mode */

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  if (xsane.mail_project_save)
  {
    xsane.mail_project_save = 0;
    xsane_mail_project_save();
  }

  xsane.mail_progress_size  = 0;
  xsane.mail_progress_bytes = 0;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    image = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type  = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&image);
    snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.mail_project, image, type);
    snprintf(mail_filename, sizeof(mail_filename), "%s/mail-%s%s", preferences.mail_project, image, preferences.mail_filetype);
    free(image);
    free(type);
    DBG(DBG_info, "converting %s to %s\n", source_filename, mail_filename);
    output_format = xsane_identify_output_format(mail_filename, NULL, NULL);
    xsane_save_image_as(mail_filename, source_filename, output_format, xsane.mail_progress_bar, &cancel_save);
    list = list->next;
    xsane.mail_progress_size += xsane_get_filesize(mail_filename);
  }


  if (xsane.mail_status)
  {
    free(xsane.mail_status);
  }
  xsane.mail_status = strdup(TEXT_MAIL_STATUS_SENDING);
  xsane.mail_progress_val = 0.0;
  xsane_mail_project_display_status(); /* display status before creating lockfile! */
  xsane_front_gtk_mail_project_update_lockfile_status(); /* create lockfile and update status */

  pid = fork();
 
  if (pid == 0) /* new process */
  {
   FILE *ipc_file = NULL;

    if (xsane.ipc_pipefd[0])
    {
      close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
      ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
    }

    DBG(DBG_info, "trying to change user id for new subprocess:\n");
    DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
    setuid(getuid());
    DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

    xsane_mail_send_process();

    _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
  }
  else /* parent process */
  {
    xsane_add_process_to_list(pid); /* add pid to child process list */
  }

  xsane_mail_send_timer = gtk_timeout_add(100, (GtkFunction) xsane_mail_send_timer_callback, NULL);
  DBG(DBG_info, "enabling mail send timer (%d)\n", xsane_mail_send_timer);

  xsane_set_sensitivity(TRUE); /* allow changing xsane mode */
#if 0
  gtk_widget_set_sensitive(xsane.mail_project_entry_box, TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
  gtk_widget_set_sensitive(xsane.mail_project_box, FALSE);
#endif
  xsane_mail_project_set_sensitive(FALSE);
}

#endif
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_toggle_tooltips(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_pref_toggle_tooltips\n");

  preferences.tooltips_enabled = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  xsane_back_gtk_set_tooltips(preferences.tooltips_enabled);
  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_eula(GtkWidget *widget, gpointer data)
{
  xsane_display_eula(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_gpl(GtkWidget *widget, gpointer data)
{
  xsane_display_gpl();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_doc_via_nsr(GtkWidget *widget, gpointer data) /* show via netscape remote */
{
 char *name = (char *) data;
 char buf[256];
 pid_t pid;
 char *arg[5];
 struct stat st;
 char netscape_lock_path[PATH_MAX];

  DBG(DBG_proc, "xsane_show_doc_via_nsr(%s)\n", name);


  /* at first we have to test if netscape is running */
  /* a simple way is to take a look at ~/.netscape/lock */
  /* when this is a link we can assume that netscape is running */

  if (getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)) != NULL) /* $HOME defined? */
  {
    snprintf(netscape_lock_path, sizeof(netscape_lock_path), "%s%c.netscape%clock",
             getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)), SLASH, SLASH); 
  }
  else
  {
    *netscape_lock_path = 0; /* empty path */
  }

#ifdef HAVE_LSTAT
  if ((strlen(netscape_lock_path) > 0) && (lstat(netscape_lock_path, &st) == 0)) /*  netscape is running */
#else
  if ((strlen(netscape_lock_path) > 0) && (stat(netscape_lock_path, &st) == 0)) /*  netscape is running */
#endif
  {
    DBG(DBG_proc, "xsane_show_doc_via_nsr: netscape is running\n");
    snprintf(buf, sizeof(buf), "openFile(%s, new-window)", name);
    arg[0] = "netscape";
    arg[1] = "-no-about-splash";
    arg[2] = "-remote";
    arg[3] = buf;
    arg[4] = 0;
 
    pid = fork();
 
    if (pid == 0) /* new process */
    {
     FILE *ipc_file = NULL;

      if (xsane.ipc_pipefd[0])
      {
        close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
        ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
      }

      DBG(DBG_info, "trying to change user id for new subprocess:\n");
      DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
      setuid(getuid());
      DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

      execvp(arg[0], arg); /* does not return if successfully */
      DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser);

      /* send error message via IPC pipe to parent process */
      if (ipc_file)
      {
        fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser, strerror(errno));
        fflush(ipc_file); /* make sure message is displayed */
        fclose(ipc_file);
      }

      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
    else /* parent process */
    {
      xsane_add_process_to_list(pid); /* add pid to child process list */
    }
  }
  else /* netscape not running */
  { 
    DBG(DBG_proc, "xsane_show_doc_via_nsr: netscape is not running, trying to start netscape\n");
    arg[0] = "netscape";
    arg[1] = name;
    arg[2] = 0;
 
    pid = fork();
 
    if (pid == 0) /* new process */
    {
     FILE *ipc_file = NULL;

      if (xsane.ipc_pipefd[0])
      {
        close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
        ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
      }

      DBG(DBG_info, "trying to change user id for new subprocess:\n");
      DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
      setuid(getuid());
      DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

      execvp(arg[0], arg); /* does not return if successfully */
      DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser);

      /* send error message via IPC pipe to parent process */
      if (ipc_file)
      {
        fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser, strerror(errno));
        fflush(ipc_file); /* make sure message is displayed */
        fclose(ipc_file);
      }

      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
    else /* parent process */
    {
      xsane_add_process_to_list(pid); /* add pid to child process list */
    }
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
/* may be used to parse command lines for execvp like popen does  */
static char **xsane_parse_command(char *command_line, char *url)
{
 char **argv = NULL;
 //char *command = strdup(command_line);
 char command[1024];
 char *command_pos = command;
 char *arg_end;
 int size = 0;

  snprintf(command, sizeof(command), command_line, url);

  argv = malloc(sizeof(char *) * 2);

  while (command_pos)
  {
    argv = realloc(argv, sizeof(char *) * (size+2));

    arg_end = strchr(command_pos, ' ');
    if (arg_end)
    {
      *arg_end = 0;
    }

    argv[size] = strdup(command_pos);
 
    if (arg_end)
    {
      command_pos = arg_end + 1;
    }
    else
    {
      command_pos = NULL;
    }
   
    size++;
  }

  argv[size] = NULL;

  free(command);

 return (argv);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_doc(GtkWidget *widget, gpointer data)
{
 char *name = (char *) data;
 char path[256];
 pid_t pid;
 char *arg[3];
 struct stat st;
 char *language_dir = NULL;

  DBG(DBG_proc, "xsane_show_doc(%s)\n", name);

  /* translation of language_dir gives the name of the subdirectory in */
  /* which there may be a translation of a documentation */
  language_dir = XSANE_LANGUAGE_DIR;
  snprintf(path, sizeof(path), "%s/%s/%s-doc.html", STRINGIFY(PATH_XSANE_DOC_DIR), language_dir, name);  
  if (stat(path, &st) != 0) /* test if file does exist */
  {
    snprintf(path, sizeof(path), "%s/%s-doc.html", STRINGIFY(PATH_XSANE_DOC_DIR), name); /* no, we use original doc */
  }

  if (!strcmp(preferences.browser, BROWSER_NETSCAPE))
  {
    xsane_show_doc_via_nsr(widget, (void *) path);
  }
  else
  {
    arg[0] = preferences.browser;
    arg[1] = path;
    arg[2] = 0;

    pid = fork();

    if (pid == 0) /* new process */
    { 
     FILE *ipc_file = NULL;

      if (xsane.ipc_pipefd[0])
      {
        close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
        ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
      }

      DBG(DBG_info, "trying to change user id for new subprocess:\n");
      DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
      setuid(getuid());
      DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

      DBG(DBG_info, "executing %s %s\n", arg[0], arg[1]);
      execvp(arg[0], arg); /* does not return if successfully */
      DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser);

      /* send error message via IPC pipe to parent process */
      if (ipc_file)
      {
        fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_DOC_VIEWER, preferences.browser, strerror(errno));
        fflush(ipc_file); /* make sure message is displayed */
        fclose(ipc_file);
      }

      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
    else /* parent process */
    {
      xsane_add_process_to_list(pid); /* add pid to child process list */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scan_callback(void)
{
  xsane.scan_rotation = xsane.preview->rotation;
  xsane_scan_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_view_build_menu(void)
{
 GtkWidget *menu, *item, *submenu, *subitem;

  DBG(DBG_proc, "xsane_view_build_menu\n");

  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);

  /* show tooltips */

  item = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_TOOLTIPS);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_T, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), preferences.tooltips_enabled);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
  g_signal_connect(GTK_OBJECT(item), "toggled", (GtkSignalFunc) xsane_pref_toggle_tooltips, NULL);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* show resolution list */

  xsane.show_resolution_list_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_RESOLUTIONLIST);
  gtk_widget_add_accelerator(xsane.show_resolution_list_widget, "activate", xsane.accelerator_group, GDK_L, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_resolution_list_widget), preferences.show_resolution_list);
  gtk_menu_append(GTK_MENU(menu), xsane.show_resolution_list_widget);
  gtk_widget_show(xsane.show_resolution_list_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_resolution_list_widget), "toggled", (GtkSignalFunc) xsane_show_resolution_list_callback, NULL);


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
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_CONTINUOUS);
  gtk_widget_show(subitem);
  xsane.update_policy_continu = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_POLICY_DISCONTINU);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.gtk_update_policy == GTK_UPDATE_DISCONTINUOUS)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_DISCONTINUOUS);
  gtk_widget_show(subitem);
  xsane.update_policy_discont = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_POLICY_DELAYED);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_update_policy_callback, (void *) GTK_UPDATE_DELAYED);
  gtk_widget_show(subitem);
  xsane.update_policy_delayed = subitem;

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* length unit */

  item = gtk_menu_item_new_with_label(MENU_ITEM_LENGTH_UNIT);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  submenu = gtk_menu_new();

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_MILLIMETERS);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if ( (preferences.length_unit > 0.9) && (preferences.length_unit < 1.1))
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_pref_unit_callback, "mm");
  gtk_widget_show(subitem);
  xsane.length_unit_mm = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_CENTIMETERS);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if ( (preferences.length_unit > 9.9) && (preferences.length_unit < 10.1))
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_pref_unit_callback, "cm");
  gtk_widget_show(subitem);
  xsane.length_unit_cm = subitem;

  subitem = gtk_check_menu_item_new_with_label(SUBMENU_ITEM_LENGTH_INCHES);
  gtk_menu_append(GTK_MENU(submenu), subitem);
  if ( (preferences.length_unit > 25.3) && (preferences.length_unit < 25.5))
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(subitem), TRUE);
  }
  g_signal_connect(GTK_OBJECT(subitem), "toggled", (GtkSignalFunc) xsane_set_pref_unit_callback, "in");
  gtk_widget_show(subitem);
  xsane.length_unit_in = subitem;

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_window_build_menu(void)
{
 GtkWidget *menu;

  DBG(DBG_proc, "xsane_window_build_menu\n");

  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);


  /* show preview */

  xsane.show_preview_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_PREVIEW);
  gtk_widget_add_accelerator(xsane.show_preview_widget, "activate", xsane.accelerator_group, GDK_1, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), xsane.show_preview_widget);
  gtk_widget_show(xsane.show_preview_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_preview_widget), "toggled", (GtkSignalFunc) xsane_show_preview_callback, NULL);
 
  /* show histogram */

  xsane.show_histogram_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_HISTOGRAM);
  gtk_widget_add_accelerator(xsane.show_histogram_widget, "activate", xsane.accelerator_group, GDK_2, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  gtk_menu_append(GTK_MENU(menu), xsane.show_histogram_widget);
  gtk_widget_show(xsane.show_histogram_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_histogram_widget), "toggled", (GtkSignalFunc) xsane_show_histogram_callback, NULL);

  
#ifdef HAVE_WORKING_GTK_GAMMACURVE
  /* show gamma */

  xsane.show_gamma_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_GAMMA);
  gtk_widget_add_accelerator(xsane.show_gamma_widget, "activate", xsane.accelerator_group, GDK_3, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_gamma_widget), preferences.show_gamma);
  gtk_menu_append(GTK_MENU(menu), xsane.show_gamma_widget);
  gtk_widget_show(xsane.show_gamma_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_gamma_widget), "toggled", (GtkSignalFunc) xsane_show_gamma_callback, NULL);
#endif

  /* show batch_scan */

  xsane.show_batch_scan_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_BATCH_SCAN);
  gtk_widget_add_accelerator(xsane.show_batch_scan_widget, "activate", xsane.accelerator_group, GDK_4, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_batch_scan_widget), preferences.show_batch_scan);
  gtk_menu_append(GTK_MENU(menu), xsane.show_batch_scan_widget);
  gtk_widget_show(xsane.show_batch_scan_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_batch_scan_widget), "toggled", (GtkSignalFunc) xsane_show_batch_scan_callback, NULL);
  
  /* show standard options */

  xsane.show_standard_options_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_STANDARDOPTIONS);
  gtk_widget_add_accelerator(xsane.show_standard_options_widget, "activate", xsane.accelerator_group, GDK_5, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_standard_options_widget);
  gtk_widget_show(xsane.show_standard_options_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_standard_options_widget), "toggled", (GtkSignalFunc) xsane_show_standard_options_callback, NULL);


  /* show advanced options */

  xsane.show_advanced_options_widget = gtk_check_menu_item_new_with_label(MENU_ITEM_SHOW_ADVANCEDOPTIONS);
  gtk_widget_add_accelerator(xsane.show_advanced_options_widget, "activate", xsane.accelerator_group, GDK_6, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_advanced_options_widget);
  gtk_widget_show(xsane.show_advanced_options_widget);
  g_signal_connect(GTK_OBJECT(xsane.show_advanced_options_widget), "toggled", (GtkSignalFunc) xsane_show_advanced_options_callback, NULL);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_preferences_build_menu(void)
{
 GtkWidget *menu, *item;

  DBG(DBG_proc, "xsane_preferences_build_menu\n");

  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);


  /* XSane setup dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_SETUP);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_S, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_setup_dialog, NULL);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);



  /* change working directory */

  item = gtk_menu_item_new_with_label(MENU_ITEM_CHANGE_WORKING_DIR);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_change_working_directory, NULL);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* edit medium definitions */

  item = gtk_check_menu_item_new_with_label(MENU_ITEM_EDIT_MEDIUM_DEF);
  g_signal_connect(GTK_OBJECT(item), "toggled", (GtkSignalFunc) xsane_edit_medium_definition_callback, NULL);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* Save device setting */

  item = gtk_menu_item_new_with_label(MENU_ITEM_SAVE_DEVICE_SETTINGS);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_P, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_device_preferences_save, NULL);
  gtk_widget_show(item);

  /* Load device setting */

  item = gtk_menu_item_new_with_label(MENU_ITEM_LOAD_DEVICE_SETTINGS);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_G, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_device_preferences_load, NULL);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_help_build_menu(void)
{
 GtkWidget *menu, *item;

  DBG(DBG_proc, "xsane_help_build_menu\n");

  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);



  /* XSane doc -> html viewer */

  item = gtk_menu_item_new_with_label(MENU_ITEM_XSANE_DOC);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-xsane");
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F1, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);


  /* separator */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* Backend doc -> html viewer */

  if (xsane.backend)
  {
    item = gtk_menu_item_new_with_label(MENU_ITEM_BACKEND_DOC);
    gtk_menu_append(GTK_MENU(menu), item);
    g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) xsane.backend);
    gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F2, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
    gtk_widget_show(item);
  }


  /* available backends -> html viewer */

  item = gtk_menu_item_new_with_label(MENU_ITEM_AVAILABLE_BACKENDS);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-backends");
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F3, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);

  
  /* problems -> html viewer */

  item = gtk_menu_item_new_with_label(MENU_ITEM_PROBLEMS);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-problems");
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F4, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* scantips -> html viewer */

  item = gtk_menu_item_new_with_label(MENU_ITEM_SCANTIPS);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "sane-scantips");
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F5, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);


  /* separator */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* XSane about dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_ABOUT_XSANE);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_about_dialog, NULL);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F6, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);

  /* XSane about translation dialog */

  item = gtk_menu_item_new_with_label(MENU_ITEM_ABOUT_TRANSLATION);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_about_translation_dialog, NULL);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F7, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);


  /* separator */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* XSane eula */

  item = gtk_menu_item_new_with_label(MENU_ITEM_XSANE_EULA);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_eula, NULL);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F8, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);

  /* gpl */

  item = gtk_menu_item_new_with_label(MENU_ITEM_XSANE_GPL);
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_gpl, NULL);
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_F9, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_widget_show(item);


  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_panel_build()
{
 GtkWidget *standard_vbox;
 GtkWidget *advanced_vbox;
 GtkWidget *standard_parent;
 GtkWidget *advanced_parent;
 GtkWidget *parent, *button, *label;
 const SANE_Option_Descriptor *opt;
 SANE_Handle dev = xsane.dev;
 double dval, dmin, dmax, dquant;
 char *buf, str[16], title[256];
 DialogElement *elem;
 SANE_Word quant, val;
 SANE_Status status;
 SANE_Int num_words;
 char **str_list;
 int i, j;
 int num_vector_opts = 0;
 int *vector_opts;

  DBG(DBG_proc, "xsane_panel_build\n");

  /* reset well-known options: */
  xsane.well_known.scanmode        = -1;
  xsane.well_known.scansource      = -1;
  xsane.well_known.preview         = -1;
  xsane.well_known.dpi             = -1;
  xsane.well_known.dpi_x           = -1;
  xsane.well_known.dpi_y           = -1;
  xsane.well_known.coord[xsane_back_gtk_TL_X] = -1;
  xsane.well_known.coord[xsane_back_gtk_TL_Y] = -1;
  xsane.well_known.coord[xsane_back_gtk_BR_X] = -1;
  xsane.well_known.coord[xsane_back_gtk_BR_Y] = -1;
  xsane.well_known.gamma_vector    = -1;
  xsane.well_known.gamma_vector_r  = -1;
  xsane.well_known.gamma_vector_g  = -1;
  xsane.well_known.gamma_vector_b  = -1;
  xsane.well_known.bit_depth       = -1;
  xsane.well_known.threshold       = -1;
  xsane.well_known.shadow          = -1;
  xsane.well_known.shadow_r        = -1;
  xsane.well_known.shadow_g        = -1;
  xsane.well_known.shadow_b        = -1;
  xsane.well_known.highlight       = -1;
  xsane.well_known.highlight_r     = -1;
  xsane.well_known.highlight_g     = -1;
  xsane.well_known.highlight_b     = -1;
  xsane.well_known.batch_scan_start     = -1;
  xsane.well_known.batch_scan_loop      = -1;
  xsane.well_known.batch_scan_end       = -1;
  xsane.well_known.batch_scan_next_tl_y = -1;


  /* standard options */
  xsane.standard_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(xsane.standard_hbox);
  standard_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(standard_vbox);
/*  gtk_box_pack_start(GTK_BOX(xsane.standard_hbox), standard_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(xsane.standard_hbox), standard_vbox, TRUE, TRUE, 0); /* make frame sizeable */

  /* advanced options */
  xsane.advanced_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(xsane.advanced_hbox);
  advanced_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(advanced_vbox);
/*   gtk_box_pack_start(GTK_BOX(xsane.advanced_hbox), advanced_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(xsane.advanced_hbox), advanced_vbox, TRUE, TRUE, 0); /* make frame sizeable */

#if 0
  /* free gamma curve */
  xsane.gamma_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(xsane.gamma_hbox);
  gamma_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(gamma_vbox);
/*   gtk_box_pack_start(GTK_BOX(xsane.gamma_hbox), gamma_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(xsane.gamma_hbox), gamma_vbox, TRUE, TRUE, 0); /* make frame sizeable */
#endif

  vector_opts = alloca(xsane.num_elements * sizeof (int));

  standard_parent = standard_vbox;
  advanced_parent = advanced_vbox;
  for (i = 1; i < xsane.num_elements; ++i)
  {
    opt = xsane_get_option_descriptor(dev, i);
    if (!SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      continue;
    }

    /* pick up well-known options as we go: */
    if (opt->name)
    {
      if (strcmp(opt->name, SANE_NAME_PREVIEW) == 0 && opt->type == SANE_TYPE_BOOL)
      {
        xsane.well_known.preview = i;
        continue;
      }
      else if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) == 0
               && opt->unit == SANE_UNIT_DPI && (opt->type == SANE_TYPE_INT || opt->type == SANE_TYPE_FIXED))
        xsane.well_known.dpi = i;
      else if (strcmp(opt->name, SANE_NAME_SCAN_X_RESOLUTION) == 0
               && opt->unit == SANE_UNIT_DPI && (opt->type == SANE_TYPE_INT || opt->type == SANE_TYPE_FIXED))
        xsane.well_known.dpi_x = i;
      else if (strcmp(opt->name, SANE_NAME_SCAN_Y_RESOLUTION) == 0
               && opt->unit == SANE_UNIT_DPI && (opt->type == SANE_TYPE_INT || opt->type == SANE_TYPE_FIXED))
        xsane.well_known.dpi_y = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_MODE) == 0)
        xsane.well_known.scanmode = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) == 0)
        xsane.well_known.scansource = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_TL_X) == 0)
        xsane.well_known.coord[xsane_back_gtk_TL_X] = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_TL_Y) == 0)
        xsane.well_known.coord[xsane_back_gtk_TL_Y] = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_BR_X) == 0)
        xsane.well_known.coord[xsane_back_gtk_BR_X] = i;
      else if (strcmp (opt->name, SANE_NAME_SCAN_BR_Y) == 0)
        xsane.well_known.coord[xsane_back_gtk_BR_Y] = i;
      else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR) == 0)
        xsane.well_known.gamma_vector = i;
      else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_R) == 0)
        xsane.well_known.gamma_vector_r = i;
      else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_G) == 0)
        xsane.well_known.gamma_vector_g = i;
      else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_B) == 0)
        xsane.well_known.gamma_vector_b = i;
      else if (strcmp (opt->name, SANE_NAME_BIT_DEPTH) == 0)
        xsane.well_known.bit_depth = i;
      else if (strcmp (opt->name, SANE_NAME_THRESHOLD) == 0)
        xsane.well_known.threshold = i;
      else if (strcmp (opt->name, SANE_NAME_HIGHLIGHT) == 0)
        xsane.well_known.highlight = i;
      else if (strcmp (opt->name, SANE_NAME_HIGHLIGHT_R) == 0)
        xsane.well_known.highlight_r = i;
      else if (strcmp (opt->name, SANE_NAME_HIGHLIGHT_G) == 0)
        xsane.well_known.highlight_g = i;
      else if (strcmp (opt->name, SANE_NAME_HIGHLIGHT_B) == 0)
        xsane.well_known.highlight_b = i;
      else if (strcmp (opt->name, SANE_NAME_SHADOW) == 0)
        xsane.well_known.shadow = i;
      else if (strcmp (opt->name, SANE_NAME_SHADOW_R) == 0)
        xsane.well_known.shadow_r = i;
      else if (strcmp (opt->name, SANE_NAME_SHADOW_G) == 0)
        xsane.well_known.shadow_g = i;
      else if (strcmp (opt->name, SANE_NAME_SHADOW_B) == 0)
        xsane.well_known.shadow_b = i;
      else if (strcmp (opt->name, SANE_NAME_BATCH_SCAN_START) == 0)
        xsane.well_known.batch_scan_start = i;
      else if (strcmp (opt->name, SANE_NAME_BATCH_SCAN_LOOP) == 0)
        xsane.well_known.batch_scan_loop = i;
      else if (strcmp (opt->name, SANE_NAME_BATCH_SCAN_END) == 0)
        xsane.well_known.batch_scan_end = i;
      else if (strcmp (opt->name, SANE_NAME_BATCH_SCAN_NEXT_TL_Y) == 0)
        xsane.well_known.batch_scan_next_tl_y = i;
    }

    elem = xsane.element + i;

    if (opt->unit == SANE_UNIT_NONE)
    {
      snprintf(title, sizeof(title), "%s", _BGT(opt->title));
    }
    else
    {
      snprintf(title, sizeof(title), "%s [%s]", _BGT(opt->title), xsane_back_gtk_unit_string(opt->unit));
    }

    if (opt->cap & SANE_CAP_ADVANCED)
    {
      parent = advanced_parent;
    }
    else
    {
      parent = standard_parent;
    }

    switch (opt->type)
    {
      case SANE_TYPE_GROUP:
        /* group a set of options */
        if (opt->cap & SANE_CAP_ADVANCED)
        {
          /* advanced group: put all options to the advanced options window */
          standard_parent = xsane_back_gtk_group_new(advanced_vbox, title);
          advanced_parent = standard_parent;
          elem->widget = standard_parent;
        }
        else
        {
          /* we create the group twice. if no option is defined in one of this groups */
          /* then the group is not shown */

           /* standard group: put standard options to the standard options window */
          standard_parent = xsane_back_gtk_group_new(standard_vbox, title);
          elem->widget = standard_parent;

          /* standard group: put advanced options to the advanced options window */
          advanced_parent = xsane_back_gtk_group_new(advanced_vbox, title);
          elem->widget2 = advanced_parent;
        }
        break;

      case SANE_TYPE_BOOL:
        if (!opt->name)
        {
          DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_BOOL\n", i);
          DBG(DBG_error, "=> %s\n", ERR_OPTION_NAME_NULL);
          DBG(DBG_error, "=> %s\n", ERR_BACKEND_BUG);
         break;
        }

        if ( (strcmp(opt->name, SANE_NAME_BATCH_SCAN_START)  ) && /* do not show batch scan options */
             (strcmp(opt->name, SANE_NAME_BATCH_SCAN_LOOP)  ) && 
             (strcmp(opt->name, SANE_NAME_BATCH_SCAN_END)  ) )
        {
          assert(opt->size == sizeof(SANE_Word));
          status = xsane_control_option(xsane.dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
          {
            goto get_value_failed;
          }
          xsane_back_gtk_button_new(parent, title, val, elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
          gtk_widget_show(parent->parent);
        }
       break;

      case SANE_TYPE_INT:
        if (!opt->name)
        {
          DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_INT\n", i);
          DBG(DBG_error, "=> %s\n", ERR_OPTION_NAME_NULL);
          DBG(DBG_error, "=> %s\n", ERR_BACKEND_BUG);
         break;
        }

        if (opt->size != sizeof(SANE_Word))
        {
          vector_opts[num_vector_opts++] = i;
          break;
        }

        status = xsane_control_option(xsane.dev, i, SANE_ACTION_GET_VALUE, &val, 0);
        if (status != SANE_STATUS_GOOD)
        {
          goto get_value_failed;
        }

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_NONE:
            xsane_back_gtk_value_new(parent, title, val, 1,
                      (opt->cap & SANE_CAP_AUTOMATIC), elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
            gtk_widget_show(parent->parent);
           break;

          case SANE_CONSTRAINT_RANGE:
            if ( (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION)  ) && /* do not show resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_X_RESOLUTION)) && /* do not show x-resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_Y_RESOLUTION)) )  /* do not show y-resolution */
            {
              /* use a scale */
              quant = opt->constraint.range->quant;
              if (quant == 0)
              {
                quant = 1; /* we have integers */
              }

              xsane_back_gtk_range_new(parent, title, val, opt->constraint.range->min, opt->constraint.range->max, quant,
                        (opt->cap & SANE_CAP_AUTOMATIC), elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
              gtk_widget_show(parent->parent);
            }
           break;

          case SANE_CONSTRAINT_WORD_LIST:
            if ( (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION)  ) && /* do not show resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_X_RESOLUTION)) && /* do not show x-resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_Y_RESOLUTION)) )  /* do not show y-resolution */
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
              xsane_back_gtk_option_menu_new(parent, title, str_list, str, elem, xsane.tooltips, _BGT(opt->desc),
                                  SANE_OPTION_IS_SETTABLE(opt->cap));
              free(str_list);
              gtk_widget_show(parent->parent);
            }
           break;

          default:
            DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_INT\n", i);
            DBG(DBG_error, "=> %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
           break;
        }
       break;

      case SANE_TYPE_FIXED:
        if (!opt->name)
        {
          DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_FIXED\n", i);
          DBG(DBG_error, "=> %s\n", ERR_OPTION_NAME_NULL);
          DBG(DBG_error, "=> %s\n", ERR_BACKEND_BUG);
         break;
        }

        if (opt->size != sizeof (SANE_Word))
        {
          vector_opts[num_vector_opts++] = i;
          break;
        }
        status = xsane_control_option(xsane.dev, i, SANE_ACTION_GET_VALUE, &val, 0);
        if (status != SANE_STATUS_GOOD)
        {
          goto get_value_failed;
        }

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_NONE:
            dval = SANE_UNFIX(val);

            if (opt->unit == SANE_UNIT_MM)
            {
              dval /= preferences.length_unit;
            }

            xsane_back_gtk_value_new(parent, title, dval, 0.001, (opt->cap & SANE_CAP_AUTOMATIC), elem,
                                     xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
            gtk_widget_show(parent->parent);
           break;

          case SANE_CONSTRAINT_RANGE:
            if ( (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION)  ) && /* do not show resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_X_RESOLUTION)) && /* do not show x-resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_Y_RESOLUTION)) && /* do not show y-resolution */
                 ((strcmp(opt->name, SANE_NAME_THRESHOLD) || (xsane.lineart_mode == XSANE_LINEART_STANDARD))) &&
                  /* do not show threshold if user wants the slider in the xsane main window */
                 (strcmp(opt->name, SANE_NAME_BATCH_SCAN_NEXT_TL_Y)  ) /* do not show batch scan options */
               )
            {
              /* use a scale */
              dval   = SANE_UNFIX(val);
              dmin   = SANE_UNFIX(opt->constraint.range->min);
              dmax   = SANE_UNFIX(opt->constraint.range->max);
              dquant = SANE_UNFIX(quant = opt->constraint.range->quant);

              if (opt->unit == SANE_UNIT_MM)
              {
                dval /= preferences.length_unit;
                dmin /= preferences.length_unit;
                dmax /= preferences.length_unit;
                dquant /= preferences.length_unit;
              }

              if (dquant == 0) /* no quantization specified */
              {
                dquant = 0.001; /* display x.3 digits */
              }

              xsane_back_gtk_range_new(parent, title, dval, dmin, dmax, dquant, (opt->cap & SANE_CAP_AUTOMATIC), elem,
                                       xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
              gtk_widget_show(parent->parent);
            }
           break;

          case SANE_CONSTRAINT_WORD_LIST:
            if ( (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION)  ) && /* do not show resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_X_RESOLUTION)) && /* do not show x-resolution */
                 (strcmp(opt->name, SANE_NAME_SCAN_Y_RESOLUTION)) )  /* do not show y-resolution */
            {
              /* use a "list-selection" widget */
              num_words = opt->constraint.word_list[0];
              str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
              for (j = 0; j < num_words; ++j)
              {
                sprintf(str, "%g", SANE_UNFIX(opt->constraint.word_list[j + 1]));
                str_list[j] = strdup(str);
              }
              str_list[j] = 0;
              sprintf(str, "%g", SANE_UNFIX(val));
              xsane_back_gtk_option_menu_new(parent, title, str_list, str, elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
              free (str_list);
              gtk_widget_show(parent->parent);
            }
           break;

          default:
            DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_FIXED\n", i);
            DBG(DBG_error, "=> %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
           break;
        }
       break;

      case SANE_TYPE_STRING:
        if (!opt->name)
        {
          DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_STRING\n", i);
          DBG(DBG_error, "=> %s\n", ERR_OPTION_NAME_NULL);
          DBG(DBG_error, "=> %s\n", ERR_BACKEND_BUG);
         break;
        }

        buf = malloc(opt->size);
        status = xsane_control_option(xsane.dev, i, SANE_ACTION_GET_VALUE, buf, 0);
        if (status != SANE_STATUS_GOOD)
        {
          free(buf);
          goto get_value_failed;
        }

        switch (opt->constraint_type)
        {
          case SANE_CONSTRAINT_STRING_LIST:
            if ( (strcmp (opt->name, SANE_NAME_SCAN_MODE) != 0) &&  /* do not show scanmode */
                 (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) != 0) ) /* do not show scansource */
            {
              /* use a "list-selection" widget */
              xsane_back_gtk_option_menu_new(parent, title, (char **) opt->constraint.string_list, buf,
                                  elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
              gtk_widget_show (parent->parent);
            }
           break;

          case SANE_CONSTRAINT_NONE:
            xsane_back_gtk_text_entry_new(parent, title, buf, elem, xsane.tooltips, _BGT(opt->desc), SANE_OPTION_IS_SETTABLE(opt->cap));
            gtk_widget_show (parent->parent);
           break;

          default:
            DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_STRING\n", i);
            DBG(DBG_error, "=> %s %d!\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
           break;
        }
        free(buf);
       break;

      case SANE_TYPE_BUTTON:
        if (!opt->name)
        {
          DBG(DBG_error, "xsane_panel_build: Option %d, type SANE_TYPE_BUTTON\n", i);
          DBG(DBG_error, "=> %s\n", ERR_OPTION_NAME_NULL);
          DBG(DBG_error, "=> %s\n", ERR_BACKEND_BUG);
         break;
        }

        button = gtk_button_new();
        g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_back_gtk_push_button_callback, elem);
        xsane_back_gtk_set_tooltip(xsane.tooltips, button, _BGT(opt->desc));

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
        DBG(DBG_error, "xsane_panel_build: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
       break;
    }
    continue;

    get_value_failed:
    {
      char msg[256];

      sprintf(msg, "%s %s: %s.", ERR_GET_OPTION, opt->name, XSANE_STRSTATUS(status));
      xsane_back_gtk_error(msg, TRUE);
    }
  }

  if ((xsane.well_known.dpi_x == -1) && (xsane.well_known.dpi_y != -1))
  {
    xsane.well_known.dpi_x = xsane.well_known.dpi;
  }

  xsane.xsane_hbox = xsane_update_xsane_callback();

  gtk_container_add(GTK_CONTAINER(xsane.xsane_window),    xsane.xsane_hbox);
  gtk_container_add(GTK_CONTAINER(xsane.standard_window), xsane.standard_hbox);
  gtk_container_add(GTK_CONTAINER(xsane.advanced_window), xsane.advanced_hbox);

  xsane_update_histogram(TRUE /* update raw */);
  xsane_update_sliders();

#ifdef HAVE_WORKING_GTK_GAMMACURVE
  xsane_update_gamma_dialog();
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* connect to backend and create main dialogs:
 - sane_open
 - create dialog xsane.shell
 -   build menues
 - create dialog xsane.standard_options_shell
 - create dialog xsane.advanced_options_shell
 - create tooltip style
 - create dialog xsane.histogram_dialog
 - create dialog xsane.gamma_dialog
 - create dialog xsane.batch_scan
 - panel_build()
 - create dialog xsane.preview
*/

static void xsane_device_dialog(void)
{
 GtkWidget *vbox, *hbox, *button, *frame, *infobox;
 GtkWidget *menubar, *menubar_item;
 GtkStyle *current_style;
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
 SANE_Status status;

  DBG(DBG_proc, "xsane_device_dialog\n");

  devname = xsane.devlist[xsane.selected_dev]->name;

  status = sane_open(devname, (SANE_Handle *) &xsane.dev);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s `%s':\n %s.", ERR_DEVICE_OPEN_FAILED, devname, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
    xsane_exit();
    /* will never come to here */
  }

  if (sane_control_option(xsane.dev, 0, SANE_ACTION_GET_VALUE, &xsane.num_elements, 0) != SANE_STATUS_GOOD)
  {
    xsane_back_gtk_error(ERR_OPTION_COUNT, TRUE);
    sane_close(xsane.dev);
    xsane_exit();
    /* will never come to here */
  }

  snprintf(buf, sizeof(buf), "%s", xsane.devlist[xsane.selected_dev]->name);	/* generate "sane-BACKENDNAME" */
  textptr = strrchr(buf, ':'); /* format is midend:midend:midend:backend:device or backend:device */
  if (textptr)
  {
    *textptr = 0; /* erase ":device" at end of text */
    textptr  = strrchr(buf, ':');
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
    DBG(DBG_info, "Setting backend name \"%s\"\n", xsane.backend);

    xsane.backend_translation = xsane.backend;
    bindtextdomain(xsane.backend_translation, STRINGIFY(SANELOCALEDIR)); /* set path for backend translation texts */
#ifdef HAVE_GTK2
    bind_textdomain_codeset(xsane.backend_translation, "UTF-8");
#endif

    if (!strlen(dgettext(xsane.backend_translation, ""))) /* translation not valid, use general translation table */
    {
      xsane.backend_translation = "sane-backends";
      DBG(DBG_info, "Setting general translation table \"sane-backends\" with localedir: %s\n", STRINGIFY(LOCALEDIR));
    }
    else /* we have a valid table for the backend */
    {
      DBG(DBG_info, "Setting backend translation table \"%s\" with localedir: %s\n", xsane.backend_translation, STRINGIFY(LOCALEDIR));
    }
  }
  else /* use general backend name "sane-backends" for sane */
  {
    xsane.backend = strdup("sane-backends");
    DBG(DBG_info, "Setting general backend name \"%s\"\n", xsane.backend);

    xsane.backend_translation = xsane.backend;
    DBG(DBG_info, "Setting general translation table \"sane-backends\" with localedir: %s\n", STRINGIFY(LOCALEDIR));
  }

  bindtextdomain("sane-backends", STRINGIFY(SANELOCALEDIR)); /* set path for backend translation texts */
#ifdef HAVE_GTK2
  bind_textdomain_codeset(xsane.backend_translation, "UTF-8");
#endif


  /* create device-text for window titles */

  snprintf(devicetext, sizeof(devicetext), "%s", xsane.devlist[xsane.selected_dev]->model);
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

  xsane.device_text = strdup(devicetext);


  /* if no preferences filename is given on commandline create one from devicenaname */
 
  if (!xsane.device_set_filename)
  {
    if (!strcmp(xsane.devlist[xsane.selected_dev]->vendor, TEXT_UNKNOWN))
    {
      snprintf(buf, sizeof(buf), "%s",  xsane.devlist[xsane.selected_dev]->name);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%s:%s", xsane.devlist[xsane.selected_dev]->vendor, xsane.devlist[xsane.selected_dev]->model);
    }
    xsane.device_set_filename = strdup(buf); /* set preferences filename */
  }

  if (xsane.main_window_fixed == -1) /* no command line option given */
  {
    xsane.main_window_fixed = preferences.main_window_fixed;
  }


  /* create the xsane dialog box */

  xsane.shell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_move(GTK_WINDOW(xsane.shell), XSANE_SHELL_POS_X, XSANE_SHELL_POS_Y);
  sprintf(windowname, "%s %s %s", xsane.prog_name, XSANE_VERSION, xsane.device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.shell), (char *) windowname);
  g_signal_connect(GTK_OBJECT(xsane.shell), "delete_event", GTK_SIGNAL_FUNC(xsane_scan_win_delete), NULL);

  xsane_set_window_icon(xsane.shell, 0);

  /* create the xsane main window accelerator table */
  xsane.accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(xsane.shell), xsane.accelerator_group); 


  /* set the main vbox */
  xsane_window = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_window), 0);
  gtk_container_add(GTK_CONTAINER(xsane.shell), xsane_window);
  gtk_widget_show(xsane_window);

  /* create the menubar */
  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(xsane_window), menubar, FALSE, FALSE, 0);

  /* "Files" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_FILE);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_files_build_menu());
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item);

  /* "Preferences" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_PREFERENCES);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_preferences_build_menu());
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_P, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item);

  /* "View" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_VIEW);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_view_build_menu());
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_V, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item);


  /* "Window" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_WINDOW);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_window_build_menu());
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_V, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item);


  /* "Help" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_HELP);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_right_justify((GtkMenuItem *) menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_help_build_menu());
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_H, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item);

  gtk_widget_show(menubar);
  xsane.menubar = menubar;


  if (xsane.main_window_fixed) /* fixed window: use it like it is */
  {
                                              /* shrink grow auto_shrink */
    gtk_window_set_resizable(GTK_WINDOW(xsane.shell), FALSE);

    xsane_vbox_main = gtk_vbox_new(TRUE, 5); /* we need this to set the wanted borders */
    gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
    gtk_container_add(GTK_CONTAINER(xsane_window), xsane_vbox_main);          
  }
  else /* scrolled window: create a scrolled window and put it into the xsane dialog box */
  {
    gtk_window_set_default_size(GTK_WINDOW(xsane.shell), XSANE_SHELL_WIDTH, XSANE_SHELL_HEIGHT); /* set default size */

                                              /* shrink grow auto_shrink */
    gtk_window_set_resizable(GTK_WINDOW(xsane.shell), TRUE); /* allow resizing */

    xsane.main_dialog_scrolled = gtk_scrolled_window_new(0, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(xsane_window), xsane.main_dialog_scrolled);
    gtk_widget_show(xsane.main_dialog_scrolled);

    xsane_vbox_main = gtk_vbox_new(TRUE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled), xsane_vbox_main);
  }

  /* create  a subwindow so the main dialog keeps its position on rebuilds: */
  xsane.xsane_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_main), xsane.xsane_window, TRUE, TRUE, 0);
  gtk_widget_show(xsane.xsane_window);

  gtk_widget_show(xsane_vbox_main);

#if 0
  /* add vendor`s logo */
  xsane_vendor_pixmap_new(xsane.shell->window, xsane_window);
#endif


  /* create the scanner standard options dialog box */

  xsane.standard_options_shell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_move(GTK_WINDOW(xsane.standard_options_shell), XSANE_STD_OPTIONS_POS_X, XSANE_STD_OPTIONS_POS_Y);
  sprintf(windowname, "%s %s", WINDOW_STANDARD_OPTIONS, xsane.device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.standard_options_shell), (char *) windowname);

  gtk_window_set_resizable(GTK_WINDOW(xsane.standard_options_shell), FALSE);
  g_signal_connect(GTK_OBJECT(xsane.standard_options_shell), "delete_event", GTK_SIGNAL_FUNC(xsane_standard_option_win_delete), NULL);

  xsane_set_window_icon(xsane.standard_options_shell, 0);
  gtk_window_add_accel_group(GTK_WINDOW(xsane.standard_options_shell), xsane.accelerator_group); 

  xsane_vbox_standard = gtk_vbox_new(FALSE, 5); /* has been TRUE before I added backend pixmap */
  gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_standard), 5);
  gtk_container_add(GTK_CONTAINER(xsane.standard_options_shell), xsane_vbox_standard);
  gtk_widget_show(xsane_vbox_standard);

  /* add vendor`s logo */
  xsane_vendor_pixmap_new(xsane.standard_options_shell->window, xsane_vbox_standard);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  xsane.standard_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_standard), xsane.standard_window, TRUE, TRUE, 0);
  gtk_widget_show(xsane.standard_window);


  /* create the scanner advanced options dialog box */

  xsane.advanced_options_shell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_move(GTK_WINDOW(xsane.advanced_options_shell), XSANE_ADV_OPTIONS_POS_X, XSANE_ADV_OPTIONS_POS_Y);
  sprintf(windowname, "%s %s",WINDOW_ADVANCED_OPTIONS, xsane.device_text);
  gtk_window_set_title(GTK_WINDOW(xsane.advanced_options_shell), (char *) windowname);

  gtk_window_set_resizable(GTK_WINDOW(xsane.advanced_options_shell), FALSE);
  g_signal_connect(GTK_OBJECT(xsane.advanced_options_shell), "delete_event", GTK_SIGNAL_FUNC(xsane_advanced_option_win_delete), NULL);

  xsane_set_window_icon(xsane.advanced_options_shell, 0);
  gtk_window_add_accel_group(GTK_WINDOW(xsane.advanced_options_shell), xsane.accelerator_group); 

  xsane_vbox_advanced = gtk_vbox_new(FALSE, 5); /* has been TRUE before I added backend pixmap */
  gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_advanced), 5);
  gtk_container_add(GTK_CONTAINER(xsane.advanced_options_shell), xsane_vbox_advanced);
  gtk_widget_show(xsane_vbox_advanced);

  /* add vendor´s logo */
  xsane_vendor_pixmap_new(xsane.advanced_options_shell->window, xsane_vbox_advanced);

  /* create  a subwindow so the advanced dialog keeps its position on rebuilds: */
  xsane.advanced_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_advanced), xsane.advanced_window, TRUE, TRUE, 0);
  gtk_widget_show(xsane.advanced_window);


  /* fill in dialog structure */

  xsane.dev_name = strdup(devname);
  xsane.element  = malloc(xsane.num_elements * sizeof(xsane.element[0]));
  memset(xsane.element, 0, xsane.num_elements * sizeof(xsane.element[0]));


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

  xsane.tooltips = gtk_tooltips_new();
  colormap = gdk_drawable_get_colormap(xsane.shell->window);

  /* use black as foreground: */
  xsane.tooltips_fg.red   = 0;
  xsane.tooltips_fg.green = 0;
  xsane.tooltips_fg.blue  = 0;
  gdk_color_alloc(colormap, &xsane.tooltips_fg);

  /* postit yellow (khaki) as background: */
  xsane.tooltips_bg.red   = 61669;
  xsane.tooltips_bg.green = 59113;
  xsane.tooltips_bg.blue  = 35979;
  gdk_color_alloc(colormap, &xsane.tooltips_bg);

  gtk_tooltips_force_window(xsane.tooltips);

  current_style = gtk_style_copy(gtk_widget_get_style(xsane.tooltips->tip_window));
  current_style->bg[GTK_STATE_NORMAL] = xsane.tooltips_bg;
  current_style->fg[GTK_STATE_NORMAL] = xsane.tooltips_fg;
  gtk_widget_set_style(xsane.tooltips->tip_window, current_style);  

  xsane_back_gtk_set_tooltips(preferences.tooltips_enabled);


  /* create histogram dialog and set colors */
  xsane_create_histogram_dialog(xsane.device_text); /* create the histogram dialog */

#ifdef HAVE_WORKING_GTK_GAMMACURVE
  /* create gamma dialog and set colors */
  xsane_create_gamma_dialog(xsane.device_text); /* create the free gamma curve dialog */
#endif

  /* create batch_scan dialog */
  xsane_create_batch_scan_dialog(xsane.device_text);

  /* The bottom area: info frame, progress bar, start and cancel button */
  hbox = gtk_hbox_new(FALSE, 3);
  gtk_box_pack_end(GTK_BOX(xsane_window), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 3);
  gtk_widget_show(hbox);


  /* vertical box for info frame and progress bar */
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
  gtk_widget_show(vbox);


  /* Info frame */
  frame = gtk_frame_new(0);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show(frame);

  infobox = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(infobox), 4);
  gtk_container_add(GTK_CONTAINER(frame), infobox);
  gtk_widget_show(infobox);

  xsane.info_label = gtk_label_new(TEXT_INFO_BOX);
  gtk_box_pack_start(GTK_BOX(infobox), xsane.info_label, TRUE, TRUE, 0);
  gtk_widget_show(xsane.info_label);

  /* progress bar */
  xsane.progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), (GtkWidget *) xsane.progress_bar, TRUE, TRUE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.progress_bar));


  /* vertical box for scan and cancel button */
  vbox = gtk_vbox_new(FALSE, 3);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  /* The Scan button */
  button = gtk_button_new_with_label(BUTTON_SCAN);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SCAN_START);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_Return, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_scan_callback, NULL);
  gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  xsane.start_button = GTK_OBJECT(button);

  /* The Cancel button */
#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SCAN_CANCEL);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_Escape, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
  gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE); 
  xsane.cancel_button = GTK_OBJECT(button);


  /* create backend dependend options */
  xsane_panel_build();


  /* create preview dialog */
  xsane.preview = preview_new();
  g_signal_connect(GTK_OBJECT(xsane.preview->top), "delete_event", GTK_SIGNAL_FUNC(xsane_preview_window_destroyed), NULL);

  xsane_device_preferences_restore();	/* restore device-settings */
  xsane_set_modus_defaults();
  xsane_update_param(0);
  xsane_update_gamma_curve(TRUE);

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

  if (preferences.show_batch_scan)
  {
    gtk_widget_show(xsane.batch_scan_dialog);
  }

  gtk_widget_show(xsane.shell); /* call as last so focus is on it */

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_update_sliders();

  if (xsane.show_preview)
  {
    gtk_widget_show(xsane.preview->top);
  }
  else
  {
    gtk_widget_hide(xsane.preview->top);
  }
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_preview_widget), xsane.show_preview);

  xsane_define_maximum_output_size(); /* draw maximum output frame in preview window if necessary */
  xsane_refresh_dialog();
  xsane_set_all_resolutions(); /* make sure resolution, resolution_x and resolution_y are up to date */

  if (xsane.batch_scan_load_default_list)
  {
   char filename[PATH_MAX];

    DBG(DBG_proc, "batch_scan:load default list\n");
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", "batch-lists", 0, "default", ".xbl", XSANE_PATH_LOCAL_SANE);
    xsane_batch_scan_load_list_from_file(filename);

    xsane.batch_scan_load_default_list = 0; /* mark list is loaded, we only want to load the list at program startup */
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_choose_dialog_ok_callback(void)
{
  DBG(DBG_proc, "xsane_choose_dialog_ok_callback\n");

  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.choose_device_dialog), GTK_SIGNAL_FUNC(xsane_exit), 0);
  gtk_widget_destroy(xsane.choose_device_dialog);
  xsane_device_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_select_device_by_key_callback(GtkWidget * widget, gpointer data)
{
  DBG(DBG_proc, "xsane_select_device_by_key_callback\n");

  xsane.selected_dev = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_select_device_by_mouse_callback(GtkWidget * widget, GdkEventButton *event, gpointer data)
{
  DBG(DBG_proc, "xsane_select_device_by_mouse_callback\n");

  xsane.selected_dev = (int) data;
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    xsane_choose_dialog_ok_callback();
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_choose_device(void)
{
 GtkWidget *main_vbox, *vbox, *hbox, *button, *device_frame, *device_vbox, *pixmapwidget, *label;
 GdkBitmap *mask = NULL;
 GdkPixmap *pixmap = NULL;
 GtkStyle *style;
 GdkColor *bg_trans;
 GSList *owner;
 GtkAccelGroup *device_selection_accelerator_group;
 gint i;
 const SANE_Device *adev;
 char buf[256];
 char vendor[12];
 char model[17];
 char type[20];
 char filename[PATH_MAX];
 int j;
 char *xsane_default_device = NULL;
 int ndevs;

#define TEXT_NO_VENDOR "no vendor\0"
#define TEXT_NO_MODEL  "no model\0"
#define TEXT_NO_TYPE   "no type\0"

  DBG(DBG_proc, "xsane_choose_device\n");

  xsane_default_device = getenv(XSANE_DEFAULT_DEVICE);
  if (xsane_default_device)
  {
    for (ndevs = 0; xsane.devlist[ndevs]; ++ndevs)
    {
      if (!strncmp(xsane.devlist[ndevs]->name, xsane_default_device, strlen(xsane_default_device)))
      {
        xsane.selected_dev = ndevs;
        break; 
      }
    }
  }

  xsane.choose_device_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(xsane.choose_device_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(xsane.choose_device_dialog), FALSE);
  g_signal_connect(GTK_OBJECT(xsane.choose_device_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_exit), NULL);
  snprintf(buf, sizeof(buf), "%s %s %s", xsane.prog_name, XSANE_VERSION, WINDOW_DEVICE_SELECTION);
  gtk_window_set_title(GTK_WINDOW(xsane.choose_device_dialog), buf);

  device_selection_accelerator_group = gtk_accel_group_new(); /* do we have to delete it when dialog is closed ? */
  gtk_window_add_accel_group(GTK_WINDOW(xsane.choose_device_dialog), device_selection_accelerator_group); 

  main_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 0);
  gtk_container_add(GTK_CONTAINER(xsane.choose_device_dialog), main_vbox);
  gtk_widget_show(main_vbox);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
  gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(xsane.choose_device_dialog);

  style = gtk_widget_get_style(xsane.choose_device_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-logo", 0, ".xpm", XSANE_PATH_SYSTEM);
  pixmap = gdk_pixmap_create_from_xpm(xsane.choose_device_dialog->window, &mask, bg_trans, filename);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);

  xsane_set_window_icon(xsane.choose_device_dialog, (gchar **) 0);

  snprintf(buf, sizeof(buf), "%s %s\n", XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT);
  label = gtk_label_new(buf);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);


  /* list the drivers with radiobuttons */
  device_frame = gtk_frame_new(TEXT_AVAILABLE_DEVICES);
  gtk_container_set_border_width(GTK_CONTAINER(device_frame), 4);
  gtk_box_pack_start(GTK_BOX(vbox), device_frame, FALSE, FALSE, 2);
  gtk_widget_show(device_frame);

  device_vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(device_vbox), 3);
  gtk_container_add(GTK_CONTAINER(device_frame), device_vbox);

  owner = 0;
  for (i = 0; i < xsane.num_of_devs; i++)
  {
    adev = xsane.devlist[i];

    if (adev->vendor)
    {
      strncpy(vendor, adev->vendor, sizeof(vendor)-1);
    }
    else
    {
      strncpy(vendor, TEXT_NO_VENDOR, sizeof(vendor)-1);
    }

    vendor[sizeof(vendor)-1] = 0;
    for (j = strlen(vendor); j < sizeof(vendor)-1; j++)
    {
      vendor[j] = ' ';
    }

    if (adev->model)
    {
      strncpy(model, adev->model, sizeof(model)-1);
    }
    else
    {
      strncpy(model, TEXT_NO_MODEL, sizeof(model)-1);
    }

    model[sizeof(model)-1] = 0;
    for (j = strlen(model); j < sizeof(model)-1; j++)
    {
      model[j] = ' ';
    }

    if (adev->type)
    {
      strncpy(type, _(adev->type), sizeof(type)-1); /* allow translation of device type */
    }
    else
    {
      strncpy(type, TEXT_NO_TYPE, sizeof(type)-1);
    }

    type[sizeof(type)-1] = 0;
    for (j = strlen(type); j < sizeof(type)-1; j++)
    {
      type[j] = ' ';
    }

    snprintf(buf, sizeof(buf), "%s %s %s [%s]", vendor, model, type, adev->name);
    button = gtk_radio_button_new_with_label(owner, (char *) buf);

    if (i<12)
    {
      gtk_widget_add_accelerator(button, "clicked", device_selection_accelerator_group, GDK_F1+i, 0, DEF_GTK_ACCEL_LOCKED);
    }

    g_signal_connect(GTK_OBJECT(button), "button_press_event", (GtkSignalFunc) xsane_select_device_by_mouse_callback, (void *) (long) i);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_select_device_by_key_callback, (void *) (long) i);
    gtk_box_pack_start(GTK_BOX(device_vbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);
    owner = gtk_radio_button_group(GTK_RADIO_BUTTON(button));;

    if (i == xsane.selected_dev)
    {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (button), TRUE);
    }
  }
  gtk_widget_show(device_vbox);

  /* The bottom row of buttons */
  hbox = gtk_hbox_new(FALSE, 5);
  xsane_separator_new(main_vbox, 5);
  gtk_box_pack_end(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);

  /* The OK button */
#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
  button = gtk_button_new_with_label(BUTTON_OK);
#endif
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_choose_dialog_ok_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  /* The Cancel button */
#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
  gtk_widget_add_accelerator(button, "clicked", device_selection_accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_exit, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(xsane.choose_device_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_usage(void)
{
  g_print("XSane %s %s\n", TEXT_VERSION, XSANE_VERSION);
  g_print("%s %s\n\n", XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT);
  g_print("%s %s %s\n\n", TEXT_USAGE, xsane.prog_name, TEXT_USAGE_OPTIONS);
  g_print("%s\n\n", TEXT_HELP);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_init(int argc, char **argv)
/* returns 0 - if ok
           1 - if license was not accepted
           2 - if canceld because xsane was started as root
           3 - if wrong sane major version was found */
{
 GtkWidget *device_scanning_dialog;
 GtkWidget *main_vbox, *hbox;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 GtkWidget *pixmapwidget;
 GtkWidget *label;
 GtkWidget *frame;
 struct stat st;
 char filename[PATH_MAX];
 char buf[256];

  DBG(DBG_proc, "xsane_init\n");

#ifndef _WIN32
  gtk_set_locale();
#endif
  gtk_init(&argc, &argv);
  setlocale(LC_NUMERIC, "C");

#ifdef HAVE_ANY_GIMP
  gtk_rc_parse(gimp_gtkrc());

# ifdef HAVE_GIMP_2
  gdk_set_use_xshm(TRUE);
# else 
  gdk_set_use_xshm(gimp_use_xshm());
# endif

#endif

  /* before we open any windows we have to read the style file */
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-style", 0, ".rc", XSANE_PATH_LOCAL_SANE);
  if (stat(filename, &st) >= 0)
  {
    DBG(DBG_info, "loading %s\n", filename);
    gtk_rc_parse(filename);
  }
  else /* no local xsane-style.rc, look for system file */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-style", 0, ".rc", XSANE_PATH_SYSTEM);
    if (stat(filename, &st) >= 0)
    {
      DBG(DBG_info, "loading %s\n", filename);
      gtk_rc_parse(filename);
    }
  }

  if (argc > 1)
  {
    int ch;

    while((ch = getopt_long(argc, argv, "cd:fghlmnpsvFN:RV", long_options, 0)) != EOF)
    {
      switch(ch)
      {
        case 'g': /* This options is set when xsane is called from the */
                  /* GIMP. If xsane is compiled without GIMP support */
                  /* then you get the error message when GIMP does */
                  /* query or tries to start the xsane plugin! */
#ifndef HAVE_ANY_GIMP
          g_print("%s: %s\n", argv[0], ERR_GIMP_SUPPORT_MISSING);
          exit(0);
#endif
         break;

        case 'v': /* --version */
          g_print("%s-%s %s %s\n", xsane.prog_name, XSANE_VERSION, XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT);
          g_print("  %s %s\n", TEXT_EMAIL, XSANE_EMAIL);
          g_print("  %s %s\n", TEXT_PACKAGE, XSANE_PACKAGE_VERSION);
          g_print("  %s%d.%d.%d\n", TEXT_GTK_VERSION, GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);

#ifdef HAVE_ANY_GIMP
          g_print("  %s, %s%s\n", TEXT_WITH_GIMP_SUPPORT, TEXT_GIMP_VERSION, GIMP_VERSION);
#else
          g_print("  %s\n", TEXT_WITHOUT_GIMP_SUPPORT);
#endif

          g_print("  %s ", TEXT_OUTPUT_FORMATS);

#ifdef HAVE_LIBJPEG
          g_print("jpeg, ");
#endif

#ifdef HAVE_LIBZ
          g_print("pdf(compr.), ");
#else
          g_print("pdf, ");
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
          g_print("png, ");
#endif
#endif

          g_print("pnm, ");
#ifdef HAVE_LIBZ
          g_print("ps(compr.)");
#else
          g_print("ps");
#endif

#ifdef SUPPORT_RGBA
          g_print(", rgba");
#endif

#ifdef HAVE_LIBTIFF
          g_print(", tiff");
#endif

          g_print(", txt");

          g_print("\n");
          exit(0);
         break;

        case 'l': /* --license */
          g_print("%s-%s %s %s\n\n", xsane.prog_name, XSANE_VERSION, XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT);
          g_print("%s\n", TEXT_GPL);
          exit(0);
         break;

        case 'd': /* --device-settings */
          xsane.device_set_filename = strdup(optarg);
         break;

        case 'V': /* --viewer, default */
          xsane.xsane_mode = XSANE_VIEWER;
         break;

        case 's': /* --save */
          xsane.xsane_mode = XSANE_SAVE;
         break;

        case 'c': /* --copy */
          xsane.xsane_mode = XSANE_COPY;
         break;

        case 'f': /* --fax */
          xsane.xsane_mode = XSANE_FAX;
         break;

        case 'm': /* --mail */
          xsane.xsane_mode = XSANE_MAIL;
         break;

        case 'n': /* --No-mode-selection */
           xsane.mode_selection = 0;
         break;

        case 'F': /* --Fixed */
           xsane.main_window_fixed = 1;
         break;

        case 'R': /* --Resizeable */
           xsane.main_window_fixed = 0;
         break;

        case 'N': /* --No-filenameselection filename */
           xsane.force_filename = TRUE;
           xsane.external_filename  = strdup(optarg);
         break;

        case 'p': /* --print-filenames */
           xsane.print_filenames = TRUE;
         break;

        case 'h': /* --help */
        default:
          xsane_usage();
          exit(0);
      }
    }
  }

  if (xsane_pref_restore()) /* restore preferences, returns TRUE if license is not accpted yet */
  {
    if (xsane_display_eula(1)) /* show license and ask for accept/not accept */
    {
      DBG(DBG_info, "user did not accept eula, we abort\n");
      return 1; /* User did not accept eula */
    }
  }

  xsane_pref_restore_media();

#ifndef HAVE_OS2_H
  if (!getuid()) /* root ? */
  {
    if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, WARN_XSANE_AS_ROOT,
        BUTTON_CANCEL, BUTTON_CONT_AT_OWN_RISK, TRUE /* wait */) == TRUE)
    {
      return 2; /* User selected CANCEL */
    } 
  }
#endif

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);

  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    DBG(DBG_error0, "\n\n"
                    "%s %s:\n"
                    "  %s\n"
                    "  %s %d\n"
                    "  %s %d\n"
                    "%s\n\n",
                    xsane.prog_name, ERR_ERROR,
                    ERR_MAJOR_VERSION_NR_CONFLICT,
                    ERR_XSANE_MAJOR_VERSION, SANE_V_MAJOR,
                    ERR_BACKEND_MAJOR_VERSION, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                    ERR_PROGRAM_ABORTED);
    return 3;
  }

  device_scanning_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(device_scanning_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(device_scanning_dialog), FALSE);
  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, XSANE_VERSION);
  gtk_window_set_title(GTK_WINDOW(device_scanning_dialog), buf);
  g_signal_connect(GTK_OBJECT(device_scanning_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_quit), NULL);

  xsane_set_window_icon(device_scanning_dialog, 0);

  frame = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(device_scanning_dialog), frame);
  gtk_widget_show(frame);

  main_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);
  gtk_container_add(GTK_CONTAINER(frame), main_vbox);
  gtk_widget_show(main_vbox);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  /* add device icon */
  pixmap = gdk_pixmap_create_from_xpm_d(device_scanning_dialog->window, &mask, xsane.bg_trans, (gchar **) device_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 10);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);

  /* add text */
  snprintf(buf, sizeof(buf), "  %s  ", TEXT_SCANNING_DEVICES);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);
  
  gtk_widget_show(device_scanning_dialog);

  /* wait 100 ms to make sure window is displayed */
  usleep(100000); /* this makes sure that the text "scanning for devices" is displayed */

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_widget_test_uposition(device_scanning_dialog);

  /* wait 100 ms to make sure window is displayed */
  usleep(100000); /* this makes sure that the text "scanning for devices" is displayed */

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  sane_get_devices(&xsane.devlist, SANE_FALSE /* local and network devices */);


  gtk_widget_destroy(device_scanning_dialog);

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  /* if devicename is given try to identify it, if not found, open device list */
  if (optind < argc) 
  {
   int ndevs;

    for (ndevs = 0; xsane.devlist[ndevs]; ++ndevs)
    {
      if (!strncmp(xsane.devlist[ndevs]->name, argv[argc - 1], strlen(argv[argc - 1])))
      {
        xsane.selected_dev = ndevs;
        break; 
      }
    }

    if ((xsane.selected_dev < 0) && (argc > 1))
    {
     static SANE_Device dev;
     static const SANE_Device *device_list[] = { &dev, 0 };

      memset(&dev, 0, sizeof(dev));
      dev.name   = argv[argc - 1];
      dev.vendor = TEXT_UNKNOWN;
      dev.type   = TEXT_UNKNOWN;
      dev.model  = TEXT_UNKNOWN;

      xsane.devlist = device_list;
      xsane.selected_dev = 0;
    }
  }

 return 0; /* everything is ok */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_help_no_devices(void)
{
 char buf[512];

  snprintf(buf, sizeof(buf), "%s\n\n%s", ERR_NO_DEVICES, HELP_NO_DEVICES);
  xsane_back_gtk_decision(WINDOW_NO_DEVICES, (gchar**) no_device_xpm, buf, BUTTON_CLOSE, NULL, TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_interface(int argc, char **argv)
{
 struct SIGACTION act;

  DBG(DBG_proc, "xsane_interface\n");

  xsane.info_label = NULL;

  if (xsane_init(argc, argv)) /* initialize xsane variables if command line option is given, set xsane.selected_dev */
  {
    return; /* we have to abort (license not accepted, aborted because xsane runs as root) */
  }

  for (xsane.num_of_devs = 0; xsane.devlist[xsane.num_of_devs]; ++xsane.num_of_devs); /* count available devices */

  if (xsane.selected_dev >= 0) /* device name is given on cammand line */
  {
    xsane_device_dialog(); /* open device xsane.selected_dev */
  }
  else /* no device name given on command line */
  {
    if (xsane.num_of_devs > 0) /* devices available */
    {
      xsane.selected_dev = 0;
      if (xsane.num_of_devs == 1)
      {
        xsane_device_dialog(); /* open device xsane.selected_dev */
      }
      else
      {
        xsane_choose_device(); /* open device selection window and get device */
      }
    }
    else /* xsane.num_of_devs == 0, no devices available */
    {
     char buf[256];

      snprintf(buf, sizeof(buf), "%s\n", ERR_NO_DEVICES);

      if (xsane_back_gtk_decision(WINDOW_NO_DEVICES, (gchar**) no_device_xpm, buf, BUTTON_HELP, BUTTON_CLOSE, TRUE))
      {
        xsane_help_no_devices();
      }
      xsane_exit();
    }
  }

  /* define SIGTERM, SIGINT, SIGHUP-handler to make sure that e.g. all temporary files are deleted */
  /* when xsane gets such a signal */
  memset(&act, 0, sizeof(act));
  act.sa_handler = xsane_quit_handler;
  sigaction(SIGTERM, &act, 0);
  sigaction(SIGINT,  &act, 0);
  sigaction(SIGHUP,  &act, 0);

  /* add a signal handler that cleans up zombie child processes */
  memset(&act, 0, sizeof(act));
  act.sa_handler = xsane_sigchld_handler;
  sigaction(SIGCHLD, &act, 0);

  gtk_main();
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  DBG_init();

  DBG(DBG_error, "This is xsane version %s\n", XSANE_VERSION);

  memset(&xsane, 0, sizeof(xsane)); /* set all values in xsane to 0 */

  umask(XSANE_DEFAULT_UMASK); /* define permissions of new files */ 

  xsane.selected_dev        = -1;	/* no selected device */

  xsane.sensitivity         = TRUE;

  xsane.main_window_fixed   = -1; /* no command line option given, use preferences or fixed */

  xsane.mode                = XSANE_STANDALONE;
  xsane.xsane_mode          = -1;
  xsane.lineart_mode        = XSANE_LINEART_STANDARD;
  xsane.xsane_output_format = XSANE_PNM;
  xsane.mode_selection      = 1; /* enable selection of xsane mode */

  xsane.input_tag           = -1; /* no input tag */

  xsane.histogram_lines  = 1;

  xsane.zoom             = 1.0;
  xsane.zoom_x           = 1.0;
  xsane.zoom_y           = 1.0;
  xsane.resolution       = 72.0;
  xsane.resolution_x     = 72.0;
  xsane.resolution_y     = 72.0;
  xsane.copy_number      = 1;

  xsane.medium_shadow_gray     = 0.0;
  xsane.medium_shadow_red      = 0.0;
  xsane.medium_shadow_green    = 0.0;
  xsane.medium_shadow_blue     = 0.0;
  xsane.medium_highlight_gray  = 100.0;
  xsane.medium_highlight_red   = 100.0;
  xsane.medium_highlight_green = 100.0;
  xsane.medium_highlight_blue  = 100.0;
  xsane.medium_gamma_gray      = 1.0;
  xsane.medium_gamma_red       = 1.0;
  xsane.medium_gamma_green     = 1.0;
  xsane.medium_gamma_blue      = 1.0;
  xsane.medium_negative        = 0;
  xsane.medium_changed         = FALSE;

  xsane.brightness_min    = XSANE_BRIGHTNESS_MIN;
  xsane.brightness_max    = XSANE_BRIGHTNESS_MAX;
  xsane.contrast_gray_min = XSANE_CONTRAST_GRAY_MIN;
  xsane.contrast_min      = XSANE_CONTRAST_MIN;
  xsane.contrast_max      = XSANE_CONTRAST_MAX;

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
  xsane.threshold        = 50.0;

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

  xsane.xsane_colors            = -1; /* unused value to make sure that change of this vlaue is detected */
  xsane.scanner_gamma_color     = FALSE;
  xsane.scanner_gamma_gray      = FALSE;
  xsane.enhancement_rgb_default = TRUE;

  xsane.adf_page_counter = 0;
  xsane.print_filenames  = FALSE;
  xsane.force_filename   = FALSE;

  xsane.batch_scan_load_default_list = TRUE; /* load default batch scan list at program startup */

  xsane.prog_name = strrchr(argv[0], '/');
  if (xsane.prog_name)
  {
    ++xsane.prog_name;
  }
  else
  {
    xsane.prog_name = argv[0];
  }

  if (!pipe(xsane.ipc_pipefd)) /* success */
  {
    DBG(DBG_info, "created ipc_pipefd for inter progress communication\n");
#ifndef BUGGY_GDK_INPUT_EXCEPTION
    gdk_input_add(xsane.ipc_pipefd[0], GDK_INPUT_READ | GDK_INPUT_EXCEPTION, xsane_back_gtk_ipc_dialog_callback, 0);
#endif
  }
  else
  {
    DBG(DBG_info, "could not create pipe for inter progress communication\n");
    xsane.ipc_pipefd[0] = 0;
    xsane.ipc_pipefd[1] = 0;
  }

#if 0
  bindtextdomain(PACKAGE, STRINGIFY(LOCALEDIR));
  textdomain(PACKAGE);         
#ifdef HAVE_GTK2
  bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif
#else
  DBG(DBG_info, "Setting xsane translation table with localedir: %s\n", STRINGIFY(LOCALEDIR));
  bindtextdomain(xsane.prog_name, STRINGIFY(LOCALEDIR));
  textdomain(xsane.prog_name);
#ifdef HAVE_GTK2
  bind_textdomain_codeset(xsane.prog_name, "UTF-8");
#endif
#endif

#ifdef HAVE_ANY_GIMP
  {
    GPrintFunc old_print_func;
    GPrintFunc old_printerr_func;
    int result;

    /* Temporarily install a print function that discards all output.
       This is to avoid annoying "you must run this program under
       gimp" messages when xsane gets invoked in stand-alone
       mode.  */
    old_print_func    = g_set_print_handler((GPrintFunc) null_print_func);
    old_printerr_func = g_set_printerr_handler((GPrintFunc) null_print_func);

#ifdef _WIN32 
    /* don`t know why, but win32 does need this */
    set_gimp_PLUG_IN_INFO_PTR(&PLUG_IN_INFO);
#endif

#ifdef HAVE_OS2_H
    /* don`t know why, but os2 does need this one, a bit different to WIN32 */
    set_gimp_PLUG_IN_INFO(&PLUG_IN_INFO);
#endif

#ifdef HAVE_GIMP_2
    /* gimp_main() returns 1 if xsane wasn't invoked by GIMP */
    result = gimp_main(&PLUG_IN_INFO, argc, argv);
#else
    /* gimp_main() returns 1 if xsane wasn't invoked by GIMP */
    result = gimp_main(argc, argv);
#endif

#if 0
    /* this is the old version that seems to use the compatibility functions */
    g_set_message_handler(old_print_func);
    g_set_error_handler(old_printerr_func);
#else
    /* this is the new version that I think is the one that should be used */
    g_set_print_handler(old_print_func);
    g_set_printerr_handler(old_printerr_func);
#endif

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
