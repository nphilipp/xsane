/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-batch-scan.c

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
#include "xsane-scan.h"
#include "xsane-batch-scan.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-rc-io.h"
#include "xsane-preview.h"
#include "xsane-gamma.h"

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_batch_scan_create_list_entry(Batch_Scan_Parameters *parameters);

/* ---------------------------------------------------------------------------------------------------------------------- */

#define BOFFSET(field)	((char *) &((Batch_Scan_Parameters *) 0)->field - (char *) 0)

/* ---------------------------------------------------------------------------------------------------------------------- */

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc[] =
  {
    {"name",				xsane_rc_pref_string,	BOFFSET(name)},
    {"scanmode",			xsane_rc_pref_string,	BOFFSET(scanmode)},
    {"tl-x",				xsane_rc_pref_double,	BOFFSET(tl_x)},
    {"tl-y",				xsane_rc_pref_double,	BOFFSET(tl_y)},
    {"br-x",				xsane_rc_pref_double,	BOFFSET(br_x)},
    {"br-y",				xsane_rc_pref_double,	BOFFSET(br_y)},
    {"unit",				xsane_rc_pref_int,	BOFFSET(unit)},
    {"rotation",			xsane_rc_pref_int,	BOFFSET(rotation)},
    {"resolution-x",			xsane_rc_pref_double,	BOFFSET(resolution_x)},
    {"resolution-y",			xsane_rc_pref_double,	BOFFSET(resolution_y)},
    {"bit-depth",			xsane_rc_pref_int,	BOFFSET(bit_depth)},
    {"gamma",				xsane_rc_pref_double,	BOFFSET(gamma)},
    {"gamma-red",			xsane_rc_pref_double,	BOFFSET(gamma_red)},
    {"gamma-green",			xsane_rc_pref_double,	BOFFSET(gamma_green)},
    {"gamma-blue",			xsane_rc_pref_double,	BOFFSET(gamma_blue)},
    {"contrast",			xsane_rc_pref_double,	BOFFSET(contrast)},
    {"contrast-red",			xsane_rc_pref_double,	BOFFSET(contrast_red)},
    {"contrast-green",			xsane_rc_pref_double,	BOFFSET(contrast_green)},
    {"contrast-blue",			xsane_rc_pref_double,	BOFFSET(contrast_blue)},
    {"brightness",			xsane_rc_pref_double,	BOFFSET(brightness)},
    {"brightness-red",			xsane_rc_pref_double,	BOFFSET(brightness_red)},
    {"brightness-green",		xsane_rc_pref_double,	BOFFSET(brightness_green)},
    {"brightness-blue",			xsane_rc_pref_double,	BOFFSET(brightness_blue)},
    {"enhancement-rgb-default",		xsane_rc_pref_int,	BOFFSET(enhancement_rgb_default)},
    {"negative",			xsane_rc_pref_int,	BOFFSET(negative)},
    {"BATCH_END",			xsane_rc_pref_string,	0}
  };

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_get_parameters(Batch_Scan_Parameters *parameters)
{
 char buf[TEXTBUFSIZE];
 SANE_Int unit;

  DBG(DBG_proc, "xsane_batch_scan_get_parameters\n");

  xsane_back_gtk_get_option_double(xsane.well_known.coord[0], &parameters->tl_x, &unit);
  xsane_back_gtk_get_option_double(xsane.well_known.coord[1], &parameters->tl_y, &unit);
  xsane_back_gtk_get_option_double(xsane.well_known.coord[2], &parameters->br_x, &unit);
  xsane_back_gtk_get_option_double(xsane.well_known.coord[3], &parameters->br_y, &unit);

  parameters->unit = unit;

  if (!xsane_back_gtk_get_option_double(xsane.well_known.dpi_x, &parameters->resolution_x, NULL))
  {
    if (xsane_back_gtk_get_option_double(xsane.well_known.dpi_y, &parameters->resolution_y, NULL))
    {
      parameters->resolution_y = parameters->resolution_x;
    }
  }
  else /* only one resolution available */
  {
    xsane_back_gtk_get_option_double(xsane.well_known.dpi, &parameters->resolution_x, NULL);
    parameters->resolution_y = parameters->resolution_x;
  }


  if (xsane_control_option(xsane.dev, xsane.well_known.bit_depth, SANE_ACTION_GET_VALUE, &parameters->bit_depth, 0))
  {
    parameters->bit_depth = -1;
  }

  if (xsane_control_option(xsane.dev, xsane.well_known.scanmode, SANE_ACTION_GET_VALUE, buf, 0))
  {
    parameters->scanmode = NULL;
  }
  else
  {
    parameters->scanmode = strdup(buf);
  }
  
  parameters->rotation    = xsane.preview->rotation;

  parameters->gamma       = xsane.gamma;
  parameters->gamma_red   = xsane.gamma_red;
  parameters->gamma_green = xsane.gamma_green;
  parameters->gamma_blue  = xsane.gamma_blue;

  parameters->brightness       = xsane.brightness;
  parameters->brightness_red   = xsane.brightness_red;
  parameters->brightness_green = xsane.brightness_green;
  parameters->brightness_blue  = xsane.brightness_blue;

  parameters->contrast       = xsane.contrast;
  parameters->contrast_red   = xsane.contrast_red;
  parameters->contrast_green = xsane.contrast_green;
  parameters->contrast_blue  = xsane.contrast_blue;

  parameters->enhancement_rgb_default = xsane.enhancement_rgb_default;
  parameters->negative = xsane.negative;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_batch_scan_establish_parameters(Batch_Scan_Parameters *parameters, int panel_rebuild)
{
 SANE_Int info = 0;

  if ( (parameters->scanmode) && (xsane.batch_scan_use_stored_scanmode) )
  {
    xsane_control_option(xsane.dev, xsane.well_known.scanmode, SANE_ACTION_SET_VALUE, parameters->scanmode, &info);
  }

  xsane_back_gtk_set_option_double(xsane.well_known.coord[0], parameters->tl_x);
  xsane_back_gtk_set_option_double(xsane.well_known.coord[1], parameters->tl_y);
  xsane_back_gtk_set_option_double(xsane.well_known.coord[2], parameters->br_x);
  xsane_back_gtk_set_option_double(xsane.well_known.coord[3], parameters->br_y);

  xsane.scan_rotation = parameters->rotation;

  if (xsane.batch_scan_use_stored_resolution)
  {
    if (!xsane_back_gtk_set_option_double(xsane.well_known.dpi_x, parameters->resolution_x))
    {
      xsane_back_gtk_set_option_double(xsane.well_known.dpi_y, parameters->resolution_y);
    }
    else /* only one resolution */
    {
      xsane_back_gtk_set_option_double(xsane.well_known.dpi, parameters->resolution_x);
    }
  }

  if (xsane.batch_scan_use_stored_bit_depth)
  {
 /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
  }

  if (info & SANE_INFO_RELOAD_OPTIONS)
  {
    xsane_refresh_dialog();
    preview_update_surface(xsane.preview, 0);
  }

  xsane_update_param(0);
  xsane_update_gamma_curve(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_batch_scan_list_item_activated_callback(GtkObject *list_item, gpointer data)

{
 Batch_Scan_Parameters *parameters;

  if (list_item)
  {
    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      xsane_batch_scan_establish_parameters(parameters, TRUE);
    }
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_empty_list(void)
{
 GtkObject *list_item;
 GList *list = GTK_LIST(xsane.batch_scan_list)->children;
 Batch_Scan_Parameters *parameters = NULL;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      free(parameters);
    }

    list = list->next;

    gtk_widget_destroy(GTK_WIDGET(list_item)); /* delete list element */
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_batch_scan_read_parameters(Wire *w, Batch_Scan_Parameters *parameters)
/* returns 0 if ok, otherwise error/eof */
{
 SANE_String name;
 int i;

  DBG(DBG_proc, "batch_scan_read_parameters\n");

  while (1)
  {
    xsane_rc_io_w_space(w, 3);
    if (w->status)
    {
      return -1;
    }

    xsane_rc_io_w_string(w, &name);
    if (w->status || !name)
    {
      return -2;
    }

    if (!strcmp(name, "BATCH_END"))
    {
      return 0; /* ok */
    }

    for (i = 0; i < NELEMS (desc); ++i)
    {
      if (strcmp(name, desc[i].name) == 0)
      {
        DBG(DBG_info2, "reading batch-scan-parameter for %s\n", desc[i].name);
        (*desc[i].codec) (w, parameters, desc[i].offset);
        break;
      }
    }
  }

 return -3; /* we should never come here */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* returns 0 if OK, -1 if file could not be loaded */
int xsane_batch_scan_load_list_from_file(char *filename)
{
 Batch_Scan_Parameters *parameters = NULL;
 int fd;
 int eof = 0;
 Wire w;

  DBG(DBG_proc, "xsane_batch_scan_load_list_from_file(%s)\n", filename);

  xsane_batch_scan_empty_list();

  fd = open(filename, O_RDONLY);

  if (fd > 0)
  {
    w.io.fd = fd;
    w.io.read = read;
    w.io.write = write;
    xsane_rc_io_w_init(&w);
    xsane_rc_io_w_set_dir(&w, WIRE_DECODE);

    while (!eof)
    {
      eof = 1;

      parameters = calloc(1, sizeof(Batch_Scan_Parameters));

      if (parameters)
      {
        eof = xsane_batch_scan_read_parameters(&w, parameters);

        if (!eof)
        {
          xsane_batch_scan_create_list_entry(parameters);
        }
      }
    }
    free(parameters); /* last one is unused */

    xsane_rc_io_w_exit(&w);

    close(fd);

    /* scroll list to beginning */
    gtk_adjustment_set_value(xsane.batch_scan_vadjustment, xsane.batch_scan_vadjustment->lower);
    gtk_adjustment_value_changed(xsane.batch_scan_vadjustment);

   return 0;
  }

 return -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_load_list(void)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_batch_scan_load_list\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_LOAD_BATCH_LIST, xsane.device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", "batch-lists", 0, "default", ".xbl", XSANE_PATH_LOCAL_SANE);

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_OPEN, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_BATCHLIST, XSANE_FILE_FILTER_BATCHLIST))
  {
    if (xsane_batch_scan_load_list_from_file(filename)) /* error while loading file ? */
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
    }
  }

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_write_parameters(Wire *w, Batch_Scan_Parameters *parameters)
{
 int i;

  DBG(DBG_proc, "batch_scan_parameters_save\n");

  for (i = 0; i < NELEMS(desc)-1; ++i)
  {
    DBG(DBG_info2, "saving batch-scan-parameter for %s\n", desc[i].name);
    xsane_rc_io_w_string(w, &desc[i].name);
    (*desc[i].codec) (w, parameters, desc[i].offset);
  }

  xsane_rc_io_w_string(w, &desc[NELEMS(desc)-1].name); /* write BATCH_END */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_save_list(void)
{
 GtkObject *list_item;
 GList *list = GTK_LIST(xsane.batch_scan_list)->children;
 Batch_Scan_Parameters *parameters = NULL;
 int fd;
 Wire w;
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_batch_scan_save_list\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_SAVE_BATCH_LIST, xsane.device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", "batch-lists", 0, "default", ".xbl", XSANE_PATH_LOCAL_SANE);

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SAVE, XSANE_GET_FILENAME_SHOW_NOTHING, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_BATCHLIST, XSANE_FILE_FILTER_BATCHLIST));
  {
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd > 0)
    {
      w.io.fd = fd;
      w.io.read = read;
      w.io.write = write;
      xsane_rc_io_w_init(&w);
      xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

      while (list)
      {
        list_item = GTK_OBJECT(list->data);

        parameters = gtk_object_get_data(list_item, "parameters");

        if (parameters)
        {
          xsane_batch_scan_write_parameters(&w, parameters);
        }

        list = list->next;
      }

      xsane_rc_io_w_set_dir(&w, WIRE_DECODE);	/* flush it out */
      xsane_rc_io_w_exit(&w);

      close(fd);
    }
    else
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
    }
  }

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_scan_list(void)
{
 GtkObject *list_item;
 GList *list = GTK_LIST(xsane.batch_scan_list)->children;
 Batch_Scan_Parameters *parameters = NULL;
 SANE_Int val_start = SANE_TRUE;
 SANE_Int val_loop  = BATCH_MODE_LOOP;
 SANE_Int val_end   = SANE_FALSE;
 SANE_Word val_next_tl_y = SANE_FIX(0.0);

  gtk_list_scroll_vertical(GTK_LIST(xsane.batch_scan_list), GTK_SCROLL_JUMP, 0.0);

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }


  while (list)
  {
    if (!list->next) /* last scan */
    {
      val_loop = BATCH_MODE_LAST_SCAN;
      val_end  = SANE_TRUE;
      val_next_tl_y = SANE_FIX(0.0);
    }
    else /* not last scan */
    {
      parameters = gtk_object_get_data(GTK_OBJECT(list->next->data), "parameters");
      if (parameters)
      {
        val_next_tl_y = SANE_FIX(parameters->tl_y);
      }
    }
    xsane_control_option(xsane.dev, xsane.well_known.batch_scan_start, SANE_ACTION_SET_VALUE, &val_start, NULL);
    xsane_control_option(xsane.dev, xsane.well_known.batch_scan_loop, SANE_ACTION_SET_VALUE, &val_loop, NULL);
    xsane_control_option(xsane.dev, xsane.well_known.batch_scan_end, SANE_ACTION_SET_VALUE, &val_end, NULL);
    xsane_control_option(xsane.dev, xsane.well_known.batch_scan_next_tl_y, SANE_ACTION_SET_VALUE, &val_next_tl_y, NULL);

    val_start = SANE_FALSE;

    xsane.batch_loop = val_loop; /* tell scanning routine if we have more scans */

    list_item = GTK_OBJECT(list->data);

    gtk_list_select_child(GTK_LIST(xsane.batch_scan_list), GTK_WIDGET(list_item));
    /* selecting the child normally does establish the parameters, but not always for the first element! */

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      xsane_batch_scan_establish_parameters(parameters, FALSE);
    }

    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    xsane_scan_dialog(NULL);

    while (xsane.scanning)
    {
      /* we MUST call gtk_events_pending() or gdk_input_add will not work! */
      if (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    if ( (xsane.status_of_last_scan != SANE_STATUS_GOOD) && (xsane.status_of_last_scan != SANE_STATUS_EOF) )
    {
      break; /* cancel or error occured */
    }

    list = list->next;

    gtk_list_scroll_vertical(GTK_LIST(xsane.batch_scan_list), GTK_SCROLL_STEP_FORWARD, 1.0);
  }

  val_start = SANE_FALSE;
  val_loop  = SANE_FALSE;
  val_end   = SANE_FALSE;
  val_next_tl_y = SANE_FIX(0.0);

  /* make sure all batch scan options are reset */
  xsane_control_option(xsane.dev, xsane.well_known.batch_scan_start, SANE_ACTION_SET_VALUE, &val_start, NULL);
  xsane_control_option(xsane.dev, xsane.well_known.batch_scan_loop, SANE_ACTION_SET_VALUE, &val_loop, NULL);
  xsane_control_option(xsane.dev, xsane.well_known.batch_scan_end, SANE_ACTION_SET_VALUE, &val_end, NULL);
  xsane_control_option(xsane.dev, xsane.well_known.batch_scan_next_tl_y, SANE_ACTION_SET_VALUE, &val_next_tl_y, NULL);

  xsane.batch_loop = BATCH_MODE_OFF; /* make sure we reset the batch scan loop flag */

  if (parameters)
  {
    xsane_batch_scan_establish_parameters(parameters, FALSE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_scan_selected(void)
{
 GList *select;
 GtkObject *list_item;
 Batch_Scan_Parameters *parameters = NULL;

  select = GTK_LIST(xsane.batch_scan_list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      xsane_batch_scan_establish_parameters(parameters, TRUE);
    }

    xsane.batch_loop = BATCH_MODE_LAST_SCAN; /* to make sure we do not scan multiple times */

    xsane_scan_dialog(NULL);

    xsane.batch_loop = BATCH_MODE_OFF; /* make sure we reset the batch scan loop flag */
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_delete(void)
{
 GList *select;
 GtkObject *list_item;
 Batch_Scan_Parameters *parameters = NULL;

  DBG(DBG_proc, "xsane_batch_scan_delete\n");

  select = GTK_LIST(xsane.batch_scan_list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      free(parameters);
    }

    gtk_widget_destroy(GTK_WIDGET(list_item));
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_update_label(Batch_Scan_Parameters *parameters)
{
  char buf[TEXTBUFSIZE];
  const char *unit_str;
  double tl_x = parameters->tl_x;
  double tl_y = parameters->tl_y;
  double br_x = parameters->br_x;
  double br_y = parameters->br_y;
  char areaname_str[100];
  char geometry_tl_str[100];
  char geometry_size_str[100];
  char scanmode_str[100];
  char resolution_str[100];
  char bit_depth_str[100];


  snprintf(areaname_str, sizeof(areaname_str), "%s %s", TEXT_BATCH_LIST_AREANAME, parameters->name);

  if (xsane.batch_scan_use_stored_scanmode)
  {
    snprintf(scanmode_str, sizeof(scanmode_str), "%s %s", TEXT_BATCH_LIST_SCANMODE, parameters->scanmode);
  }
  else
  {
    snprintf(scanmode_str, sizeof(scanmode_str), "%s %s", TEXT_BATCH_LIST_SCANMODE, TEXT_BATCH_LIST_BY_GUI);
  }

  unit_str = xsane_back_gtk_unit_string(parameters->unit);
  if (parameters->unit == SANE_UNIT_MM)
  {
    tl_x /= preferences.length_unit;
    tl_y /= preferences.length_unit;
    br_x /= preferences.length_unit;
    br_y /= preferences.length_unit;
  }
  snprintf(geometry_tl_str, sizeof(geometry_tl_str), "%s %0.2f %s, %0.2f %s", TEXT_BATCH_LIST_GEOMETRY_TL,
                                                   tl_x, unit_str, tl_y, unit_str);

  snprintf(geometry_size_str, sizeof(geometry_size_str), "%s %0.2f %s x %0.2f %s", TEXT_BATCH_LIST_GEOMETRY_SIZE,
                                                   br_x - tl_x, unit_str, br_y - tl_y, unit_str);

  if (xsane.batch_scan_use_stored_resolution)
  {
    snprintf(resolution_str, sizeof(resolution_str), "%s %3.0f dpi x %3.0f dpi", TEXT_BATCH_LIST_RESOLUTION,
             parameters->resolution_x, parameters->resolution_y);
  }
  else
  {
    snprintf(resolution_str, sizeof(resolution_str), "%s %s", TEXT_BATCH_LIST_RESOLUTION, TEXT_BATCH_LIST_BY_GUI);
  }

  if (xsane.batch_scan_use_stored_bit_depth)
  {
    snprintf(bit_depth_str, sizeof(bit_depth_str), "%s %d", TEXT_BATCH_LIST_BIT_DEPTH, parameters->bit_depth);
  }
  else
  {
    snprintf(bit_depth_str, sizeof(bit_depth_str), "%s %s", TEXT_BATCH_LIST_BIT_DEPTH, TEXT_BATCH_LIST_BY_GUI);
  }

  snprintf(buf, sizeof(buf), "%s\n" /* name */
                             "%s\n" /* scanmode */
                             "%s\n" /* geometry_tl */
                             "%s\n" /* geometry_size */
                             "%s\n" /* resolution */
                             "%s",  /* bit_depth */
       areaname_str,
       scanmode_str,
       geometry_tl_str,
       geometry_size_str,
       resolution_str,
       bit_depth_str);

  gtk_label_set_text(GTK_LABEL(parameters->label), buf);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_batch_scan_create_list_entry(Batch_Scan_Parameters *parameters)
{
 GtkWidget *list_item;
 GtkWidget *hbox;
 int size = 120;
 char *data;

  list_item = gtk_list_item_new();

  hbox = gtk_hbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(list_item), hbox);
  gtk_widget_show(hbox);

  data = calloc(size, size);

  parameters->gtk_preview = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(parameters->gtk_preview), size, size);
  gtk_box_pack_start(GTK_BOX(hbox), parameters->gtk_preview, FALSE, FALSE, 0);
  gtk_widget_show(parameters->gtk_preview);

  parameters->gtk_preview_size = size;

  preview_create_batch_icon(xsane.preview, parameters);


  parameters->label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), parameters->label, FALSE, FALSE, 0);
  gtk_label_set_justify(GTK_LABEL(parameters->label), GTK_JUSTIFY_LEFT);
  gtk_widget_show(parameters->label);
  xsane_batch_scan_update_label(parameters);

  gtk_object_set_data(GTK_OBJECT(list_item), "parameters", parameters);
  g_signal_connect(GTK_OBJECT(list_item), "select", (GtkSignalFunc) xsane_batch_scan_list_item_activated_callback, NULL);
  gtk_container_add(GTK_CONTAINER(xsane.batch_scan_list), list_item);
  gtk_widget_show(list_item);

 return(list_item);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_batch_scan_add()
{
 Batch_Scan_Parameters *parameters;
 GtkWidget *list_item;
 
  DBG(DBG_proc, "xsane_batch_scan_add\n");

  parameters = calloc(1, sizeof(Batch_Scan_Parameters));

  if (parameters)
  {
    xsane_batch_scan_get_parameters(parameters);

    parameters->name = strdup(TEXT_BATCH_AREA_DEFAULT_NAME);
  }

  list_item = xsane_batch_scan_create_list_entry(parameters);

  /* scroll list to end */
  gtk_adjustment_set_value(xsane.batch_scan_vadjustment, xsane.batch_scan_vadjustment->upper);
  gtk_adjustment_value_changed(xsane.batch_scan_vadjustment);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_batch_scan_update_label_list(void)
{
 GtkObject *list_item;
 GList *list = GTK_LIST(xsane.batch_scan_list)->children;
 Batch_Scan_Parameters *parameters = NULL;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      xsane_batch_scan_update_label(parameters);
    }

    list = list->next;
  }

  gtk_widget_queue_draw(xsane.batch_scan_list); /* update gtk_pixmap widgets */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_batch_scan_update_icon_list(void)
{
 GtkObject *list_item;
 GList *list = GTK_LIST(xsane.batch_scan_list)->children;
 Batch_Scan_Parameters *parameters = NULL;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      preview_create_batch_icon(xsane.preview, parameters);
    }

    list = list->next;
  }

  gtk_widget_queue_draw(xsane.batch_scan_list); /* update gtk_pixmap widgets */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_rotate_mirror(GtkWidget *widget, gpointer data)
{
 GList *select;
 GtkObject *list_item;
 Batch_Scan_Parameters *parameters = NULL;
 int rotate_info = (int) data;
 int rotate, mirror;

  DBG(DBG_proc, "xsane_batch_scan_rotate_mirror\n");

  select = GTK_LIST(xsane.batch_scan_list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      rotate = parameters->rotation & 3;
      mirror = parameters->rotation & 4;

      if (parameters->rotation < 4)
      {
        parameters->rotation = ( (rotate + rotate_info) & 3 ) + ( (mirror+ rotate_info) & 4);
      }
      else 
      {
        parameters->rotation = ( (rotate - rotate_info) & 3 ) + ( (mirror+ rotate_info) & 4);
      }

      xsane_batch_scan_update_icon_list();
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */
int xsane_batch_scan_rename;

static void xsane_batch_scan_rename_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_batch_scan_rename\n");

  xsane_batch_scan_rename = (int) data;
}


static void xsane_batch_scan_rename_callback(GtkWidget *widget, gpointer data)
{
 GList *select;
 GtkObject *list_item;
 Batch_Scan_Parameters *parameters = NULL;
 GtkWidget *rename_dialog;
 GtkWidget *text;
 GtkWidget *button;
 GtkWidget *vbox, *hbox;
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_batch_scan_rename\n");

  select = GTK_LIST(xsane.batch_scan_list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);

    parameters = gtk_object_get_data(list_item, "parameters");

    if (parameters)
    {
      xsane_set_sensitivity(FALSE);

      rename_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      xsane_set_window_icon(rename_dialog, 0);

      /* set the main vbox */
      vbox = gtk_vbox_new(FALSE, 10);
      gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
      gtk_container_add(GTK_CONTAINER(rename_dialog), vbox);
      gtk_widget_show(vbox);

      /* set the main hbox */
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
      gtk_widget_show(hbox);

      gtk_window_set_position(GTK_WINDOW(rename_dialog), GTK_WIN_POS_CENTER);
      gtk_window_set_resizable(GTK_WINDOW(rename_dialog), FALSE);
      snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_BATCH_RENAME);
      gtk_window_set_title(GTK_WINDOW(rename_dialog), buf);
      g_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_batch_scan_rename_button_callback,(void *) -1);
      gtk_widget_show(rename_dialog);

      text = gtk_entry_new();
      xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_BATCH_RENAME);
      gtk_entry_set_max_length(GTK_ENTRY(text), 64);
      gtk_entry_set_text(GTK_ENTRY(text), parameters->name);
      gtk_widget_set_size_request(text, 300, -1);
      gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
      gtk_widget_show(text);


#ifdef HAVE_GTK2
      button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
      button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
      g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_rename_button_callback, (void *) -1);
      gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
      gtk_widget_show(button);


#ifdef HAVE_GTK2
      button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
      button = gtk_button_new_with_label(BUTTON_OK);
#endif
      GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
      g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_rename_button_callback, (void *) 1);
      gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
      gtk_widget_grab_default(button);
      gtk_widget_show(button);


      xsane_batch_scan_rename = 0;

      while (xsane_batch_scan_rename == 0)
      {
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }
      }

      if (xsane_batch_scan_rename == 1)
      {
        free(parameters->name);
        parameters->name = strdup(gtk_entry_get_text(GTK_ENTRY(text)));
        xsane_batch_scan_update_label(parameters);
      }
      gtk_widget_destroy(rename_dialog);

      xsane_set_sensitivity(TRUE);
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_batch_scan_win_delete(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_batch_scan_win_delete\n");

  if (preferences.show_batch_scan)
  {
    xsane_window_get_position(xsane.batch_scan_dialog, &xsane.batch_dialog_posx, &xsane.batch_dialog_posy);
    gtk_window_move(GTK_WINDOW(xsane.batch_scan_dialog), xsane.batch_dialog_posx, xsane.batch_dialog_posy);
  }

  gtk_widget_hide(widget);
  preferences.show_batch_scan = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_batch_scan_widget), preferences.show_batch_scan);
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_batch_scan_dialog(const char *devicetext)
{
 GtkWidget *batch_scan_vbox, *button, *scrolled_window;
 char buf[64];

  DBG(DBG_proc, "xsane_batch_scan_dialog\n");

  xsane.batch_scan_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  xsane_set_window_icon(xsane.batch_scan_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(xsane.batch_scan_dialog), xsane.accelerator_group);

  snprintf(buf, sizeof(buf), "%s %s", WINDOW_BATCH_SCAN, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.batch_scan_dialog), buf);
  gtk_widget_set_size_request(xsane.batch_scan_dialog, 400, -1);

  g_signal_connect(GTK_OBJECT(xsane.batch_scan_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_batch_scan_win_delete), NULL);

  /* set the main vbox */
  batch_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(batch_scan_vbox), 5);
  gtk_container_add(GTK_CONTAINER(xsane.batch_scan_dialog), batch_scan_vbox);
  gtk_widget_show(batch_scan_vbox);

  /* set the hbox for load/save */
  xsane.batch_scan_button_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(batch_scan_vbox), xsane.batch_scan_button_box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(xsane.batch_scan_button_box), 0);
  gtk_widget_show(xsane.batch_scan_button_box); 

  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, empty_batch_xpm, DESC_BATCH_LIST_EMPTY, (GtkSignalFunc) xsane_batch_scan_empty_list, NULL);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, load_xpm,        DESC_BATCH_LIST_LOAD,  (GtkSignalFunc) xsane_batch_scan_load_list,  NULL);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, save_xpm,        DESC_BATCH_LIST_SAVE,  (GtkSignalFunc) xsane_batch_scan_save_list,  NULL);

  xsane_vseparator_new(xsane.batch_scan_button_box, 3);

  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, add_batch_xpm,   DESC_BATCH_ADD, (GtkSignalFunc) xsane_batch_scan_add,    NULL);

  xsane_vseparator_new(xsane.batch_scan_button_box, 3);

  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, del_batch_xpm,   DESC_BATCH_DEL, (GtkSignalFunc) xsane_batch_scan_delete, NULL);

  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, ascii_xpm,       DESC_BATCH_RENAME, (GtkSignalFunc) xsane_batch_scan_rename_callback, NULL);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, rotate90_xpm,    DESC_ROTATE90,     (GtkSignalFunc) xsane_batch_scan_rotate_mirror, (void *) 1);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, rotate180_xpm,   DESC_ROTATE180,    (GtkSignalFunc) xsane_batch_scan_rotate_mirror, (void *) 2);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, rotate270_xpm,   DESC_ROTATE270,    (GtkSignalFunc) xsane_batch_scan_rotate_mirror, (void *) 3);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, mirror_x_xpm,    DESC_MIRROR_X,     (GtkSignalFunc) xsane_batch_scan_rotate_mirror, (void *) 4);
  xsane_button_new_with_pixmap(xsane.batch_scan_dialog->window, xsane.batch_scan_button_box, mirror_y_xpm,    DESC_MIRROR_Y,     (GtkSignalFunc) xsane_batch_scan_rotate_mirror, (void *) 6);


  /* the scolled window with the list */
  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 400, 200);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(batch_scan_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  xsane.batch_scan_list = gtk_list_new();

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), xsane.batch_scan_list);
 
  xsane.batch_scan_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
  gtk_container_set_focus_vadjustment(GTK_CONTAINER(xsane.batch_scan_list), xsane.batch_scan_vadjustment);

  gtk_widget_show(xsane.batch_scan_list);


  xsane_separator_new(batch_scan_vbox, 2);


  /* set the hbox for scan all / scan selected */
  xsane.batch_scan_action_box = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(batch_scan_vbox), xsane.batch_scan_action_box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(xsane.batch_scan_action_box), 0);
  gtk_widget_show(xsane.batch_scan_action_box); 

  button = gtk_button_new_with_label(BUTTON_BATCH_LIST_SCAN);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_scan_list, NULL);
  gtk_box_pack_start(GTK_BOX(xsane.batch_scan_action_box), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_BATCH_AREA_SCAN);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_scan_selected, NULL);
  gtk_box_pack_start(GTK_BOX(xsane.batch_scan_action_box), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_realize(xsane.batch_scan_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
