/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
 
   xsane-viewer.c
 
   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2001 Oliver Rauch
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
/* #include <sys/param.h> */
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-preferences.h"
#include "xsane-viewer.h"
#include "xsane-gamma.h"
#include "xsane-icons.h"
#include "xsane-save.h"
#include <gdk/gdkkeysyms.h>
 
 
#ifndef PATH_MAX
# define PATH_MAX       1024
#endif
 
/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_viewer_zoom[] = {35, 50, 71, 100, 141, 200, 282, 400 };
#define XSANE_VIEWER_ZOOM_ITEMS 8

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_viewer_close_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_save_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_clone_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_despeckle_callback(GtkWidget *window, gpointer data);
static void xsane_viewer_blur_callback(GtkWidget *window, gpointer data);
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

  if (!v->image_saved)
  {
   char buf[256];

    snprintf(buf, sizeof(buf), WARN_VIEWER_IMAGE_NOT_SAVED);
    if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_DO_NOT_CLOSE, BUTTON_DISCARD_IMAGE, TRUE /* wait */))
    {
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

  free(v);

 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_save_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 char outputfilename[1024];
 char *inputfilename;
 char windowname[256];
 int output_format;

  DBG(DBG_proc, "xsane_viewer_save_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  if (v->output_filename)
  {
    strncpy(outputfilename, v->output_filename, sizeof(outputfilename));
  }
  else
  {
   int abort = 0;

    strcpy(outputfilename, preferences.filename);
 
    snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_VIEWER_OUTPUT_FILENAME, xsane.device_text);
 
    umask((mode_t) preferences.directory_umask); /* define new file permissions */
    abort = xsane_back_gtk_get_filename(windowname, outputfilename, sizeof(outputfilename), outputfilename, TRUE, TRUE, FALSE);
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */ 

    if (abort)
    {
      gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
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
       return;
      }
    }
  } 

  inputfilename = strdup(v->filename);

  if (v->reduce_to_lineart) /* reduce grayscale image to lineart before saving */
  {
   char dummyfilename[1024];

    xsane_back_gtk_make_path(sizeof(dummyfilename), dummyfilename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

    gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_PACKING_DATA);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

    xsane_save_image_as_lineart(v->filename, dummyfilename, v->progress_bar, &v->cancel_save);

    remove(dummyfilename);

    free(inputfilename);
    inputfilename = strdup(dummyfilename);
  }


  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_SAVING_DATA);
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  output_format = xsane_identify_output_format(outputfilename, 0);

  xsane_save_image_as(v->filename, outputfilename, output_format, v->progress_bar, &v->cancel_save);

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  free(inputfilename);

  v->image_saved = TRUE;

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_clone_callback(GtkWidget *window, gpointer data)
{
 Viewer *v = (Viewer *) data;
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_clone_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "cloning image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_viewer_despeckle_callback(GtkWidget *window, gpointer data)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Viewer *v = (Viewer *) data;
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_despeckle_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "despeckling image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_DESPECKLING_DATA);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_despeckle_image(outfile, infile, &image_info, 1, v->progress_bar, &v->cancel_save);

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

static void xsane_viewer_blur_callback(GtkWidget *window, gpointer data)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Viewer *v = (Viewer *) data;
 Image_info image_info;

  DBG(DBG_proc, "xsane_viewer_blur_callback\n");

  gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), FALSE);

  infile = fopen(v->filename, "rb");
  if (!infile)
  {
    DBG(DBG_error, "could not load file %s\n", v->filename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  xsane_read_pnm_header(infile, &image_info);

  DBG(DBG_info, "bluring image %s with geometry: %d x %d x %d, %d colors\n", v->filename, image_info.image_width, image_info.image_height, image_info.depth, image_info.colors);

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

  outfile = fopen(outfilename, "wb");
  if (!outfile)
  {
    DBG(DBG_error, "could not save file %s\n", outfilename);
    gtk_widget_set_sensitive(GTK_WIDGET(v->button_box), TRUE);

   return;
  }

  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), PROGRESS_BLURING_DATA);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(v->progress_bar), 0.0);

  xsane_save_blur_image(outfile, infile, &image_info, 1, v->progress_bar, &v->cancel_save);

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

static void xsane_viewer_rotate(Viewer *v, int rotation)
{
 FILE *outfile;
 FILE *infile;
 char outfilename[256];
 Image_info image_info;

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

  xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);

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
  gtk_accel_group_attach(xsane.accelerator_group, GTK_OBJECT(menu));
 
  /* XSane save dialog */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_SAVE_IMAGE);
//  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_save_callback, v);
  gtk_widget_show(item);
 
 
  /* Close */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_CLOSE);
//  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_object_set_data(GTK_OBJECT(item), "Viewer", (void *) v);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_close_callback, v);
  gtk_widget_show(item);
 
  return menu;    
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_viewer_filters_build_menu(Viewer *v)
{
 GtkWidget *menu, *item;
 
  DBG(DBG_proc, "xsane_viewer_filters_build_menu\n");
 
  menu = gtk_menu_new();
  gtk_accel_group_attach(xsane.accelerator_group, GTK_OBJECT(menu));
 
  /* Despeckle */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_DESPECKLE);
//  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_I, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_despeckle_callback, v);
  gtk_widget_show(item);
 
 
  /* Blur */
 
  item = gtk_menu_item_new_with_label(MENU_ITEM_BLUR);
