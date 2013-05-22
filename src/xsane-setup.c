/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-setup.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2010 Oliver Rauch
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
#include "xsane-device-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-batch-scan.h"
#include "xsane-setup.h"

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
static int xsane_update_double(GtkWidget *widget, double *val);
static void xsane_setup_printer_update(void);
static void xsane_setup_printer_callback(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_menu_build(GtkWidget *option_menu);
static void xsane_setup_printer_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_new(GtkWidget *widget, gpointer data);
static void xsane_setup_printer_delete(GtkWidget *widget, gpointer data);
static void xsane_setup_display_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_image_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_fax_apply_changes(GtkWidget *widget, gpointer data);
#ifdef XSANE_ACTIVATE_EMAIL
static void xsane_setup_email_apply_changes(GtkWidget *widget, gpointer data);
#endif
static void xsane_setup_ocr_apply_changes(GtkWidget *widget, gpointer data);
static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data);

static void xsane_printer_notebook(GtkWidget *notebook);
static void xsane_saving_notebook(GtkWidget *notebook);
static void xsane_filetype_notebook(GtkWidget *notebook);
static void xsane_fax_notebook(GtkWidget *notebook);
#ifdef XSANE_ACTIVATE_EMAIL
static void xsane_email_notebook(GtkWidget *notebook);
#endif
static void xsane_display_notebook(GtkWidget *notebook);
static void xsane_enhance_notebook_sensitivity(int lineart_mode);
static void xsane_setup_authentication_type_callback(GtkWidget *widget, gpointer data);
static void xsane_setup_show_range_mode_callback(GtkWidget *widget, gpointer data);
static void xsane_setup_lineart_mode_callback(GtkWidget *widget, gpointer data);
static void xsane_enhance_notebook(GtkWidget *notebook);
#ifdef HAVE_LIBLCMS
static void xsane_color_management_notebook(GtkWidget *notebook);
#endif

void xsane_setup_dialog(GtkWidget *widget, gpointer data);

/* ---------------------------------------------------------------------------------------------------------------------- */

static int device_options_changed = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */


void xsane_new_printer(void)
{
 void *newprinters;

  DBG(DBG_proc, "xsane_new_printer\n");

  newprinters = realloc(preferences.printer, (preferences.printerdefinitions+1) * sizeof(void *));

  if (newprinters) /* realloc returns NULL if failed, in this case the old memory keeps alive */
  {
    preferences.printer = newprinters;

    preferences.printer[preferences.printerdefinitions] = calloc(sizeof(Preferences_printer_t), 1);

    if (preferences.printer[preferences.printerdefinitions])
    {
      preferences.printernr = preferences.printerdefinitions;
      preferences.printerdefinitions++;

      preferences.printer[preferences.printernr]->name               = strdup(PRINTERNAME);
      preferences.printer[preferences.printernr]->command            = strdup(PRINTERCOMMAND);
      preferences.printer[preferences.printernr]->copy_number_option = strdup(PRINTERCOPYNUMBEROPTION);
      preferences.printer[preferences.printernr]->lineart_resolution   = 300;
      preferences.printer[preferences.printernr]->grayscale_resolution = 150;
      preferences.printer[preferences.printernr]->color_resolution     = 150;
      preferences.printer[preferences.printernr]->width                = 203.2;
      preferences.printer[preferences.printernr]->height               = 294.6;
      preferences.printer[preferences.printernr]->leftoffset           = 3.5;
      preferences.printer[preferences.printernr]->bottomoffset         = 3.5;
      preferences.printer[preferences.printernr]->gamma                = 1.0;
      preferences.printer[preferences.printernr]->gamma_red            = 1.0;
      preferences.printer[preferences.printernr]->gamma_green          = 1.0;
      preferences.printer[preferences.printernr]->gamma_blue           = 1.0;
      preferences.printer[preferences.printernr]->icm_profile          = NULL;
      preferences.printer[preferences.printernr]->embed_csa            = 1;
      preferences.printer[preferences.printernr]->embed_crd            = 0;
      preferences.printer[preferences.printernr]->cms_bpc              = 0;
      preferences.printer[preferences.printernr]->ps_flatedecoded      = 1;
    }
    else
    {
      DBG(DBG_error, "could not allocate memory for new printer definition\n");
    }
  }
  else
  {
    DBG(DBG_error, "could not allocate memory for new printer definition\n");
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_int(GtkWidget *widget, int *val)
{
 const char *start;
 char *end;
 int v;

  DBG(DBG_proc, "xsane_update_init\n");

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
  DBG(DBG_proc, "xsane_update_bool\n");

  *val = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_scale(GtkWidget *widget, double *val)
{
  DBG(DBG_proc, "xsane_update_scale\n");

  *val = GTK_ADJUSTMENT(widget)->value;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_geometry_double(GtkWidget *widget, double *val, double length_multiplier)
{
 const char *start;
 char *end;
 double v;

  DBG(DBG_proc, "xsane_update_geometry_double\n");

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
  {
    return;
  }

  v = strtod(start, &end);
  if (end > start)
  {
    *val = v * length_multiplier;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* returns 0 if value is unchaned */
static int xsane_update_double(GtkWidget *widget, double *val)
{
 const char *start;
 char *end;
 double v;
 int value_changed = 0;

  DBG(DBG_proc, "xsane_update_double\n");

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
  {
    return 0;
  }

  v = strtod(start, &end);
  if (end > start)
  {
    value_changed = (fabs(*val - v) >= 0.001);
    *val = v;
  }

 return value_changed;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_update()
{
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_printer_update\n");

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_name_entry),   
                     (char *) preferences.printer[preferences.printernr]->name);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_command_entry),
                     (char *) preferences.printer[preferences.printernr]->command);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_copy_number_option_entry),
                     (char *) preferences.printer[preferences.printernr]->copy_number_option);

  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->lineart_resolution);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_lineart_resolution_entry), buf);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->grayscale_resolution);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_grayscale_resolution_entry), buf);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->color_resolution);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_color_resolution_entry), buf);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->width / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_width_entry), buf);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->height / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_height_entry), buf);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->leftoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_leftoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->bottomoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_bottomoffset_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_red_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_green_entry), buf);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_blue_entry), buf);

#ifdef HAVE_LIBLCMS
  if (preferences.printer[preferences.printernr]->icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_icm_profile_entry), (char *) preferences.printer[preferences.printernr]->icm_profile);
  }
  else
  {
    gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_icm_profile_entry), "");
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane_setup.printer_embed_csa_button), preferences.printer[preferences.printernr]->embed_csa);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane_setup.printer_embed_crd_button), preferences.printer[preferences.printernr]->embed_crd);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane_setup.printer_cms_bpc_button),   preferences.printer[preferences.printernr]->cms_bpc);
#endif

#ifdef HAVE_LIBZ
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane_setup.printer_ps_flatedecoded_button), preferences.printer[preferences.printernr]->ps_flatedecoded);
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_printer_callback\n");

  preferences.printernr = (int) data;
  xsane_setup_printer_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_menu_build(GtkWidget *option_menu)
{
 GtkWidget *printer_menu, *printer_item;
 int i;

  DBG(DBG_proc, "xsane_setup_printer_menu_build\n");

  printer_menu = gtk_menu_new();

  for (i=0; i < preferences.printerdefinitions; i++)
  {
    printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
    gtk_container_add(GTK_CONTAINER(printer_menu), printer_item);
    g_signal_connect(GTK_OBJECT(printer_item), "activate", (GtkSignalFunc) xsane_setup_printer_callback, (void *) i);
    gtk_widget_show(printer_item);
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), printer_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), preferences.printernr);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_apply_changes(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;

  DBG(DBG_proc, "xsane_setup_printer_apply_changes\n");

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

  xsane_update_int(xsane_setup.printer_lineart_resolution_entry,   &preferences.printer[preferences.printernr]->lineart_resolution);
  xsane_update_int(xsane_setup.printer_grayscale_resolution_entry, &preferences.printer[preferences.printernr]->grayscale_resolution);
  xsane_update_int(xsane_setup.printer_color_resolution_entry,     &preferences.printer[preferences.printernr]->color_resolution);

  xsane_update_geometry_double(xsane_setup.printer_width_entry,        &preferences.printer[preferences.printernr]->width,        preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.printer_height_entry,       &preferences.printer[preferences.printernr]->height,       preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.printer_leftoffset_entry,   &preferences.printer[preferences.printernr]->leftoffset,   preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.printer_bottomoffset_entry, &preferences.printer[preferences.printernr]->bottomoffset, preferences.length_unit);

  xsane_update_double(xsane_setup.printer_gamma_entry,       &preferences.printer[preferences.printernr]->gamma);
  xsane_update_double(xsane_setup.printer_gamma_red_entry,   &preferences.printer[preferences.printernr]->gamma_red);
  xsane_update_double(xsane_setup.printer_gamma_green_entry, &preferences.printer[preferences.printernr]->gamma_green);
  xsane_update_double(xsane_setup.printer_gamma_blue_entry,  &preferences.printer[preferences.printernr]->gamma_blue);

#ifdef HAVE_LIBLCMS
  if (preferences.printer[preferences.printernr]->icm_profile)
  {
    free(preferences.printer[preferences.printernr]->icm_profile);
  }
  preferences.printer[preferences.printernr]->icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_icm_profile_entry)));
  xsane_update_bool(xsane_setup.printer_embed_csa_button,  &preferences.printer[preferences.printernr]->embed_csa);
  xsane_update_bool(xsane_setup.printer_embed_crd_button,  &preferences.printer[preferences.printernr]->embed_crd);
  xsane_update_bool(xsane_setup.printer_cms_bpc_button,    &preferences.printer[preferences.printernr]->cms_bpc);
#endif

