/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
 
   xsane-viewer.c
 
   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2004 Oliver Rauch
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
#include "xsane-viewer.h"
#include "xsane-gamma.h"
#include "xsane-icons.h"
#include "xsane-save.h"
#include <gdk/gdkkeysyms.h>
#include <sys/wait.h>
 
 
#ifndef PATH_MAX
# define PATH_MAX       1024
#endif
 
/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_viewer_zoom[] = {9, 13, 18, 25, 35, 50, 71, 100, 141, 200, 282, 400 };

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_viewer_close_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_dialog_cancel(GtkWidget *window, gpointer data);
static void xsane_viewer_save_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_ocr_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_clone_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_despeckle_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_blur_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_scale_image(GtkWidget *window, gpointer data);
static void xsane_viewer_despeckle_image(GtkWidget *window, gpointer data);
static void xsane_viewer_blur_image(GtkWidget *window, gpointer data);
static void xsane_viewer_rotate(Viewer *v, int rotation);
static void xsane_viewer_rotate90_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_rotate180_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_rotate270_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_mirror_x_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_mirror_y_callback(GtkWidget *window, gpointer data);
static GtkWidget *xsane_viewer_files_build_menu(Viewer *v);
static GtkWidget *xsane_viewer_filters_build_menu(Viewer *v);
static int xsane_viewer_read_image(Viewer *v);
Viewer *xsane_viewer_new(char *filename, int reduce_to_lineart, char *output_filename);

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_viewer_close_callback(GtkWidget *widget, gpointer data)
{
 Viewer *v, *list, **prev_list;

  v = (Viewer*) gtk_object_get_data(GTK_OBJECT(widget), "Viewer");

  DBG(DBG_proc, "xsane_viewer_close_callback\n");

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_close_callback: actions are blocked\n");
   return TRUE;
  }

  v->block_actions = TRUE;

  if (!v->image_saved)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), WARN_VIEWER_IMAGE_NOT_SAVED);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);
    if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_DO_NOT_CLOSE, BUTTON_DISCARD_IMAGE, TRUE /* wait */))
    {
      gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
      v->block_actions = FALSE;
      return TRUE;
    } 
  }

  remove(v->filename);

  gtk_widget_destroy(v->top);


  list = xsane.viewer_list;
  prev_list = &xsane.viewer_list;

  while (list)
  {
    if (list == v)
    {
      DBG(DBG_info, "removing viewer from viewer list\n");
      *prev_list = list->next_viewer;
      break;
    }

    prev_list = &list->next_viewer;
    list = list->next_viewer;
  }

  if (v->active_dialog)
  {
    gtk_widget_destroy(v->active_dialog);
  }

  free(v);

 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_dialog_cancel(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->active_dialog = NULL;
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_save_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 char outputfilename[1024];
 char *inputfilename;
 char windowname[256];
 int output_format;
 char *filetype = NULL;

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_save_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_save_callback\n");

  v->block_actions = TRUE;
  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  if (v->output_filename)
  {
    strncpy(outputfilename, v->output_filename, sizeof(outputfilename));
  }
  else
  {
   int abort = 0;

    strncpy(outputfilename, preferences.filename, sizeof(outputfilename));
 
    snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_VIEWER_OUTPUT_FILENAME, xsane.device_text);
 
    umask((mode_t) preferences.directory_umask); /* define new file permissions */
    abort = xsane_back_gtk_get_filename(windowname, outputfilename, sizeof(outputfilename), outputfilename, &filetype, TRUE, TRUE, FALSE, TRUE);
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */ 

    if (abort)
    {
      gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
      v->block_actions = FALSE;
     return;
    }
  }

  if (preferences.overwrite_warning)  /* test if filename already used */
  {
   FILE *testfile;

    testfile = fopen(outputfilename, "rb"); /* read binary (b for win32) */
    if (testfile) /* filename used: skip */
    {
     char buf[256];
 
      fclose(testfile);
      snprintf(buf, sizeof(buf), WARN_FILE_EXISTS, outputfilename);
      if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_OVERWRITE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
      {
        gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
        v->block_actions = FALSE;
       return;
      }
    }
  } 

  inputfilename = strdup(v->filename);

  output_format = xsane_identify_output_format(outputfilename, filetype, 0);

  if (v->reduce_to_lineart) /* reduce grayscale image to lineart before saving */
  {
   char dummyfilename[1024];

    if (output_format != XSANE_PNM)
    {
      xsane_back_gtk_make_path(sizeof(dummyfilename), dummyfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);
    }
    else /* pbm: no further conversion, we save to destination filename */
    {
      snprintf(dummyfilename, sizeof(dummyfilename), "%s", outputfilename);
      if (xsane_create_secure_file(dummyfilename)) /* remove possibly existing symbolic links for security */
      {
       char buf[256];

        snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, dummyfilename);
        xsane_back_gtk_error(buf, TRUE);
        v->block_actions = FALSE;
       return; /* error */
      }
    }

    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_PACKING_DATA);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

    /* the outputfile always is a temporary file, so we do not have to care about symlinks here */
    xsane_save_image_as_lineart(v->filename, dummyfilename, v->progress_bar, &v->cancel_save);

    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
    gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

    free(inputfilename);
    inputfilename = strdup(dummyfilename);
  }


  if ((!v->reduce_to_lineart) || (output_format != XSANE_PNM)) /* pbm already is saved above, otherwise save now */
  {
    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_SAVING_DATA);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

    xsane_save_image_as(inputfilename, outputfilename, output_format, v->progress_bar, &v->cancel_save);

    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
    gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);
  }

  free(inputfilename);

  v->image_saved = TRUE;

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_ocr_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 char outputfilename[1024];
 char *extensionptr;
 char windowname[256];
 int abort = 0;

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_ocr_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_ocr_callback\n");

  v->block_actions = TRUE;
  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  strncpy(outputfilename, preferences.filename, sizeof(outputfilename)-5);

  extensionptr = strchr(outputfilename, '.');
  if (!extensionptr)
  {
    extensionptr=outputfilename + strlen(outputfilename);
  }
  strcpy(extensionptr, ".txt");
 
  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_OCR_OUTPUT_FILENAME, xsane.device_text);
 
  umask((mode_t) preferences.directory_umask); /* define new file permissions */
  abort = xsane_back_gtk_get_filename(windowname, outputfilename, sizeof(outputfilename), outputfilename, NULL, TRUE, TRUE, FALSE, FALSE);
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */ 

  if (abort)
  {
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;
   return;
  }

  while (gtk_events_pending()) /* give gtk the chance to remove the file selection dialog */
  {
    gtk_main_iteration();
  }

  xsane_save_image_as_text(v->filename, outputfilename, v->progress_bar, &v->cancel_save);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_clone_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Image_info image_info;

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_clone_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_clone_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);
  v->block_actions = TRUE;

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "cloning image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_CLONING_DATA);
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_rotate_image(outfile, infile, &image_info, 0, v->progress_bar, &v->cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

  xsane_viewer_new(outfilename, v->reduce_to_lineart, NULL);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_adjustment_float_changed(GtkAdjustment *adj_data, float *val)
{
  *val = (float) adj_data->value;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_adjustment_int_changed(GtkAdjustment *adj_data, int *val)
{
  *val = (int) adj_data->value;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_button_changed(GtkWidget *button, int *val)
{
  *val = GTK_TOGGLE_BUTTON(button)->active;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_scale_set_scale_value_and_adjustments(GtkAdjustment *adj_data, double *scale_val)
{
 GtkAdjustment *adj;
 int image_width, image_height;

  *scale_val = adj_data->value;

  image_width  = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_width");
  image_height = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_height");

  adj = (GtkAdjustment*) gtk_object_get_data(GTK_OBJECT(adj_data), "size-x-adjustment");
  if ((adj) && (image_width))
  {
    gtk_adjustment_set_value(adj, (*scale_val) * image_width);
  }

  adj = (GtkAdjustment*) gtk_object_get_data(GTK_OBJECT(adj_data), "size-y-adjustment");
  if ((adj) && (image_height))
  {
    gtk_adjustment_set_value(adj, (*scale_val) * image_height);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_scale_set_size_x_value_and_adjustments(GtkAdjustment *adj_data, double *scale_val)
{
 GtkAdjustment *adj;
 int image_width, image_height;

  image_width  = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_width");
  image_height = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_height");

  if (!image_width)
  {
    return; /* we are not able to calulate the scale value */
  }

  *scale_val = adj_data->value / image_width;

  adj = (GtkAdjustment*) gtk_object_get_data(GTK_OBJECT(adj_data), "scale-adjustment");
  if (adj)
  {
    gtk_adjustment_set_value(adj, *scale_val);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_scale_set_size_y_value_and_adjustments(GtkAdjustment *adj_data, double *scale_val)
{
 GtkAdjustment *adj;
 int image_width, image_height;

  image_width  = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_width");
  image_height = (int) gtk_object_get_data(GTK_OBJECT(adj_data), "image_height");

  if (!image_height)
  {
    return; /* we are not able to calulate the scale value */
  }

  *scale_val = adj_data->value / image_height;

  adj = (GtkAdjustment*) gtk_object_get_data(GTK_OBJECT(adj_data), "scale-adjustment");
  if (adj)
  {
    gtk_adjustment_set_value(adj, *scale_val);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_scale_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 GtkWidget *selection_dialog;
 GtkWidget *frame;
 GtkWidget *hbox, *vbox;
 GtkWidget *button;
 GtkObject *scale_widget, *scalex_widget, *scaley_widget;
 GtkAdjustment *adjustment_size_x;
 GtkAdjustment *adjustment_size_y;
 GtkWidget *spinbutton;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 GtkWidget *pixmapwidget;
 char buf[256];
 FILE *infile;
 Image_info image_info;

  if (v->block_actions == TRUE) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_scale_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_scale_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);
  v->block_actions = 2; /* do not set it to TRUE because we have to recall this dialog! */

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  fclose(infile);


  if (v->output_filename)
  {
    snprintf(buf, sizeof(buf), "%s: %s", WINDOW_SCALE, v->output_filename); 
  }
  else
  {
    snprintf(buf, sizeof(buf), WINDOW_SCALE); 
  }

  if (v->active_dialog) /* use active dialog */
  {
    selection_dialog = v->active_dialog;
    gtk_container_foreach(GTK_CONTAINER(selection_dialog), (GtkCallback) gtk_widget_destroy, NULL);
    if (!v->bind_scale)
    {
      v->y_scale_factor = v->x_scale_factor;
    }
  }
  else /* first time the dialog is opened */
  {
    selection_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(selection_dialog), FALSE);
    gtk_window_set_position(GTK_WINDOW(selection_dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_title(GTK_WINDOW(selection_dialog), buf);
    xsane_set_window_icon(selection_dialog, 0);
    g_signal_connect(GTK_OBJECT(selection_dialog), "destroy", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);

    v->active_dialog = selection_dialog;
    v->x_scale_factor = 1.0;
    v->y_scale_factor = 1.0;
    v->bind_scale = TRUE;
  }

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(selection_dialog), frame);
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 4); 
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* bind scale */
  button = gtk_check_button_new_with_label(BUTTON_SCALE_BIND);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), v->bind_scale);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_viewer_button_changed, (void *) &v->bind_scale);
  g_signal_connect_after(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_viewer_scale_callback, (void *) v);
  gtk_widget_show(button);

  if (v->bind_scale)
  {
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    /* scale factor: <-> */
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(hbox), zoom_xpm, 
                                DESC_SCALE_FACTOR,
                                0.01, 4.0, 0.01, 0.1, 2, &v->x_scale_factor, &scale_widget,
                                0, xsane_viewer_scale_set_scale_value_and_adjustments,
                                TRUE);

    /* x-size */
    pixmap = gdk_pixmap_create_from_xpm_d(selection_dialog->window, &mask, xsane.bg_trans, (gchar **) size_x_xpm);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 20);
    gtk_widget_show(pixmapwidget);
    gdk_drawable_unref(pixmap);

    adjustment_size_x = (GtkAdjustment *) gtk_adjustment_new(v->x_scale_factor * image_info.image_width , 0.01 * image_info.image_width, 4.0 * image_info.image_width, 1.0, 5.0, 0.0);
    spinbutton = gtk_spin_button_new(adjustment_size_x, 0, 0);
    g_signal_connect(GTK_OBJECT(adjustment_size_x), "value_changed", (GtkSignalFunc) xsane_viewer_scale_set_size_x_value_and_adjustments, (void *) &v->x_scale_factor);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    gtk_widget_set_size_request(spinbutton, 80, -1);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_widget_show(spinbutton);
    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, DESC_SCALE_WIDTH); 

    /* y-size */
    pixmap = gdk_pixmap_create_from_xpm_d(selection_dialog->window, &mask, xsane.bg_trans, (gchar **) size_y_xpm);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 20);
    gtk_widget_show(pixmapwidget);
    gdk_drawable_unref(pixmap);

    adjustment_size_y = (GtkAdjustment *) gtk_adjustment_new(v->x_scale_factor * image_info.image_height , 0.01 * image_info.image_height, 4.0 * image_info.image_height, 1.0, 5.0, 0.0);
    spinbutton = gtk_spin_button_new(adjustment_size_y, 0, 0);
    g_signal_connect(GTK_OBJECT(adjustment_size_y), "value_changed", (GtkSignalFunc) xsane_viewer_scale_set_size_y_value_and_adjustments, (void *) &v->x_scale_factor);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    gtk_widget_set_size_request(spinbutton, 80, -1);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_widget_show(spinbutton);
    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, DESC_SCALE_HEIGHT); 

    gtk_object_set_data(GTK_OBJECT(scale_widget), "size-x-adjustment", (void *) adjustment_size_x);
    gtk_object_set_data(GTK_OBJECT(scale_widget), "size-y-adjustment", (void *) adjustment_size_y);
    gtk_object_set_data(GTK_OBJECT(scale_widget), "image_width",       (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(scale_widget), "image_height",      (void *) image_info.image_height);

    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "scale-adjustment",   (void *) scale_widget);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "size-y-adjustment",  (void *) adjustment_size_y);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "image_width",        (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "image_height",       (void *) image_info.image_height);

    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "scale-adjustment",   (void *) scale_widget);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "size-x-adjustment",  (void *) adjustment_size_x);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "image_width",        (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "image_height",       (void *) image_info.image_height);
  }
  else
  {
    /* X */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    /* x_scale factor: <-> */
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(hbox), zoom_x_xpm,
                                DESC_X_SCALE_FACTOR,
                                0.01, 4.0, 0.01, 0.1, 2, &v->x_scale_factor, &scalex_widget,
                                0, xsane_viewer_scale_set_scale_value_and_adjustments,
                                TRUE);

    /* x-size */
    pixmap = gdk_pixmap_create_from_xpm_d(selection_dialog->window, &mask, xsane.bg_trans, (gchar **) size_x_xpm);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 20);
    gtk_widget_show(pixmapwidget);
    gdk_drawable_unref(pixmap);

    adjustment_size_x = (GtkAdjustment *) gtk_adjustment_new(v->x_scale_factor * image_info.image_width , 0.01 * image_info.image_width, 4.0 * image_info.image_width, 1.0, 5.0, 0.0);
    spinbutton = gtk_spin_button_new(adjustment_size_x, 0, 0);
    g_signal_connect(GTK_OBJECT(adjustment_size_x), "value_changed", (GtkSignalFunc) xsane_viewer_scale_set_size_x_value_and_adjustments, (void *) &v->x_scale_factor);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    gtk_widget_set_size_request(spinbutton, 80, -1);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_widget_show(spinbutton);
    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, DESC_SCALE_WIDTH); 

    gtk_object_set_data(GTK_OBJECT(scalex_widget), "size-x-adjustment",  (void *) adjustment_size_x);
    gtk_object_set_data(GTK_OBJECT(scalex_widget), "image_width",        (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(scalex_widget), "image_height",       (void *) image_info.image_height);

    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "scale-adjustment",   (void *) scalex_widget);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "image_width",        (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_x), "image_height",       (void *) image_info.image_height);


    /* Y */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    /* y_scale factor: <-> */
    xsane_range_new_with_pixmap(xsane.xsane_window->window, GTK_BOX(hbox), zoom_y_xpm,
                                DESC_Y_SCALE_FACTOR,
                                0.01, 4.0, 0.01, 0.1, 2, &v->y_scale_factor, &scaley_widget,
                                0, xsane_viewer_scale_set_scale_value_and_adjustments,
                                TRUE);

    /* y-size */
    pixmap = gdk_pixmap_create_from_xpm_d(selection_dialog->window, &mask, xsane.bg_trans, (gchar **) size_y_xpm);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 20);
    gtk_widget_show(pixmapwidget);
    gdk_drawable_unref(pixmap);

    adjustment_size_y = (GtkAdjustment *) gtk_adjustment_new(v->y_scale_factor * image_info.image_height , 0.01 * image_info.image_height, 4.0 * image_info.image_height, 1.0, 5.0, 0.0);
    spinbutton = gtk_spin_button_new(adjustment_size_y, 0, 0);
    g_signal_connect(GTK_OBJECT(adjustment_size_y), "value_changed", (GtkSignalFunc) xsane_viewer_scale_set_size_y_value_and_adjustments, (void *) &v->y_scale_factor);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    gtk_widget_set_size_request(spinbutton, 80, -1);
    gtk_box_pack_start(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 0);
    gtk_widget_show(spinbutton);
    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, DESC_SCALE_HEIGHT); 

    gtk_object_set_data(GTK_OBJECT(scaley_widget), "size-y-adjustment", (void *) adjustment_size_y);
    gtk_object_set_data(GTK_OBJECT(scaley_widget), "image_width",       (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(scaley_widget), "image_height",      (void *) image_info.image_height);

    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "scale-adjustment",   (void *) scaley_widget);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "image_width",        (void *) image_info.image_width);
    gtk_object_set_data(GTK_OBJECT(adjustment_size_y), "image_height",       (void *) image_info.image_height);
  }

  /* Apply Cancel */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_scale_image, (void *) v);
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked", (GtkSignalFunc) gtk_widget_destroy, (GtkObject *) selection_dialog);

  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_CANCEL);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked", (GtkSignalFunc) gtk_widget_destroy, (GtkObject *) selection_dialog);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_show(button);

  gtk_widget_show(selection_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_despeckle_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 GtkWidget *selection_dialog;
 GtkWidget *frame;
 GtkWidget *hbox, *vbox;
 GtkWidget *label, *spinbutton, *button;
 GtkAdjustment *adjustment;
 char buf[256];

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_despeckle_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_despeckle_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);
  v->block_actions = TRUE;

  if (v->output_filename)
  {
    snprintf(buf, sizeof(buf), "%s: %s", WINDOW_DESPECKLE, v->output_filename); 
  }
  else
  {
    snprintf(buf, sizeof(buf), WINDOW_DESPECKLE); 
  }

  selection_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(selection_dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title(GTK_WINDOW(selection_dialog), buf);
  xsane_set_window_icon(selection_dialog, 0);
  g_signal_connect(GTK_OBJECT(selection_dialog), "destroy", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);

  v->active_dialog = selection_dialog;

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(selection_dialog), frame);
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 4); 
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* Despeckle radius: <-> */

  v->despeckle_radius = 2;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_DESPECKLE_RADIUS); 
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);
  gtk_widget_show(label);

  adjustment = (GtkAdjustment *) gtk_adjustment_new(2.0, 2.0, 10.0, 1.0, 5.0, 0.0);
  spinbutton = gtk_spin_button_new(adjustment, 0, 0);
  g_signal_connect(GTK_OBJECT(adjustment), "value_changed", (GtkSignalFunc) xsane_viewer_adjustment_int_changed, (void *) &v->despeckle_radius);
  gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), TRUE);
  gtk_box_pack_end(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 10);
  gtk_widget_show(spinbutton);

  /* Apply Cancel */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_despeckle_image, (void *) v);
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked", (GtkSignalFunc) gtk_widget_destroy, (GtkObject *) selection_dialog);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_CANCEL);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked", (GtkSignalFunc) gtk_widget_destroy, (GtkObject *) selection_dialog);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_show(button);

  gtk_widget_show(selection_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_blur_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 GtkWidget *selection_dialog;
 GtkWidget *frame;
 GtkWidget *hbox, *vbox;
 GtkWidget *label, *spinbutton, *button;
 GtkAdjustment *adjustment;
 char buf[256];

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_blur_callback: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_blur_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);
  v->block_actions = TRUE;

  if (v->output_filename)
  {
    snprintf(buf, sizeof(buf), "%s: %s", WINDOW_BLUR, v->output_filename); 
  }
  else
  {
    snprintf(buf, sizeof(buf), WINDOW_BLUR); 
  }

  selection_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(selection_dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title(GTK_WINDOW(selection_dialog), buf);
  xsane_set_window_icon(selection_dialog, 0);
  g_signal_connect(GTK_OBJECT(selection_dialog), "destroy", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);

  v->active_dialog = selection_dialog;

  frame = gtk_frame_new(0);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(selection_dialog), frame);
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 4); 
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);


  /* Blur radius: <-> */

  v->blur_radius = 1.0;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show(hbox);

  label = gtk_label_new(TEXT_BLUR_RADIUS); 
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);
  gtk_widget_show(label);

  adjustment = (GtkAdjustment *) gtk_adjustment_new(1.0, 1.0, 20.0, 0.1, 1.0, 0.0);
  spinbutton = gtk_spin_button_new(adjustment, 0, 2);
  g_signal_connect(GTK_OBJECT(adjustment), "value_changed", (GtkSignalFunc) xsane_viewer_adjustment_float_changed, (void *) &v->blur_radius);
  gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), TRUE);
  gtk_box_pack_end(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 10);
  gtk_widget_show(spinbutton);

  /* Apply Cancel */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4); 
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  button = gtk_button_new_with_label(BUTTON_APPLY);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_blur_image, (void *) v);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_CANCEL);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_viewer_dialog_cancel, (void *) v);
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked", (GtkSignalFunc) gtk_widget_destroy, (GtkObject *) selection_dialog);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_show(button);

  gtk_widget_show(selection_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_scale_image(GtkWidget *window, gpointer data)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Viewer *v = (Viewer *) data;
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_scale_image\n");

  gtk_widget_destroy(v->active_dialog);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "scaling image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_SCALING_DATA);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  if (v->bind_scale)
  {
    v->y_scale_factor = v->x_scale_factor;
  }

  xsane_save_scaled_image(outfile, infile, &image_info, v->x_scale_factor, v->y_scale_factor, v->progress_bar, &v->cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  DBG(DBG_info, "removing file %s\n", v->filename);
  remove(v->filename);
  free(v->filename);

  v->filename = strdup(outfilename);
  v->image_saved = FALSE;

  xsane_viewer_read_image(v);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_despeckle_image(GtkWidget *window, gpointer data)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Viewer *v = (Viewer *) data;
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_despeckle_image\n");

  gtk_widget_destroy(v->active_dialog);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "despeckling image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_DESPECKLING_DATA);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_despeckle_image(outfile, infile, &image_info, v->despeckle_radius, v->progress_bar, &v->cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  DBG(DBG_info, "removing file %s\n", v->filename);
  remove(v->filename);
  free(v->filename);

  v->filename = strdup(outfilename);
  v->image_saved = FALSE;

  xsane_viewer_read_image(v);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_blur_image(GtkWidget *window, gpointer data)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Viewer *v = (Viewer *) data;
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_blur_image\n");

  gtk_widget_destroy(v->active_dialog);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "bluring image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
    v->block_actions = FALSE;

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_BLURING_DATA);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_blur_image(outfile, infile, &image_info, v->blur_radius, v->progress_bar, &v->cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  DBG(DBG_info, "removing file %s\n", v->filename);
  remove(v->filename);
  free(v->filename);

  v->filename = strdup(outfilename);
  v->image_saved = FALSE;

  xsane_viewer_read_image(v);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
  v->block_actions = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_rotate(Viewer *v, int rotation)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Image_info image_info;

  if (v->block_actions) /* actions blocked: return */
  {
    gdk_beep();
    DBG(DBG_info, "xsane_viewer_rotate: actions are blocked\n");
   return;
  }

  DBG(DBG_proc, "xsane_viewer_rotate(%d)\n", rotation);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "rotating image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  if (rotation <4)
  {
    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_ROTATING_DATA);
  }
  else
  {
    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_MIRRORING_DATA);
  }

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_rotate_image(outfile, infile, &image_info, rotation, v->progress_bar, &v->cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  DBG(DBG_info, "removing file %s\n", v->filename);
  remove(v->filename);
  free(v->filename);

  v->filename = strdup(outfilename);
  v->image_saved = FALSE;

  xsane_viewer_read_image(v);

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_rotate90_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  DBG(DBG_proc, "xsane_viewer_rotate90_callback\n");
  xsane_viewer_rotate(v, 1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_rotate180_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  DBG(DBG_proc, "xsane_viewer_rotate180_callback\n");
  xsane_viewer_rotate(v, 2);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_rotate270_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  DBG(DBG_proc, "xsane_viewer_rotate270_callback\n");
  xsane_viewer_rotate(v, 3);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_mirror_x_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  DBG(DBG_proc, "xsane_viewer_mirror_x_callback\n");
  xsane_viewer_rotate(v, 4);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_mirror_y_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;

  DBG(DBG_proc, "xsane_viewer_mirror_y_callback\n");
  xsane_viewer_rotate(v, 6);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_zoom_callback(GtkWidget *widget, gpointer data)
{
 Viewer *v = (Viewer *) data;
 int val;

  DBG(DBG_proc, "xsane_viewer_zoom_callback\n");

  val = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");
  v->zoom = (float) val / 100;
  DBG(DBG_info, "setting zoom factor to %f\n", v->zoom);
  xsane_viewer_read_image(v);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_viewer_files_build_menu(Viewer *v)
{
 GtkWidget *menu, *item;
 
  DBG(DBG_proc, "xsane_viewer_files_build_menu\n");
 
  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);
 
  /* XSane save dialog */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_SAVE_IMAGE);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_save_callback, v);
  gtk_widget_show(item);

  /* XSane save as text (ocr) */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_OCR);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_ocr_callback, v);
  gtk_widget_show(item);

  /* Clone */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_CLONE);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_clone_callback, v);
  gtk_widget_show(item);


  /* Scale */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_SCALE);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_scale_callback, v);
  gtk_widget_show(item);

 
  /* Close */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_CLOSE);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_object_set_data(GTK_OBJECT(item), "Viewer", (void *) v);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_close_callback, v);
  gtk_widget_show(item);
 
  return menu;    
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_viewer_filters_build_menu(Viewer *v)
{
 GtkWidget *menu, *item;
 
  DBG(DBG_proc, "xsane_viewer_filters_build_menu\n");
 
  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), xsane.accelerator_group);
 
  /* Despeckle */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_DESPECKLE);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_menu_append(GTK_MENU(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_despeckle_callback, v);
  gtk_widget_show(item);
 
 
  /* Blur */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_BLUR);