//  gtk_widget_add_accelerator(item, "activate", xsane.accelerator_group, GDK_Q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_viewer_blur_callback, v);
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

  if (v->window) /* we already have an existing preview? */
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

  gtk_preview_put(GTK_PREVIEW(v->window), v->window->window, v->window->style->black_gc, 0, 0, 0, 0, image_info.image_width * v->zoom, image_info.image_height * v->zoom);

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
  }

  snprintf(buf, sizeof(buf), "%s %s", WINDOW_VIEWER, xsane.device_text);
  v->top = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(v->top), buf);
  xsane_set_window_icon(v->top, 0);
  gtk_accel_group_attach(xsane.accelerator_group, GTK_OBJECT(v->top));   
  gtk_window_set_default_size(GTK_WINDOW(v->top), 400, 400);
  gtk_object_set_data(GTK_OBJECT(v->top), "Viewer", (void *) v);
  gtk_signal_connect(GTK_OBJECT(v->top), "delete_event", GTK_SIGNAL_FUNC(xsane_viewer_close_callback), NULL);

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
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED); */
  gtk_widget_show(menubar_item); 

  /* "Filters" submenu: */
  menubar_item = gtk_menu_item_new_with_label(MENU_FILTERS);
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_viewer_filters_build_menu(v));
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED); */
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

  v->save      = xsane_button_new_with_pixmap(v->top->window, v->button_box, file_xpm,      DESC_VIEWER_SAVE,      (GtkSignalFunc) xsane_viewer_save_callback, v);
  v->clone     = xsane_button_new_with_pixmap(v->top->window, v->button_box, clone_xpm,     DESC_VIEWER_CLONE,     (GtkSignalFunc) xsane_viewer_clone_callback, v);
  v->despeckle = xsane_button_new_with_pixmap(v->top->window, v->button_box, despeckle_xpm, DESC_VIEWER_DESPECKLE, (GtkSignalFunc) xsane_viewer_despeckle_callback, v);
  v->blur      = xsane_button_new_with_pixmap(v->top->window, v->button_box, blur_xpm,      DESC_VIEWER_BLUR,      (GtkSignalFunc) xsane_viewer_blur_callback, v);
  v->rotate90  = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate90_xpm,  DESC_VIEWER_ROTATE90,  (GtkSignalFunc) xsane_viewer_rotate90_callback, v);
  v->rotate180 = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate180_xpm, DESC_VIEWER_ROTATE180, (GtkSignalFunc) xsane_viewer_rotate180_callback, v);
  v->rotate270 = xsane_button_new_with_pixmap(v->top->window, v->button_box, rotate270_xpm, DESC_VIEWER_ROTATE270, (GtkSignalFunc) xsane_viewer_rotate270_callback, v);
  v->mirror_x  = xsane_button_new_with_pixmap(v->top->window, v->button_box, mirror_x_xpm,  DESC_VIEWER_MIRROR_X,  (GtkSignalFunc) xsane_viewer_mirror_x_callback, v);
  v->mirror_y  = xsane_button_new_with_pixmap(v->top->window, v->button_box, mirror_y_xpm,  DESC_VIEWER_MIRROR_Y,  (GtkSignalFunc) xsane_viewer_mirror_y_callback, v);


  /* "Zoom" submenu: */
  zoom_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, zoom_option_menu, DESC_VIEWER_ZOOM); 
  gtk_box_pack_start(GTK_BOX(v->button_box), zoom_option_menu, FALSE, FALSE, 0);
  gtk_widget_show(zoom_option_menu); 

  zoom_menu = gtk_menu_new();
  selection = 0;
  for (i = 0; i < XSANE_VIEWER_ZOOM_ITEMS; i++)
  {
    snprintf(buf, sizeof(buf), "%d %%", xsane_viewer_zoom[i]);
    zoom_menu_item = gtk_menu_item_new_with_label(buf);
    gtk_menu_append(GTK_MENU(zoom_menu), zoom_menu_item);
    gtk_signal_connect(GTK_OBJECT(zoom_menu_item), "activate", (GtkSignalFunc) xsane_viewer_zoom_callback, v);
    gtk_object_set_data(GTK_OBJECT(zoom_menu_item), "Selection", (void *) xsane_viewer_zoom[i]);
    gtk_widget_show(zoom_menu_item);
    if (v->zoom*100 == xsane_viewer_zoom[i])
    {
      selection = i;
    }
  }
  gtk_option_menu_set_menu(GTK_OPTION_MENU(zoom_option_menu), zoom_menu); 
  gtk_option_menu_set_history(GTK_OPTION_MENU(zoom_option_menu), selection);
/*  gtk_widget_add_accelerator(menubar_item, "select", xsane.accelerator_group, GDK_F, 0, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED); */
  gtk_widget_show(zoom_menu); 



  scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show(scrolled_window);


  /* the viewport */
  v->viewport = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(v->viewport), GTK_SHADOW_IN);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), v->viewport); 
  gtk_widget_show(v->viewport);


  if (xsane_viewer_read_image(v)) /* read image and add preview to the viewport */
  {
    /* error */ 
  }

  gtk_widget_show(v->top);

  v->progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), (GtkWidget *) v->progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(v->progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(v->progress_bar), "");
  gtk_widget_show(GTK_WIDGET(v->progress_bar));

 return v;
}