#ifdef HAVE_LIBZ
  xsane_update_bool(xsane_setup.printer_ps_flatedecoded_button,  &preferences.printer[preferences.printernr]->ps_flatedecoded);
#endif

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

  DBG(DBG_proc, "xsane_setup_printer_new\n");

  xsane_new_printer();
  xsane_setup_printer_update();

  xsane_setup_printer_menu_build(option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_delete(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;
 int i;

  DBG(DBG_proc, "xsane_setup_printer_delete\n");

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

static void xsane_setup_filename_counter_len_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_filename_counter_len_callback\n");

  xsane_setup.filename_counter_len = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBTIFF
static void xsane_setup_tiff_compression16_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_tiff_compression16_callback\n");

  xsane_setup.tiff_compression16_nr = (int) data;
}

/* -------------------------------------- */

static void xsane_setup_tiff_compression8_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_tiff_compression8_callback\n");

  xsane_setup.tiff_compression8_nr = (int) data;
}

/* -------------------------------------- */

static void xsane_setup_tiff_compression1_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_tiff_compression1_callback\n");
  xsane_setup.tiff_compression1_nr = (int) data;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_display_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_display_apply_changes\n");

  xsane_update_bool(xsane_setup.main_window_fixed_button,          &preferences.main_window_fixed);
  xsane_update_bool(xsane_setup.preview_own_cmap_button,           &preferences.preview_own_cmap);

  preferences.show_range_mode = xsane_setup.show_range_mode;

  xsane_update_double(xsane_setup.preview_gamma_entry,             &preferences.preview_gamma);
  xsane_update_double(xsane_setup.preview_gamma_red_entry,         &preferences.preview_gamma_red);
  xsane_update_double(xsane_setup.preview_gamma_green_entry,       &preferences.preview_gamma_green);
  xsane_update_double(xsane_setup.preview_gamma_blue_entry,        &preferences.preview_gamma_blue);
  xsane_update_bool(xsane_setup.disable_gimp_preview_gamma_button, &preferences.disable_gimp_preview_gamma);

  xsane_update_double(xsane_setup.preview_oversampling_entry,      &preferences.preview_oversampling);

  if (preferences.browser)
  {
    free((void *) preferences.browser);
  }
  preferences.browser = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.browser_entry)));

  xsane_update_gamma_curve(TRUE /* update raw */);

  xsane_batch_scan_update_icon_list(); /* update gamma of batch scan icons */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_enhance_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_enhance_apply_changes\n");

  device_options_changed |= (xsane.lineart_mode != xsane_setup.lineart_mode);
  xsane.lineart_mode = xsane_setup.lineart_mode;

  device_options_changed |= xsane_update_double(xsane_setup.preview_threshold_min_entry, &xsane.threshold_min);
  device_options_changed |= xsane_update_double(xsane_setup.preview_threshold_max_entry, &xsane.threshold_max);
  device_options_changed |= xsane_update_double(xsane_setup.preview_threshold_mul_entry, &xsane.threshold_mul);
  device_options_changed |= xsane_update_double(xsane_setup.preview_threshold_off_entry, &xsane.threshold_off);

  if (xsane.grayscale_scanmode)
  {
    if (xsane_setup.grayscale_scanmode)
    {
      device_options_changed |= (strcmp(xsane_setup.grayscale_scanmode, xsane.grayscale_scanmode));
    }
    else
    {
      device_options_changed |= 1;
    }

    free((void *) xsane.grayscale_scanmode);
    xsane.grayscale_scanmode = NULL;
  }
  else if (xsane_setup.grayscale_scanmode)
  {
    device_options_changed |= 1;
  }

  if (xsane_setup.grayscale_scanmode)
  {
    xsane.grayscale_scanmode = strdup(xsane_setup.grayscale_scanmode);
  }

  preferences.preview_pipette_range = xsane_setup.preview_pipette_range;

  xsane_update_bool(xsane_setup.auto_enhance_gamma_button,  &preferences.auto_enhance_gamma);
  xsane_update_bool(xsane_setup.preselect_scan_area_button,  &preferences.preselect_scan_area);
  xsane_update_bool(xsane_setup.auto_correct_colors_button, &preferences.auto_correct_colors);

  xsane_update_gamma_curve(TRUE /* update raw */);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
static void xsane_setup_color_management_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_colormagaement_apply_changes\n");

  preferences.cms_intent = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(gtk_option_menu_get_menu(GTK_OPTION_MENU(xsane_setup.cms_intent_option_menu))))), "Selection");
  xsane_update_bool(xsane_setup.cms_bpc_button, &preferences.cms_bpc);

  if (xsane.scanner_default_color_icm_profile)
  {
    free(xsane.scanner_default_color_icm_profile);
  }
  xsane.scanner_default_color_icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.scanner_default_color_icm_profile_entry)));

  if (xsane.scanner_default_gray_icm_profile)
  {
    free(xsane.scanner_default_gray_icm_profile);
  }
  xsane.scanner_default_gray_icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.scanner_default_gray_icm_profile_entry)));

  if (preferences.display_icm_profile)
  {
    free(preferences.display_icm_profile);
  }
  preferences.display_icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.display_icm_profile_entry)));

  if (preferences.custom_proofing_icm_profile)
  {
    free(preferences.custom_proofing_icm_profile);
  }
  preferences.custom_proofing_icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.custom_proofing_icm_profile_entry)));

  if (preferences.working_color_space_icm_profile)
  {
    free(preferences.working_color_space_icm_profile);
  }
  preferences.working_color_space_icm_profile = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.working_color_space_icm_profile_entry)));
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data)
{
 int level;

  DBG(DBG_proc, "xsane_setup_saving_apply_changes\n");

  preferences.filename_counter_len  = xsane_setup.filename_counter_len;

  if (strcmp(preferences.tmp_path, gtk_entry_get_text(GTK_ENTRY(xsane_setup.tmp_path_entry))))
  {
    for(level = 0; level <= 2; level++)
    {
      if (xsane.preview->filename[level])
      {
        remove(xsane.preview->filename[level]); /* remove existing preview files */
      }
    }

    if (preferences.tmp_path)
    {
      free((void *) preferences.tmp_path);
    }

    preferences.tmp_path = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.tmp_path_entry)));

    preview_generate_preview_filenames(xsane.preview);
  }

  xsane_update_bool(xsane_setup.save_devprefs_at_exit_button, &preferences.save_devprefs_at_exit);
  xsane_update_bool(xsane_setup.overwrite_warning_button,     &preferences.overwrite_warning);
  xsane_update_bool(xsane_setup.skip_existing_numbers_button, &preferences.skip_existing_numbers);

  preferences.image_umask     = 0777 - xsane_setup.image_permissions;
  preferences.directory_umask = 0777 - xsane_setup.directory_permissions;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_image_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_image_apply_changes\n");

#ifdef HAVE_LIBJPEG
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#else
#ifdef HAVE_LIBTIFF
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#endif
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_update_scale(xsane_setup.png_image_compression_scale, &preferences.png_compression);
#endif
#endif

#ifdef HAVE_LIBTIFF
  xsane_update_scale(xsane_setup.tiff_image_zip_compression_scale, &preferences.tiff_zip_compression);
  preferences.tiff_compression16_nr = xsane_setup.tiff_compression16_nr;
  preferences.tiff_compression8_nr  = xsane_setup.tiff_compression8_nr;
  preferences.tiff_compression1_nr  = xsane_setup.tiff_compression1_nr;
#endif

  xsane_update_bool(xsane_setup.reduce_16bit_to_8bit_button,  &preferences.reduce_16bit_to_8bit);
  xsane_update_bool(xsane_setup.save_pnm16_as_ascii_button,   &preferences.save_pnm16_as_ascii);

#ifdef HAVE_LIBZ
  xsane_update_bool(xsane_setup.save_ps_flatedecoded_button,    &preferences.save_ps_flatedecoded);
  xsane_update_bool(xsane_setup.save_pdf_flatedecoded_button,   &preferences.save_pdf_flatedecoded);
#endif

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_fax_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_fax_apply_changes\n");

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

  xsane_update_geometry_double(xsane_setup.fax_leftoffset_entry,   &preferences.fax_leftoffset,   preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.fax_bottomoffset_entry, &preferences.fax_bottomoffset, preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.fax_width_entry,        &preferences.fax_width,        preferences.length_unit);
  xsane_update_geometry_double(xsane_setup.fax_height_entry,       &preferences.fax_height,       preferences.length_unit);

#ifdef HAVE_LIBZ
  xsane_update_bool(xsane_setup.fax_ps_flatedecoded_button,  &preferences.fax_ps_flatedecoded);
#endif

  xsane_define_maximum_output_size();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_ACTIVATE_EMAIL