#if 0
  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED);
#endif
  gtk_container_add(GTK_CONTAINER(menu), item);
  g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_blur_callback, v);
  gtk_widget_show(item);
 
  return menu;    
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_viewer_read_image(Viewer *v)
{
 unsigned char *row, *src_row;
 int x, y;
 int last_y;
 int nread;
 int pos0;
 FILE *infile;
 Image_info image_info;
 char buf[256];
 float size;
 char *size_unit;
 int width, height;

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
   return -1;
  }

  xsane_read_pnm_header(infile, &image_info);

  pos0 = ftell(infile);

  if (!image_info.colors) /* == 0 (grayscale) ? */
  {
    image_info.colors = 1; /* we have one color component */
  }

  DBG(DBG_info, "reading image %s with  geometry: %d x %d x %d, %d colors\n", v->filename,
                 image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);
  /* open infile */

  if (v->window) /* we already have an existing viewer preview window? */
  {
    gtk_widget_destroy(v->window);
  }

  /* the preview area */
  if (image_info.colors == 3) /* RGB */
  {
    v->window = gtk_preview_new(GTK_PREVIEW_COLOR);
  }
  else /* grayscale */
  {
    v->window = gtk_preview_new(GTK_PREVIEW_GRAYSCALE);
  }

  gtk_preview_size(GTK_PREVIEW(v->window), image_info.image_width * v->zoom, image_info.image_height * v->zoom);
  gtk_container_add(GTK_CONTAINER(v->viewport), v->window);
  gtk_widget_show(v->window);



  /* get memory for one row of the image */
  src_row = malloc(image_info.image_width * image_info.colors * image_info.depth / 8);
  row = malloc(((int) image_info.image_width * v->zoom) * image_info.colors);

  if (!row || !src_row)
  {
    if (src_row)
    {
      free(src_row);
    }

    if (row)
    {
      free(row);
    }

    fclose(infile);
    DBG(DBG_error, "could not allocate memory\n");
   return -1;
  }


  last_y = -99999;

  /* read the image from file */
  for (y = 0; y < (int) (image_info.image_height * v->zoom); y++)
  {
    if ((int) (last_y / v->zoom) != (int) (y / v->zoom))
    {
      last_y = y;

      if (image_info.depth == 8) /* 8 bits/pixel */
      {
        fseek(infile, pos0 + (((int) (y / v->zoom)) * image_info.image_width) * image_info.colors, SEEK_SET);
        nread = fread(src_row, image_info.colors, image_info.image_width, infile);

        if (image_info.colors > 1)
        {
          for (x=0; x < (int) (image_info.image_width * v->zoom); x++)
          {
           int xoff = ((int) (x / v->zoom)) * image_info.colors;

            row[3*x+0] = src_row[xoff + 0];
            row[3*x+1] = src_row[xoff + 1];
            row[3*x+2] = src_row[xoff + 2];
          }
        }
        else
        {
          for (x=0; x < (int) (image_info.image_width * v->zoom); x++)
          {
            row[x] = src_row[((int) (x / v->zoom))];
          }
        }
      }
      else /* 16 bits/pixel => reduce to 8 bits/pixel */
      {
       guint16 *src_row16 = (guint16 *) src_row;

        fseek(infile, pos0 + (((int) (y / v->zoom)) * image_info.image_width) * image_info.colors * 2, SEEK_SET);
        nread = fread(src_row, 2 * image_info.colors, image_info.image_width, infile);

        if (image_info.colors > 1)
        {
          for (x=0; x < (int) (image_info.image_width * v->zoom); x++)
          {
           int xoff = ((int) (x / v->zoom)) * image_info.colors;

            row[3*x+0] = (unsigned char) (src_row16[xoff + 0] / 256);
            row[3*x+1] = (unsigned char) (src_row16[xoff + 1] / 256);
            row[3*x+2] = (unsigned char) (src_row16[xoff + 2] / 256);
          }
        }
        else
        {
          for (x=0; x < (int) (image_info.image_width * v->zoom); x++)
          {
            row[x] = (unsigned char) (src_row16[(int) (x / v->zoom)] / 256);
          }
        }
      }
    }

    gtk_preview_draw_row(GTK_PREVIEW(v->window), row, 0, y, image_info.image_width * v->zoom);
  }

  gtk_preview_put(GTK_PREVIEW(v->window), v->window->window, v->window->style->black_gc, 0, 0, 0, 0, 
                  image_info.image_width * v->zoom, image_info.image_height * v->zoom);

  size = (float) image_info.image_width * image_info.image_height * image_info.colors;
  if (image_info.depth == 16)
  {
    size *= 2.0;
  }

  size_unit = "B";

  if (size >= 1024 * 1024)
  {
    size /= (1024.0 * 1024.0);
    size_unit = "MB";
  }
  else if (size >= 1024)
  {
    size /= 1024.0;
    size_unit = "KB";
  }

  if (v->reduce_to_lineart)
  {
    snprintf(buf, sizeof(buf), TEXT_VIEWER_IMAGE_INFO, image_info.image_width, image_info.image_height, 1, image_info.colors, 
             image_info.resolution_x, image_info.resolution_y, size, size_unit);
  }
  else
  {
    snprintf(buf, sizeof(buf), TEXT_VIEWER_IMAGE_INFO, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors,
             image_info.resolution_x, image_info.resolution_y, size, size_unit);
  }
  gtk_label_set(GTK_LABEL(v->image_info_label), buf);

  width = image_info.image_width + 26;
  height = image_info.image_height + 136;

  if (width > gdk_screen_width())
  {
    width = gdk_screen_width();
  }

  if (height > gdk_screen_height())
  {
    height = gdk_screen_height();
  }