static void xsane_setup_email_apply_changes(GtkWidget *widget, gpointer data)
{
 int i;

  DBG(DBG_proc, "xsane_setup_email_apply_changes\n");

  preferences.email_from        = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_from_entry)));
  preferences.email_reply_to    = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_reply_to_entry)));
  preferences.email_smtp_server = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_smtp_server_entry)));
  preferences.email_auth_user   = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_auth_user_entry)));
  preferences.email_auth_pass   = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_auth_pass_entry)));
  preferences.email_pop3_server = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.email_pop3_server_entry)));

  /* make sure password is not stored in ascii text */
  /* this is very simple but better than nothing */
  for (i=0; i<strlen(preferences.email_auth_pass); i++)
  {
    preferences.email_auth_pass[i] ^= 0xa3;
  }

  xsane_update_int(xsane_setup.email_smtp_port_entry, &preferences.email_smtp_port);
  xsane_update_int(xsane_setup.email_pop3_port_entry, &preferences.email_pop3_port);

  preferences.email_authentication = xsane_setup.email_authentication;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_ocr_apply_changes(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_ocr_apply_changes\n");

  if (preferences.ocr_command)
  {
    free((void *) preferences.ocr_command);
  }
  preferences.ocr_command = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.ocr_command_entry)));

  if (preferences.ocr_inputfile_option)
  {
    free((void *) preferences.ocr_inputfile_option);
  }
  preferences.ocr_inputfile_option = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.ocr_inputfile_option_entry)));

  if (preferences.ocr_outputfile_option)
  {
    free((void *) preferences.ocr_outputfile_option);
  }
  preferences.ocr_outputfile_option = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.ocr_outputfile_option_entry)));

  xsane_update_bool(xsane_setup.ocr_use_gui_pipe_entry, &preferences.ocr_use_gui_pipe);

  if (preferences.ocr_gui_outfd_option)
  {
    free((void *) preferences.ocr_gui_outfd_option);
  }
  preferences.ocr_gui_outfd_option = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.ocr_gui_outfd_option_entry)));

  if (preferences.ocr_progress_keyword)
  {
    free((void *) preferences.ocr_progress_keyword);
  }
  preferences.ocr_progress_keyword = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.ocr_progress_keyword_entry)));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_options_ok_callback\n");

  xsane_setup_printer_apply_changes(0, 0);
  xsane_setup_display_apply_changes(0, 0);
  xsane_setup_enhance_apply_changes(0, 0);
  xsane_setup_saving_apply_changes(0, 0);
  xsane_setup_image_apply_changes(0, 0);
  xsane_setup_fax_apply_changes(0, 0);
#ifdef XSANE_ACTIVATE_EMAIL
  xsane_setup_email_apply_changes(0, 0);
#endif
  xsane_setup_ocr_apply_changes(0, 0);
#ifdef HAVE_LIBLCMS
  xsane_setup_color_management_apply_changes(0, 0);
#endif

  if (xsane_setup.grayscale_scanmode)
  {
    free((void *) xsane_setup.grayscale_scanmode);
    xsane_setup.grayscale_scanmode = NULL;
  }

  xsane_pref_save();

  gtk_widget_destroy((GtkWidget *)data); /* => xsane_destroy_setup_dialog_callback */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* this routine is called when the setup dialog window is closed, no matter */
/* if "OK", "CANCEL" or the window manager destroy button has been pressed */
void xsane_destroy_setup_dialog_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_destroy_setup_dialog_callback\n");

  if (device_options_changed)
  {
    xsane_device_preferences_save();
  }

  xsane_set_sensitivity(TRUE);

  xsane.preview->calibration = 0;
  xsane_back_gtk_refresh_dialog();
  preview_update_surface(xsane.preview, 1);
  xsane_update_gamma_curve(TRUE /* update raw */);
}
                
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_close_setup_dialog_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_close_setup_dialog_callback\n");

  gtk_widget_destroy((GtkWidget *)data); /* => xsane_destroy_setup_dialog_callback */
}
                
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_permission_toggled(GtkWidget *widget, gpointer data)
{
 int mask = (int) data;
 int *permission = 0;
 const gchar *name = gtk_widget_get_name(widget);

  DBG(DBG_proc, "xsane_permission_toggled\n");

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

  DBG(DBG_proc, "xsane_permission_box\n");

  if (header)
  {
    hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 2);

    label = gtk_label_new(TEXT_SETUP_PERMISSION_USER);
    gtk_widget_set_size_request(label, 75, -1);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    label = gtk_label_new(TEXT_SETUP_PERMISSION_GROUP);
    gtk_widget_set_size_request(label, 75, -1);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    label = gtk_label_new(TEXT_SETUP_PERMISSION_ALL);
    gtk_widget_set_size_request(label, 75, -1);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    gtk_widget_show(hbox);
  }


  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 2);

  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 256 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_READ);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 256);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, user_sensitivity);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 128 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_WRITE);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 128);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, user_sensitivity);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 64 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_SEARCH);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 64);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, x_sensitivity & user_sensitivity);



  hspace = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hspace, FALSE, FALSE, 6);
  gtk_widget_show(hspace);



  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 32 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_READ);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 32);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 16 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_WRITE);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 16);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 8 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_SEARCH);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 8);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);
  gtk_widget_set_sensitive(button, x_sensitivity);



  hspace = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), hspace, FALSE, FALSE, 6);
  gtk_widget_show(hspace);



  button = gtk_toggle_button_new_with_label("r");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 4 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_READ);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 4);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("w");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 2 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_WRITE);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 2);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_widget_show(button);

  button = gtk_toggle_button_new_with_label("x");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *permission & 1 );
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PERMISSION_SEARCH);
  gtk_widget_set_size_request(button, 26, -1);
  gtk_widget_set_name(button, name);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_permission_toggled, (void *) 1);
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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMSx
static void xsane_setup_display_icm_profile_info_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_display_icm_profile_info_callback\n");

        const char* cmsTakeProductName(cmsHPROFILE hProfile);
        const char* cmsTakeProductDesc(cmsHPROFILE hProfile);

        int   cmsTakeRenderingIntent(cmsHPROFILE hProfile);

        #define LCMS_USED_AS_INPUT      0
        #define LCMS_USED_AS_OUTPUT     1
        #define LCMS_USED_AS_PROOF      2
        BOOL cmsIsIntentSupported(cmsHPROFILE hProfile, int Intent, int UsedDirection);

}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
static void xsane_setup_browse_printer_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_printer_icm_profile;
 char printer_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_printer_icm_profile_callback\n");

  old_printer_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_icm_profile_entry));
#if 0
  strncpy(printer_icm_profile, old_printer_icm_profile, sizeof(printer_icm_profile));
#else
  if (strlen(old_printer_icm_profile)) /* if (old_printer_icm_profile[0]=='/') XXX */
  {
    strncpy(printer_icm_profile, old_printer_icm_profile, sizeof(printer_icm_profile));
  }
  else
  {
    strncpy(printer_icm_profile, "/", sizeof(printer_icm_profile));
  }
#endif

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_PRINTER_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, printer_icm_profile, sizeof(printer_icm_profile), printer_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_icm_profile_entry), printer_icm_profile);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printer_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *title_hbox, *button_box, *button, *label, *text, *table, *separator;
 GtkWidget *printer_option_menu;
 char buf[64];

  DBG(DBG_proc, "xsane_printer_notebook\n");

  /* Printer options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_COPY_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);


  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight, 2 pixels at top and bottom */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6); /* 6 pixels left and right */
  gtk_widget_show(vbox);


  /* printer selection : */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_SEL);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  printer_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, printer_option_menu, DESC_PRINTER_SETUP);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_NAME);
  gtk_widget_set_size_request(text, 350, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_COMMAND);
  gtk_widget_set_size_request(text, 350, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_COPY_NUMBER_OPTION);
  gtk_widget_set_size_request(text, 350, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->copy_number_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_copy_number_option_entry = text;


  xsane_separator_new(vbox, 2);

  table = gtk_table_new(8, 5, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 2);
  gtk_widget_show(table);


  /* title */
  title_hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  label = gtk_label_new(TEXT_SETUP_SCAN_RESOLUTION_PRINTER);
  gtk_box_pack_start(GTK_BOX(title_hbox), label, FALSE, FALSE, 2);
  gtk_table_attach(GTK_TABLE(table), title_hbox, 0, 1, 0, 1, GTK_FILL, 0, 0 , 0);
  gtk_widget_show(label);
  gtk_widget_show(title_hbox);


  /* printer lineart resolution : */

  label = gtk_label_new(TEXT_SETUP_PRINTER_LINEART_RES);
  gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_LINEART_RESOLUTION);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->lineart_resolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 1, 2, 1, 2, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_lineart_resolution_entry = text;


  /* printer grayscale resolution : */

  label = gtk_label_new(TEXT_SETUP_PRINTER_GRAYSCALE_RES);
  gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_GRAYSCALE_RESOLUTION);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->grayscale_resolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 2, 3, 1, 2, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_grayscale_resolution_entry = text;


  /* printer color resolution : */

  label = gtk_label_new(TEXT_SETUP_PRINTER_COLOR_RES);
  gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_COLOR_RESOLUTION);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%d", preferences.printer[preferences.printernr]->color_resolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 3, 4, 1, 2, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_color_resolution_entry = text;


  gtk_table_set_row_spacing(GTK_TABLE(table), 1, 4);
  separator = gtk_hseparator_new();
  gtk_table_attach(GTK_TABLE(table), separator, 0, 5, 2, 3, GTK_FILL, 0, 0 , 0);
  gtk_widget_show(separator);
  gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);


  /* title */
  title_hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  label = gtk_label_new(TEXT_SETUP_PRINTER_PAPER_GEOMETRIE);
  gtk_box_pack_start(GTK_BOX(title_hbox), label, FALSE, FALSE, 2);
  gtk_table_attach(GTK_TABLE(table), title_hbox, 0, 1, 3, 4, GTK_FILL, 0, 0 , 0);
  gtk_widget_show(label);
  gtk_widget_show(title_hbox);


  /* printer width: */

  snprintf(buf, sizeof(buf), "%s [%s]", TEXT_SETUP_PRINTER_WIDTH, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_WIDTH);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->width / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 1, 2, 4, 5, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_width_entry = text;

  /* printer height: */

  snprintf(buf, sizeof(buf), "%s [%s]", TEXT_SETUP_PRINTER_HEIGHT, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_table_attach(GTK_TABLE(table), label, 2, 3, 3, 4, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_HEIGHT);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->height / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 2, 3, 4, 5, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_height_entry = text;

  /* printer left offset : */

  snprintf(buf, sizeof(buf), "%s [%s]", TEXT_SETUP_PRINTER_LEFT, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_table_attach(GTK_TABLE(table), label, 3, 4, 3, 4, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_LEFTOFFSET);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->leftoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 3, 4, 4, 5, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_leftoffset_entry = text;

  /* printer bottom offset : */

  snprintf(buf, sizeof(buf), "%s [%s]", TEXT_SETUP_PRINTER_BOTTOM, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_table_attach(GTK_TABLE(table), label, 4, 5, 3, 4, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_BOTTOMOFFSET);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.printer[preferences.printernr]->bottomoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 4, 5, 4, 5, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_bottomoffset_entry = text;


  gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);
  separator = gtk_hseparator_new();
  gtk_table_attach(GTK_TABLE(table), separator, 0, 5, 5, 6, GTK_FILL, 0, 0 , 0);
  gtk_widget_show(separator);
  gtk_table_set_row_spacing(GTK_TABLE(table), 5, 4);


  /* title */
  title_hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_CORRECTION);
  gtk_box_pack_start(GTK_BOX(title_hbox), label, FALSE, FALSE, 2);
  gtk_table_attach(GTK_TABLE(table), title_hbox, 0, 1, 6, 7, GTK_FILL, 0, 0 , 0);
  gtk_widget_show(label);
  gtk_widget_show(title_hbox);


  /* printer gamma: */

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA);
  gtk_table_attach(GTK_TABLE(table), label, 1, 2, 6, 7, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_GAMMA);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 1, 2, 7, 8, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_gamma_entry = text;

  /* printer gamma red: */

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_RED);
  gtk_table_attach(GTK_TABLE(table), label, 2, 3, 6, 7, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_GAMMA_RED);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 2, 3, 7, 8, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_gamma_red_entry = text;

  /* printer gamma green: */

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_GREEN);
  gtk_table_attach(GTK_TABLE(table), label, 3, 4, 6, 7, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_GAMMA_GREEN);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 3, 4, 7, 8, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_gamma_green_entry = text;

  /* printer gamma blue: */

  label = gtk_label_new(TEXT_SETUP_PRINTER_GAMMA_BLUE);
  gtk_table_attach(GTK_TABLE(table), label, 4, 5, 6, 7, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_GAMMA_BLUE);
  gtk_widget_set_size_request(text, 80, -1);
  snprintf(buf, sizeof(buf), "%1.2f", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_table_attach(GTK_TABLE(table), text, 4, 5, 7, 8, GTK_SHRINK | GTK_EXPAND, 0, 0 , 0);
  gtk_widget_show(text);
  xsane_setup.printer_gamma_blue_entry = text;

#ifdef HAVE_LIBLCMS
  xsane_separator_new(vbox, 2);

  /* printer ICM profile: */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_PRINTER_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, 70, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PRINTER_ICM_PROFILE);

  if (preferences.printer[preferences.printernr]->icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->icm_profile);
  }
  else
  {
    gtk_entry_set_text(GTK_ENTRY(text), "");
  }

  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_widget_show(text);
  xsane_setup.printer_icm_profile_entry = text;

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_printer_icm_profile_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_PRINTER_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  gtk_widget_show(hbox);


  /* embed csa */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(TEXT_SETUP_PRINTER_EMBED_CSA);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PRINTER_EMBED_CSA);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.printer[preferences.printernr]->embed_csa);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.printer_embed_csa_button = button;


  /* embed crd */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(TEXT_SETUP_PRINTER_EMBED_CRD);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PRINTER_EMBED_CRD);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.printer[preferences.printernr]->embed_crd);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.printer_embed_crd_button = button;

  /* black point compensation */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(TEXT_SETUP_PRINTER_CMS_BPC);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PRINTER_CMS_BPC);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.printer[preferences.printernr]->cms_bpc);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.printer_cms_bpc_button = button;

#endif

#ifdef HAVE_LIBZ
  xsane_separator_new(vbox, 2);



  /* flatedecoded = ps level 3 */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(TEXT_SETUP_PRINTER_PS_FLATEDECODED);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PRINTER_PS_FLATEDECODED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.printer[preferences.printernr]->ps_flatedecoded);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.printer_ps_flatedecoded_button = button;
#endif


  xsane_separator_new(vbox, 2);

  /* "apply" "add printer" "delete printer" */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  button_box = gtk_hbox_new(/* homogeneous */ TRUE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);
  gtk_widget_show(button_box);

  button = gtk_button_new_with_label(BUTTON_ADD_PRINTER);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_new, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PRINTER);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_delete, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 8);
  gtk_widget_show(button);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_apply_changes, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_browse_tmp_path_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_tmp_path;
 char tmp_path[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_tmp_path_callback\n");

  old_tmp_path = gtk_entry_get_text(GTK_ENTRY(xsane_setup.tmp_path_entry));
  strncpy(tmp_path, old_tmp_path, sizeof(tmp_path));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_TMP_PATH);
  xsane_back_gtk_get_filename(windowname, tmp_path, sizeof(tmp_path), tmp_path, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_FOLDER, XSANE_GET_FILENAME_SHOW_NOTHING, 0, 0);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.tmp_path_entry), tmp_path);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
static void xsane_setup_browse_scanner_default_color_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_scanner_default_color_icm_profile;
 char scanner_default_color_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_scanner_default_color_icm_profile_callback\n");

  old_scanner_default_color_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.scanner_default_color_icm_profile_entry));
  strncpy(scanner_default_color_icm_profile, old_scanner_default_color_icm_profile, sizeof(scanner_default_color_icm_profile));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_SCANNER_DEFAULT_COLOR_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, scanner_default_color_icm_profile, sizeof(scanner_default_color_icm_profile), scanner_default_color_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.scanner_default_color_icm_profile_entry), scanner_default_color_icm_profile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_browse_scanner_default_gray_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_scanner_default_gray_icm_profile;
 char scanner_default_gray_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_scanner_default_gray_icm_profile_callback\n");

  old_scanner_default_gray_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.scanner_default_gray_icm_profile_entry));
  strncpy(scanner_default_gray_icm_profile, old_scanner_default_gray_icm_profile, sizeof(scanner_default_gray_icm_profile));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_SCANNER_DEFAULT_GRAY_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, scanner_default_gray_icm_profile, sizeof(scanner_default_gray_icm_profile), scanner_default_gray_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.scanner_default_gray_icm_profile_entry), scanner_default_gray_icm_profile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_browse_display_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_display_icm_profile;
 char display_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_display_icm_profile_callback\n");

  old_display_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.display_icm_profile_entry));
  strncpy(display_icm_profile, old_display_icm_profile, sizeof(display_icm_profile));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_DISPLAY_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, display_icm_profile, sizeof(display_icm_profile), display_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.display_icm_profile_entry), display_icm_profile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_browse_custom_proofing_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_custom_proofing_icm_profile;
 char custom_proofing_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_custom_proofing_icm_profile_callback\n");

  old_custom_proofing_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.custom_proofing_icm_profile_entry));
  strncpy(custom_proofing_icm_profile, old_custom_proofing_icm_profile, sizeof(custom_proofing_icm_profile));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_CUSTOM_PROOFING_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, custom_proofing_icm_profile, sizeof(custom_proofing_icm_profile), custom_proofing_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.custom_proofing_icm_profile_entry), custom_proofing_icm_profile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_browse_working_color_space_icm_profile_callback(GtkWidget *widget, gpointer data)
{
 const gchar *old_working_color_space_icm_profile;
 char working_color_space_icm_profile[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_setup_browse_working_color_space_icm_profile_callback\n");

  old_working_color_space_icm_profile = gtk_entry_get_text(GTK_ENTRY(xsane_setup.working_color_space_icm_profile_entry));
  strncpy(working_color_space_icm_profile, old_working_color_space_icm_profile, sizeof(working_color_space_icm_profile));

  snprintf(windowname, sizeof(windowname), "%s %s", xsane.prog_name, WINDOW_WORKING_COLOR_SPACE_ICM_PROFILE);
  xsane_back_gtk_get_filename(windowname, working_color_space_icm_profile, sizeof(working_color_space_icm_profile), working_color_space_icm_profile, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_ICM, XSANE_FILE_FILTER_ICM);

  gtk_entry_set_text(GTK_ENTRY(xsane_setup.working_color_space_icm_profile_entry), working_color_space_icm_profile);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_saving_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text;
 GtkWidget *filename_counter_len_option_menu, *filename_counter_len_menu, *filename_counter_len_item;
 char buf[64];
 int i, select = 1;

  DBG(DBG_proc, "xsane_saving_notebook\n");

  /* Saving options notebook page */
  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_SAVING_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);




  /* tmp path : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_TMP_PATH);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, 70, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_TMP_PATH);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.tmp_path);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_widget_show(text);
  xsane_setup.tmp_path_entry = text;

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_tmp_path_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_TMP_PATH_BROWSE);
  gtk_widget_show(button);

  gtk_widget_show(hbox);


  xsane_separator_new(vbox, 4);


  /* permissions */
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_OVERWRITE_WARNING);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.overwrite_warning);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.overwrite_warning_button = button;

  /* skip existing numbers */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SKIP_EXISTING_NRS);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SKIP_EXISTING);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.skip_existing_numbers);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.skip_existing_numbers_button = button;

  /* filename counter length */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_FILENAME_COUNTER_LEN);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  filename_counter_len_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, filename_counter_len_option_menu, DESC_FILENAME_COUNTER_LEN);
  gtk_box_pack_end(GTK_BOX(hbox), filename_counter_len_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(filename_counter_len_option_menu);
  gtk_widget_show(hbox);

  filename_counter_len_menu = gtk_menu_new();

  for (i=0; i <= 9; i++)
  {
    if (i)
    {
      snprintf(buf, sizeof(buf), "%d", i);
    }
    else
    {
      snprintf(buf, sizeof(buf), MENU_ITEM_COUNTER_LEN_INACTIVE);
    }
    filename_counter_len_item = gtk_menu_item_new_with_label(buf);
    gtk_container_add(GTK_CONTAINER(filename_counter_len_menu), filename_counter_len_item);
    g_signal_connect(GTK_OBJECT(filename_counter_len_item), "activate", (GtkSignalFunc) xsane_setup_filename_counter_len_callback, (void *) i);
    gtk_widget_show(filename_counter_len_item);
    if (preferences.filename_counter_len == i)
    {
      select = i;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(filename_counter_len_option_menu), filename_counter_len_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(filename_counter_len_option_menu), select);
  xsane_setup.filename_counter_len = preferences.filename_counter_len;


  xsane_separator_new(vbox, 4);


  /* save device preferences at exit */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SAVE_DEVPREFS_AT_EXIT);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SAVE_DEVPREFS_AT_EXIT);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.save_devprefs_at_exit);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.save_devprefs_at_exit_button = button;


  xsane_separator_new(vbox, 4);




  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_saving_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_SETUP_FILETYPE_MENU_SIZE 450