#ifdef HAVE_GTK2
  if (GTK_WIDGET_REALIZED(v->top))
  {
    gtk_window_resize(GTK_WINDOW(v->top), width, height);
  }
  else
#endif
  {
    gtk_window_set_default_size(GTK_WINDOW(v->top), width, height);
  }

  free(row);
  free(src_row);
  fclose(infile);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

Viewer *xsane_viewer_new(char *filename, int reduce_to_lineart, char *output_filename)
{
 char buf[256];
 Viewer *v;
 GtkWidget *vbox, *hbox;
 GtkWidget *menubar, *menubar_item;
 GtkWidget *scrolled_window;
 GtkWidget *zoom_option_menu, *zoom_menu, *zoom_menu_item;
 int i, selection;

  DBG(DBG_proc, "viewer_new(%s)\n", filename);

  /* create viewer structure v */
  v = malloc(sizeof(*v));
  if (!v)
  {
    DBG(DBG_error, "could not allocate memory\n");
    return 0;
  }
  memset(v, 0, sizeof(*v));   

  v->filename = strdup(filename);
  v->reduce_to_lineart = reduce_to_lineart;
  v->zoom = 1.0;
  v->image_saved = FALSE;
  v->next_viewer = xsane.viewer_list;
  xsane.viewer_list = v;

  if (output_filename)
  {
    v->output_filename = strdup(output_filename);
    snprintf(buf, sizeof(buf), "%s %s - %s", WINDOW_VIEWER, v->output_filename, xsane.device_text);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s %s", WINDOW_VIEWER, xsane.device_text);
  }

  v->top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(v->top), buf);
  xsane_set_window_icon(v->top, 0);
  gtk_window_add_accel_group(GTK_WINDOW(v->top), xsane.accelerator_group);
  gtk_object_set_data(GTK_OBJECT(v->top), "Viewer", (void *) v);
  g_signal_connect(GTK_OBJECT(v->top), "delete_event", GTK_SIGNAL_FUNC(xsane_viewer_close_callback), NULL);

  /* set the main vbox */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
  gtk_container_add(GTK_CONTAINER(v->top), vbox);
  gtk_widget_show(vbox);


  /* create the menubar */
  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

  /* "Files" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_FILE);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_viewer_files_build_menu(v));
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item); 

  /* "Filters" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_FILTERS);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_viewer_filters_build_menu(v));
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item); 

  gtk_widget_show(menubar);

 
  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);
 
 
  /* top hbox for icons */
  v->button_box = gtk_hbox_new(FALSE, 1);
  gtk_container_set_border_width(GTK_CONTAINER(v->button_box), 1);
  gtk_box_pack_start(GTK_BOX(vbox), v->button_box, FALSE, FALSE, 0);
  gtk_widget_show(v->button_box);

  v->save      = xsane_button_new_with_pixmap(v->top->window, v->button_box, save_xpm,      DESC_VIEWER_SAVE,      (GtkSignalFunc) xsane_viewer_save_callback, v);
  v->ocr       = xsane_button_new_with_pixmap(v->top->window, v->button_box, ocr_xpm,       DESC_VIEWER_OCR,       (GtkSignalFunc) xsane_viewer_ocr_callback, v);
  v->clone     = xsane_button_new_with_pixmap(v->top->window, v->button_box, clone_xpm,     DESC_VIEWER_CLONE,     (GtkSignalFunc) xsane_viewer_clone_callback, v);
  v->scale     = xsane_button_new_with_pixmap(v->top->window, v->button_box, scale_xpm,     DESC_VIEWER_SCALE,     (GtkSignalFunc) xsane_viewer_scale_callback, v);
  v->despeckle = xsane_button_new_with_pixmap(v->top->window, v->button_box, despeckle_xpm, DESC_VIEWER_DESPECKLE, (GtkSignalFunc) xsane_viewer_despeckle_callback, v);
  v->blur      = xsane_button_new_with_pixmap(v->top->window, v->button_box, blur_xpm,      DESC_VIEWER_BLUR,      (GtkSignalFunc) xsane_viewer_blur_callback, v);
  v->rotate90  = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate90_xpm,  DESC_ROTATE90,         (GtkSignalFunc) xsane_viewer_rotate90_callback, v);
  v->rotate180 = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate180_xpm, DESC_ROTATE180,        (GtkSignalFunc) xsane_viewer_rotate180_callback, v);
  v->rotate270 = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate270_xpm, DESC_ROTATE270,        (GtkSignalFunc) xsane_viewer_rotate270_callback, v);
  v->mirror_x  = xsane_button_new_with_pixmap(v->top->window, v->button_box, mirror_x_xpm,  DESC_MIRROR_X,         (GtkSignalFunc) xsane_viewer_mirror_x_callback, v);
  v->mirror_y  = xsane_button_new_with_pixmap(v->top->window, v->button_box, mirror_y_xpm,  DESC_MIRROR_Y,         (GtkSignalFunc) xsane_viewer_mirror_y_callback, v);


  /* "Zoom" submenu: */
  zoom_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, zoom_option_menu, DESC_VIEWER_ZOOM); 
  gtk_box_pack_start(GTK_BOX(v->button_box), zoom_option_menu, FALSE, FALSE, 0);
  gtk_widget_show(zoom_option_menu); 

  zoom_menu = gtk_menu_new();
  selection = 0;

  for (i = 0; i < sizeof(xsane_viewer_zoom) / sizeof(int); i++)
  {
    snprintf(buf, sizeof(buf), "%d %%", xsane_viewer_zoom[i]);
    zoom_menu_item = gtk_menu_item_new_with_label(buf);
    gtk_menu_append(GTK_MENU(zoom_menu), zoom_menu_item);
    g_signal_connect(GTK_OBJECT(zoom_menu_item), "activate", (GtkSignalFunc) xsane_viewer_zoom_callback, v);
    gtk_object_set_data(GTK_OBJECT(zoom_menu_item), "Selection", (void *) xsane_viewer_zoom[i]);
    gtk_widget_show(zoom_menu_item);
    if (v->zoom*100 == xsane_viewer_zoom[i])
    {
      selection = i;
    }
  }
  gtk_option_menu_set_menu(GTK_OPTION_MENU(zoom_option_menu), zoom_menu); 
  gtk_option_menu_set_history(GTK_OPTION_MENU(zoom_option_menu), selection);
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | DEF_GTK_ACCEL_LOCKED); */
  gtk_widget_show(zoom_menu); 



  scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show(scrolled_window);


  /* the viewport */
  v->viewport = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(v->viewport), GTK_SHADOW_IN);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), v->viewport); 
  gtk_widget_show(v->viewport);


  /* image info label */
  hbox = gtk_hbox_new(FALSE, 1);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);
  v->image_info_label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), v->image_info_label, FALSE, FALSE, 2);
  gtk_widget_show(v->image_info_label);


  if (xsane_viewer_read_image(v)) /* read image and add preview to the viewport */
  {
    /* error */ 
  }
  gtk_widget_show(v->top);

  v->progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
#if 0
  gtk_widget_set_size_request(v->progress_bar, 0, 25);
#endif
  gtk_box_pack_start(GTK_BOX(vbox), (GtkWidget *) v->progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(v->progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_widget_show(GTK_WIDGET(v->progress_bar));

 return v;
}