static void xsane_filetype_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label;
#ifdef HAVE_LIBTIFF
 int i, select = 1;

 GtkWidget *tiff_compression_option_menu, *tiff_compression_menu, *tiff_compression_item;

 typedef struct tiff_compression_t
 {
  char *name;
  int number;
 } tiff_compression;

#define TIFF_COMPRESSION16_NUMBER 3
#define TIFF_COMPRESSION8_NUMBER 4
#define TIFF_COMPRESSION1_NUMBER 6

 tiff_compression tiff_compression16_strings[TIFF_COMPRESSION16_NUMBER];
 tiff_compression tiff_compression8_strings[TIFF_COMPRESSION8_NUMBER];
 tiff_compression tiff_compression1_strings[TIFF_COMPRESSION1_NUMBER];
 
 tiff_compression16_strings[0].name   = MENU_ITEM_TIFF_COMP_NONE;
 tiff_compression16_strings[0].number = COMPRESSION_NONE;
 tiff_compression16_strings[1].name   = MENU_ITEM_TIFF_COMP_PACKBITS;
 tiff_compression16_strings[1].number = COMPRESSION_PACKBITS;
 tiff_compression16_strings[2].name   = MENU_ITEM_TIFF_COMP_DEFLATE;
 tiff_compression16_strings[2].number = COMPRESSION_DEFLATE;
 
 tiff_compression8_strings[0].name   = MENU_ITEM_TIFF_COMP_NONE;
 tiff_compression8_strings[0].number = COMPRESSION_NONE;
 tiff_compression8_strings[1].name   = MENU_ITEM_TIFF_COMP_JPEG;
 tiff_compression8_strings[1].number = COMPRESSION_JPEG;
 tiff_compression8_strings[2].name   = MENU_ITEM_TIFF_COMP_PACKBITS;
 tiff_compression8_strings[2].number = COMPRESSION_PACKBITS;
 tiff_compression8_strings[3].name   = MENU_ITEM_TIFF_COMP_DEFLATE;
 tiff_compression8_strings[3].number = COMPRESSION_DEFLATE;

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

  DBG(DBG_proc, "xsane_filetype_notebook\n");

  /* Image options notebook page */
  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_FILETYPE_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);



  /* reduce 16bit to 8bit */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_REDUCE_16BIT_TO_8BIT);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_REDUCE_16BIT_TO_8BIT);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.reduce_16bit_to_8bit);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.reduce_16bit_to_8bit_button = button;


  xsane_separator_new(vbox, 4);


  /* save pnm16 as ascii */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SAVE_PNM16_AS_ASCII);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SAVE_PNM16_AS_ASCII);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.save_pnm16_as_ascii);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.save_pnm16_as_ascii_button = button;


#ifdef HAVE_LIBZ
  /* save ps with zlib compression / flatedecode = ps level 3 */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SAVE_PS_FLATEDECODED);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SAVE_PS_FLATEDECODED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.save_ps_flatedecoded);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.save_ps_flatedecoded_button = button;


  /* save pdf with zlib compression / flatedecode */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_SAVE_PDF_FLATEDECODED);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_SAVE_PDF_FLATEDECODED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.save_pdf_flatedecoded);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.save_pdf_flatedecoded_button = button;
#endif


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
  xsane_range_new(GTK_BOX(vbox), TEXT_SETUP_JPEG_QUALITY, DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 10.0, 0,
                  &preferences.jpeg_quality, &xsane_setup.jpeg_image_quality_scale, 0, TRUE);
#else
#ifdef HAVE_LIBTIFF
  xsane_range_new(GTK_BOX(vbox), TEXT_SETUP_JPEG_QUALITY, DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 10.0, 0,
                  &preferences.jpeg_quality, &xsane_setup.jpeg_image_quality_scale, 0, TRUE);
#endif
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_range_new(GTK_BOX(vbox), TEXT_SETUP_PNG_COMPRESSION, DESC_PNG_COMPRESSION, 0.0, Z_BEST_COMPRESSION, 1.0, 10.0, 0,
                  &preferences.png_compression, &xsane_setup.png_image_compression_scale, 0, TRUE);
#endif
#endif

#ifdef HAVE_LIBTIFF
  xsane_range_new(GTK_BOX(vbox), TEXT_SETUP_TIFF_ZIP_COMPRESSION, DESC_TIFF_ZIP_COMPRESSION, 1.0, 9.0, 1.0, 6.0, 0,
                  &preferences.tiff_zip_compression, &xsane_setup.tiff_image_zip_compression_scale, 0, TRUE);

  /* TIFF 16 BIT IMAGES COMPRESSION */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_TIFF_COMPRESSION_16);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  tiff_compression_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, tiff_compression_option_menu, DESC_TIFF_COMPRESSION_16);
  gtk_box_pack_end(GTK_BOX(hbox), tiff_compression_option_menu, FALSE, FALSE, 2);
  gtk_widget_set_size_request(tiff_compression_option_menu, XSANE_SETUP_FILETYPE_MENU_SIZE, -1);
  gtk_widget_show(tiff_compression_option_menu);
  gtk_widget_show(hbox);

  tiff_compression_menu = gtk_menu_new();

  for (i=1; i <= TIFF_COMPRESSION16_NUMBER; i++)
  {
    tiff_compression_item = gtk_menu_item_new_with_label(tiff_compression16_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(tiff_compression_menu), tiff_compression_item);
    g_signal_connect(GTK_OBJECT(tiff_compression_item), "activate", (GtkSignalFunc) xsane_setup_tiff_compression16_callback, (void *) tiff_compression16_strings[i-1].number);
    gtk_widget_show(tiff_compression_item);
    if (tiff_compression16_strings[i-1].number == preferences.tiff_compression16_nr)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(tiff_compression_option_menu), tiff_compression_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(tiff_compression_option_menu), select);
  xsane_setup.tiff_compression16_nr = preferences.tiff_compression16_nr;


  /* TIFF 8 BIT IMAGES COMPRESSION */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_TIFF_COMPRESSION_8);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  tiff_compression_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, tiff_compression_option_menu, DESC_TIFF_COMPRESSION_8);
  gtk_box_pack_end(GTK_BOX(hbox), tiff_compression_option_menu, FALSE, FALSE, 2);
  gtk_widget_set_size_request(tiff_compression_option_menu, XSANE_SETUP_FILETYPE_MENU_SIZE, -1);
  gtk_widget_show(tiff_compression_option_menu);
  gtk_widget_show(hbox);

  tiff_compression_menu = gtk_menu_new();

  for (i=1; i <= TIFF_COMPRESSION8_NUMBER; i++)
  {
    tiff_compression_item = gtk_menu_item_new_with_label(tiff_compression8_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(tiff_compression_menu), tiff_compression_item);
    g_signal_connect(GTK_OBJECT(tiff_compression_item), "activate", (GtkSignalFunc) xsane_setup_tiff_compression8_callback, (void *) tiff_compression8_strings[i-1].number);
    gtk_widget_show(tiff_compression_item);
    if (tiff_compression8_strings[i-1].number == preferences.tiff_compression8_nr)
    {
      select = i-1;
    }
  }


  gtk_option_menu_set_menu(GTK_OPTION_MENU(tiff_compression_option_menu), tiff_compression_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(tiff_compression_option_menu), select);
  xsane_setup.tiff_compression8_nr = preferences.tiff_compression8_nr;


  /* TIFF 1 BIT IMAGES COMPRESSION */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_TIFF_COMPRESSION_1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  tiff_compression_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, tiff_compression_option_menu, DESC_TIFF_COMPRESSION_1);
  gtk_box_pack_end(GTK_BOX(hbox), tiff_compression_option_menu, FALSE, FALSE, 2);
  gtk_widget_set_size_request(tiff_compression_option_menu, XSANE_SETUP_FILETYPE_MENU_SIZE, -1);
  gtk_widget_show(tiff_compression_option_menu);
  gtk_widget_show(hbox);

  tiff_compression_menu = gtk_menu_new();

  for (i=1; i <= TIFF_COMPRESSION1_NUMBER; i++)
  {
    tiff_compression_item = gtk_menu_item_new_with_label(tiff_compression1_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(tiff_compression_menu), tiff_compression_item);
    g_signal_connect(GTK_OBJECT(tiff_compression_item), "activate", (GtkSignalFunc) xsane_setup_tiff_compression1_callback, (void *) tiff_compression1_strings[i-1].number);
    gtk_widget_show(tiff_compression_item);
    if (tiff_compression1_strings[i-1].number == preferences.tiff_compression1_nr)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(tiff_compression_option_menu), tiff_compression_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(tiff_compression_option_menu), select);
  xsane_setup.tiff_compression1_nr = preferences.tiff_compression1_nr;

#endif


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_image_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
{
  char *identifier;
  char *fax_command;
  char *fax_receiver_option;
  char *fax_postscript_option;
  char *fax_normal_option;
  char *fax_fine_option;
} fax_program_options_type;

fax_program_options_type fax_program[] =
{
  {" hylafax ",        "sendfax",  "-d", "",  "-l", "-m"},
  {" mgetty+sendfax ", "faxspool", "",   "",  "-n", ""},
  {" efax ",           "fax send", "",   "",  "-l", ""},
};

static void xsane_fax_notebook_set_faxprogram_default_callback(GtkWidget *widget, int program_number)
{
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.fax_command_entry),           (char *) fax_program[program_number].fax_command);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.fax_receiver_option_entry),   (char *) fax_program[program_number].fax_receiver_option);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.fax_postscript_option_entry), (char *) fax_program[program_number].fax_postscript_option);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.fax_normal_option_entry),     (char *) fax_program[program_number].fax_normal_option);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.fax_fine_option_entry),       (char *) fax_program[program_number].fax_fine_option);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_SETUP_FAX_ENTRY_SIZE 400
static void xsane_fax_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text;
 char buf[64];
 int i;

  DBG(DBG_proc, "xsane_fax_notebook\n");

  /* Fax options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_FAX_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);


  /* faxcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_COMMAND);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_COMMAND);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_RECEIVER_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_POSTSCRIPT_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_NORMAL_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_FINE_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_fine_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_fine_option_entry = text;


  /* fax set program default options : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_PROGRAM_DEFAULTS);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  for (i=0; i < sizeof(fax_program)/sizeof(fax_program_options_type); i++)
  {
    button = gtk_button_new_with_label(fax_program[i].identifier);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_notebook_set_faxprogram_default_callback, (void *) i);
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 10);
    gtk_widget_show(button);
  }

  gtk_widget_show(hbox);


  xsane_separator_new(vbox, 2);


  /* faxviewer */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_FAX_VIEWER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_VIEWER);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_viewer);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_viewer_entry = text;


  xsane_separator_new(vbox, 4);

  /* fax width: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  snprintf(buf, sizeof(buf), "%s [%s]:", TEXT_SETUP_FAX_WIDTH, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_WIDTH);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.fax_width / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_width_entry = text;

  /* fax height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  snprintf(buf, sizeof(buf), "%s [%s]:", TEXT_SETUP_FAX_HEIGHT, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_HEIGHT);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.fax_height / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_height_entry = text;

  /* fax left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  snprintf(buf, sizeof(buf), "%s [%s]:", TEXT_SETUP_FAX_LEFT, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_LEFTOFFSET);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.fax_leftoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_leftoffset_entry = text;

  /* fax bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  snprintf(buf, sizeof(buf), "%s [%s]:", TEXT_SETUP_FAX_BOTTOM, xsane_back_gtk_unit_string(SANE_UNIT_MM));
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAX_BOTTOMOFFSET);
  gtk_widget_set_size_request(text, XSANE_SETUP_FAX_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%4.3f", preferences.fax_bottomoffset / preferences.length_unit);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_bottomoffset_entry = text;


#ifdef HAVE_LIBZ
  xsane_separator_new(vbox, 4);


  /* flatedecoded = ps level 3 */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(TEXT_SETUP_FAX_PS_FLATEDECODED);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_FAX_PS_FLATEDECODED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.fax_ps_flatedecoded);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.fax_ps_flatedecoded_button = button;
#endif


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_fax_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_ACTIVATE_EMAIL

static void xsane_email_notebook(GtkWidget *notebook)
{
 typedef struct show_range_mode_t
 {
  char *name;
  int number;
 } authentication_type;

#define AUTHENTICATION_NUMBER 4
#define XSANE_SETUP_EMAIL_ENTRY_SIZE 400

 GtkWidget *setup_vbox, *vbox, *pop3_vbox, *hbox, *button, *label, *text;
 GtkWidget *authentication_option_menu, *authentication_menu, *authentication_menu_item;
 char buf[64];
 char *password;
 authentication_type authentication_strings[AUTHENTICATION_NUMBER];
 int i, select = 0;

  DBG(DBG_proc, "xsane_email_notebook\n");

  /* Mail options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_EMAIL_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);



  /* from */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_EMAIL_FROM);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_FROM);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_from);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_from_entry = text;


  /* reply to */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_EMAIL_REPLY_TO);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_REPLY_TO);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_reply_to);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_reply_to_entry = text;


  xsane_separator_new(vbox, 2);


  /* smtp server */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SMTP_SERVER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SMTP_SERVER);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_smtp_server);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_smtp_server_entry = text;


  /* smtp port */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SMTP_PORT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SMTP_PORT);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%d", preferences.email_smtp_port);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_smtp_port_entry = text;


  xsane_separator_new(vbox, 2);


  /* create vbox for pop3 settings */
  pop3_vbox = gtk_vbox_new(FALSE, 5);
  gtk_widget_show(pop3_vbox);

  /* email authentication */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_EMAIL_AUTHENTICATION);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  authentication_strings[0].name   = MENU_ITEM_AUTH_NONE;
  authentication_strings[0].number = EMAIL_AUTH_NONE;
  authentication_strings[1].name   = MENU_ITEM_AUTH_POP3;
  authentication_strings[1].number = EMAIL_AUTH_POP3;
  authentication_strings[2].name   = MENU_ITEM_AUTH_ASMTP_PLAIN;
  authentication_strings[2].number = EMAIL_AUTH_ASMTP_PLAIN;
  authentication_strings[3].name   = MENU_ITEM_AUTH_ASMTP_LOGIN;
  authentication_strings[3].number = EMAIL_AUTH_ASMTP_LOGIN;
#if 0
  authentication_strings[4].name   = MENU_ITEM_AUTH_ASMTP_CRAM_MD5;
  authentication_strings[4].number = EMAIL_AUTH_ASMTP_CRAM_MD5;
#endif

  authentication_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, authentication_option_menu, DESC_EMAIL_AUTHENTICATION);
  gtk_box_pack_end(GTK_BOX(hbox), authentication_option_menu, FALSE, FALSE, 2);
  gtk_widget_set_size_request(authentication_option_menu, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_widget_show(authentication_option_menu);
  gtk_widget_show(hbox);

  authentication_menu = gtk_menu_new();

  for (i=1; i <= AUTHENTICATION_NUMBER; i++)
  {
    authentication_menu_item = gtk_menu_item_new_with_label(authentication_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(authentication_menu), authentication_menu_item);
    g_signal_connect(GTK_OBJECT(authentication_menu_item), "activate", (GtkSignalFunc) xsane_setup_authentication_type_callback, (void *) authentication_strings[i-1].number);
    gtk_widget_show(authentication_menu_item);
    if (authentication_strings[i-1].number == preferences.email_authentication)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(authentication_option_menu), authentication_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(authentication_option_menu), select);

  xsane_setup.email_authentication = preferences.email_authentication;

  /* email authorization username */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_EMAIL_AUTH_USER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_AUTH_USER);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_auth_user);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_auth_user_entry = text;


  /* email authorization password */
  password = strdup(preferences.email_auth_pass);
 
  for (i=0; i<strlen(password); i++)
  {
    password[i] ^= 0xa3;
  }
 
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_EMAIL_AUTH_PASS);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_AUTH_PASS);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) password);
  gtk_entry_set_visibility(GTK_ENTRY(text), 0);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_auth_pass_entry = text;

  free(password);

  /* place vbox for pop3 settings after check_button */
  gtk_box_pack_start(GTK_BOX(vbox), pop3_vbox, FALSE, FALSE, 2);
  gtk_widget_set_sensitive(pop3_vbox, preferences.email_authentication == EMAIL_AUTH_POP3);

  /* pop3 server */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pop3_vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_POP3_SERVER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_POP3_SERVER);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_pop3_server);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_pop3_server_entry = text;


  /* pop3 port */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pop3_vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_POP3_PORT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_POP3_PORT);
  gtk_widget_set_size_request(text, XSANE_SETUP_EMAIL_ENTRY_SIZE, -1);
  snprintf(buf, sizeof(buf), "%d", preferences.email_pop3_port);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.email_pop3_port_entry = text;


  xsane_setup.pop3_vbox = pop3_vbox;


  xsane_separator_new(vbox, 4);

  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_email_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_SETUP_OCR_ENTRY_SIZE 400
static void xsane_ocr_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *label, *text, *button;

  DBG(DBG_proc, "xsane_ocr_notebook\n");

  /* OCR options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_OCR_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);


  /* ocr command : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_OCR_COMMAND);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_OCR_COMMAND);
  gtk_widget_set_size_request(text, XSANE_SETUP_OCR_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.ocr_command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.ocr_command_entry = text;


  /* ocr inputfile option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_OCR_INPUTFILE_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_OCR_INPUTFILE_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_OCR_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.ocr_inputfile_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.ocr_inputfile_option_entry = text;


  /* ocr outputfile option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_OCR_OUTPUTFILE_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_OCR_OUTPUTFILE_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_OCR_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.ocr_outputfile_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.ocr_outputfile_option_entry = text;


  /* ocr use gui pipe button */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_OCR_USE_GUI_PIPE);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_OCR_USE_GUI_PIPE_OPT);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.ocr_use_gui_pipe);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.ocr_use_gui_pipe_entry = button;


  /* ocr gui outfd option : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_OCR_OUTFD_OPT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_OCR_OUTFD_OPT);
  gtk_widget_set_size_request(text, XSANE_SETUP_OCR_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.ocr_gui_outfd_option);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.ocr_gui_outfd_option_entry = text;


  /* ocr progress keyword : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_OCR_PROGRESS_KEYWORD);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_OCR_PROGRESS_KEYWORD);
  gtk_widget_set_size_request(text, XSANE_SETUP_OCR_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.ocr_progress_keyword);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.ocr_progress_keyword_entry = text;

  xsane_separator_new(vbox, 4);

  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_ocr_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_SETUP_DISPLAY_ENTRY_SIZE 400
static void xsane_display_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text;
 GtkWidget *show_range_mode_option_menu, *show_range_mode_menu, *show_range_mode_item;
 char buf[64];
 int i, select = 1;

 typedef struct show_range_mode_t
 {
  char *name;
  int number;
 } show_range_mode;

#define SHOW_RANGE_MODE_NUMBER 5
 show_range_mode show_range_mode_strings[SHOW_RANGE_MODE_NUMBER];

  DBG(DBG_proc, "xsane_display_notebook\n");

  /* Display options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_DISPLAY_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);


  /* main window fixed: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_WINDOW_FIXED);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_MAIN_WINDOW_FIXED);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.main_window_fixed);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.main_window_fixed_button = button;

  /* private colormap: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_PRIVATE_COLORMAP);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PREVIEW_COLORMAP);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.preview_own_cmap);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_own_cmap_button = button;


  xsane_separator_new(vbox, 2);


  /* show range type menu  */
  /* bit 0 (val 1) : show scale */
  /* bit 1 (val 2) : show scrollbar */
  /* bit 2 (val 4) : show spinbutton */
  /* bit 3 (val 8) : show scale val */
  show_range_mode_strings[0].name   = MENU_ITEM_RANGE_SCALE;
  show_range_mode_strings[0].number = 9; /* 1 + 8 = scale + value */
  show_range_mode_strings[1].name   = MENU_ITEM_RANGE_SCROLLBAR;
  show_range_mode_strings[1].number = 10; /* 2 + 4 = scrollbar + value */
  show_range_mode_strings[2].name   = MENU_ITEM_RANGE_SPINBUTTON;
  show_range_mode_strings[2].number = 4;
  show_range_mode_strings[3].name   = MENU_ITEM_RANGE_SCALE_SPIN;
  show_range_mode_strings[3].number = 5; /* 1 + 4 = scale + spinbutton */
  show_range_mode_strings[4].name   = MENU_ITEM_RANGE_SCROLL_SPIN;
  show_range_mode_strings[4].number = 6; /* 2 + 4 = scrollbar + spinbutton */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_SHOW_RANGE_MODE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  show_range_mode_option_menu = gtk_option_menu_new();
  gtk_widget_set_size_request(show_range_mode_option_menu, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
  xsane_back_gtk_set_tooltip(xsane.tooltips, show_range_mode_option_menu, DESC_SHOW_RANGE_MODE);
  gtk_box_pack_end(GTK_BOX(hbox), show_range_mode_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(show_range_mode_option_menu);
  gtk_widget_show(hbox);

  show_range_mode_menu = gtk_menu_new();

  for (i=1; i <= SHOW_RANGE_MODE_NUMBER; i++)
  {
    show_range_mode_item = gtk_menu_item_new_with_label(show_range_mode_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(show_range_mode_menu), show_range_mode_item);
    g_signal_connect(GTK_OBJECT(show_range_mode_item), "activate", (GtkSignalFunc) xsane_setup_show_range_mode_callback, (void *) show_range_mode_strings[i-1].number);
    gtk_widget_show(show_range_mode_item);
    if (show_range_mode_strings[i-1].number == preferences.show_range_mode)
    {
      select = i-1;
    }
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(show_range_mode_option_menu), show_range_mode_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(show_range_mode_option_menu), select);
  xsane_setup.show_range_mode = preferences.show_range_mode;

  
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_OVERSAMPLING);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_GAMMA);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_GAMMA_RED);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_GAMMA_GREEN);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_GAMMA_BLUE);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_blue_entry = text;

  /* disable preview gamma for gimp plugin: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_DISABLE_GIMP_PREVIEW_GAMMA);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_DISABLE_GIMP_PREVIEW_GAMMA);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.disable_gimp_preview_gamma);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.disable_gimp_preview_gamma_button = button;


  xsane_separator_new(vbox, 2);


  /* docviewer */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_HELPFILE_VIEWER);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_DOC_VIEWER);
  gtk_widget_set_size_request(text, XSANE_SETUP_DISPLAY_ENTRY_SIZE, -1);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.browser);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.browser_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_display_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);

  preview_update_surface(xsane.preview, 1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhance_notebook_sensitivity(int lineart_mode)
{
 int sensitivity_val = FALSE;
 int sensitivity_mode = FALSE;

  DBG(DBG_proc, "xsane_enhance_notebook_sensitivity\n");

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
  gtk_widget_set_sensitive(xsane_setup.preview_grayscale_scanmode_widget, sensitivity_mode);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_authentication_type_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.email_authentication = (int) data;

  gtk_widget_set_sensitive(xsane_setup.pop3_vbox, (xsane_setup.email_authentication == EMAIL_AUTH_POP3));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_show_range_mode_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.show_range_mode = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_lineart_mode_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup.lineart_mode = (int) data;
  xsane_enhance_notebook_sensitivity(xsane_setup.lineart_mode);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_grayscale_mode_callback(GtkWidget *widget, gpointer data)
{
  if (xsane_setup.grayscale_scanmode)
  {
    free(xsane_setup.grayscale_scanmode);
    xsane_setup.grayscale_scanmode = NULL;
  }

  xsane_setup.grayscale_scanmode = strdup( (char *) data);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_preview_pipette_range_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_setup_preview_pipette_range_callback\n");

  xsane_setup.preview_pipette_range = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_SETUP_ENHANCED_ENTRY_SIZE 400
static void xsane_enhance_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text;
 GtkWidget *lineart_mode_option_menu, *lineart_mode_menu, *lineart_mode_item;
 GtkWidget *gray_option_menu, *gray_menu, *gray_item;
 GtkWidget *preview_pipette_range_option_menu, *preview_pipette_range_menu, *preview_pipette_range_item;
 const SANE_Option_Descriptor *opt;
 char buf[64];
 int i, j, select = 1;

 typedef struct lineart_mode_t
 {
  char *name;
  int number;
 } lineart_mode;

#define LINEART_MODE_NUMBER 3
 lineart_mode lineart_mode_strings[LINEART_MODE_NUMBER];
 
  DBG(DBG_proc, "xsane_enhance_notebook\n");

  /* enhancement options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_ENHANCE_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, lineart_mode_option_menu, DESC_LINEART_MODE);
  gtk_box_pack_end(GTK_BOX(hbox), lineart_mode_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(lineart_mode_option_menu);
  gtk_widget_show(hbox);

  lineart_mode_menu = gtk_menu_new();

  for (i=1; i <= LINEART_MODE_NUMBER; i++)
  {
    lineart_mode_item = gtk_menu_item_new_with_label(lineart_mode_strings[i-1].name);
    gtk_container_add(GTK_CONTAINER(lineart_mode_menu), lineart_mode_item);
    g_signal_connect(GTK_OBJECT(lineart_mode_item), "activate", (GtkSignalFunc) xsane_setup_lineart_mode_callback, (void *) lineart_mode_strings[i-1].number);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_THRESHOLD_MIN);
  gtk_widget_set_size_request(text, XSANE_SETUP_ENHANCED_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_THRESHOLD_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_ENHANCED_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_THRESHOLD_MUL);
  gtk_widget_set_size_request(text, XSANE_SETUP_ENHANCED_ENTRY_SIZE, -1);
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
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_PREVIEW_THRESHOLD_OFF);
  gtk_widget_set_size_request(text, XSANE_SETUP_ENHANCED_ENTRY_SIZE, -1);
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

  if (xsane.grayscale_scanmode)
  {
    xsane_setup.grayscale_scanmode = strdup(xsane.grayscale_scanmode);
  }
  else
  {
    xsane_setup.grayscale_scanmode = NULL;
  }

  select = 0;
  gray_menu = gtk_menu_new();

  opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.scanmode);
  if (opt)
  {
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      switch (opt->constraint_type)
      {
        case SANE_CONSTRAINT_STRING_LIST:
        {
         char *set;
         SANE_Status status;

          /* use a "list-selection" widget */
          set = malloc(opt->size);
          status = xsane_control_option(xsane.dev, xsane.well_known.scanmode, SANE_ACTION_GET_VALUE, set, 0);

          for (i=0; opt->constraint.string_list[i]; i++)
          {
            gray_item = gtk_menu_item_new_with_label(_BGT(opt->constraint.string_list[i]));
            gtk_container_add(GTK_CONTAINER(gray_menu), gray_item);
            g_signal_connect(GTK_OBJECT(gray_item), "activate", (GtkSignalFunc) xsane_setup_grayscale_mode_callback, (void *) opt->constraint.string_list[i]);
            gtk_widget_show(gray_item);

            if (xsane.grayscale_scanmode)
            {
              if (!strcmp(opt->constraint.string_list[i], xsane.grayscale_scanmode))
              {
                select = i;
              }
            }
          }
        }
        break;

        default:
          DBG(DBG_error, "grayscale_scanmode_selection: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
      }
    }
  }

  gray_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, gray_option_menu, DESC_GRAYSCALE_SCANMODE);
  gtk_box_pack_end(GTK_BOX(hbox), gray_option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(gray_option_menu), gray_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(gray_option_menu), select);
  gtk_widget_show(hbox);

  gtk_widget_show(gray_option_menu);  
  xsane_setup.preview_grayscale_scanmode_widget = gray_option_menu;


  xsane_separator_new(vbox, 2);

  /* autoenhance gamma */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_AUTOENHANCE_GAMMA);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_AUTOENHANCE_GAMMA);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.auto_enhance_gamma);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.auto_enhance_gamma_button = button;

  /* autoselect scan area */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_PRESELECT_SCAN_AREA);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_PRESELECT_SCAN_AREA);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.preselect_scan_area);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preselect_scan_area_button = button;

  /* autocorrect colors */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_AUTOCORRECT_COLORS);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_AUTOCORRECT_COLORS);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.auto_correct_colors);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.auto_correct_colors_button = button;


  /* preview pipette range */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(TEXT_SETUP_PREVIEW_PIPETTE_RANGE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  preview_pipette_range_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, preview_pipette_range_option_menu, DESC_PREVIEW_PIPETTE_RANGE);
  gtk_box_pack_end(GTK_BOX(hbox), preview_pipette_range_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(preview_pipette_range_option_menu);
  gtk_widget_show(hbox);

  preview_pipette_range_menu = gtk_menu_new();

  j=1;
  for (i=0; i<=3; i++)
  {
    snprintf(buf, sizeof(buf), "%d x %d pixel", j, j);
    preview_pipette_range_item = gtk_menu_item_new_with_label(buf);
    gtk_container_add(GTK_CONTAINER(preview_pipette_range_menu), preview_pipette_range_item);
    g_signal_connect(GTK_OBJECT(preview_pipette_range_item), "activate", (GtkSignalFunc) xsane_setup_preview_pipette_range_callback, (void *) j);
    gtk_widget_show(preview_pipette_range_item);
    if (preferences.preview_pipette_range == j)
    {
      select = i;
    }
    j+=2;
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(preview_pipette_range_option_menu), preview_pipette_range_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(preview_pipette_range_option_menu), select);
  xsane_setup.preview_pipette_range = preferences.preview_pipette_range;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_enhance_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  xsane_enhance_notebook_sensitivity(xsane_setup.lineart_mode);

  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
#define XSANE_SETUP_CMS_ENTRY_SIZE 300
static void xsane_color_management_notebook(GtkWidget *notebook)
{
 GtkWidget *setup_vbox, *vbox, *hbox, *button, *label, *text, *option_menu, *menu, *menu_item;
 int selection = 0;

  DBG(DBG_proc, "xsane_color_management_notebook\n");

  /* color management options notebook page */

  setup_vbox = gtk_vbox_new(FALSE, 0);

  label = gtk_label_new(NOTEBOOK_COLOR_MANAGEMENT_OPTIONS);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(setup_vbox), vbox, TRUE, TRUE, 2); /* sizeable framehight */
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
  gtk_widget_show(vbox);




  /* black point compensation */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label(RADIO_BUTTON_CMS_BPC);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_CMS_BPC);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.cms_bpc);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.cms_bpc_button = button;



  /* Intent menu */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(MENU_ITEM_CMS_RENDERING_INTENT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, option_menu, DESC_RENDERING_INTENT);
  gtk_box_pack_end(GTK_BOX(hbox), option_menu, FALSE, FALSE, 2);
  gtk_widget_show(option_menu);
  gtk_widget_show(hbox);

  menu = gtk_menu_new();

  menu_item = gtk_menu_item_new_with_label(SUBMENU_ITEM_CMS_INTENT_PERCEPTUAL);
  gtk_object_set_data(GTK_OBJECT(menu_item), "Selection", (void *) INTENT_PERCEPTUAL);
  gtk_container_add(GTK_CONTAINER(menu), menu_item);
  gtk_widget_show(menu_item);

  menu_item = gtk_menu_item_new_with_label(SUBMENU_ITEM_CMS_INTENT_RELATIVE_COLORIMETRIC);
  gtk_object_set_data(GTK_OBJECT(menu_item), "Selection", (void *) INTENT_RELATIVE_COLORIMETRIC);
  gtk_container_add(GTK_CONTAINER(menu), menu_item);
  gtk_widget_show(menu_item);

  menu_item = gtk_menu_item_new_with_label(SUBMENU_ITEM_CMS_INTENT_ABSOLUTE_COLORIMETRIC);
  gtk_object_set_data(GTK_OBJECT(menu_item), "Selection", (void *) INTENT_ABSOLUTE_COLORIMETRIC);
  gtk_container_add(GTK_CONTAINER(menu), menu_item);
  gtk_widget_show(menu_item);

  menu_item = gtk_menu_item_new_with_label(SUBMENU_ITEM_CMS_INTENT_SATURATION);
  gtk_object_set_data(GTK_OBJECT(menu_item), "Selection", (void *) INTENT_SATURATION);
  gtk_container_add(GTK_CONTAINER(menu), menu_item);
  gtk_widget_show(menu_item);

  if (preferences.cms_intent == INTENT_PERCEPTUAL)
  {
    selection = 0;
  }
  else if (preferences.cms_intent == INTENT_RELATIVE_COLORIMETRIC)
  {
    selection = 1;
  }
  else if (preferences.cms_intent == INTENT_ABSOLUTE_COLORIMETRIC)
  {
    selection = 2;
  }
  else
  {
    selection = 3;
  }


  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), selection);
  xsane_setup.cms_intent_option_menu = option_menu;


  xsane_separator_new(vbox, 4);


  /* scanner_default_color_icm_profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SCANNER_DEFAULT_COLOR_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_scanner_default_color_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_SCANNER_DEFAULT_COLOR_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SCANNER_DEFAULT_COLOR_ICM_PROFILE);

  if (xsane.scanner_default_color_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) xsane.scanner_default_color_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.scanner_default_color_icm_profile_entry = text;

  gtk_widget_show(hbox);


  /* scanner_default_gray_icm_profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SCANNER_DEFAULT_GRAY_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_scanner_default_gray_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_SCANNER_DEFAULT_GRAY_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SCANNER_DEFAULT_GRAY_ICM_PROFILE);

  if (xsane.scanner_default_gray_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) xsane.scanner_default_gray_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.scanner_default_gray_icm_profile_entry = text;

  gtk_widget_show(hbox);

#if 0
  xsane_separator_new(vbox, 4);


  /* scanner_tran icm-profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SCANNER_TRAN_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_scanner_tran_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_SCANNER_TRAN_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SCANNER_TRAN_ICM_PROFILE);

  if (xsane.scanner_tran_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) xsane.scanner_tran_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.scanner_tran_icm_profile_entry = text;

  gtk_widget_show(hbox);


  /* scanner_tran_gray icm-profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_SCANNER_TRAN_GRAY_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_scanner_tran_gray_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_SCANNER_TRAN_GRAY_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_SCANNER_TRAN_GRAY_ICM_PROFILE);

  if (xsane.scanner_tran_gray_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) xsane.scanner_tran_gray_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.scanner_tran_gray_icm_profile_entry = text;

  gtk_widget_show(hbox);
#endif

  xsane_separator_new(vbox, 4);


  /* display icm-profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_DISPLAY_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_display_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_DISPLAY_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_DISPLAY_ICM_PROFILE);

  if (preferences.display_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.display_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.display_icm_profile_entry = text;

  gtk_widget_show(hbox);



  /* custom output icm-profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_CUSTOM_PROOFING_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_custom_proofing_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_CUSTOM_PROOFING_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_CUSTOM_PROOFING_ICM_PROFILE);

  if (preferences.custom_proofing_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.custom_proofing_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.custom_proofing_icm_profile_entry = text;

  gtk_widget_show(hbox);


  xsane_separator_new(vbox, 4);


  /* working color space icm-profile filename : */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new(TEXT_SETUP_WORKING_COLOR_SPACE_ICM_PROFILE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  button = gtk_button_new_with_label(BUTTON_BROWSE); 
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_browse_working_color_space_icm_profile_callback, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_BUTTON_WORKING_COLOR_SPACE_ICM_PROFILE_BROWSE);
  gtk_widget_show(button);

  text = gtk_entry_new_with_max_length(PATH_MAX);
  gtk_widget_set_size_request(text, XSANE_SETUP_CMS_ENTRY_SIZE, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_WORKING_COLOR_SPACE_ICM_PROFILE);

  if (preferences.working_color_space_icm_profile)
  {
    gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.working_color_space_icm_profile);
  }

  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 4);
  gtk_widget_show(text);
  xsane_setup.working_color_space_icm_profile_entry = text;

  gtk_widget_show(hbox);



  xsane_separator_new(vbox, 4);



  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
#else
  button = gtk_button_new_with_label(BUTTON_APPLY);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_color_management_apply_changes, NULL);
  gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_setup_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *setup_dialog, *setup_vbox, *hbox, *button_box, *button, *notebook;
 char buf[64];

  DBG(DBG_proc, "xsane_setup_dialog\n");

  device_options_changed = 0;

  xsane_set_sensitivity(FALSE);

  xsane.preview->calibration = 1; /* show monitor calibration image */

  setup_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(setup_dialog), GTK_WIN_POS_MOUSE);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_SETUP);
  gtk_window_set_title(GTK_WINDOW(setup_dialog), buf);
  g_signal_connect(GTK_OBJECT(setup_dialog), "destroy", (GtkSignalFunc) xsane_destroy_setup_dialog_callback, setup_dialog);
  xsane_set_window_icon(setup_dialog, 0);

  /* set the main vbox */
  setup_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(setup_vbox), 0);
  gtk_container_add(GTK_CONTAINER(setup_dialog), setup_vbox);
  gtk_widget_show(setup_vbox);

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(setup_vbox), notebook, FALSE, FALSE, 0);
  gtk_widget_show(notebook);


  xsane_saving_notebook(notebook);
  xsane_filetype_notebook(notebook);
  xsane_printer_notebook(notebook);
  xsane_fax_notebook(notebook);
#ifdef XSANE_ACTIVATE_EMAIL
  xsane_email_notebook(notebook);
#endif
  xsane_ocr_notebook(notebook);
  xsane_display_notebook(notebook);
  xsane_enhance_notebook(notebook);
#ifdef HAVE_LIBLCMS
  xsane_color_management_notebook(notebook);
#endif


  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(setup_vbox), hbox, FALSE, FALSE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_widget_show(hbox);   

  button_box = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(button_box), 0);
  gtk_widget_show(button_box);   

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_setup_dialog_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 8);
  gtk_widget_show(button);

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
  button = gtk_button_new_with_label(BUTTON_OK);
#endif
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_options_ok_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  gtk_widget_show(setup_dialog);

  xsane_update_gamma_curve(TRUE /* update raw */);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
