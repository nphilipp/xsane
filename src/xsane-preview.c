/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preview.c

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

/*

  The preview strategy is as follows:
  -----------------------------------

   1) The preview is done on the full scan area or a part of it.

   2) The preview is zoomable so the user can precisely pick
      the selection area even for small scans on a large scan
      surface.

   3) The preview window is resizeable.

   4) The preview scan resolution depends on preview window size
      and the selected preview surface (zoom area).

   5) We let the user/backend pick whether a preview is in color,
      grayscale, lineart or what not.  The only options that the
      preview may (temporarily) modify are:

	- resolution (set so the preview fills the window)
	- scan area options (top-left corner, bottom-right corner)
	- preview option (to let the backend know we're doing a preview)
        - gamma table is set to default (gamma=1.0)

   5) The initial size of the scan surface is determined based on the constraints
      of the four corner coordinates.  Missing constraints are replaced
      by 0/+INF as appropriate (0 for top-left, +INF for bottom-right coords).

   6) Given the preview window size and the scan surface size, we
      select the resolution so the acquired preview image just fits
      in the preview window.  The resulting resolution may be out
      of range in which case we pick the minum/maximum if there is
      a range or word-list constraint or a default value if there is
      no such constraint.

   7) Once a preview image has been acquired, we know the size of the
      preview image (in pixels).  An initial scale factor is chosen
      so the image fits into the preview window.

   8) Surface definitions:
      device = surface of the scanner
      image = same oriantation like device
      preview = rotated (0/90/180/270 degree) device surface
      window = same oriantation like device, may be different scaling

*/

/* ---------------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
/* #include <sys/param.h> */
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-batch-scan.h"
#include "xsane-preview.h"
#include "xsane-preferences.h"
#include "xsane-gamma.h"
#include <gdk/gdkkeysyms.h>


#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Cut fp conversion routines some slack: */
#define GROSSLY_DIFFERENT(f1,f2)	(fabs ((f1) - (f2)) > 1e-3)
#define GROSSLY_EQUAL(f1,f2)		(fabs ((f1) - (f2)) < 1e-3) 

#ifdef __alpha__
  /* This seems to be necessary for at least some XFree86 3.1.2
     servers.  It's known to be necessary for the XF86_TGA server for
     Linux/Alpha.  Fortunately, it's no great loss so we turn this on
     by default for now.  */
# define XSERVER_WITH_BUGGY_VISUALS
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_ZOOM_SIZE 80
#define XSANE_ZOOM_FACTOR 4

/* ---------------------------------------------------------------------------------------------------------------------- */

static u_char *preview_gamma_data_red   = 0;
static u_char *preview_gamma_data_green = 0;
static u_char *preview_gamma_data_blue  = 0;

static u_char *histogram_gamma_data_red   = 0;
static u_char *histogram_gamma_data_green = 0;
static u_char *histogram_gamma_data_blue  = 0;

/* histogram_medium_gamma_data_* is used when medium correction is done after preview-scan by xsane */
static u_char *histogram_medium_gamma_data_red   = 0;
static u_char *histogram_medium_gamma_data_green = 0;
static u_char *histogram_medium_gamma_data_blue  = 0;

static int preview_gamma_input_bits;

static char *ratio_string[] = { "free", " 2:1", "16:9", "15:10",    " 4:3",   " 1:1", " 3:4", " 9:16", "10:15",  " 1:2"};
static float ratio_value[]  =  { 0.0,    2.0, 16.0/9.0, 15.0/10.0, 4.0/3.0,  1.0,  0.75,  0.5625, 10.0/15.0, 0.5 };
/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations */
static void preview_rotate_devicesurface_to_previewsurface(int rotation, float dsurface[4], float *psurface);
static void preview_rotate_previewsurface_to_devicesurface(int rotation, float psurface[4], float *dsurface);
static void preview_transform_coordinates_device_to_window(Preview *p, float dcoordinate[4], float *win_coord);
static void preview_transform_coordinate_window_to_device(Preview *p, float winx, float winy, float *previewx, float *previewy);
static void preview_transform_coordinate_window_to_image(Preview *p, int winx, int winy, int *imagex, int *imagey);
static void preview_order_selection(Preview *p);
static void preview_bound_selection(Preview *p);
static void preview_draw_rect(Preview *p, GdkWindow *win, GdkGC *gc, float coord[4]);
static void preview_draw_selection(Preview *p);
static void preview_update_selection(Preview *p);
static void preview_establish_selection(Preview *p);
/* static void preview_update_batch_selection(Preview *p); */
static void preview_get_scale_device_to_image(Preview *p, float *xscalep, float *yscalep);
static void preview_get_scale_device_to_window(Preview *p, float *xscalep, float *yscalep);
static void preview_get_scale_window_to_image(Preview *p, float *xscalep, float *yscalep);
static void preview_paint_image(Preview *p);
static void preview_display_partial_image(Preview *p);
static void preview_display_maybe(Preview *p);
static void preview_display_image(Preview *p);
static void preview_save_option(Preview *p, int option, void *save_loc, int *valid);
static void preview_restore_option(Preview *p, int option, void *saved_value, int valid);
static void preview_set_option(Preview *p, int option, void *value);
static void preview_set_option_float(Preview *p, int option, float value);
static void preview_set_option_val(Preview *p, int option, SANE_Int value);
static int  preview_increment_image_y(Preview *p);
static void preview_read_image_data(gpointer data, gint source, GdkInputCondition cond);
static void preview_scan_done(Preview *p, int save_image);
static void preview_scan_start(Preview *p);
static int preview_make_image_path(Preview *p, size_t filename_size, char *filename, int level);
static void preview_restore_image(Preview *p);
static gint preview_expose_event_handler_start(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_expose_event_handler_end(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_hold_event_handler(gpointer data);
static gint preview_motion_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_button_press_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_button_release_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static void preview_start_button_clicked(GtkWidget *widget, gpointer data);
static void preview_cancel_button_clicked(GtkWidget *widget, gpointer data);
static void preview_area_correct(Preview *p);
static void preview_save_image(Preview *p);
static void preview_delete_images(Preview *p);
static void preview_select_zoom_point(Preview *p, int preview_x, int preview_y);
static void preview_zoom_not(GtkWidget *window, gpointer data);
static void preview_zoom_out(GtkWidget *window, gpointer data);
static void preview_zoom_in(GtkWidget *window, gpointer data);
static void preview_zoom_area(GtkWidget *window, gpointer data);
static void preview_zoom_undo(GtkWidget *window, gpointer data);
static void preview_get_color(Preview *p, int x, int y, int range, int *red, int *green, int *blue);
static void preview_add_batch(GtkWidget *window, Preview *p);
static void preview_pipette_white(GtkWidget *window, gpointer data);
static void preview_pipette_gray(GtkWidget *window, gpointer data);
static void preview_pipette_black(GtkWidget *window, gpointer data);
static void preview_init_autoraise_scan_area(GtkWidget *window, gpointer data);
void preview_select_full_preview_area(Preview *p);
static void preview_full_preview_area_callback(GtkWidget *widget, gpointer call_data);
static void preview_delete_images_callback(GtkWidget *widget, gpointer call_data);
static gint preview_preset_area_rename_callback(GtkWidget *widget, GtkWidget *preset_area_widget);
static gint preview_preset_area_add_callback(GtkWidget *widget, GtkWidget *preset_area_widget);
static gint preview_preset_area_delete_callback(GtkWidget *widget, GtkWidget *preset_area_widget);
static gint preview_preset_area_move_up_callback(GtkWidget *widget, GtkWidget *preset_area_widget);
static gint preview_preset_area_move_down_callback(GtkWidget *widget, GtkWidget *preset_area_widget);
static gint preview_preset_area_context_menu_callback(GtkWidget *widget, GdkEvent *event);
static void preview_preset_area_callback(GtkWidget *widget, gpointer data);
static void preview_rotation_callback(GtkWidget *widget, gpointer data);
static void preview_establish_ratio(Preview *p);
static void preview_ratio_callback(GtkWidget *widget, gpointer data);
static void preview_autoselect_scan_area_callback(GtkWidget *window, gpointer data);

void preview_display_with_correction(Preview *p);
void preview_do_gamma_correction(Preview *p);
int preview_do_color_correction(Preview *p);
void preview_calculate_raw_histogram(Preview *p, SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue);
void preview_calculate_enh_histogram(Preview *p, SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue);
void preview_gamma_correction(Preview *p, int gamma_input_bits,
                              u_char *gamma_red, u_char *gamma_green, u_char *gamma_blue,
                              u_char *gamma_red_hist, u_char *gamma_green_hist, u_char *gamma_blue_hist,
                              u_char *medium_gamma_red_hist, u_char *medium_gamma_green_hist, u_char *medium_gamma_blue_hist);
void preview_area_resize(Preview *p);
gint preview_area_resize_handler(GtkWidget *widget, GdkEvent *event, gpointer data);
void preview_update_maximum_output_size(Preview *p);
void preview_set_maximum_output_size(Preview *p, float width, float height, int paper_orientation);
void preview_autoraise_scan_area(Preview *p, int preview_x, int preview_y, float *autoselect_coord);
void preview_autoselect_scan_area(Preview *p, float *autoselect_coord);
void preview_display_valid(Preview *p);

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotate_devicesurface_to_previewsurface(int rotation, float dsurface[4], float *psurface) 
{
  DBG(DBG_proc, "preview_rotate_devicesurface_to_previewsurface(rotation = %d)\n", rotation);

  switch (rotation & 3)
  {
    case 0: /* 0 degree */
    default:
      *(psurface+0) = dsurface[0];
      *(psurface+1) = dsurface[1];
      *(psurface+2) = dsurface[2];
      *(psurface+3) = dsurface[3];
     break;

    case 1: /* 90 degree */
      *(psurface+0) = dsurface[3];
      *(psurface+1) = dsurface[0];
      *(psurface+2) = dsurface[1];
      *(psurface+3) = dsurface[2];
     break;

    case 2: /* 180 degree */
      *(psurface+0) = dsurface[2];
      *(psurface+1) = dsurface[3];
      *(psurface+2) = dsurface[0];
      *(psurface+3) = dsurface[1];
     break;

    case 3: /* 270 degree */
      *(psurface+0) = dsurface[1];
      *(psurface+1) = dsurface[2];
      *(psurface+2) = dsurface[3];
      *(psurface+3) = dsurface[0];
     break;
  }

  if (rotation & 4) /* mirror in x direction */
  {
   float help=*(psurface+0);

    *(psurface+0) = *(psurface+2);
    *(psurface+2) = help; 
  }

  DBG(DBG_info, "device[%3.2f %3.2f %3.2f %3.2f] -> preview[%3.2f %3.2f %3.2f %3.2f]\n",
                dsurface[0], dsurface[1], dsurface[2], dsurface[3],
                *(psurface+0), *(psurface+1), *(psurface+2), *(psurface+3));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotate_previewsurface_to_devicesurface(int rotation, float psurface[4], float *dsurface) 
{
  DBG(DBG_proc, "preview_rotate_previewsurface_to_devicesurface(rotation = %d)\n", rotation);

  switch (rotation)
  {
    case 0: /* 0 degree */
    default:
      *(dsurface+0) = psurface[0];
      *(dsurface+1) = psurface[1];
      *(dsurface+2) = psurface[2];
      *(dsurface+3) = psurface[3];
     break;

    case 1: /* 90 degree */
      *(dsurface+0) = psurface[1];
      *(dsurface+1) = psurface[2];
      *(dsurface+2) = psurface[3];
      *(dsurface+3) = psurface[0];
     break;

    case 2: /* 180 degree */
      *(dsurface+0) = psurface[2];
      *(dsurface+1) = psurface[3];
      *(dsurface+2) = psurface[0];
      *(dsurface+3) = psurface[1];
     break;

    case 3: /* 270 degree */
      *(dsurface+0) = psurface[3];
      *(dsurface+1) = psurface[0];
      *(dsurface+2) = psurface[1];
      *(dsurface+3) = psurface[2];
     break;

    case 4: /* 0 degree, x mirror */
      *(dsurface+0) = psurface[2];
      *(dsurface+1) = psurface[1];
      *(dsurface+2) = psurface[0];
      *(dsurface+3) = psurface[3];
     break;

    case 5: /* 90 degree, x mirror  */
      *(dsurface+0) = psurface[1];
      *(dsurface+1) = psurface[0];
      *(dsurface+2) = psurface[3];
      *(dsurface+3) = psurface[2];
     break;

    case 6: /* 180 degree, x mirror */
      *(dsurface+0) = psurface[0];
      *(dsurface+1) = psurface[3];
      *(dsurface+2) = psurface[2];
      *(dsurface+3) = psurface[1];
     break;

    case 7: /* 270 degree, x mirror */
      *(dsurface+0) = psurface[3];
      *(dsurface+1) = psurface[2];
      *(dsurface+2) = psurface[1];
      *(dsurface+3) = psurface[0];
     break;
  }

  DBG(DBG_info, "preview[%3.2f %3.2f %3.2f %3.2f] -> device[%3.2f %3.2f %3.2f %3.2f]\n",
                psurface[0], psurface[1], psurface[2], psurface[3],
                *(dsurface+0), *(dsurface+1), *(dsurface+2), *(dsurface+3));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_transform_coordinates_device_to_window(Preview *p, float preview_coord[4], float *win_coord)
{
 float minx, maxx, miny, maxy;
 float xscale, yscale;

  DBG(DBG_proc, "preview_transform_coordinates_device_to_window\n");

  preview_get_scale_device_to_window(p, &xscale, &yscale);

  minx = preview_coord[0];
  miny = preview_coord[1];
  maxx = preview_coord[2];
  maxy = preview_coord[3];

  if (minx > maxx)
  {
   float val = minx;
    minx = maxx;
    maxx = val;
  }

  if (miny > maxy)
  {
   float val = miny;
    miny = maxy;
    maxy = val;
  }

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      *(win_coord+0) = xscale * (minx - p->surface[0]);
      *(win_coord+1) = yscale * (miny - p->surface[1]);
      *(win_coord+2) = xscale * (maxx - p->surface[0]);
      *(win_coord+3) = yscale * (maxy - p->surface[1]);
     break;

    case 1: /* 90 degree */
      *(win_coord+0) = xscale * (p->surface[0] - maxx);
      *(win_coord+1) = yscale * (miny - p->surface[1]);
      *(win_coord+2) = xscale * (p->surface[0] - minx);
      *(win_coord+3) = yscale * (maxy - p->surface[1]); 
     break;

    case 2: /* 180 degree */
      *(win_coord+0) = xscale * (p->surface[0] - maxx);
      *(win_coord+1) = yscale * (p->surface[1] - maxy);
      *(win_coord+2) = xscale * (p->surface[0] - minx);
      *(win_coord+3) = yscale * (p->surface[1] - miny);
     break;

    case 3: /* 270 degree */
      *(win_coord+0) = xscale * (minx - p->surface[0]);
      *(win_coord+1) = yscale * (p->surface[1] - maxy);
      *(win_coord+2) = xscale * (maxx - p->surface[0]);
      *(win_coord+3) = yscale * (p->surface[1] - miny);
     break;

    case 4: /* 0 degree, x mirror */
      *(win_coord+0) = xscale * (p->surface[0] - maxx);
      *(win_coord+1) = yscale * (miny - p->surface[1]);
      *(win_coord+2) = xscale * (p->surface[0] - minx);
      *(win_coord+3) = yscale * (maxy - p->surface[1]);
     break;

    case 5: /* 90 degree, x mirror */
      *(win_coord+0) = xscale * (minx - p->surface[0]);
      *(win_coord+1) = yscale * (miny - p->surface[1]);
      *(win_coord+2) = xscale * (maxx - p->surface[0]);
      *(win_coord+3) = yscale * (maxy - p->surface[1]); 
     break;

    case 6: /* 180 degree, x mirror */
      *(win_coord+0) = xscale * (minx - p->surface[0]);
      *(win_coord+1) = yscale * (p->surface[1] - maxy);
      *(win_coord+2) = xscale * (maxx - p->surface[0]);
      *(win_coord+3) = yscale * (p->surface[1] - miny);
     break;

    case 7: /* 270 degree, x mirror */
      *(win_coord+0) = xscale * (p->surface[0] - maxx);
      *(win_coord+1) = yscale * (p->surface[1] - maxy);
      *(win_coord+2) = xscale * (p->surface[0] - minx);
      *(win_coord+3) = yscale * (p->surface[1] - miny);
     break;
  }

  DBG(DBG_info, "preview[%3.2f %3.2f %3.2f %3.2f] -> window[%3.2f %3.2f %3.2f %3.2f]\n",
                preview_coord[0], preview_coord[1], preview_coord[2], preview_coord[3],
                *(win_coord+0), *(win_coord+1), *(win_coord+2), *(win_coord+3) );
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_transform_coordinate_window_to_device(Preview *p, float winx, float winy, float *devicex, float *devicey) 
{
 float xscale, yscale;

  DBG(DBG_proc, "preview_transform_coordinate_window_to_device\n");

  preview_get_scale_device_to_window(p, &xscale, &yscale);

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      *devicex = p->surface[0] + winx / xscale;
      *devicey = p->surface[1] + winy / yscale;
     break;

    case 1: /* 90 degree */
      *devicex = p->surface[0] - winx / xscale;
      *devicey = p->surface[1] + winy / yscale;
     break;

    case 2: /* 180 degree */
      *devicex = p->surface[0] - winx / xscale;
      *devicey = p->surface[1] - winy / yscale;
     break;

    case 3: /* 270 degree */
      *devicex = p->surface[0] + winx / xscale;
      *devicey = p->surface[1] - winy / yscale;
     break;

    case 4: /* 0 degree, x mirror */
      *devicex = p->surface[0] - winx / xscale;
      *devicey = p->surface[1] + winy / yscale;
     break;

    case 5: /* 90 degree, x mirror */
      *devicex = p->surface[0] + winx / xscale;
      *devicey = p->surface[1] + winy / yscale;
     break;

    case 6: /* 180 degree, x mirror */
      *devicex = p->surface[0] + winx / xscale;
      *devicey = p->surface[1] - winy / yscale;
     break;

    case 7: /* 270 degree, x mirror */
      *devicex = p->surface[0] - winx / xscale;
      *devicey = p->surface[1] - winy / yscale;
     break;
  }

  DBG(DBG_info, "window[%3.2f %3.2f] -> device[%3.2f %3.2f]\n", winx, winy, *devicex, *devicey);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_transform_coordinate_window_to_image(Preview *p, int winx, int winy, int *imagex, int *imagey)
{
 float xscale, yscale;

  DBG(DBG_proc, "preview_transform_coordinate_window_to_image\n");

  preview_get_scale_window_to_image(p, &xscale, &yscale);

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      *imagex = winx * xscale;
      *imagey = winy * yscale;
     break;

    case 1: /* 90 degree */
      *imagex = winy * yscale;
      *imagey = p->image_height - winx * xscale;
     break;

    case 2: /* 180 degree */
      *imagex = p->image_width - winx * xscale;
      *imagey = p->image_height - winy * yscale;
     break;

    case 3: /* 270 degree */
      *imagex = p->image_width - winy * yscale;
      *imagey = winx * xscale;
     break;

    case 4: /* 0 degree, x mirror */
      *imagex = p->image_width - winx * xscale;
      *imagey = winy * yscale;
     break;

    case 5: /* 90 degree, x mirror */
      *imagex = winy * yscale;
      *imagey = winx * xscale;
     break;

    case 6: /* 180 degree, x mirror */
      *imagex = winx * xscale;
      *imagey = p->image_height - winy * yscale;
     break;

    case 7: /* 270 degree, x mirror */
      *imagex = p->image_width - winy * yscale;
      *imagey = p->image_height - winx * xscale;
     break;
  }

  DBG(DBG_info, "window[%d %d] -> image[%d %d]\n", winx, winy, *imagex, *imagey);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_order_selection(Preview *p)
{
 float tmp_coordinate;

  DBG(DBG_proc, "preview_order_selection\n");

  p->selection.active = ( (p->selection.coordinate[0] != p->selection.coordinate[2]) &&
                          (p->selection.coordinate[1] != p->selection.coordinate[3]) );


  if (p->selection.active)
  {

    if (p->selection.coordinate[p->index_xmin] > p->selection.coordinate[p->index_xmax])
    {
      tmp_coordinate = p->selection.coordinate[p->index_xmin];
      p->selection.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmax];
      p->selection.coordinate[p->index_xmax] = tmp_coordinate;

      p->selection_xedge = (p->selection_xedge + 2) & 3;
    }

    if (p->selection.coordinate[p->index_ymin] > p->selection.coordinate[p->index_ymax])
    {
      tmp_coordinate = p->selection.coordinate[p->index_ymin];
      p->selection.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymax];
      p->selection.coordinate[p->index_ymax] = tmp_coordinate;

      p->selection_yedge = (p->selection_yedge + 2) & 3;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_bound_selection(Preview *p)
{
  DBG(DBG_proc, "preview_bound_selection\n");

  p->selection.active = ( (p->selection.coordinate[0] != p->selection.coordinate[2]) &&
                          (p->selection.coordinate[1] != p->selection.coordinate[3]) );


  if (p->selection.active)
  {
#if 1
    xsane_bound_float(&p->selection.coordinate[0], p->scanner_surface[0], p->scanner_surface[2]);
    xsane_bound_float(&p->selection.coordinate[2], p->scanner_surface[0], p->scanner_surface[2]);
    xsane_bound_float(&p->selection.coordinate[1], p->scanner_surface[1], p->scanner_surface[3]);
    xsane_bound_float(&p->selection.coordinate[3], p->scanner_surface[1], p->scanner_surface[3]);
#else
    xsane_bound_float(&p->selection.coordinate[0], p->surface[0], p->surface[2]);
    xsane_bound_float(&p->selection.coordinate[2], p->surface[0], p->surface[2]);
    xsane_bound_float(&p->selection.coordinate[1], p->surface[1], p->surface[3]);
    xsane_bound_float(&p->selection.coordinate[3], p->surface[1], p->surface[3]);
#endif
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_draw_rect(Preview *p, GdkWindow *win, GdkGC *gc, float preview_coord[4])
{
 float win_coord[4];

  DBG(DBG_proc, "preview_draw_rect [%3.2f %3.2f %3.2f %3.2f]\n", preview_coord[0], preview_coord[1], preview_coord[2], preview_coord[3]);

  preview_transform_coordinates_device_to_window(p, preview_coord, win_coord);
  gdk_draw_rectangle(win, gc, FALSE, win_coord[0], win_coord[1], win_coord[2]-win_coord[0] + 1, win_coord[3] - win_coord[1] + 1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_draw_selection(Preview *p)
{
  DBG(DBG_proc, "preview_draw_selection\n");

  if (!p->gc_selection) /* window isn't mapped yet */
  {
    return;
  }

  if ( (p->show_selection == FALSE) || (p->calibration) )
  {
    return;
  }

  if (p->previous_selection.active)
  {
    preview_draw_rect(p, p->window->window, p->gc_selection, p->previous_selection.coordinate);
  }

  if (p->selection.active)
  {
    preview_draw_rect(p, p->window->window, p->gc_selection, p->selection.coordinate);
  }

  p->previous_selection = p->selection;


  if (!p->gc_selection_maximum) /* window isn't mapped yet */
  {
    return;
  }

  if (p->previous_selection_maximum.active)
  {
    preview_draw_rect(p, p->window->window, p->gc_selection_maximum, p->previous_selection_maximum.coordinate);
  }

  if (p->selection_maximum.active)
  {
    preview_draw_rect(p, p->window->window, p->gc_selection_maximum, p->selection_maximum.coordinate);
  }

  p->previous_selection_maximum = p->selection_maximum;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_update_selection(Preview *p)
/* draw selection box as defined in backend */
{
 const SANE_Option_Descriptor *opt;
 SANE_Status status;
 SANE_Word val;
 int i, optnum;
 float coord[4];

  DBG(DBG_proc, "preview_update_selection\n");

  p->previous_selection = p->selection;

  for (i = 0; i < 4; ++i)
  {
    optnum = xsane.well_known.coord[i];
    if (optnum > 0)
    {
      opt = xsane_get_option_descriptor(xsane.dev, optnum);
      status = xsane_control_option(xsane.dev, optnum, SANE_ACTION_GET_VALUE, &val, 0);
      if (status != SANE_STATUS_GOOD)
      {
        continue;
      }
      if (opt->type == SANE_TYPE_FIXED)
      {
        coord[i] = SANE_UNFIX(val);
      }
      else
      {
        coord[i] = val;
      }
    }
    else /* backend does not use scan area options */
    {
      switch (i)
      {
        case 0:
        case 1:
          coord[i] = 0;
         break;

        case 2:
          coord[i] = p->preview_width;
         break;

        case 3:
          coord[i] = p->preview_height;
         break;
      }
    }
  }

  preview_rotate_devicesurface_to_previewsurface(p->rotation, coord, p->selection.coordinate);

  p->selection.active = ( (p->selection.coordinate[0] != p->selection.coordinate[2]) &&
                          (p->selection.coordinate[1] != p->selection.coordinate[3]));

  preview_update_maximum_output_size(p);
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_establish_selection(Preview *p)
{
  /* This routine only shall be called if the preview area really is changed. */
 int i;
 float coord[4];

  DBG(DBG_proc, "preview_establish_selection\n");

  preview_order_selection(p);

  xsane.block_update_param = TRUE; /* do not change parameters each time */

  preview_rotate_previewsurface_to_devicesurface(p->rotation, p->selection.coordinate, coord);

  for (i = 0; i < 4; ++i)
  {
    preview_set_option_float(p, xsane.well_known.coord[i], coord[i]);
  }

  xsane_back_gtk_update_scan_window();

  xsane.block_update_param = FALSE;

  xsane_update_param(0); 
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0
static void preview_update_batch_selection(Preview *p)
{
 Batch_selection *batch_selection;

  DBG(DBG_proc, "preview_update_batch_selection\n");

  if (!p->gc_selection) /* window isn't mapped yet */
  {
    return;
  }

  batch_selection = p->batch_selection;

  while (batch_selection)
  {
    preview_draw_rect(p, p->window->window, p->gc_selection, batch_selection->coordinate);

    batch_selection = batch_selection->next;
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_scale_device_to_image(Preview *p, float *xscalep, float *yscalep)
{
 float device_width, device_height;
 float xscale = 1.0;
 float yscale = 1.0;

  device_width  = fabs(p->image_surface[2] - p->image_surface[0]);
  device_height = fabs(p->image_surface[3] - p->image_surface[1]);

  if ( ((p->rotation & 3) == 0) || ((p->rotation & 3) == 2) ) /* 0 or 180 degree */
  {
    if ( (device_width >0) && (device_width < INF) )
    {
      xscale = p->image_width / device_width;
    }

    if ( (device_height >0) && (device_height < INF) )
    {
      yscale = p->image_height / device_height;
    }
  }
  else /* 90 or 270 degree */
  {
    if ( (device_width >0) && (device_width < INF) )
    {
      xscale = p->image_height / device_width;
    }

    if ( (device_height >0) && (device_height < INF) )
    {
      yscale = p->image_width / device_height;
    }
  }

  /* make sure pixels have square dimension */
  if (xscale > yscale)
  {
    yscale = xscale;
  }
  else
  {
    xscale = yscale;
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info, "preview_get_scale_device_to_image: scale = %f, %f\n", xscale, yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_scale_device_to_window(Preview *p, float *xscalep, float *yscalep)
{
 float device_width, device_height;
 float xscale = 1.0;
 float yscale = 1.0;

  /* device_* in device coords */
  device_width  = fabs(p->image_surface[2] - p->image_surface[0]);
  device_height = fabs(p->image_surface[3] - p->image_surface[1]);

  if ( (device_width >0) && (device_width < INF) )
  {
    xscale = p->preview_width / device_width; /* preview width is in window coords */
  }

  if ( (device_height >0) && (device_height < INF) )
  {
    yscale = p->preview_height / device_height; /* preview height is in window coords */
  }

  /* make sure pixels have square dimension */
  if (xscale > yscale)
  {
    yscale = xscale;
  }
  else
  {
    xscale = yscale;
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info, "preview_get_scale_device_to_window: scale = %f, %f\n", xscale, yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_scale_window_to_image(Preview *p, float *xscalep, float *yscalep)
{
 float xscale = 1.0;
 float yscale = 1.0;

  switch (p->rotation & 3)
  {
    case 0: /* do not rotate - 0 degree */
    case 2: /* rotate 180 degree */
    default:
      if (p->image_width > 0)
      {
        xscale = p->image_width / (float) p->preview_width;
      }

      if (p->image_height > 0)
      {
        yscale = p->image_height / (float) p->preview_height;
      }
    break;

    case 1: /* rotate 90 degree */ 
    case 3: /* rotate 270 degree */ 
      if (p->image_height > 0)
      {
        xscale = p->image_height / (float) p->preview_width;
      }

      if (p->image_width > 0)
      {
        yscale = p->image_width / (float) p->preview_height;
      }
    break;

  }

  /* make sure pixels have square dimension */
  if (xscale > yscale)
  {
    yscale = xscale;
  }
  else
  {
    xscale = yscale;
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info, "preview_get_scale_window_to_image: scale = %f, %f\n", xscale, yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_paint_image(Preview *p)
{
 float xscale, yscale, src_x, src_y;
 int dst_x, dst_y, height, x, y, old_y, src_offset, x_direction;
 int rotation;

  DBG(DBG_proc, "preview_paint_image (rotation=%d)\n", p->rotation);

  if (!p->image_data_enh)
  {
    return; /* no image data */
  }

  memset(p->preview_row, 0x80, 3 * p->preview_window_width);

  old_y = -1;
  height = 0;

  rotation = p->rotation;
  if (p->calibration) /* do not rotate calibration image */
  {
    p->rotation = 0;
    xscale=1.0;
    yscale=1.0;
  }
  else
  {
    preview_get_scale_window_to_image(p, &xscale, &yscale);
  }


  switch (p->rotation & 3)
  {
    case 0: /* do not rotate - 0 degree */
    default:

      /* don't draw last line unless it's complete: */
      height = p->image_y; /* last line */

      if (p->image_x == 0 && height < p->image_height)
      {
        ++height; /* use last line if it is complete */
      }

      src_y = 0.0; /* Source Y position index */

      DBG(DBG_info, "preview_height=%d\n", p->preview_height);

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= height)
        {
          break;
        }

        if (p->rotation & 4) /* mirror in x direction */
        {
          src_offset = (y+1) * 3 * p->image_width - 3;
          x_direction = -1;
        }
        else /* not mirrored */
        {
          src_offset = y * 3 * p->image_width;
          x_direction = 1;
        }

        if (old_y != y) /* create new line ? - not necessary if the same line is used several times */
        {
          old_y = y;
          src_x = 0.0; /* Source X position index */

          for (dst_x = 0; dst_x < p->preview_width; ++dst_x)
          {
            x = (int) (src_x + 0.5);
            if (x >= p->image_width)
            {
              break;
            }

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + x_direction * 3 * x + 0]; /* R */
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + x_direction * 3 * x + 1]; /* G */
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + x_direction * 3 * x + 2]; /* B */
            src_x += xscale; /* calculate new source x position index */
          }
        }

        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
        src_y += yscale; /* calculate new source y position index */
      }

      memset(p->preview_row, 0x80, 3*p->preview_window_width);
      for (; dst_y < p->preview_window_height; ++dst_y)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
      }
     break;


    case 1: /* 90 degree */ 
      /* because we run in x direction we have to draw all rows all the time */

      src_y = 0.0;

      DBG(DBG_info, "height=%d\n", height);
      DBG(DBG_info, "preview_height=%d\n", p->preview_height);

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= p->image_width)
        {
          break;
        }

        if (p->rotation & 4) /* mirror in x direction */
        {
          src_offset = y * 3 + 3 * p->image_width * (p->image_height-1);
          x_direction = -1;
        }
        else /* not mirrored */
        {
          src_offset = y * 3;
          x_direction = 1;
        }

        if (old_y != y) /* create new line ? - not necessary if the same line is used several times */
        {
          old_y = y;
          src_x = p->image_height - 1;

          for (dst_x = 0; dst_x < p->preview_width; ++dst_x)
          {
            x = (int) (src_x + 0.5);
            if (x < 0)
            {
              break;
            }

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 0]; /* R */
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 1]; /* G */
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 2]; /* B */
            src_x -= xscale;
          }
        }

        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
        src_y += yscale;
      }

      memset(p->preview_row, 0x80, 3*p->preview_window_width);
      for (; dst_y < p->preview_window_height; ++dst_y)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
      }
     break;


    case 2: /* 180 degree */ 

      /* don't draw last line unless it's complete: */
      height = p->image_y; /* last line */

      if ( (p->image_x == 0) && (height < p->image_height) )
      {
        ++height; /* use last line if it is complete */
      }

      src_y = 0; /* Source Y position index */

      DBG(DBG_info, "height=%d\n", height);
      DBG(DBG_info, "preview_height=%d\n", p->preview_height);

      /* it looks like it is necessary to write row 0 at first */
      memset(p->preview_row, 0x80, 3*p->preview_window_width);
      gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, 0, p->preview_window_width);
      for (dst_y = p->preview_height-1; dst_y >=0; --dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= height)
        {
          break;
        }

        if (p->rotation & 4) /* mirror in x direction */
        {
          src_offset = (y+1) * 3 * p->image_width - 3;
          x_direction = -1;
        }
        else /* not mirrored */
        {
          src_offset = y * 3 * p->image_width;
          x_direction = 1;
        }

        if (old_y != y) /* create new line ? - not necessary if the same line is used several times */
        {
          old_y = y;
          src_x = p->image_width - 1;

          for (dst_x = 0; dst_x < p->preview_width; ++dst_x)
          {
            x = (int) (src_x + 0.5);
            if (x < 0)
            {
              break;
            }

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + x_direction * 3 * x + 0]; /* R */
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + x_direction * 3 * x + 1]; /* G */
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + x_direction * 3 * x + 2]; /* B */
            src_x -= xscale;
          }
        }

        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
        src_y += yscale;
      }
      dst_y = p->preview_height;

      memset(p->preview_row, 0x80, 3*p->preview_window_width);
      for (; dst_y < p->preview_window_height; ++dst_y)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
      }
     break;


    case 3: /* 270 degree */ 
      /* because we run in x direction we have to draw all rows all the time */

      src_y = 0.0;

      DBG(DBG_info, "preview_height=%d\n", p->preview_height);

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= p->image_width)
        {
          break;
        }

        if (p->rotation & 4) /* mirror in x direction */
        {
          src_offset = (p->image_width - y - 1) * 3 + 3 * p->image_width * (p->image_height - 1);
          x_direction = -1;
        }
        else /* not mirrored */
        {
          src_offset = (p->image_width - y - 1) * 3;
          x_direction = 1;
        }

        if (old_y != y) /* create new line ? - not necessary if the same line is used several times */
        {
          old_y = y;
          src_x = 0.0;

          for (dst_x = 0; dst_x < p->preview_width; ++dst_x)
          {
            x = (int) (src_x + 0.5);
            if (x >= p->image_height)
            {
              break;
            }

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 0]; /* R */
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 1]; /* G */
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + x_direction * 3 * x * p->image_width + 2]; /* B */
            src_x += xscale;
          }
        }

        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
        src_y += yscale;
      }

      memset(p->preview_row, 0x80, 3*p->preview_window_width);
      for (; dst_y < p->preview_window_height; ++dst_y)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, dst_y, p->preview_window_width);
      }
     break;
  }

  if (p->calibration) /* do not rotate calibration image */
  {
    p->rotation = rotation;
  }

  /* image is redrawn, we have no visible selections */
  p->previous_selection.active = FALSE;
  p->previous_selection_maximum.active = FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_display_partial_image(Preview *p)
{
  DBG(DBG_proc, "preview_display_partial_image\n");

  preview_paint_image(p);

  if (GTK_WIDGET_DRAWABLE(p->window))
  {
    GtkPreview *preview = GTK_PREVIEW(p->window);
    int src_x, src_y;

    src_x = (p->window->allocation.width - preview->buffer_width)/2;
    src_y = (p->window->allocation.height - preview->buffer_height)/2;

    gtk_preview_put(preview, p->window->window, p->window->style->black_gc, src_x, src_y,
                    0, 0, p->preview_window_width, p->preview_window_height);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_display_maybe(Preview *p)
{
 time_t now;

  DBG(DBG_proc, "preview_display_maybe\n");

  time(&now);

  if (now > p->image_last_time_updated) /* wait at least one secone */
  {
    p->image_last_time_updated = now;
    preview_display_partial_image(p);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_display_image(Preview *p)
{
  DBG(DBG_proc, "preview_display_image\n");

  /* if image height was unknown and got larger than expected get missing memory */
  if (p->params.lines <= 0 && p->image_y < p->image_height)
  {
    p->image_height   = p->image_y;

    p->image_data_raw = realloc(p->image_data_raw, 6 * p->image_width * p->image_height);
    p->image_data_enh = realloc(p->image_data_enh, 3 * p->image_width * p->image_height);
    assert(p->image_data_raw);
    assert(p->image_data_enh);
  }

  preview_display_with_correction(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_save_option(Preview *p, int option, void *save_loc, int *valid)
{
 SANE_Status status;

  DBG(DBG_proc, "preview_save_option\n");

  if (option <= 0)
  {
    *valid = 0;
    return;
  }

  status = xsane_control_option(xsane.dev, option, SANE_ACTION_GET_VALUE, save_loc, 0);
  *valid = (status == SANE_STATUS_GOOD);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_restore_option(Preview *p, int option, void *saved_value, int valid)
{
 const SANE_Option_Descriptor *opt;
 SANE_Status status;
 SANE_Handle dev;

  DBG(DBG_proc, "preview_restore_option\n");

  if (!valid)
  {
    return;
  }

  dev = xsane.dev;
  status = xsane_control_option(dev, option, SANE_ACTION_SET_VALUE, saved_value, 0);

  if (status != SANE_STATUS_GOOD)
  {
    char buf[TEXTBUFSIZE];
    opt = xsane_get_option_descriptor(dev, option);
    if (opt && opt->name)
    {
      snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_SET_OPTION, opt->name, XSANE_STRSTATUS(status));
    }
    else
    {
      snprintf(buf, sizeof(buf), "%s %d: %s.", ERR_SET_OPTION, option, XSANE_STRSTATUS(status));
    }
    xsane_back_gtk_error(buf, TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_set_option_float(Preview *p, int option, float value)
{
 const SANE_Option_Descriptor *opt;
 SANE_Handle dev;
 SANE_Word word;

  DBG(DBG_proc, "preview_set_option_float\n");

  if (option <= 0 || value <= -INF || value >= INF)
  {
    return;
  }

  dev = xsane.dev;
  opt = xsane_get_option_descriptor(dev, option);
  if (opt->type == SANE_TYPE_FIXED)
  {
    word = SANE_FIX(value);
  }
  else
  {
    word = value + 0.5;
  }

  xsane_control_option(dev, option, SANE_ACTION_SET_VALUE, &word, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_set_option(Preview *p, int option, void *value)
{
 SANE_Handle dev;

  DBG(DBG_proc, "preview_set_option\n");

  if (option <= 0)
  {
    return;
  }

  dev = xsane.dev;
  xsane_control_option(dev, option, SANE_ACTION_SET_VALUE, value, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_set_option_val(Preview *p, int option, SANE_Int value)
{
 SANE_Handle dev;

  DBG(DBG_proc, "preview_set_option_val\n");

  if (option <= 0)
  {
    return;
  }

  dev = xsane.dev;
  xsane_control_option(dev, option, SANE_ACTION_SET_VALUE, &value, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_test_image_y(Preview *p)
{
  if (p->image_y >= p->image_height) /* make sure backend does not send more data then expected */
  {
   char buf[TEXTBUFSIZE];

    --p->image_y;
    preview_scan_done(p, 1);
    snprintf(buf, sizeof(buf), "%s", ERR_TOO_MUCH_DATA);
    xsane_back_gtk_error(buf, TRUE);
   return -1;
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_increment_image_y(Preview *p)
{
 size_t extra_size, offset;
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "preview_increment_image_y\n");

  p->image_x = 0;
  ++p->image_y;

  if (p->params.lines <= 0 && p->image_y >= p->image_height) /* backend said it does not know image height */
  {
    offset = 3 * p->image_width*p->image_height;
    extra_size = 3 * 32 * p->image_width;
    p->image_height += 32;

    p->image_data_raw = realloc(p->image_data_raw, (offset + extra_size) * 2);
    p->image_data_enh = realloc(p->image_data_enh, offset + extra_size);

    if ( (!p->image_data_enh) || (!p->image_data_raw) )
    {
      snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_ALLOCATE_IMAGE, strerror(errno));
      preview_scan_done(p, 0);
      xsane_back_gtk_error(buf, TRUE);
     return -1;
    }
    memset(p->image_data_enh + offset, 0xff, extra_size);
  }

  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_read_image_data(gpointer data, gint source, GdkInputCondition cond)
{
 SANE_Status status;
 Preview *p = data;
 char buf[TEXTBUFSIZE];
 u_char imagebuf8[8192];
 guint16 *imagebuf16 = (guint16 *) imagebuf8;
 SANE_Handle dev;
 SANE_Int len;
 int i, j;

  DBG(DBG_proc, "preview_read_image_data\n");

  dev = xsane.dev;
  while (1)
  {
    if ((p->params.depth == 1) || (p->params.depth == 8))
    {
      status = sane_read(dev, imagebuf8, sizeof(imagebuf8), &len);
    }
    else if (p->params.depth == 16)
    {
      if (p->read_offset_16)
      {
        imagebuf8[0] = p->last_offset_16_byte; 
        /* use imagebuf8 and sizeof(imagebuf8) here because sizeof(imagebuf16) returns the size of a pointer */
        status = sane_read(dev, imagebuf8+1, sizeof(imagebuf8) - 1, &len);
        len++;
      }
      else
      {
        status = sane_read(dev, (SANE_Byte *) imagebuf16, sizeof(imagebuf8), &len);
      }

      if (len % 2) /* odd number of bytes */
      {
        len--;
        p->last_offset_16_byte = imagebuf16[len];
        p->read_offset_16 = 1;
      }
      else /* even number of bytes */
      {
        p->read_offset_16 = 0;
      }
    }
    else /* bad bitdepth */
    {
      snprintf(buf, sizeof(buf), "%s %d.", ERR_PREVIEW_BAD_DEPTH, p->params.depth);
      preview_scan_done(p, 0);
      xsane_back_gtk_error(buf, TRUE);
     return;
    }


    if (!p->scanning) /* preview scan may have been canceled while sane_read was executed */
    {
      return; /* ok, the scan has been canceled */
    }


    if (status != SANE_STATUS_GOOD)
    {
      if (status == SANE_STATUS_EOF)
      {
        if (p->params.last_frame) /* got all preview image data */
        {
          p->invalid = FALSE; /* preview is valid now */
          preview_scan_done(p, 1);     /* scan is done, save image */
         return; /* ok, all finished */
        }
        else
        {
          if (p->input_tag >= 0)
          {
            gdk_input_remove(p->input_tag);
            p->input_tag = -1;
          }
          preview_scan_start(p);
          break; /* exit while loop, display_maybe */
        }
      }
      else if (status == SANE_STATUS_CANCELLED)
      {
        p->invalid = FALSE; /* preview is valid now - although it is cancled */
        p->scan_incomplete = TRUE; /* preview is incomplete */
        preview_scan_done(p, 1); /* save scanned part of the preview */
        snprintf(buf, sizeof(buf), "%s", XSANE_STRSTATUS(status));
        xsane_back_gtk_info(buf, TRUE);
       return;
      }

      /* not SANE_STATUS_GOOD and not SANE_STATUS_EOF and not SANE_STATUS_CANCELLED */
      preview_scan_done(p, 0);
      snprintf(buf, sizeof(buf), "%s %s.", ERR_DURING_READ, XSANE_STRSTATUS(status));
      xsane_back_gtk_error(buf, TRUE);
     return;
    }

    if (!len)
    {
      if (p->input_tag >= 0) /* we have a selecet fd */
      {
        break; /* leave preview_read_image_data, will be called by gdk when select_fd event occurs */
      }
      else
      {
        while (gtk_events_pending())
        {
          DBG(DBG_info, "preview_read_image_data: calling gtk_main_iteration\n");
          gtk_main_iteration();
        }
        continue; /* we have to keep this loop running because it will never be called again */
      }
    }

    switch (p->params.format)
    {
      case SANE_FRAME_RGB:
        switch (p->params.depth)
        {
          case 8:
            {
              for (i = 0; i < len; ++i)
              {
                if (preview_test_image_y(p))
                {
                  return; /* backend sends too much image data */
                }

                p->image_data_raw[p->image_offset]   = imagebuf8[i] * 256;
                p->image_data_enh[p->image_offset++] = imagebuf8[i];

                if (p->image_offset%3 == 0)
                {
                  if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                }
              }
            }
            break;

          case 16:
            {
              for (i = 0; i < len/2; ++i)
              {
                if (preview_test_image_y(p))
                {
                  return; /* backend sends too much image data */
                }
  
                p->image_data_raw[p->image_offset]   = imagebuf16[i];
                p->image_data_enh[p->image_offset++] = (u_char) (imagebuf16[i]/256);
  
                if (p->image_offset%3 == 0)
                {
                  if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                }
              }
            }
            break;

          default:
            snprintf(buf, sizeof(buf), "%s %d.", ERR_PREVIEW_BAD_DEPTH, p->params.depth);
            preview_scan_done(p, 0);
            xsane_back_gtk_error(buf, TRUE);
           return;
        }
       break;

      case SANE_FRAME_GRAY:
        switch (p->params.depth)
        {
          case 1:
            for (i = 0; i < len; ++i)
            {
              u_char mask = imagebuf8[i];

              if (preview_test_image_y(p))
              {
                return; /* backend sends too much image data */
              }

              for (j = 7; j >= 0; --j)
              {
                u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;

                p->image_data_raw[p->image_offset]   = gl * 256;
                p->image_data_enh[p->image_offset++] = gl;

                p->image_data_raw[p->image_offset]   = gl * 256;
                p->image_data_enh[p->image_offset++] = gl;

                p->image_data_raw[p->image_offset]   = gl * 256;
                p->image_data_enh[p->image_offset++] = gl;

                if (++p->image_x >= p->image_width)
                {
                  if (preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                  break;	/* skip padding bits */
                }
              }
            }
           break;

          case 8:
            for (i = 0; i < len; ++i)
            {
             u_char gray = imagebuf8[i];

              if (preview_test_image_y(p))
              {
                return; /* backend sends too much image data */
              }

              p->image_data_raw[p->image_offset]   = gray * 256;
              p->image_data_enh[p->image_offset++] = gray;

              p->image_data_raw[p->image_offset]   = gray * 256;
              p->image_data_enh[p->image_offset++] = gray;

              p->image_data_raw[p->image_offset]   = gray * 256;
              p->image_data_enh[p->image_offset++] = gray;
              if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
	      {
                return;
              }
            }
           break;

          case 16:
            for (i = 0; i < len/2; ++i)
            {
             u_char gray = imagebuf16[i]/256;

              if (preview_test_image_y(p))
              {
                return; /* backend sends too much image data */
              }

              p->image_data_raw[p->image_offset]   = imagebuf16[i];
              p->image_data_enh[p->image_offset++] = gray;

              p->image_data_raw[p->image_offset]   = imagebuf16[i];
              p->image_data_enh[p->image_offset++] = gray;

              p->image_data_raw[p->image_offset]   = imagebuf16[i];
              p->image_data_enh[p->image_offset++] = gray;

              if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
	      {
                return;
              }
            }
           break;

          default:
            snprintf(buf, sizeof(buf), "%s %d.", ERR_PREVIEW_BAD_DEPTH, p->params.depth);
            preview_scan_done(p, 0);
            xsane_back_gtk_error(buf, TRUE);
           return;
        }
       break;

          case SANE_FRAME_RED:
          case SANE_FRAME_GREEN:
          case SANE_FRAME_BLUE:
            switch (p->params.depth)
            {
              case 1:
                for (i = 0; i < len; ++i)
                {
                 u_char mask = imagebuf8[i];

                  if (preview_test_image_y(p))
                  {
                    return; /* backend sends too much image data */
                  }

                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;

                    p->image_data_raw[p->image_offset] = gl * 256;
                    p->image_data_enh[p->image_offset] = gl;

                    p->image_offset += 3;
                    if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                    {
                      return;
                    }
                  }
                }
               break;

              case 8:
                for (i = 0; i < len; ++i)
                {
                  if (preview_test_image_y(p))
                  {
                    return; /* backend sends too much image data */
                  }

                  p->image_data_raw[p->image_offset] = imagebuf8[i] * 256;
                  p->image_data_enh[p->image_offset] = imagebuf8[i];

                  p->image_offset += 3;
                  if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                }
               break;

              case 16:
                for (i = 0; i < len/2; ++i)
                {
                  if (preview_test_image_y(p))
                  {
                    return; /* backend sends too much image data */
                  }

                  p->image_data_raw[p->image_offset] = imagebuf16[i];
                  p->image_data_enh[p->image_offset] = (u_char) (imagebuf16[i]/256);

                  p->image_offset += 3;
                  if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                }
               break;

              default:
                snprintf(buf, sizeof(buf), "%s %d.", ERR_PREVIEW_BAD_DEPTH, p->params.depth);
                preview_scan_done(p, 0);
                xsane_back_gtk_error(buf, TRUE);
               return;
            }
           break;

          default:
            snprintf(buf, sizeof(buf), "%s %d.", ERR_BAD_FRAME_FORMAT, p->params.format);
            preview_scan_done(p, 0);
            xsane_back_gtk_error(buf, TRUE);
           return;
    }

    if (p->input_tag < 0)
    {
      preview_display_maybe(p);
      while (gtk_events_pending())
      {
        DBG(DBG_info, "preview_read_image_data: calling gtk_main_iteration\n");
        gtk_main_iteration();
      }
    }
  }
  preview_display_maybe(p);

 return;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_scan_done(Preview *p, int save_image)
{
 int i;

  DBG(DBG_proc, "preview_scan_done\n");

  p->scanning = FALSE;

  if (p->input_tag >= 0)
  {
    gdk_input_remove(p->input_tag);
    p->input_tag = -1;
  }

  sane_cancel(xsane.dev);

  xsane.block_update_param = TRUE; /* do not change parameters each time */

  preview_restore_option(p, xsane.well_known.dpi,   &p->saved_dpi,   p->saved_dpi_valid);
  preview_restore_option(p, xsane.well_known.dpi_x, &p->saved_dpi_x, p->saved_dpi_x_valid);
  preview_restore_option(p, xsane.well_known.dpi_y, &p->saved_dpi_y, p->saved_dpi_y_valid);

  for (i = 0; i < 4; ++i)
  {
    preview_restore_option(p, xsane.well_known.coord[i], &p->saved_coord[i], p->saved_coord_valid[i]);
  }

  preview_restore_option(p, xsane.well_known.scanmode, &p->saved_scanmode, p->saved_scanmode_valid);

  preview_restore_option(p, xsane.well_known.bit_depth, &p->saved_bit_depth, p->saved_bit_depth_valid);

  preview_set_option_val(p, xsane.well_known.preview, SANE_FALSE);

  gtk_widget_set_sensitive(p->cancel, FALSE);
  xsane_set_sensitivity(TRUE);

  xsane.block_update_param = FALSE;

  preview_update_selection(p);

  if (save_image)
  {
    preview_save_image(p);    /* save preview image */
    preview_display_image(p);
  }

  preview_update_surface(p, 1); /* if surface was not defined it's necessary to redefine it now */

  xsane_update_histogram(TRUE /* update raw */);

  sane_get_parameters(xsane.dev, &xsane.param); /* update xsane.param */

  if ((preferences.preselect_scan_area) && (!p->startimage) && (!xsane.medium_calibration))
  {
    preview_autoselect_scan_area(p, p->selection.coordinate); /* get autoselection coordinates */
    preview_draw_selection(p); 
    preview_establish_selection(p); 
    xsane_update_histogram(TRUE /* update_raw */); /* update histogram (necessary because overwritten by preview_update_surface) */
  }

  if ((preferences.auto_correct_colors) && (!xsane.medium_calibration) && (!xsane.enable_color_management))
  {
    xsane_calculate_raw_histogram();
    xsane_set_auto_enhancement();
    xsane_enhancement_by_histogram(preferences.auto_enhance_gamma);
  }

  xsane_batch_scan_update_icon_list();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_get_memory(Preview *p)
{
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "preview_get_memory\n");

  if (p->image_data_enh)
  {
    free(p->image_data_enh);
    p->image_data_enh = 0;
  }

  if (p->image_data_raw)
  {
    free(p->image_data_raw);
    p->image_data_raw = 0;
  }

  if (p->preview_row)
  {
    free(p->preview_row);
    p->preview_row = 0;
  }

  p->image_data_raw = malloc(6 * p->image_width * p->image_height);
  p->image_data_enh = malloc(3 * p->image_width * p->image_height);
  p->preview_row    = malloc(3 * p->preview_window_width);

  if ( (!p->image_data_raw) || (!p->image_data_enh) || (!p->preview_row) )
  {
    if (p->image_data_enh)
    {
      free(p->image_data_enh);
      p->image_data_enh = 0;
    }

    if (p->image_data_raw)
    {
      free(p->image_data_raw);
      p->image_data_raw = 0;
    }

    if (p->preview_row)
    {
      free(p->preview_row);
      p->preview_row = 0;
    }

    DBG(DBG_error, "failed to allocate image buffer: %s", strerror(errno));
    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_ALLOCATE_IMAGE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);

    return -1; /* error */
  }

  memset(p->image_data_raw, 0x80, 6*p->image_width*p->image_height); /* clean memory */
  memset(p->image_data_enh, 0x80, 3*p->image_width*p->image_height); /* clean memory */

  return 0; /* ok */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* preview_scan_start is called 3 times in 3 pass color scanning mode */
static void preview_scan_start(Preview *p)
{
 SANE_Handle dev = xsane.dev;
 SANE_Status status;
 char buf[TEXTBUFSIZE];
 int fd, y;

  DBG(DBG_proc, "preview_scan_start\n");

  p->read_offset_16 = 0;

  xsane.medium_changed = FALSE;

  preview_display_valid(p);

  p->startimage = 0; /* we start the scan so lets say the startimage is not displayed any more */

  p->image_surface[0]   = p->surface[p->index_xmin];
  p->image_surface[1]   = p->surface[p->index_ymin];
  p->image_surface[2]   = p->surface[p->index_xmax];
  p->image_surface[3]   = p->surface[p->index_ymax];

  gtk_widget_set_sensitive(p->cancel, TRUE);
  xsane_set_sensitivity(FALSE);

  /* clear preview row */
  memset(p->preview_row, 0xff, 3*p->preview_width);

  for (y = 0; y < p->preview_height; ++y)
  {
    gtk_preview_draw_row(GTK_PREVIEW(p->window), p->preview_row, 0, y, p->preview_width);
  }

  if (p->input_tag >= 0)
  {
    gdk_input_remove(p->input_tag);
    p->input_tag = -1;
  }

  status = sane_start(dev);
  if (status != SANE_STATUS_GOOD)
  {
    preview_scan_done(p, 0);
    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_START_SCANNER, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
   return;
  }

  status = sane_get_parameters(dev, &p->params);
  if (status != SANE_STATUS_GOOD)
  {
    preview_scan_done(p, 0);
    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_GET_PARAMS, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
   return;
  }

  p->image_offset = p->image_x = p->image_y = 0;

  if (p->params.format >= SANE_FRAME_RED && p->params.format <= SANE_FRAME_BLUE)
  {
    p->image_offset = p->params.format - SANE_FRAME_RED;
  }

  if ( (!p->image_data_enh)  || (p->params.pixels_per_line != p->image_width)
      || ( (p->params.lines >= 0) && (p->params.lines != p->image_height) ) )
  {
    p->image_width  = p->params.pixels_per_line;
    p->image_height = p->params.lines;

    if (p->image_height < 0)
    {
      p->image_height = 32;	/* may have to adjust as we go... */
    }

    if (preview_get_memory(p))
    {
      preview_scan_done(p, 0); /* error */
      snprintf(buf, sizeof(buf), "%s", ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return;
    }
  }
  else if (p->scanning == FALSE) /* single pass scan or first run in 3 pass mode */
  {
    memset(p->image_data_raw, 0x80, 6*p->image_width*p->image_height); /* clean memory */
    memset(p->image_data_enh, 0x80, 3*p->image_width*p->image_height); /* clean memory */
  }

  /* we do not have any active selection (image is redrawn while scanning) */
  p->selection.active = FALSE;
  p->previous_selection_maximum.active = FALSE;

#ifndef BUGGY_GDK_INPUT_EXCEPTION
  if ((sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD) && (sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD))
  {
    p->input_tag = gdk_input_add(fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, preview_read_image_data, p);
  }
  else
#endif
  {
    preview_read_image_data(p, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_make_image_path(Preview *p, size_t filename_size, char *filename, int level)
{
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "preview_make_image_path\n");

  snprintf(buf, sizeof(buf), "xsane-preview-level-%d-", level);
  return xsane_back_gtk_make_path(filename_size, filename, 0, 0, buf, xsane.dev_name, ".ppm", XSANE_PATH_TMP);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int preview_create_batch_icon_from_file(Preview *p, FILE *in, Batch_Scan_Parameters *parameters, int min_quality, int *min_time)
{
 u_int psurface_type, psurface_unit;
 int image_width, image_height;
 int xoffset, yoffset, width, height;
 int max_val;
 int x, y, dx, dy;
 int time;
 float psurface[4];
 float dsurface[4];
 char buf[TEXTBUFSIZE];
 float scale;
 int header = 0;
 int rotate16 = 16 - preview_gamma_input_bits;
 int rotate8 = preview_gamma_input_bits - 8;
 guint16 r,g,b;
 int c;
 int maximum_size;
 int quality = 0;
 int xx, yy;
 int offset = 0;
 guchar *data;
 size_t bytes_read;

  DBG(DBG_proc, "preview_create_batch_icon_from_file\n");

  if (!in)
  {
    return min_quality;
  }

  /* See whether there is a saved preview and load it if present: */
  if (fscanf(in, "P6\n"
                 "# surface: %g %g %g %g %u %u\n"
                 "# time: %d\n"
                 "%d %d\n%d",
	      psurface + 0, psurface + 1, psurface + 2, psurface + 3,
	      &psurface_type, &psurface_unit,
              &time,
	      &image_width, &image_height,
              &max_val) != 10)
  {
    DBG(DBG_info, "no preview image\n");
   return min_quality;
  }

  fgets(buf, sizeof(buf), in); /* skip newline character. this made a lot of problems in the past, so I skip it this way */

  header = ftell(in);

  if (min_quality >= 0) /* read real preview */
  {
    if ((psurface_type != p->surface_type) || (psurface_unit != p->surface_unit))
    {
      DBG(DBG_info, "incompatible surface types %d <> %d\n", psurface_type, p->surface_type);
     return min_quality;
    }

    preview_rotate_previewsurface_to_devicesurface(p->rotation, p->surface, dsurface);


    DBG(DBG_info, "stored image surface = [%3.2f %3.2f %3.2f %3.2f]\n",
                   psurface[0], psurface[1], psurface[2], psurface[3]);
    DBG(DBG_info, "batch selection = [%3.2f %3.2f %3.2f %3.2f]\n",
                   parameters->tl_x, parameters->tl_y, parameters->br_x, parameters->br_y);
    DBG(DBG_info, "preview device surface    = [%3.2f %3.2f %3.2f %3.2f]\n",
                   dsurface[0], dsurface[1], dsurface[2], dsurface[3]);

    xoffset = (parameters->tl_x - psurface[0])/(psurface[2] - psurface[0]) * image_width;
    yoffset = (parameters->tl_y - psurface[1])/(psurface[3] - psurface[1]) * image_height;
    width   = (parameters->br_x - parameters->tl_x)/(psurface[2] - psurface[0]) * image_width;
    height  = (parameters->br_y - parameters->tl_y)/(psurface[3] - psurface[1]) * image_height;

    quality = width;

    if ((xoffset < 0) || (yoffset < 0) ||
        (xoffset+width > image_width) || (yoffset+height > image_height) ||
        (width == 0) || (height == 0))
    {
      DBG(DBG_info, "image does not cover wanted surface part\n");
     return min_quality;
    }

    DBG(DBG_info, "quality = %d\n", quality);

    if ( ((float) min_quality / (quality+1)) > 1.05) /* already loaded image has better quality */
    {
      DBG(DBG_info, "already loaded image has higher quality\n");
     return min_quality;
    }

    if ( ((float) min_quality / (quality+1)) > 0.95) /* qualities are comparable */
    {
      if (*min_time > time) /* take more recent scan */
      {
        DBG(DBG_info, "images have comparable quality, already loaded is more up to date\n");
       return min_quality;
      }
      DBG(DBG_info, "images have comparable quality, this image is more up to date\n");
    }
    else
    {
      DBG(DBG_info, "image has best quality\n");
    }
  }
  else
  {
    xoffset = 0;
    yoffset = 0;
    width   = image_width;
    height  = image_height;
  }


  {
    float xscale = (float)width / parameters->gtk_preview_size;
    float yscale = (float)height / parameters->gtk_preview_size;

    if (xscale > yscale)
    {
      scale = xscale;
    }
    else
    {
      scale = yscale;
    }
  }

  width = width / scale;
  height = height / scale;

  if (width > parameters->gtk_preview_size)
  {
    width = parameters->gtk_preview_size;
  }

  if (height > parameters->gtk_preview_size)
  {
    height = parameters->gtk_preview_size;
  }

  maximum_size = parameters->gtk_preview_size -1;

  dx = (parameters->gtk_preview_size - width) / 2;
  dy = (parameters->gtk_preview_size - height) / 2;


  data = malloc(parameters->gtk_preview_size * parameters->gtk_preview_size * 3);
  if (!data)
  {
    DBG(DBG_error, "preview_create_batch_icon_from_file: out of memory\n");
   return min_quality;
  }

  /* make unused parts white */
  for (x = 0; x< parameters->gtk_preview_size * parameters->gtk_preview_size * 3; x++)
  {
    data[x] = 0xF0;
  }

  if (max_val == 65535)
  {
    for (y=0; y < height; y++)
    { 
      for (x=0; x < width; x++)
      {
        fseek(in, header + (xoffset + (int)(x * scale) + (yoffset + (int)(y * scale)) * image_width) * 6, SEEK_SET);

        bytes_read = fread(&r, 2, 1, in); /* read 16 bit value in machines byte order */
        r = preview_gamma_data_red[r >> rotate16];

        bytes_read = fread(&g, 2, 1, in);
        g = preview_gamma_data_green[g >> rotate16];

        bytes_read = fread(&b, 2, 1, in);
        b = preview_gamma_data_blue[b >> rotate16];

        c = r * 65536 + g * 256 + b;

        switch (parameters->rotation)
        {
          case 0: /* 0 degree */
            xx = x + dx;
            yy = y + dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 1: /* 90 degree */
            xx = maximum_size - y;
            yy = x + dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 2: /* 180 degree */
            xx = maximum_size - x - dx;
            yy = maximum_size - y - dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 3: /* 270 degree */
            xx = y + dy;
            yy = maximum_size - x - dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 4: /* 0 degree, x-mirror */
            xx = maximum_size - x - dx;
            yy = y + dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 5: /* 90 degree, x-mirror */
            xx = y + dy;
            yy = x + dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 6: /* 180 degree, x-mirror */
            xx = x + dx;
            yy = maximum_size - y - dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 7: /* 270 degree, x-mirror */
            xx = maximum_size - y - dy;
            yy = maximum_size - x - dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;
        }

        data[offset + 0] = r;
        data[offset + 1] = g;
        data[offset + 2] = b;
      }
    }

    for (y = 0; y < parameters->gtk_preview_size; y++)
    {
      gtk_preview_draw_row(GTK_PREVIEW(parameters->gtk_preview), data + 3 * parameters->gtk_preview_size * y,
                           0, y, parameters->gtk_preview_size);
    }
  }
  else /* depth = 8 */
  {
    for (y=0; y < height; y++)
    { 
      for (x=0; x < width; x++)
      {
        fseek(in, header + (xoffset + (int)(x * scale) + (yoffset + (int)(y * scale)) * image_width) * 3, SEEK_SET);

        r = fgetc(in);
        r = preview_gamma_data_red[r << rotate8];

        g = fgetc(in);
        g = preview_gamma_data_green[g << rotate8];

        b = fgetc(in);
        b = preview_gamma_data_blue[b << rotate8];
        switch (parameters->rotation)
        {
          case 0: /* 0 degree */
            xx = x + dx;
            yy = y + dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 1: /* 90 degree */
            xx = maximum_size - y;
            yy = x + dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 2: /* 180 degree */
            xx = maximum_size - x - dx;
            yy = maximum_size - y - dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 3: /* 270 degree */
            xx = y + dy;
            yy = maximum_size - x - dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 4: /* 0 degree, x-mirror */
            xx = maximum_size - x - dx;
            yy = y + dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 5: /* 90 degree, x-mirror */
            xx = y + dy;
            yy = x + dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 6: /* 180 degree, x-mirror */
            xx = x + dx;
            yy = maximum_size - y - dy;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;

          case 7: /* 270 degree, x-mirror */
            xx = maximum_size - y - dy;
            yy = maximum_size - x - dx;
            offset = parameters->gtk_preview_size * 3 * yy + 3*(xx);
          break;
        }

        data[offset + 0] = r;
        data[offset + 1] = g;
        data[offset + 2] = b;
      }
    }

    for (y = 0; y < parameters->gtk_preview_size; y++)
    {
      gtk_preview_draw_row(GTK_PREVIEW(parameters->gtk_preview), data + 3 * parameters->gtk_preview_size * y,
                           0, y, parameters->gtk_preview_size);
    }
  }

  free(data);

 return quality;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_create_batch_icon(Preview *p, Batch_Scan_Parameters *parameters)
{
 FILE *in = NULL;
 int quality = 0;
 int time = 0;
 int level;

  for(level = 2; level >= 0; level--)
  {
    if (p->filename[level])
    {
      in = fopen(p->filename[level], "rb"); /* read binary (b for win32) */
      if (in)
      {
        quality = preview_create_batch_icon_from_file(xsane.preview, in, parameters, quality, &time);
        fclose(in);
      }
    }
  }

  if (quality <= 0)
  {
   char filename[PATH_MAX];

    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-startimage", 0, ".pnm", XSANE_PATH_SYSTEM);
    in = fopen(filename, "rb"); /* read binary (b for win32) */
    if (in)
    {
      preview_create_batch_icon_from_file(xsane.preview, in, parameters, -1, &time);
      fclose(in);
    }
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_restore_image_from_file(Preview *p, FILE *in, int min_quality, int *min_time)
{
 u_int psurface_type, psurface_unit;
 int image_width, image_height;
 int xoffset, yoffset, width, height;
 int max_val;
 int quality = 0;
 int x, y;
 int time;
 float psurface[4];
 float dsurface[4];
 size_t nread;
 guint16 *imagep;
 guint16 *imagepx;
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "preview_restore_image_from_file\n");

  if (!in)
  {
    return min_quality;
  }

  /* See whether there is a saved preview and load it if present: */
  if (fscanf(in, "P6\n"
                 "# surface: %g %g %g %g %u %u\n"
                 "# time: %d\n"
                 "%d %d\n%d",
	      psurface + 0, psurface + 1, psurface + 2, psurface + 3,
	      &psurface_type, &psurface_unit,
              &time,
	      &image_width, &image_height,
              &max_val) != 10)
  {
    DBG(DBG_info, "no preview image\n");
    return min_quality;
  }

  fgets(buf, sizeof(buf), in); /* skip newline character. this made a lot of problems in the past, so I skip it this way */


  if (min_quality >= 0) /* read real preview */
  {
    if ((psurface_type != p->surface_type) || (psurface_unit != p->surface_unit))
    {
      DBG(DBG_info, "incompatible surface types %d <> %d\n", psurface_type, p->surface_type);
     return min_quality;
    }

    preview_rotate_previewsurface_to_devicesurface(p->rotation, p->surface, dsurface);


    DBG(DBG_info, "stored image surface      = [%3.2f %3.2f %3.2f %3.2f]\n",
                   psurface[0], psurface[1], psurface[2], psurface[3]);
    DBG(DBG_info, "preview selection surface = [%3.2f %3.2f %3.2f %3.2f]\n",
                   p->surface[0], p->surface[1], p->surface[2], p->surface[3]);
    DBG(DBG_info, "preview device surface    = [%3.2f %3.2f %3.2f %3.2f]\n",
                   dsurface[0], dsurface[1], dsurface[2], dsurface[3]);

    xoffset = (dsurface[0] - psurface[0])/(psurface[2] - psurface[0]) * image_width;
    yoffset = (dsurface[1] - psurface[1])/(psurface[3] - psurface[1]) * image_height;
    width   = (dsurface[2] - dsurface[0])/(psurface[2] - psurface[0]) * image_width;
    height  = (dsurface[3] - dsurface[1])/(psurface[3] - psurface[1]) * image_height;

    quality = width;

    if ((xoffset < 0) || (yoffset < 0) ||
        (xoffset+width > image_width) || (yoffset+height > image_height) ||
        (width == 0) || (height == 0))
    {
      DBG(DBG_info, "image does not cover wanted surface part\n");
      return min_quality; /* file does not cover wanted surface part */
    }

    DBG(DBG_info, "quality = %d\n", quality);

    if ( ((float) min_quality / (quality+1)) > 1.05) /* already loaded image has better quality */
    {
      DBG(DBG_info, "already loaded image has higher quality\n");
      return min_quality;
    }

    if ( ((float) min_quality / (quality+1)) > 0.95) /* qualities are comparable */
    {
      if (*min_time > time) /* take more recent scan */
      {
        DBG(DBG_info, "images have comparable quality, already loaded is more up to date\n");
        return min_quality;
      }
      DBG(DBG_info, "images have comparable quality, this image is more up to date\n");
    }
    else
    {
      DBG(DBG_info, "image has best quality\n");
    }
  }
  else /* read startimage or calibrationimage */
  {
    xoffset = 0;
    yoffset = 0;
    width   = image_width;
    height  = image_height;
  }

  if (max_val == 65535)
  {
    p->params.depth = 16;
  }
  else
  {
    p->params.depth = 8;
  }

  p->image_width  = width;
  p->image_height = height;

  if (preview_get_memory(p))
  {
    return min_quality; /* error allocating memory */
  }

  if (p->params.depth == 16)
  {
    fseek(in, yoffset * image_width * 6, SEEK_CUR); /* skip unused lines */

    imagep = p->image_data_raw;

    for (y = yoffset; y < yoffset + height; y++)
    {
      fseek(in, xoffset * 6, SEEK_CUR); /* skip unused pixel left of area */

      nread = fread(imagep, 6, width, in);
      imagep += width * 3; /* imagep is a pointer to a 2 byte value, so we use 3 instead 6 here */

      fseek(in, (image_width - width - xoffset) * 6, SEEK_CUR); /* skip unused pixel right of area */
    }
  }
  else /* depth = 8 */
  {
    fseek(in, yoffset * image_width * 3, SEEK_CUR); /* skip unused lines */

    imagep = p->image_data_raw;

    for (y = yoffset; y < yoffset + height; y++)
    {
      fseek(in, xoffset * 3, SEEK_CUR); /* skip unused pixel left of area */

      imagepx = imagep;
      for (x = 0; x < width; x++)
      {
        *imagepx++ = ((guint16) fgetc(in)) * 256; /* transfrom to 16 bit image with correct byte order */
        *imagepx++ = ((guint16) fgetc(in)) * 256;
        *imagepx++ = ((guint16) fgetc(in)) * 256;
      }
      imagep += width * 3; /* imagep is a pointer to a 2 byte value, so we use 3 instead 6 here */

      fseek(in, (image_width - width - xoffset) * 3, SEEK_CUR); /* skip unused pixel right of area */
    }
  }

  p->image_x = width;
  p->image_y = height;

  p->image_surface[0]   = p->surface[p->index_xmin];
  p->image_surface[1]   = p->surface[p->index_ymin];
  p->image_surface[2]   = p->surface[p->index_xmax];
  p->image_surface[3]   = p->surface[p->index_ymax];

  *min_time = time;

 return quality;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_restore_image(Preview *p)
{
 FILE *in;
 int quality = 0;
 int time = 0;
 int level;

  DBG(DBG_proc, "preview_restore_image\n");

  p->startimage = 0;

  if (p->calibration)
  {
   char filename[PATH_MAX];

    DBG(DBG_proc, "calibration mode\n");
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-calibration", 0, ".pnm", XSANE_PATH_SYSTEM);
    in = fopen(filename, "rb"); /* read binary (b for win32) */
    if (in)
    {
      quality = preview_restore_image_from_file(p, in, -1, &time);
      fclose(in);
    }
  }
  else
  {
    /* See whether there is a saved preview and load it if present: */
    for(level = 2; level >= 0; level--)
    {
      if (p->filename[level])
      {
        in = fopen(p->filename[level], "rb"); /* read binary (b for win32) */
        if (in)
        {
          quality = preview_restore_image_from_file(p, in, quality, &time);
          fclose(in);
        }
      }
    }

    if (quality == 0) /* no image found, read startimage */
    {
     char filename[PATH_MAX];

      DBG(DBG_proc, "no suitable image available, using startimage\n");
      xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-startimage", 0, ".pnm", XSANE_PATH_SYSTEM);
      in = fopen(filename, "rb"); /* read binary (b for win32) */
      if (in)
      {
        quality = preview_restore_image_from_file(p, in, -1, &time);
        fclose(in);
      }
      else
      {
       guint16 *imagep;

        DBG(DBG_error0, "ERROR: xsane-startimage not found. Looks like xsane is not installed correct.\n");

        p->image_width  = 1;
        p->image_height = 1;
        p->params.depth = 16;

        preview_get_memory(p);

        imagep = p->image_data_raw;
        *imagep++ = 65535;
        *imagep++ = 00000;
        *imagep++ = 00000;

        p->image_x = p->image_width;
        p->image_y = p->image_height;

        p->image_surface[0]   = p->surface[p->index_xmin];
        p->image_surface[1]   = p->surface[p->index_ymin];
        p->image_surface[2]   = p->surface[p->index_xmax];
        p->image_surface[3]   = p->surface[p->index_ymax];
      }
      p->startimage = 1;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_get_pixel_color(Preview *p, int x, int y, int *raw_red, int *raw_green, int *raw_blue,
                                                              int *enh_red, int *enh_green, int *enh_blue)
{
 int image_x, image_y;
 int offset;
 int rotate = 16 - preview_gamma_input_bits;

  DBG(DBG_proc, "preview_get_pixel_color\n");
 
  if (p->image_data_raw)
  {
    preview_transform_coordinate_window_to_image(p, x, y, &image_x, &image_y);

    if ( (image_x >= 0) && (image_x < p->image_width) && (image_y >=0) && (image_y < p->image_height) )
    {
      offset = 3 * (image_y * p->image_width + image_x);

      if (!xsane.negative) /* positive */
      {
        *raw_red   = (p->image_data_raw[offset    ]) >> 8;
        *raw_green = (p->image_data_raw[offset + 1]) >> 8;
        *raw_blue  = (p->image_data_raw[offset + 2]) >> 8;
      }
      else /* negative */
      {
        *raw_red   = 255 - ((p->image_data_raw[offset    ]) >> 8);
        *raw_green = 255 - ((p->image_data_raw[offset + 1]) >> 8);
        *raw_blue  = 255 - ((p->image_data_raw[offset + 2]) >> 8);
      }

      /* the enhanced pixels are already inverted when negative is selected */ 
      /* do not use image_data_enh because the preview gamma value is applied to this */
      *enh_red   = histogram_gamma_data_red  [(p->image_data_raw[offset    ]) >> rotate];
      *enh_green = histogram_gamma_data_green[(p->image_data_raw[offset + 1]) >> rotate];
      *enh_blue  = histogram_gamma_data_blue [(p->image_data_raw[offset + 2]) >> rotate];

     return 0;
    }
  }

 return -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_swap_int(int *a, int *b)
{
 int i;
  i = *a;
  *a = *b;
  *b = i; 
}

static void preview_display_zoom(Preview *p, int x, int y, int zoom)
{
 int pointer_x, pointer_y;
 int image_x, image_y;
 int image_x_lof, image_x_rof;
 int image_y_tof, image_y_bof;
 int image_x_min, image_y_min;
 int image_x_max, image_y_max;
 int image_x_direction, image_y_direction;
 int offset;
 int r, g, b;
 int px, py;
 int i;
 u_char *row;

  DBG(DBG_proc, "preview_display_zoom");

  if (!p->image_data_raw)
  {
    return;
  }

  row = calloc(XSANE_ZOOM_SIZE, 3);

  if (row)
  {
    preview_transform_coordinate_window_to_image(p, x, y, &image_x, &image_y);
    preview_transform_coordinate_window_to_image(p, x, y, &pointer_x, &pointer_y);

    image_x_min = image_x - XSANE_ZOOM_SIZE/(zoom*2);
    image_y_min = image_y - XSANE_ZOOM_SIZE/(zoom*2);

    image_x_max = image_x_min + XSANE_ZOOM_SIZE/zoom + 1;
    image_y_max = image_y_min + XSANE_ZOOM_SIZE/zoom + 1;

    xsane_bound_int(&image_x_min, 0, p->image_width - 1);
    xsane_bound_int(&image_x_max, 0, p->image_width - 1);
    xsane_bound_int(&image_y_min, 0, p->image_height - 1);
    xsane_bound_int(&image_y_max, 0, p->image_height - 1);

    if ((image_x_max - image_x_min) && (image_y_max - image_y_min))
    {
      image_x_lof = image_x_min - (image_x - XSANE_ZOOM_SIZE/(zoom*2));
      image_x_rof = image_x - XSANE_ZOOM_SIZE/(zoom*2) + XSANE_ZOOM_SIZE/zoom + 1 - image_x_max;
      image_y_tof = image_y_min - (image_y - XSANE_ZOOM_SIZE/(zoom*2));
      image_y_bof = image_y - XSANE_ZOOM_SIZE/(zoom*2) + XSANE_ZOOM_SIZE/zoom + 1 - image_y_max;

      image_x_direction = 1;
      image_y_direction = 1;

      switch (p->rotation & 3)
      {
        case 0: /* do not rotate - 0 degree */
        default:
         break;

        case 1: /* 90 degree */
          xsane_swap_int(&image_y_min, &image_y_max);
          xsane_swap_int(&image_y_tof, &image_y_bof);
          image_y_direction *= -1;
         break;

        case 2: /* 180 degree */
          xsane_swap_int(&image_x_min, &image_x_max);
          xsane_swap_int(&image_x_lof, &image_x_rof);
          xsane_swap_int(&image_y_min, &image_y_max);
          xsane_swap_int(&image_y_tof, &image_y_bof);
          image_x_direction *= -1;
          image_y_direction *= -1;
         break;

        case 3: /* 270 degree */
          xsane_swap_int(&image_x_min, &image_x_max);
          xsane_swap_int(&image_x_lof, &image_x_rof);
          image_x_direction *= -1;
         break;
      }


      if ((p->rotation & 1) == 0)
      {
        if (p->rotation & 4) /* mirror */
        {
          xsane_swap_int(&image_x_min, &image_x_max);
          xsane_swap_int(&image_x_lof, &image_x_rof);
          image_x_direction *= -1;
        }

        py = image_y_tof * zoom;

        for (i=0; i < py; i++)
        {
          gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, i, XSANE_ZOOM_SIZE);
        }

        for (image_y = image_y_min; image_y != image_y_max + image_y_direction; image_y += image_y_direction)
        {
          px = image_x_lof * zoom;

          for (image_x = image_x_min; image_x != image_x_max + image_x_direction; image_x += image_x_direction)
          {
            offset = 3 * (image_y * p->image_width + image_x);
 
            r = (p->image_data_enh[offset  ]);
            g = (p->image_data_enh[offset+1]);
            b = (p->image_data_enh[offset+2]);

            if ( (image_x == pointer_x) && (image_y == pointer_y) )
            {
              r = g = b = (128 + (r+g+b) / 3) & 255; /* mark the cursor position */
            }

            for (i=0; (i<zoom) && (px < XSANE_ZOOM_SIZE); i++)
            {
              row[px*3+0] = r;
              row[px*3+1] = g;
              row[px*3+2] = b;
              px++;
            }
          }

          for (i=0; (i<zoom) && (py < XSANE_ZOOM_SIZE); i++)
          {
            gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, py++, XSANE_ZOOM_SIZE);
          }
        }
      }
      else /* swap x and y */
      {
        if (p->rotation & 4) /* mirror */
        {
          xsane_swap_int(&image_y_min, &image_y_max);
          xsane_swap_int(&image_y_tof, &image_y_bof);
          image_y_direction *= -1;
        }

        py = image_x_lof * zoom;

        for (i=0; i < py; i++)
        {
          gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, i, XSANE_ZOOM_SIZE);
        }

        for (image_x = image_x_min; image_x != image_x_max + image_x_direction; image_x += image_x_direction)
        {
          px = image_y_tof * zoom;

          for (image_y = image_y_min; image_y != image_y_max + image_y_direction; image_y += image_y_direction)
          {
            offset = 3 * (image_y * p->image_width + image_x);
 
            r = (p->image_data_enh[offset  ]);
            g = (p->image_data_enh[offset+1]);
            b = (p->image_data_enh[offset+2]);

            if ( (image_x == pointer_x) && (image_y == pointer_y) )
            {
              r = g = b = (128 + (r+g+b) / 3) & 255; /* mark the cursor position */
            }

            for (i=0; (i<zoom) && (px < XSANE_ZOOM_SIZE); i++)
            {
              row[px*3+0] = r;
              row[px*3+1] = g;
              row[px*3+2] = b;
              px++;
            }
          }

          for (i=0; (i<zoom) && (py < XSANE_ZOOM_SIZE); i++)
          {
            gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, py++, XSANE_ZOOM_SIZE);
          }
        }
      }

      for (i=0; i < XSANE_ZOOM_SIZE; i++)
      {
        row[i*3+0] = 0;
        row[i*3+1] = 0;
        row[i*3+2] = 0;
      }

      for (i=py; i < XSANE_ZOOM_SIZE; i++)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, i, XSANE_ZOOM_SIZE);
      }
    }
    else
    {
      for (i = 0; i < XSANE_ZOOM_SIZE; i++)
      {
        gtk_preview_draw_row(GTK_PREVIEW(p->zoom), row, 0, i, XSANE_ZOOM_SIZE);
      }
    }
    gtk_widget_queue_draw(p->zoom);

    free(row);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_display_color_components(Preview *p, int x, int y)
{
 char buffer[TEXTBUFSIZE];
 int raw_red, raw_green, raw_blue, enh_red, enh_green, enh_blue;

  if (! preview_get_pixel_color(p, x, y, &raw_red, &raw_green, &raw_blue, &enh_red, &enh_green, &enh_blue))
  {
    snprintf(buffer, sizeof(buffer), " %03d, %03d, %03d \n" \
                                     " %03d, %03d, %03d ",
    raw_red, raw_green, raw_blue, enh_red, enh_green, enh_blue);
  }
  else
  {
    snprintf(buffer, sizeof(buffer), " ###, ###, ### \n" \
                                     " ###, ###, ### ");
  }

  gtk_label_set_text(GTK_LABEL(p->rgb_label), buffer);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_hold_event_handler(gpointer data)
{
 Preview *p = data;

  DBG(DBG_proc, "preview_hold_event_handler\n");

  preview_draw_selection(p);
  p->gamma_functions_interruptable = TRUE;
  preview_establish_selection(p);
  p->gamma_functions_interruptable = FALSE;

  gtk_timeout_remove(p->hold_timer);
  p->hold_timer = 0;

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_motion_event_handler(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkCursor *cursor;
 float preview_selection[4];
 float preview_x, preview_y;
 float xscale, yscale;
 int cursornr;

  DBG(DBG_proc, "preview_motion_event_handler\n");

  /* preview selection (device) -> cursor-position (window) */
  preview_transform_coordinates_device_to_window(p, p->selection.coordinate, preview_selection);

  /* cursor-prosition (window) -> preview coordinate (device) */
  preview_transform_coordinate_window_to_device(p, event->button.x, event->button.y, &preview_x, &preview_y);

  preview_get_scale_device_to_window(p, &xscale, &yscale);

  if (!p->scanning)
  {
    preview_display_zoom(p, event->motion.x, event->motion.y, XSANE_ZOOM_FACTOR);
    preview_display_color_components(p, event->motion.x, event->motion.y);

    switch (((GdkEventMotion *)event)->state &
            GDK_Num_Lock & GDK_Caps_Lock & GDK_Shift_Lock & GDK_Scroll_Lock) /* mask all Locks */
    {
      case 256: /* left button */

        DBG(DBG_info2, "left button\n");

        if ( (p->selection_drag) || (p->selection_drag_edge) )
        {
          p->selection.active = TRUE;

          if (preview_x < p->scanner_surface[p->index_xmin])
          {
            preview_x = p->scanner_surface[p->index_xmin];
          }
          else if (preview_x > p->scanner_surface[p->index_xmax])
          {
            preview_x = p->scanner_surface[p->index_xmax];
          }

          if (preview_y < p->scanner_surface[p->index_ymin])
          {
            preview_y = p->scanner_surface[p->index_ymin];
          }
          else if (preview_y > p->scanner_surface[p->index_ymax])
          {
            preview_y = p->scanner_surface[p->index_ymax];
          }


          if (p->selection_xedge != -1)
          {
            p->selection.coordinate[p->selection_xedge] = preview_x;
          }

          if (p->selection_yedge != -1)
          {
            p->selection.coordinate[p->selection_yedge] = preview_y;
          }

          if (p->ratio) /* forced preview ratio ? */
          {
            if ( (p->selection_xedge == p->index_xmin) && (p->selection_yedge == p->index_ymin) ) /* top left corner */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + width / p->ratio;

              if (p->selection.coordinate[p->index_ymax] > p->scanner_surface[p->index_ymax])
              {
               float height;
                p->selection.coordinate[p->index_ymax] = p->scanner_surface[p->index_ymax];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmax] - height * p->ratio;
              }
            }
            else if ( (p->selection_xedge == p->index_xmax) && (p->selection_yedge == p->index_ymin) )/* top right corner */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + width / p->ratio;

              if (p->selection.coordinate[p->index_ymax] > p->scanner_surface[p->index_ymax])
              {
               float height;
                p->selection.coordinate[p->index_ymax] = p->scanner_surface[p->index_ymax];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + height * p->ratio;
              }
            }
            else if ( (p->selection_xedge == p->index_xmin) && (p->selection_yedge == p->index_ymax) ) /* bottom left edge */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymax] - width / p->ratio;

              if (p->selection.coordinate[p->index_ymin] <  p->scanner_surface[p->index_ymin])
              {
               float height;
                p->selection.coordinate[p->index_ymin] = p->scanner_surface[p->index_ymin];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmax] - height * p->ratio;
              }
            }
            else if ( (p->selection_xedge == p->index_xmax) && (p->selection_yedge == p->index_ymax) ) /* bottom right edge */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymax] - width / p->ratio;

              if (p->selection.coordinate[p->index_ymin] <  p->scanner_surface[p->index_ymin])
              {
               float height;
                p->selection.coordinate[p->index_ymin] = p->scanner_surface[p->index_ymin];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + height * p->ratio;
              }
            }
            else if (p->selection_xedge == p->index_xmin) /* left edge */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + width / p->ratio;

              if (p->selection.coordinate[p->index_ymax] > p->scanner_surface[p->index_ymax])
              {
               float height;
                p->selection.coordinate[p->index_ymax] = p->scanner_surface[p->index_ymax];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmax] - height * p->ratio;
              }
            }
            else if (p->selection_xedge == p->index_xmax) /* right edge */
            {
             float width;
              width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
            
              p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + width / p->ratio;

              if (p->selection.coordinate[p->index_ymax] > p->scanner_surface[p->index_ymax])
              {
               float height;
                p->selection.coordinate[p->index_ymax] = p->scanner_surface[p->index_ymax];
                height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
                p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + height * p->ratio;
              }
            }
            else if (p->selection_yedge == p->index_ymin) /* top edge */
            {
             float height;
              height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
            
              p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + height * p->ratio;

              if (p->selection.coordinate[p->index_xmax] > p->scanner_surface[p->index_xmax])
              {
               float width;
                p->selection.coordinate[p->index_xmax] = p->scanner_surface[p->index_xmax];
                width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
                p->selection.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymax] - width / p->ratio;
              }
            }
            else if (p->selection_yedge == p->index_ymax) /* bottom edge */
            {
             float height;
              height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);
            
              p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + height * p->ratio;

              if (p->selection.coordinate[p->index_xmax] > p->scanner_surface[p->index_xmax])
              {
               float width;
                p->selection.coordinate[p->index_xmax] = p->scanner_surface[p->index_xmax];
                width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
                p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + width / p->ratio;
              }
            }
          }

          preview_order_selection(p);
          preview_bound_selection(p);

          if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
          {
            if (!p->hold_timer) /* hold timer active? then remove it, we had a motion */
            {
              p->hold_timer = gtk_timeout_add(XSANE_CONTINUOUS_HOLD_TIME, preview_hold_event_handler, (gpointer *) p);
            }
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
          else if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
          {
            /* call preview_hold_event_hanlder if mouse is not moved for ??? ms */
            if (p->hold_timer) /* hold timer active? then remove it, we had a motion */
            {
              gtk_timeout_remove(p->hold_timer);
              p->hold_timer = 0;
            }
            p->hold_timer = gtk_timeout_add(XSANE_HOLD_TIME, preview_hold_event_handler, (gpointer *) p);
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
          else /* discontinuous */
          {
            preview_update_maximum_output_size(p);
            preview_draw_selection(p); /* only draw selection, do not update backend geometry options */
          }
        }

        cursornr = p->cursornr;

        if ( (p->selection_xedge != -1) && (p->selection_yedge != -1) ) /* move corner */
        {
          if ( ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                 (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) && /* left  */
               ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                 (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) ) /* top */
          {
            cursornr = GDK_TOP_LEFT_CORNER;
          }
          else if ( ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                      (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) && /* right */
                    ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                      (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) ) /* top */
          {
            cursornr = GDK_TOP_RIGHT_CORNER;
          }
          else if ( ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                      (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) && /* left  */
                    ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                      (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) ) /* bottom */
          {
            cursornr = GDK_BOTTOM_LEFT_CORNER;
          }
          else if ( ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                      (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) && /* right */
                    ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                      (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) ) /* bottom */
          {
            cursornr = GDK_BOTTOM_RIGHT_CORNER;
          }
        }
        else if ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                  (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) /* left */
        {
          if (cursornr == GDK_RIGHT_SIDE)
          {
            cursornr = GDK_LEFT_SIDE;
          }
        }
        else if ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                  (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) )  /* right */
        {
          if (cursornr == GDK_LEFT_SIDE)
          {
            cursornr = GDK_RIGHT_SIDE;
          }
        }
        else if ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                  (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) )  /* top */
        {
          if (cursornr == GDK_BOTTOM_SIDE)
          {
            cursornr = GDK_TOP_SIDE;
          }
        }
        else if ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                    (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) )  /* bottom */
        {
          if (cursornr == GDK_TOP_SIDE)
          {
            cursornr = GDK_BOTTOM_SIDE;
          }
        }

        if (cursornr != p->cursornr)
        {
          cursor = gdk_cursor_new(cursornr);	/* set curosr */
          gdk_window_set_cursor(p->window->window, cursor);
          gdk_cursor_unref(cursor);
          p->cursornr = cursornr;
        }
       break;

      case 512: /* middle button */
      case 1024: /* right button */
        DBG(DBG_info2, "middle or right button\n");

        if (p->selection_drag)
        {
         double dx, dy;

          switch (p->rotation)
          {
            case 0: /* 0 degree */
            default:
               dx = (p->selection_xpos - event->motion.x) / xscale;
               dy = (p->selection_ypos - event->motion.y) / yscale;
             break;

            case 1: /* 90 degree */
               dx = (event->motion.x - p->selection_xpos) / xscale;
               dy = (p->selection_ypos - event->motion.y) / yscale;
             break;

            case 2: /* 180 degree */
               dx = (event->motion.x - p->selection_xpos) / xscale;
               dy = (event->motion.y - p->selection_ypos) / yscale;
             break;

            case 3: /* 270 degree */
               dx = (p->selection_xpos - event->motion.x) / xscale;
               dy = (event->motion.y - p->selection_ypos) / yscale;
             break;

            case 4: /* 0 degree, x mirror */
               dx = (event->motion.x - p->selection_xpos) / xscale;
               dy = (p->selection_ypos - event->motion.y) / yscale;
             break;

            case 5: /* 90 degree, x mirror */
               dx = (p->selection_xpos - event->motion.x) / xscale;
               dy = (p->selection_ypos - event->motion.y) / yscale;
             break;

            case 6: /* 180 degree, x mirror */
               dx = (p->selection_xpos - event->motion.x) / xscale;
               dy = (event->motion.y - p->selection_ypos) / yscale;
             break;

            case 7: /* 270 degree, x mirror */
               dx = (event->motion.x - p->selection_xpos) / xscale;
               dy = (event->motion.y - p->selection_ypos) / yscale;
             break;
          }

          p->selection_xpos = event->motion.x;
          p->selection_ypos = event->motion.y;

          if (dx > p->selection.coordinate[p->index_xmin] - p->scanner_surface[p->index_xmin]) 
          {
            dx = p->selection.coordinate[p->index_xmin] - p->scanner_surface[p->index_xmin];
          }

          if (dy > p->selection.coordinate[p->index_ymin] - p->scanner_surface[p->index_ymin])
          {
            dy = p->selection.coordinate[p->index_ymin] - p->scanner_surface[p->index_ymin];
          }

          if (dx < p->selection.coordinate[p->index_xmax] - p->scanner_surface[p->index_xmax])
          { 
            dx = p->selection.coordinate[p->index_xmax] - p->scanner_surface[p->index_xmax];
          }

          if (dy < p->selection.coordinate[p->index_ymax] - p->scanner_surface[p->index_ymax])
          {
            dy = p->selection.coordinate[p->index_ymax] - p->scanner_surface[p->index_ymax];
          }

          p->selection.active = TRUE;
          p->selection.coordinate[0] -= dx;
          p->selection.coordinate[1] -= dy;
          p->selection.coordinate[2] -= dx;
          p->selection.coordinate[3] -= dy;

          if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
          {
            if (!p->hold_timer) /* hold timer active? then remove it, we had a motion */
            {
              p->hold_timer = gtk_timeout_add(XSANE_CONTINUOUS_HOLD_TIME, preview_hold_event_handler, (gpointer *) p);
            }
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
          else if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
          {
            if (p->hold_timer) /* hold timer active? then remove it, we had a motion */
            {
              gtk_timeout_remove(p->hold_timer);
              p->hold_timer = 0;
            }
            p->hold_timer = gtk_timeout_add (XSANE_HOLD_TIME, preview_hold_event_handler, (gpointer *) p);
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
          else /* discontinuous */
          {
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
        }
       break;

      default:
        if ( ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
               (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) && /* left  */
             ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
               (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) ) /* top */
        {
          cursornr = GDK_TOP_LEFT_CORNER;
        }
        else if ( ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                    (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) && /* right */
                  ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                    (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) ) /* top */
        {
          cursornr = GDK_TOP_RIGHT_CORNER;
        }
        else if ( ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                    (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) && /* left  */
                  ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                    (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) ) /* bottom */
        {
          cursornr = GDK_BOTTOM_LEFT_CORNER;
        }
        else if ( ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                    (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) && /* right */
                  ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                    (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) ) /* bottom */
        {
          cursornr = GDK_BOTTOM_RIGHT_CORNER;
        }
        else if ( ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                    (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) &&  /* left */
                  ( (event->button.y > preview_selection[1]) && (event->button.y < preview_selection[3]) ) ) /* in height */
        {
           cursornr = GDK_LEFT_SIDE;
        }
        else if ( ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                    (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) &&  /* right */
                  ( (event->button.y > preview_selection[1]) && (event->button.y < preview_selection[3]) ) ) /* in height */
        {
           cursornr = GDK_RIGHT_SIDE;
        }
        else if ( ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                    (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) &&  /* top */
                  ( (event->button.x > preview_selection[0]) && (event->button.x < preview_selection[2]) ) ) /* in width */
        {
           cursornr = GDK_TOP_SIDE;
        }
        else if ( ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                    (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) &&  /* bottom */
                  ( (event->button.x > preview_selection[0]) && (event->button.x < preview_selection[2]) ) ) /* in width */
        {
           cursornr = GDK_BOTTOM_SIDE;
        }
        else
        {
          cursornr = XSANE_CURSOR_PREVIEW;
        }

        if ((cursornr != p->cursornr) && (p->cursornr != -1))
        {
          cursor = gdk_cursor_new(cursornr);	/* set curosr */
          gdk_window_set_cursor(p->window->window, cursor);
          gdk_cursor_unref(cursor);
          p->cursornr = cursornr;
        }
       break;
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_button_press_event_handler(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkCursor *cursor;
 float preview_selection[4];
 float preview_x, preview_y;
 int cursornr;

  DBG(DBG_proc, "preview_button_press_event_handler\n");

  /* preview selection (device) -> cursor-position (window) */
  preview_transform_coordinates_device_to_window(p, p->selection.coordinate, preview_selection);

  /* cursor-prosition (window) -> preview coordinate (device) */
  preview_transform_coordinate_window_to_device(p, event->button.x, event->button.y, &preview_x, &preview_y);

  if (!p->scanning)
  {
    switch (p->mode)
    {
      case MODE_PIPETTE_WHITE:
      {
        DBG(DBG_info, "pipette white mode\n");
        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) && (p->image_data_raw) ) /* left or middle button */
        {
         int r=255, g=255, b=255; /* preset color to white */

          preview_get_color(p, event->button.x, event->button.y, preferences.preview_pipette_range, &r, &g, &b);

          xsane.slider_gray.value[2]  = sqrt( (r*r+g*g+b*b) / 3)/2.55;

          if ( (!xsane.enhancement_rgb_default) && (((GdkEventButton *)event)->button == 2) ) /* middle button */
          {
            xsane.slider_red.value[2]   = r/2.55;
            xsane.slider_green.value[2] = g/2.55;
            xsane.slider_blue.value[2]  = b/2.55;
          }
          else
          {
            xsane.slider_red.value[2]   = xsane.slider_gray.value[2];
            xsane.slider_green.value[2] = xsane.slider_gray.value[2];
            xsane.slider_blue.value[2]  = xsane.slider_gray.value[2];
          }

          if (xsane.slider_gray.value[2] < 2)
          {
            xsane.slider_gray.value[2] = 2;
          }
          if (xsane.slider_gray.value[1] >= xsane.slider_gray.value[2])
          {
            xsane.slider_gray.value[1] = xsane.slider_gray.value[2]-1;
            if (xsane.slider_gray.value[0] >= xsane.slider_gray.value[1])
            {
              xsane.slider_gray.value[0] = xsane.slider_gray.value[1]-1;
            }
          }

          if (xsane.slider_red.value[2] < 2)
          {
            xsane.slider_red.value[2] = 2;
          }
          if (xsane.slider_red.value[1] >= xsane.slider_red.value[2])
          {
            xsane.slider_red.value[1] = xsane.slider_red.value[2]-1;
            if (xsane.slider_red.value[0] >= xsane.slider_red.value[1])
            {
              xsane.slider_red.value[0] = xsane.slider_red.value[1]-1;
            }
          }

          if (xsane.slider_green.value[2] < 2)
          {
            xsane.slider_green.value[2] = 2;
          }
          if (xsane.slider_green.value[1] >= xsane.slider_green.value[2])
          {
            xsane.slider_green.value[1] = xsane.slider_green.value[2]-1;
            if (xsane.slider_green.value[0] >= xsane.slider_green.value[1])
            {
              xsane.slider_green.value[0] = xsane.slider_green.value[1]-1;
            }
          }

          if (xsane.slider_blue.value[2] < 2)
          {
            xsane.slider_blue.value[2] = 2;
          }
          if (xsane.slider_blue.value[1] >= xsane.slider_blue.value[2])
          {
            xsane.slider_blue.value[1] = xsane.slider_blue.value[2]-1;
            if (xsane.slider_blue.value[0] >= xsane.slider_blue.value[1])
            {
              xsane.slider_blue.value[0] = xsane.slider_blue.value[1]-1;
            }
          }

          xsane_enhancement_by_histogram(TRUE);
        }

        p->mode = MODE_NORMAL;

        cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);
        gdk_window_set_cursor(p->window->window, cursor);
        gdk_cursor_unref(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_PIPETTE_GRAY:
      {
        DBG(DBG_info, "pipette gray mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) && (p->image_data_raw) ) /* left or middle button */
        {
         int r=128, g=128, b=128; /* preset color to gray */

          preview_get_color(p, event->button.x, event->button.y, preferences.preview_pipette_range, &r, &g, &b);

          xsane.slider_gray.value[1] = sqrt( (r*r+g*g+b*b) / 3)/2.55;

          if ( (!xsane.enhancement_rgb_default) && (((GdkEventButton *)event)->button == 2) ) /* middle button */
          {
            xsane.slider_red.value[1]   = r/2.55;
            xsane.slider_green.value[1] = g/2.55;
            xsane.slider_blue.value[1]  = b/2.55;
          }
          else
          {
            xsane.slider_red.value[1]   = xsane.slider_gray.value[1];
            xsane.slider_green.value[1] = xsane.slider_gray.value[1];
            xsane.slider_blue.value[1]  = xsane.slider_gray.value[1];
          }

          if (xsane.slider_gray.value[1] == 0)
          {
            xsane.slider_gray.value[1] += 1;
          }
          if (xsane.slider_gray.value[1] == 100)
          {
            xsane.slider_gray.value[1] -= 1;
          }
          if (xsane.slider_gray.value[1] >= xsane.slider_gray.value[2])
          {
            xsane.slider_gray.value[2] = xsane.slider_gray.value[1]+1;
          }
          if (xsane.slider_gray.value[1] <= xsane.slider_gray.value[0])
          {
            xsane.slider_gray.value[0] = xsane.slider_gray.value[1]-1;
          }

          if (xsane.slider_red.value[1] == 0)
          {
            xsane.slider_red.value[1] += 1;
          }
          if (xsane.slider_red.value[1] == 100)
          {
            xsane.slider_red.value[1] -= 1;
          }
          if (xsane.slider_red.value[1] >= xsane.slider_red.value[2])
          {
            xsane.slider_red.value[2] = xsane.slider_red.value[1]+1;
          }
          if (xsane.slider_red.value[1] <= xsane.slider_red.value[0])
          {
            xsane.slider_red.value[0] = xsane.slider_red.value[1]-1;
          }

          if (xsane.slider_green.value[1] == 0)
          {
            xsane.slider_green.value[1] += 1;
          }
          if (xsane.slider_green.value[1] == 100)
          {
            xsane.slider_green.value[1] -= 1;
          }
          if (xsane.slider_green.value[1] >= xsane.slider_green.value[2])
          {
            xsane.slider_green.value[2] = xsane.slider_green.value[1]+1;
          }
          if (xsane.slider_green.value[1] <= xsane.slider_green.value[0])
          {
            xsane.slider_green.value[0] = xsane.slider_green.value[1]-1;
          }

          if (xsane.slider_blue.value[1] == 0)
          {
            xsane.slider_blue.value[1] += 1;
          }
          if (xsane.slider_blue.value[1] == 100)
          {
            xsane.slider_blue.value[1] -= 1;
          }
          if (xsane.slider_blue.value[1] >= xsane.slider_blue.value[2])
          {
            xsane.slider_blue.value[2] = xsane.slider_blue.value[1]+1;
          }
          if (xsane.slider_blue.value[1] <= xsane.slider_blue.value[0])
          {
            xsane.slider_blue.value[0] = xsane.slider_blue.value[1]-1;
          }

          xsane_enhancement_by_histogram(TRUE);
        }

        p->mode = MODE_NORMAL;

        cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);
        gdk_window_set_cursor(p->window->window, cursor);
        gdk_cursor_unref(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_PIPETTE_BLACK:
      {
        DBG(DBG_info, "pipette black mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) &&
             (p->image_data_raw) ) /* left or middle button */
        {
         int r=0, g=0, b=0; /* preset color to black */

          preview_get_color(p, event->button.x, event->button.y, preferences.preview_pipette_range, &r, &g, &b);

          xsane.slider_gray.value[0] = sqrt( (r*r+g*g+b*b) / 3)/2.55;

          if ( (!xsane.enhancement_rgb_default) && (((GdkEventButton *)event)->button == 2) ) /* middle button */
          {
            xsane.slider_red.value[0]   = r/2.55;
            xsane.slider_green.value[0] = g/2.55;
            xsane.slider_blue.value[0]  = b/2.55;
          }
          else
          {
            xsane.slider_red.value[0]   = xsane.slider_gray.value[0];
            xsane.slider_green.value[0] = xsane.slider_gray.value[0];
            xsane.slider_blue.value[0]  = xsane.slider_gray.value[0];
          }

          if (xsane.slider_gray.value[0] > 98)
          {
            xsane.slider_gray.value[0] = 98;
          }
          if (xsane.slider_gray.value[1] <= xsane.slider_gray.value[0])
          {
            xsane.slider_gray.value[1] = xsane.slider_gray.value[0]+1;
            if (xsane.slider_gray.value[2] <= xsane.slider_gray.value[1])
            {
              xsane.slider_gray.value[2] = xsane.slider_gray.value[1]+1;
            }
          }

          if (xsane.slider_red.value[0] > 98)
          {
            xsane.slider_red.value[0] = 98;
          }
          if (xsane.slider_red.value[1] <= xsane.slider_red.value[0])
          {
            xsane.slider_red.value[1] = xsane.slider_red.value[0]+1;
            if (xsane.slider_red.value[2] <= xsane.slider_red.value[1])
            {
              xsane.slider_red.value[2] = xsane.slider_red.value[1]+1;
            }
          }

          if (xsane.slider_green.value[0] > 98)
          {
            xsane.slider_green.value[0] = 98;
          }
          if (xsane.slider_green.value[1] <= xsane.slider_green.value[0])
          {
            xsane.slider_green.value[1] = xsane.slider_green.value[0]+1;
            if (xsane.slider_green.value[2] <= xsane.slider_green.value[1])
            {
              xsane.slider_green.value[2] = xsane.slider_green.value[1]+1;
            }
          }

          if (xsane.slider_blue.value[0] > 98)
          {
            xsane.slider_blue.value[0] = 98;
          }
          if (xsane.slider_blue.value[1] <= xsane.slider_blue.value[0])
          {
            xsane.slider_blue.value[1] = xsane.slider_blue.value[0]+1;
            if (xsane.slider_blue.value[2] <= xsane.slider_blue.value[1])
            {
              xsane.slider_blue.value[2] = xsane.slider_blue.value[1]+1;
            }
          }

          xsane_enhancement_by_histogram(TRUE);
        }

        p->mode = MODE_NORMAL;

        cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);
        gdk_window_set_cursor(p->window->window, cursor);
        gdk_cursor_unref(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_AUTORAISE_SCAN_AREA:
      {
        DBG(DBG_info, "autoraise scan area mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) &&
             (p->image_data_raw) ) /* left or middle button */
        {
          preview_autoraise_scan_area(p, event->button.x, event->button.y, p->selection.coordinate); /* raise selection area */
        }

        p->mode = MODE_NORMAL;

        cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);
        gdk_window_set_cursor(p->window->window, cursor);
        gdk_cursor_unref(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_ZOOM_IN:
      {
        DBG(DBG_info, "zoom in mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) &&
             (p->image_data_raw) ) /* left or middle button */
        {
          preview_select_zoom_point(p, event->button.x, event->button.y); /* select zoom point */
        }
        else
        {
          p->mode = MODE_NORMAL;

          cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);
          gdk_window_set_cursor(p->window->window, cursor);
          gdk_cursor_unref(cursor);
          p->cursornr = XSANE_CURSOR_PREVIEW;
        }
      }
      break;

      case MODE_NORMAL:
      {
        DBG(DBG_info, "normal mode\n");

        if (p->show_selection)
        {
          switch (((GdkEventButton *)event)->button)
          {
            case 1: /* left button: define selection area */
              DBG(DBG_info, "left button\n");

              p->selection_xedge = -1;
              if ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                   (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) /* left */
              {
                DBG(DBG_info, "-left\n");
                p->selection_xedge = 0;
              }
              else if ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                        (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) /* right */
              {
                DBG(DBG_info, "-right\n");
                p->selection_xedge = 2;
              }

              p->selection_yedge = -1;
              if ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                   (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) /* top */
              {
                DBG(DBG_info, "-top\n");
                p->selection_yedge = 1;
              }
              else if ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                        (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) /* bottom */
              {
                DBG(DBG_info, "-bottom\n");
                p->selection_yedge = 3;
              }

              if ( (p->selection_xedge != -1) && (p->selection_yedge != -1) ) /* move corner */
              {
                DBG(DBG_info, "-move corner (%f, %f)\n", preview_x, preview_y);
                p->selection_drag_edge = TRUE;
                p->selection.coordinate[p->selection_xedge] = preview_x;
                p->selection.coordinate[p->selection_yedge] = preview_y;
                preview_draw_selection(p);
              }
              else if ( (p->selection_xedge != -1) && (event->button.y > preview_selection[1])
                        && (event->button.y < preview_selection[3]) ) /* move x-edge */
              {
                DBG(DBG_info, "-move x-edge %f\n", preview_x);
                p->selection_drag_edge = TRUE;
                p->selection.coordinate[p->selection_xedge] = preview_x;
                preview_draw_selection(p);
              }
              else if ( (p->selection_yedge != -1)  && (event->button.x > preview_selection[0])
                        && (event->button.x < preview_selection[2]) ) /* move y-edge */
              {
                DBG(DBG_info, "-move y-edge %f\n", preview_y);
                p->selection_drag_edge = TRUE;
                p->selection.coordinate[p->selection_yedge] = preview_y;
                preview_draw_selection(p);
              } 
              else /* select new area */
              {
                DBG(DBG_info, "-define new area (%f, %f)\n", preview_x, preview_y);
                p->selection_xedge = 2;
                p->selection_yedge = 3;
                p->selection.coordinate[0] = preview_x;
                p->selection.coordinate[1] = preview_y;
                p->selection_drag = TRUE;

                cursornr = GDK_CROSS;
                cursor = gdk_cursor_new(cursornr);	/* set curosr */
                gdk_window_set_cursor(p->window->window, cursor);
                gdk_cursor_unref(cursor);
                p->cursornr = cursornr;
              }
             break;

            case 2: /* middle button */
            case 3: /* right button */
              DBG(DBG_info, "middle or right button\n");

              if ( (preview_selection[0]-SELECTION_RANGE_OUT < event->button.x) &&
                   (preview_selection[2]+SELECTION_RANGE_OUT > event->button.x) &&
                   (preview_selection[1]-SELECTION_RANGE_OUT < event->button.y) &&
                   (preview_selection[3]+SELECTION_RANGE_OUT > event->button.y) )
              {
                DBG(DBG_info, "move selection area\n");
                p->selection_drag = TRUE;
                p->selection_xpos = event->button.x;
                p->selection_ypos = event->button.y;

                cursornr = GDK_HAND2;
                cursor = gdk_cursor_new(cursornr);	/* set curosr */
                gdk_window_set_cursor(p->window->window, cursor);
                gdk_cursor_unref(cursor);
                p->cursornr = cursornr;
              }
             break;

            default:
             break;
          }
        }
      }
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_button_release_event_handler(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkCursor *cursor;
 float preview_selection[4];
 int cursornr;

  DBG(DBG_proc, "preview_button_release_event_handler\n");

  /* preview selection (device) -> cursor-position (window) */
  preview_transform_coordinates_device_to_window(p, p->selection.coordinate, preview_selection);

  if (!p->scanning)
  {
    if (p->show_selection)
    {
      switch (((GdkEventButton *)event)->button)
      {
        case 1: /* left button */
        case 2: /* middle button */
        case 3: /* right button */
          if (p->selection_drag)
          {
            DBG(DBG_info, "selection finished\n");
            cursornr = XSANE_CURSOR_PREVIEW;
            cursor = gdk_cursor_new(cursornr);	/* set curosr */
            gdk_window_set_cursor(p->window->window, cursor);
            gdk_cursor_unref(cursor);
            p->cursornr = cursornr;
          }

          preview_draw_selection(p);
          preview_establish_selection(p);

          p->selection_drag_edge = FALSE;
          p->selection_drag = FALSE;
        break;

        default:
         break;
      }
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
static int expose_event_selection_active, expose_event_selection_maximum_active;

static gint preview_expose_event_handler_start(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkColor color;
 GdkColormap *colormap; 

  DBG(DBG_proc, "preview_expose_event_handler_start\n");

  if (event->type == GDK_EXPOSE)
  {
    if (!p->gc_selection)
    {
      DBG(DBG_info, "defining line styles for selection and page frames\n");
      colormap = gdk_drawable_get_colormap(p->window->window);

      p->gc_selection = gdk_gc_new(p->window->window);
      gdk_gc_set_function(p->gc_selection, GDK_INVERT);
      gdk_gc_set_line_attributes(p->gc_selection, 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);

      p->gc_selection_maximum = gdk_gc_new(p->window->window);
      gdk_gc_set_function(p->gc_selection_maximum, GDK_XOR);
      gdk_gc_set_line_attributes(p->gc_selection_maximum, 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);

      color.red   = 0;
      color.green = 65535;
      color.blue  = 30000;
      gdk_color_alloc(colormap, &color);
      gdk_gc_set_foreground(p->gc_selection_maximum, &color);   
    }
    else
    {
      expose_event_selection_active         = p->selection.active;
      expose_event_selection_maximum_active = p->selection_maximum.active;
      p->selection_maximum.active = FALSE;
      p->selection.active = FALSE; /* do not draw new selections */
      p->selection_maximum.active = FALSE;
      preview_draw_selection(p); /* undraw selections */
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_expose_event_handler_end(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkColor color;
 GdkColormap *colormap; 

  DBG(DBG_proc, "preview_expose_event_handler_end\n");

  if (event->type == GDK_EXPOSE)
  {
    if (!p->gc_selection)
    {
      DBG(DBG_info, "defining line styles for selection and page frames\n");
      colormap = gdk_drawable_get_colormap(p->window->window);

      p->gc_selection = gdk_gc_new(p->window->window);
      gdk_gc_set_function(p->gc_selection, GDK_INVERT);
      gdk_gc_set_line_attributes(p->gc_selection, 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);

      p->gc_selection_maximum = gdk_gc_new(p->window->window);
      gdk_gc_set_function(p->gc_selection_maximum, GDK_XOR);
      gdk_gc_set_line_attributes(p->gc_selection_maximum, 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);

      color.red   = 0;
      color.green = 65535;
      color.blue  = 30000;
      gdk_color_alloc(colormap, &color);
      gdk_gc_set_foreground(p->gc_selection_maximum, &color);   
    }
    else
    {
      p->selection.active         = expose_event_selection_active;
      p->selection_maximum.active = expose_event_selection_maximum_active;
      preview_draw_selection(p); /* draw selections again */
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_start_button_clicked(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "preview_start_button_clicked\n");

  preview_scan(data);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_cancel_button_clicked(GtkWidget *widget, gpointer data)
{
 Preview *p = (Preview *) data;

  DBG(DBG_proc, "preview_cancel_button_clicked\n");

  sane_cancel(xsane.dev);
  gtk_widget_set_sensitive(p->cancel, FALSE); /* disable cancel button */

  /* we have to make sure that xsane does detect that the scan has been cancled */
  /* but the select_fd does not make sure that preview_read_image_data is called */
  /* when the select_fd is closed by the backend, so we have to make sure that */
  /* preview_read_image_data is called */
  preview_read_image_data(p, -1, GDK_INPUT_READ);

  p->scan_incomplete = TRUE;
  preview_display_valid(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_create_preset_area_menu(Preview *p, int selection)
{
 int i;
 GtkWidget *preset_area_menu, *preset_area_item;

  preset_area_menu = gtk_menu_new();

  for (i = 0; i < preferences.preset_area_definitions; ++i)
  {
    preset_area_item = gtk_menu_item_new_with_label(preferences.preset_area[i]->name);
    gtk_container_add(GTK_CONTAINER(preset_area_menu), preset_area_item);
    g_signal_connect(GTK_OBJECT(preset_area_item), "button_press_event", (GtkSignalFunc) preview_preset_area_context_menu_callback, p);
    g_signal_connect(GTK_OBJECT(preset_area_item), "activate", (GtkSignalFunc) preview_preset_area_callback, p);
    gtk_object_set_data(GTK_OBJECT(preset_area_item), "Selection", (void *) i);  
    gtk_object_set_data(GTK_OBJECT(preset_area_item), "Preview", (void *) p);  

    gtk_widget_show(preset_area_item);
  }                  

  gtk_option_menu_set_menu(GTK_OPTION_MENU(p->preset_area_option_menu), preset_area_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(p->preset_area_option_menu), selection); 

  gtk_widget_show(preset_area_menu);
  gtk_widget_queue_draw(p->preset_area_option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_generate_preview_filenames(Preview *p)
{
 char filename[PATH_MAX];
 int i;

  DBG(DBG_proc, "preview_generate_preview_filenames\n");

  for(i=0; i<=2; i++) /* create random filenames for previews */
  {
    if (preview_make_image_path(p, sizeof(filename), filename, i)>=0)
    {
     FILE *testfile;

      testfile = fopen(filename, "wb");
      if (testfile)
      {
        fclose(testfile);
        p->filename[i] = strdup(filename);/* store filename */
        DBG(DBG_info, "preview file %s created\n", filename);
      }
      else
      {
        DBG(DBG_error, "ERROR: could not create preview file %s\n", filename);
        p->filename[0] = NULL; /* mark filename does not exist */
        p->filename[1] = NULL; /* mark filename does not exist */
        p->filename[2] = NULL; /* mark filename does not exist */
       break; /* do not try next preview level, one error is enough */
      }
    }
    else
    {
      DBG(DBG_error, "ERROR: could not create filename for preview level %d\n", i);
      p->filename[0] = NULL; /* mark filename does not exist */
      p->filename[1] = NULL; /* mark filename does not exist */
      p->filename[2] = NULL; /* mark filename does not exist */
     break; /* do not try next preview level, one error is enough */
    }
  }

 return;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

Preview *preview_new(void)
{
 GtkWidget *table, *frame;
 GtkSignalFunc signal_func;
 GtkWidgetClass *class;
 GtkWidget *vbox, *action_box;
 GtkWidget *outer_hbox, *middle_vbox;
 GdkCursor *cursor;
 GtkWidget *preset_area_option_menu;
 GtkWidget *rotation_option_menu, *rotation_menu, *rotation_item;
 GtkWidget *ratio_option_menu, *ratio_menu, *ratio_item;
 GtkWidget *delete_images;
 GdkBitmap *mask;
 GdkPixmap *pixmap = NULL;
 GtkWidget *pixmapwidget;
 Preview *p;
 int i;
 char buf[TEXTBUFSIZE];
 int ratio_nr = 0;

  DBG(DBG_proc, "preview_new\n");

  p = malloc(sizeof(*p));
  if (!p)
  {
    return 0;
  }
  memset(p, 0, sizeof(*p));

  p->mode        = MODE_NORMAL; /* no pipette functions etc */
  p->calibration = 0; /* do not display calibration image */
  p->input_tag   = -1;
  p->rotation    = 0;
  p->gamma_functions_interruptable = FALSE;

  p->index_xmin        = 0;
  p->index_xmax        = 2;
  p->index_ymin        = 1;
  p->index_ymax        = 3;

  p->max_scanner_surface[0] = -INF;
  p->max_scanner_surface[1] = -INF;
  p->max_scanner_surface[2] = INF;
  p->max_scanner_surface[3] = INF;

  p->scanner_surface[0] = -INF;
  p->scanner_surface[1] = -INF;
  p->scanner_surface[2] = INF;
  p->scanner_surface[3] = INF;

  p->surface[0] = -INF;
  p->surface[1] = -INF;
  p->surface[2] = INF;
  p->surface[3] = INF;

  gtk_preview_set_gamma(1.0);
  gtk_preview_set_install_cmap(preferences.preview_own_cmap);

  preview_generate_preview_filenames(p);

  p->preset_surface[0] = 0;
  p->preset_surface[1] = 0;
  p->preset_surface[2] = INF;
  p->preset_surface[3] = INF;

  p->maximum_output_width  = INF; /* full output with */
  p->maximum_output_height = INF; /* full output height */
  p->block_update_maximum_output_size_clipping = FALSE;

  p->preview_channels = -1;
  p->invalid = TRUE; /* no valid preview */
  p->ratio = 0.0;

#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_push_visual(gtk_preview_get_visual()); /* this has no function for gtk+-2.0 */
#endif
  gtk_widget_push_colormap(gtk_preview_get_cmap());

  snprintf(buf, sizeof(buf), "%s %s", WINDOW_PREVIEW, xsane.device_text);
  p->top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(p->top), buf);
  xsane_set_window_icon(p->top, 0);
  gtk_window_add_accel_group(GTK_WINDOW(p->top), xsane.accelerator_group);


  /* set the main vbox */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
  gtk_container_add(GTK_CONTAINER(p->top), vbox);
  gtk_widget_show(vbox);     



  /* the button_box (hbox) */
  p->button_box = gtk_hbox_new(FALSE, 1);
  gtk_container_set_border_width(GTK_CONTAINER(p->button_box), 0);
  gtk_box_pack_start(GTK_BOX(vbox), p->button_box, FALSE, FALSE, 0);


  /* add new selection for batch scanning */
  p->add_batch  = xsane_button_new_with_pixmap(p->top->window, p->button_box, add_batch_xpm, DESC_ADD_BATCH, (GtkSignalFunc) preview_add_batch, p);

  xsane_vseparator_new(p->button_box, 3);

  /* White, gray and black pipette button */
  p->pipette_white = xsane_button_new_with_pixmap(p->top->window, p->button_box, pipette_white_xpm, DESC_PIPETTE_WHITE, (GtkSignalFunc) preview_pipette_white, p);
  p->pipette_gray  = xsane_button_new_with_pixmap(p->top->window, p->button_box, pipette_gray_xpm,  DESC_PIPETTE_GRAY,  (GtkSignalFunc) preview_pipette_gray,  p);
  p->pipette_black = xsane_button_new_with_pixmap(p->top->window, p->button_box, pipette_black_xpm, DESC_PIPETTE_BLACK, (GtkSignalFunc) preview_pipette_black, p);

  xsane_vseparator_new(p->button_box, 3);

  /* Zoom not, zoom out and zoom in button */
  p->zoom_not    = xsane_button_new_with_pixmap(p->top->window, p->button_box, zoom_not_xpm,  DESC_ZOOM_FULL, (GtkSignalFunc) preview_zoom_not,  p);
  p->zoom_out    = xsane_button_new_with_pixmap(p->top->window, p->button_box, zoom_out_xpm,  DESC_ZOOM_OUT,  (GtkSignalFunc) preview_zoom_out,  p);
  p->zoom_in     = xsane_button_new_with_pixmap(p->top->window, p->button_box, zoom_in_xpm,   DESC_ZOOM_IN,   (GtkSignalFunc) preview_zoom_in,   p);
  p->zoom_area   = xsane_button_new_with_pixmap(p->top->window, p->button_box, zoom_area_xpm, DESC_ZOOM_AREA, (GtkSignalFunc) preview_zoom_area, p);
  p->zoom_undo   = xsane_button_new_with_pixmap(p->top->window, p->button_box, zoom_undo_xpm, DESC_ZOOM_UNDO, (GtkSignalFunc) preview_zoom_undo, p);

  xsane_vseparator_new(p->button_box, 3);

  p->full_area   = xsane_button_new_with_pixmap(p->top->window, p->button_box, auto_select_preview_area_xpm, DESC_AUTOSELECT_SCAN_AREA, (GtkSignalFunc) preview_autoselect_scan_area_callback, p);
  p->autoraise   = xsane_button_new_with_pixmap(p->top->window, p->button_box, auto_raise_preview_area_xpm,  DESC_AUTORAISE_SCAN_AREA,  (GtkSignalFunc) preview_init_autoraise_scan_area,      p);
  p->autoselect  = xsane_button_new_with_pixmap(p->top->window, p->button_box, full_preview_area_xpm,        DESC_FULL_PREVIEW_AREA,   (GtkSignalFunc) preview_full_preview_area_callback,   p);

  xsane_vseparator_new(p->button_box, 3);

  delete_images  = xsane_button_new_with_pixmap(p->top->window, p->button_box, delete_images_xpm,            DESC_DELETE_IMAGES,       (GtkSignalFunc) preview_delete_images_callback,       p);

  gtk_widget_add_accelerator(p->zoom_not,   "clicked", xsane.accelerator_group, GDK_KP_Multiply, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_* */
  gtk_widget_add_accelerator(p->zoom_out,   "clicked", xsane.accelerator_group, GDK_KP_Subtract, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_- */
  gtk_widget_add_accelerator(p->zoom_in,    "clicked", xsane.accelerator_group, GDK_KP_Add,      GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_+ */
  gtk_widget_add_accelerator(p->zoom_area,  "clicked", xsane.accelerator_group, GDK_KP_Enter,    GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_Enter */
  gtk_widget_add_accelerator(p->zoom_undo,  "clicked", xsane.accelerator_group, GDK_KP_Divide,   GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_/ */
  gtk_widget_add_accelerator(p->full_area,  "clicked", xsane.accelerator_group, GDK_A, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_* */
  gtk_widget_add_accelerator(p->autoselect, "clicked", xsane.accelerator_group, GDK_V, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_* */
  gtk_widget_add_accelerator(delete_images, "clicked", xsane.accelerator_group, GDK_KP_Delete, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt keypad_* */

  gtk_widget_set_sensitive(p->zoom_not,   FALSE); /* no zoom at this point, so no zoom not */
  gtk_widget_set_sensitive(p->zoom_out,   FALSE); /* no zoom at this point, so no zoom out */
  gtk_widget_set_sensitive(p->zoom_undo,  FALSE); /* no zoom at this point, so no zoom undo */
  gtk_widget_set_sensitive(p->full_area,  FALSE); /* no selection */
  gtk_widget_set_sensitive(p->autoselect, FALSE); /* no selection */


  gtk_widget_show(p->button_box);
  /* the button box is ready */



  /* construct the preview area (table with sliders & preview window) */

  table = gtk_table_new(2, 2, /* homogeneous */ FALSE);
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 1);
  gtk_container_set_border_width(GTK_CONTAINER(table), 1);
  gtk_box_pack_start(GTK_BOX(vbox), table, /* expand */ TRUE, /* fill */ TRUE, /* padding */ 0);
  gtk_widget_show(table);

  /* the empty box in the top-left corner */
  frame = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_table_attach(GTK_TABLE(table), frame, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(frame);

  /* the unit label */
  p->unit_label = gtk_label_new("cm");
  gtk_container_add(GTK_CONTAINER(frame), p->unit_label);
  gtk_widget_show(p->unit_label);

  /* the horizontal ruler */
  p->hruler = gtk_hruler_new();
  gtk_table_attach(GTK_TABLE(table), p->hruler, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show(p->hruler);

  /* the vertical ruler */
  p->vruler = gtk_vruler_new();
  gtk_table_attach(GTK_TABLE(table), p->vruler, 0, 1, 1, 2, 0, GTK_FILL, 0, 0);
  gtk_widget_show(p->vruler);

  /* the preview area */
  p->window = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_set_expand(GTK_PREVIEW(p->window), TRUE);

  gtk_widget_set_events(p->window, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  /* the first expose_event is responsible to undraw the selection frame */
  g_signal_connect(GTK_OBJECT(p->window), "expose_event",         (GtkSignalFunc) preview_expose_event_handler_start, p);
  g_signal_connect(GTK_OBJECT(p->window), "button_press_event",   (GtkSignalFunc) preview_button_press_event_handler, p);
  g_signal_connect(GTK_OBJECT(p->window), "motion_notify_event",  (GtkSignalFunc) preview_motion_event_handler, p);
  g_signal_connect(GTK_OBJECT(p->window), "button_release_event", (GtkSignalFunc) preview_button_release_event_handler, p);

  g_signal_connect_after(GTK_OBJECT(p->window), "size_allocate", (GtkSignalFunc) preview_area_resize_handler, p);
  /* the second expose_event is responsible to redraw the selection frame */
  g_signal_connect_after(GTK_OBJECT(p->window), "expose_event",  (GtkSignalFunc) preview_expose_event_handler_end, p);

  /* Connect the motion-notify events of the preview area with the rulers.  Nifty stuff!  */

#ifdef HAVE_GTK2
  class = (GtkWidgetClass *) GTK_HSCROLLBAR_GET_CLASS(p->hruler);
#else
  class = GTK_WIDGET_CLASS(GTK_OBJECT(p->hruler)->klass);
#endif

  signal_func = (GtkSignalFunc) class->motion_notify_event;
  g_signal_connect_swapped(GTK_OBJECT(p->window), "motion_notify_event", signal_func, GTK_OBJECT(p->hruler));

#ifdef HAVE_GTK2
  class = (GtkWidgetClass *) GTK_VSCROLLBAR_GET_CLASS(p->vruler);
#else
  class = GTK_WIDGET_CLASS(GTK_OBJECT(p->vruler)->klass);
#endif

  signal_func = (GtkSignalFunc) class->motion_notify_event;
  g_signal_connect_swapped(GTK_OBJECT(p->window), "motion_notify_event", signal_func, GTK_OBJECT(p->vruler));


  p->viewport = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(p->viewport), GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(p->viewport), p->window);
  gtk_widget_show(p->viewport);

  gtk_table_attach(GTK_TABLE(table), p->viewport, 1, 2, 1, 2,
		   GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
  /* the preview area is ready */



  /* the outer hbox at the bottom */
  outer_hbox = gtk_hbox_new(FALSE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(outer_hbox), 1);
  gtk_box_pack_start(GTK_BOX(vbox), outer_hbox, FALSE, FALSE, 0);
  gtk_widget_show(outer_hbox);

  /* the middle vbox at the bottom */
  middle_vbox = gtk_vbox_new(FALSE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(middle_vbox), 1);
  gtk_box_pack_start(GTK_BOX(outer_hbox), middle_vbox, FALSE, FALSE, 0);
  gtk_widget_show(middle_vbox);

  /* the menu_box (hbox) */
  p->menu_box = gtk_hbox_new(FALSE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(p->menu_box), 1);
  gtk_box_pack_start(GTK_BOX(middle_vbox), p->menu_box, FALSE, FALSE, 0);

  xsane_separator_new(middle_vbox, 1);


  /* select maximum scan area */
  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) size_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(p->menu_box), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  preset_area_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, preset_area_option_menu, DESC_PRESET_AREA);
  gtk_box_pack_start(GTK_BOX(p->menu_box), preset_area_option_menu, FALSE, FALSE, 0);
  gtk_widget_show(preset_area_option_menu);
  p->preset_area_option_menu = preset_area_option_menu;
  preview_create_preset_area_menu(p, 0); /* build menu and set default to 0=full size */

  xsane_vseparator_new(p->menu_box, 3);

  /* select rotation */
  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) rotation_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(p->menu_box), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  rotation_menu = gtk_menu_new();

  for (i = 0; i < 12; ++i)
  {
   int rot;

    if (i<4)
    {
      snprintf(buf, sizeof(buf), "%03d  ", i*90);  
      rot = i;
    }
    else if (i<8)
    {
      snprintf(buf, sizeof(buf), "%03d |", i*90-360);  
      rot = i;
    }
    else
    {
      snprintf(buf, sizeof(buf), "%03d -", i*90-2*360);  
      rot = (((i & 3) + 2) & 3) + 4;
    }
    rotation_item = gtk_menu_item_new_with_label(buf);
    gtk_container_add(GTK_CONTAINER(rotation_menu), rotation_item);
    g_signal_connect(GTK_OBJECT(rotation_item), "activate", (GtkSignalFunc) preview_rotation_callback, p);
    gtk_object_set_data(GTK_OBJECT(rotation_item), "Selection", (void *) rot);  

    gtk_widget_show(rotation_item);
  }                  

  rotation_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, rotation_option_menu, DESC_ROTATION);
  gtk_box_pack_start(GTK_BOX(p->menu_box), rotation_option_menu, FALSE, FALSE, 0);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(rotation_option_menu), rotation_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(rotation_option_menu), p->rotation); /* set rotation */

  gtk_widget_show(rotation_option_menu);
  p->rotation_option_menu = rotation_option_menu;

  xsane_vseparator_new(p->menu_box, 3);

  /* the preview aspect ratio menu */
  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) aspect_ratio_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(p->menu_box), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  ratio_menu = gtk_menu_new();

  for (i = 0; i < sizeof(ratio_value)/sizeof(float); ++i)
  {
    ratio_item = gtk_menu_item_new_with_label(ratio_string[i]);
    gtk_container_add(GTK_CONTAINER(ratio_menu), ratio_item);
    g_signal_connect(GTK_OBJECT(ratio_item), "activate", (GtkSignalFunc) preview_ratio_callback, p);
    gtk_object_set_data(GTK_OBJECT(ratio_item), "Selection", &ratio_value[i]);  

    gtk_widget_show(ratio_item);

    if (ratio_value[i] == p->ratio)
    {
      ratio_nr = i;
    }
  }                  

  ratio_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, ratio_option_menu, DESC_RATIO);
  gtk_box_pack_start(GTK_BOX(p->menu_box), ratio_option_menu, FALSE, FALSE, 0);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(ratio_option_menu), ratio_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(ratio_option_menu), ratio_nr); /* set ratio */

  gtk_widget_show(ratio_option_menu);
  p->ratio_option_menu = ratio_option_menu;

  /* the pointer zoom */
  frame = gtk_frame_new(0);
  gtk_box_pack_start(GTK_BOX(outer_hbox), frame, FALSE, FALSE, 3);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
  gtk_widget_show(frame);
  p->zoom = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(p->zoom), XSANE_ZOOM_SIZE, XSANE_ZOOM_SIZE);
  gtk_container_add(GTK_CONTAINER(frame), p->zoom);
  gtk_widget_show(p->zoom);

  gtk_widget_show(p->menu_box);
  /* the menu box is ready */



  /* set the action_hbox */
  action_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(middle_vbox), action_box, FALSE, FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(action_box), 0);
  gtk_widget_show(action_box);


  /* the (in)valid pixmaps */
  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) valid_xpm);
  p->valid_pixmap = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(action_box), p->valid_pixmap, FALSE, FALSE, 0);
  gtk_widget_show(p->valid_pixmap);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) scanning_xpm);
  p->scanning_pixmap = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(action_box), p->scanning_pixmap, FALSE, FALSE, 0);
  gtk_widget_show(p->scanning_pixmap);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) incomplete_xpm);
  p->incomplete_pixmap = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(action_box), p->incomplete_pixmap, FALSE, FALSE, 0);
  gtk_widget_show(p->incomplete_pixmap);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  pixmap = gdk_pixmap_create_from_xpm_d(p->top->window, &mask, xsane.bg_trans, (gchar **) invalid_xpm);
  p->invalid_pixmap = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(action_box), p->invalid_pixmap, FALSE, FALSE, 0);
  gtk_widget_show(p->invalid_pixmap);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  /* Start button */
  p->start = gtk_button_new_with_label(BUTTON_PREVIEW_ACQUIRE);
  xsane_back_gtk_set_tooltip(xsane.tooltips, p->start, DESC_PREVIEW_ACQUIRE);
  g_signal_connect(GTK_OBJECT(p->start), "clicked", (GtkSignalFunc) preview_start_button_clicked, p);
  gtk_box_pack_start(GTK_BOX(action_box), p->start, TRUE, TRUE, 5);
  gtk_widget_add_accelerator(p->start, "clicked", xsane.accelerator_group, GDK_P, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt P */
  gtk_widget_show(p->start);

  /* Cancel button */
  p->cancel = gtk_button_new_with_label(BUTTON_PREVIEW_CANCEL);
  xsane_back_gtk_set_tooltip(xsane.tooltips, p->cancel, DESC_PREVIEW_CANCEL);
  g_signal_connect(GTK_OBJECT(p->cancel), "clicked", (GtkSignalFunc) preview_cancel_button_clicked, p);
  gtk_box_pack_start(GTK_BOX(action_box), p->cancel, TRUE, TRUE, 5);
  gtk_widget_add_accelerator(p->cancel, "clicked", xsane.accelerator_group, GDK_Escape, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED); /* Alt ESC */
  gtk_widget_show(p->cancel);
  gtk_widget_set_sensitive(p->cancel, FALSE);

  /* the RGB label */
  frame = gtk_frame_new(0);
  gtk_box_pack_start(GTK_BOX(action_box), frame, FALSE, FALSE, 3);
  gtk_widget_show(frame);
  p->rgb_label = gtk_label_new(0);
  gtk_container_add(GTK_CONTAINER(frame), p->rgb_label);
  gtk_widget_show(p->rgb_label);
  preview_display_color_components(p, -1, -1); /* display "###, ###, ###" */

  preview_update_surface(p, 0);

  gtk_widget_show(p->window);
  gtk_widget_show(p->top);

  cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);	/* set default cursor */
  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = XSANE_CURSOR_PREVIEW;

  gtk_widget_pop_colormap();
#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_pop_visual();
#endif

  preview_update_surface(p, 0);

  preview_display_valid(p);

 return p;
}


/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_area_correct(Preview *p)
{
 float width, height, max_width, max_height;
 float aspect;

  DBG(DBG_proc, "preview_area_correct\n");

  if ( ((p->rotation & 3) == 0) || ((p->rotation & 3) == 2) || (p->calibration) )
  {
    aspect = p->aspect;
  }
  else
  {
    aspect = 1.0 / p->aspect;
  }

  max_width  = p->preview_window_width;
  max_height = p->preview_window_height;

  width  = max_width;
  height = width / aspect;

  if (height > max_height)
  {
    height = max_height;
    width  = height * aspect;
  }

  p->preview_width  = width + 0.5;
  p->preview_height = height + 0.5;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_update_surface(Preview *p, int surface_changed)
{
 float val;
 float width, height;
 float rotated_preset_surface[4];
 const SANE_Option_Descriptor *opt;
 int i;
 SANE_Value_Type type;
 SANE_Unit unit;
 double min, max;
 int expand_surface = 0;
 gint min_width, min_height;
 GdkScreen *screen;
 GdkRectangle geometry;

  DBG(DBG_proc, "preview_update_surface\n");

  unit = SANE_UNIT_PIXEL;
  type = SANE_TYPE_INT;

  preview_update_selection(p); /* make sure preview selection is up to date */

  p->show_selection = FALSE; /* at first let's say we have no corrdinate selection */

  for (i = 0; i < 4; ++i) /* test if surface (max vals of scan area) has changed */
  {
/*    val = (i & 2) ? INF : -INF; */
    val = (i & 2) ? INF : 0;

    if (xsane.well_known.coord[i] > 0)
    {
      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.coord[i]);
      assert(opt->unit == SANE_UNIT_PIXEL || opt->unit == SANE_UNIT_MM);
      unit = opt->unit;
      type = opt->type;
      p->show_selection = TRUE; /* ok, we have a coordinate selection */

      xsane_get_bounds(opt, &min, &max);

      if (i & 2)
      {
        val = max;
      }
      else
      {
        val = min;
      }
    }

    if (p->orig_scanner_surface[i] != val)
    {
      DBG(DBG_info, "preview_update_surface: orig_scanner_surface[%d] has changed\n", i);
      surface_changed = 2;
      p->orig_scanner_surface[i] = val;
    }
  }

  if (surface_changed == 2) /* redefine all surface subparts */
  {
    DBG(DBG_info, "preview_update_surface: rotating surfaces\n");

    /* max_scanner_surface are the rotated coordinates of orig_scanner_surface */
    preview_rotate_devicesurface_to_previewsurface(p->rotation, p->orig_scanner_surface, p->max_scanner_surface);

    gtk_widget_set_sensitive(p->zoom_not,  TRUE); /* allow unzoom */
    gtk_widget_set_sensitive(p->zoom_undo, FALSE); /* forbid undo zoom */

    expand_surface = 1;
    for (i = 0; i < 4; i++)
    {
      if (p->surface[i] != p->scanner_surface[i])
      {
        expand_surface = 0;
      }
    }
  } 
  else
  {
    expand_surface = 0;
  }


  /* scanner_surface are the rotated coordinates of the reduced (preset) surface */
  preview_rotate_devicesurface_to_previewsurface(p->rotation, p->preset_surface, rotated_preset_surface);
  for (i = 0; i < 4; i++)
  {
    val = rotated_preset_surface[i];

    xsane_bound_float(&val, p->max_scanner_surface[i % 2], p->max_scanner_surface[(i % 2) + 2]);
    if (val != p->scanner_surface[i]) 
    {
      surface_changed = 1; 
      p->scanner_surface[i] = val;

      if (expand_surface)
      {
        p->surface[i] = val;
      }
    }
    DBG(DBG_info, "preview_update_surface: scanner_surface[%d] = %3.2f\n", i, val);
  }

  for (i = 0; i < 4; i++)
  {
    val = p->surface[i];

    xsane_bound_float(&val, p->scanner_surface[i % 2], p->scanner_surface[(i % 2) + 2]);
    if (val != p->surface[i]) 
    {
      surface_changed = 1; 
      p->surface[i] = val;
    }
    DBG(DBG_info, "preview_update_surface: surface[%d] = %3.2f\n", i, val);
  }

/* may be we need to define  p->old_surface[i]  here too */

  if (p->surface_unit != unit)
  {
    surface_changed = 1;
    p->surface_unit = unit;
  }

  if (p->show_selection)
  {
    gtk_widget_set_sensitive(p->preset_area_option_menu, TRUE); /* enable preset area */
    gtk_widget_set_sensitive(p->zoom_in,    TRUE); /* zoom in is allowed at all */
    gtk_widget_set_sensitive(p->zoom_area,  TRUE); /* zoom area is allowed at all */
    gtk_widget_set_sensitive(p->full_area,  TRUE); /* enable selection buttons */
    gtk_widget_set_sensitive(p->autoselect, TRUE); 
  }
  else
  {
    gtk_widget_set_sensitive(p->preset_area_option_menu, FALSE); /* disable preset area */
    gtk_widget_set_sensitive(p->zoom_in,    FALSE); /* no zoom at all */
    gtk_widget_set_sensitive(p->zoom_area,  FALSE);
    gtk_widget_set_sensitive(p->zoom_out,   FALSE);
    gtk_widget_set_sensitive(p->zoom_undo,  FALSE);
    gtk_widget_set_sensitive(p->zoom_not,   FALSE); 
    gtk_widget_set_sensitive(p->full_area,  FALSE); /* no selection */
    gtk_widget_set_sensitive(p->autoselect, FALSE); /* no selection */
  }

  if (p->surface_type != type)
  {
    surface_changed = 1;
    p->surface_type = type;
  }


  if (surface_changed)
  {
    DBG(DBG_info, "preview_update_surface: surface_changed\n");
    /* guess the initial preview window size: */

    preview_restore_image(p); /* load scanned image */

    width  = p->surface[p->index_xmax] - p->surface[p->index_xmin];
    height = p->surface[p->index_ymax] - p->surface[p->index_ymin];

#if 0
    if ( (p->calibration) || (p->startimage) ) /* predefined image should have constant aspect */
#else
    if (p->calibration) /* predefined calibration image should have constant aspect */
#endif
    {
      p->aspect = fabs(p->image_width/(float) p->image_height);
    }
    else if (width >= INF || height >= INF) /* undefined size */
    {
      p->aspect = 1.0;
    }
    else /* we have a surface size that can be used to calculate the aspect ratio */
    {
      if (((p->rotation & 3) == 0) || ((p->rotation & 3) == 2)) /* 0 or 180 degree */
      {
        p->aspect = width/height;
      }
      else /* 90 or 270 degree */
      {
        p->aspect = height/width;
      }
    }
  }
#if 0
  else if ( (p->image_height) && (p->image_width) ) /* we have an image so let´s calculate the correct aspect ratio */
  {
    p->aspect = fabs(p->image_width/(float) p->image_height);
  }
#endif

  DBG(DBG_info, "preview_update_surface: aspect = %f\n", p->aspect);

  if ( (surface_changed) && (p->preview_window_width == 0) ) /* window is new */
  {
    DBG(DBG_info, "preview_update_surface: defining size of preview window\n");

/*
    p->preview_window_width  = 0.3 * gdk_screen_width();
    p->preview_window_height = 0.5 * gdk_screen_height();
*/
    /* ensure preview window fits on displays, account for multi-head */
    min_width = gdk_screen_width();
    min_height = gdk_screen_height();

    screen = gdk_screen_get_default();
    for (i = 0; i < gdk_screen_get_n_monitors(screen); i++)
    {
      gdk_screen_get_monitor_geometry(screen, i, &geometry);
      min_width = MIN(min_width, geometry.width);
      min_height = MIN(min_height, geometry.height);
    }

    p->preview_window_width  = 0.3 * min_width;
    p->preview_window_height = 0.5 * min_height;

    preview_area_correct(p); /* calculate preview_width and height */
    gtk_widget_set_size_request(GTK_WIDGET(p->window), p->preview_width, p->preview_height);
  }
  else if (surface_changed) /* establish new surface */
  {
    DBG(DBG_info, "preview_update_surface: establish new surface\n");
    preview_area_correct(p); /* calculate preview_width and height */
    preview_area_resize(p);   /* correct rulers */
    preview_display_with_correction(p); /* draw preview */
    xsane_update_histogram(TRUE /* update raw */);

    p->previous_selection.active = FALSE;
    p->previous_selection_maximum.active = FALSE;
    preview_bound_selection(p);	 /* make sure selection is not larger than surface */
    preview_draw_selection(p);   /* the selection is overpainted, we have to update it */
    preview_establish_selection(p); /* send selection to backend, it may be changed */
  }
  else /* leave everything like it is */
  {
    DBG(DBG_info, "preview_update_surface: surface unchanged\n");
    preview_update_selection(p);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* preview_scan is called once when the "Preview scan" button is pressed */
void preview_scan(Preview *p)
{
 double min, max, swidth, sheight, width, height, dpi = 0;
 const SANE_Option_Descriptor *opt;
 gint gwidth, gheight;
 int i;
 float dsurface[4];
 int gamma_gray_size  = 256; /* set this values to image depth for more than 8bpp input support!!! */
 int gamma_red_size   = 256;
 int gamma_green_size = 256;
 int gamma_blue_size  = 256;
 int gamma_gray_max   = 255; /* set this to to image depth for more than 8bpp output support */
 int gamma_red_max    = 255;
 int gamma_green_max  = 255;
 int gamma_blue_max   = 255;

  DBG(DBG_proc, "preview_scan\n");

  /* we are overpainting the image, so we do not have any visible selections */
  p->previous_selection.active = FALSE;
  p->previous_selection_maximum.active = FALSE;

  xsane.block_update_param = TRUE; /* do not change parameters each time */

  preview_save_option(p, xsane.well_known.dpi,      &p->saved_dpi,      &p->saved_dpi_valid);
  preview_save_option(p, xsane.well_known.dpi_x,    &p->saved_dpi_x,    &p->saved_dpi_x_valid);
  preview_save_option(p, xsane.well_known.dpi_y,    &p->saved_dpi_y,    &p->saved_dpi_y_valid);
  preview_save_option(p, xsane.well_known.scanmode, &p->saved_scanmode, &p->saved_scanmode_valid);

  for (i = 0; i < 4; ++i)
  {
    preview_save_option(p, xsane.well_known.coord[i], &p->saved_coord[i], p->saved_coord_valid + i);
  }

  preview_save_option(p, xsane.well_known.bit_depth, &p->saved_bit_depth, &p->saved_bit_depth_valid);

  /* determine dpi, if necessary: */

  if (xsane.well_known.dpi > 0)
  {
   float aspect;

    if ( ((p->rotation & 3) == 0) || ((p->rotation & 3) == 2) )
    {
      aspect = p->aspect;
    }
    else
    {
      aspect = 1.0 / p->aspect;
    }

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.dpi);

    gwidth  = p->preview_width;
    gheight = p->preview_height;

    height = gheight;
    width  = height * aspect;

    if (width > gwidth)
    {
      width  = gwidth;
      height = width / aspect;
    }

    swidth = fabs(p->surface[xsane_back_gtk_BR_X] - p->surface[xsane_back_gtk_TL_X]);

    if (swidth < INF)
    {
	dpi = MM_PER_INCH * width/swidth;
    }
    else
    {
      sheight = fabs(p->surface[xsane_back_gtk_BR_Y] - p->surface[xsane_back_gtk_TL_Y]);
      if (sheight < INF)
      {
        dpi = MM_PER_INCH * height/sheight;
      }
      else
      {
        dpi = 18.0;
      }
    }

    dpi = dpi * preferences.preview_oversampling; /* faktor for resolution */

    xsane_get_bounds(opt, &min, &max);

    if (dpi < min)
    {
      dpi = min;
    }

    if (dpi > max)
    {
      dpi = max;
    }

    xsane_set_resolution(xsane.well_known.dpi,   dpi); /* set resolution to dpi or next higher value that is available */
    xsane_set_resolution(xsane.well_known.dpi_x, dpi); /* set resolution to dpi or next higher value that is available */
    xsane_set_resolution(xsane.well_known.dpi_y, dpi); /* set resolution to dpi or next higher value that is available */
  }


  preview_rotate_previewsurface_to_devicesurface(p->rotation, p->surface, dsurface);

  for (i = 0; i < 4; ++i)
  {
    preview_set_option_float(p, xsane.well_known.coord[i], dsurface[i]);
  }

  preview_set_option_val(p, xsane.well_known.preview, SANE_TRUE);

  if ( (xsane.grayscale_scanmode) && (xsane.param.depth == 1) && (xsane.lineart_mode == XSANE_LINEART_GRAYSCALE) )
  {
    preview_set_option(p, xsane.well_known.scanmode, xsane.grayscale_scanmode);
  }

#if 0
  if ( (p->saved_bit_depth == 16) && (p->saved_bit_depth_valid) ) /* don't scan with 16 bpp */
  {
    preview_set_option_val(p, xsane.well_known.bit_depth, 8);
  }
#endif


  if (xsane.well_known.gamma_vector >0)
  {
   const SANE_Option_Descriptor *opt;
 
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
     SANE_Int *gamma_data;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);
      gamma_gray_size = opt->size / sizeof(opt->type);
      gamma_gray_max  = opt->constraint.range->max;

      gamma_data = malloc(gamma_gray_size  * sizeof(SANE_Int));

      if ((xsane.xsane_channels > 1) || (xsane.no_preview_medium_gamma)) /* color scan or medium preview gamma disabled */
      {
        xsane_create_gamma_curve(gamma_data, 0, 1.0, 0.0, 0.0, 0.0, 100.0, 1.0, gamma_gray_size, gamma_gray_max);
      }
      else /* grayscale scan */
      {
        xsane_create_gamma_curve(gamma_data, xsane.medium_negative, 1.0, 0.0, 0.0,
                                 xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                 gamma_gray_size, gamma_gray_max);
      }
      
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector, gamma_data);
      free(gamma_data);
    }
  }

  if (xsane.well_known.gamma_vector_r >0)
  {
   const SANE_Option_Descriptor *opt;

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_r);
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
     SANE_Int *gamma_data_red, *gamma_data_green, *gamma_data_blue;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_r);
      gamma_red_size = opt->size / sizeof(opt->type);
      gamma_red_max  = opt->constraint.range->max;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_g);
      gamma_green_size = opt->size / sizeof(opt->type);
      gamma_green_max  = opt->constraint.range->max;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_b);
      gamma_blue_size = opt->size / sizeof(opt->type);
      gamma_blue_max  = opt->constraint.range->max;

      gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
      gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
      gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

      if (xsane.no_preview_medium_gamma) /* do not use medium gamma for preview */
      {
        DBG(DBG_info, "preview: not using medium gamma table\n");

        xsane_create_gamma_curve(gamma_data_red,   0, 1.0, 0.0, 0.0, 0.0, 100.0, 1.0, gamma_red_size,   gamma_red_max);
        xsane_create_gamma_curve(gamma_data_green, 0, 1.0, 0.0, 0.0, 0.0, 100.0, 1.0, gamma_green_size, gamma_green_max);
        xsane_create_gamma_curve(gamma_data_blue,  0, 1.0, 0.0, 0.0, 0.0, 100.0, 1.0, gamma_blue_size,  gamma_blue_max);
      }
      else /* use medium gamma for preview */
      {
        DBG(DBG_info, "preview: using medium gamma table\n");

        xsane_create_gamma_curve(gamma_data_red,   xsane.medium_negative, 1.0, 0.0, 0.0,
                                 xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red,
                                 gamma_red_size,   gamma_red_max);
        xsane_create_gamma_curve(gamma_data_green, xsane.medium_negative, 1.0, 0.0, 0.0,
                                 xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green,
                                 gamma_green_size, gamma_green_max);
        xsane_create_gamma_curve(gamma_data_blue,  xsane.medium_negative, 1.0, 0.0, 0.0,
                                 xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue,
                                 gamma_blue_size,  gamma_blue_max);
      }

      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_r, gamma_data_red);
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_g, gamma_data_green);
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_b, gamma_data_blue);

      free(gamma_data_red);
      free(gamma_data_green);
      free(gamma_data_blue);
    }
  }


  xsane.block_update_param = FALSE;
  p->preview_channels  = xsane.xsane_channels;
  p->scan_incomplete = FALSE;
  p->invalid = TRUE; /* no valid preview */
  p->scanning = TRUE;

  preview_display_valid(p);


  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh);


  /* OK, all set to go */
  preview_scan_start(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_save_image_file(Preview *p, FILE *out)
{
  DBG(DBG_proc, "preview_save_image_file\n");

  if (out)
  {
   float dsurface[4];

    preview_rotate_previewsurface_to_devicesurface(p->rotation, p->surface, dsurface);

    /* always save it as a 16 bit PPM image: */
    fprintf(out, "P6\n"
                 "# surface: %g %g %g %g %u %u\n"
                 "# time: %d\n"
                 "%d %d\n65535\n",
                 dsurface[0], dsurface[1], dsurface[2], dsurface[3],
                 p->surface_type, p->surface_unit,
                 (int) time(NULL),
                 p->image_width, p->image_height);

    fwrite(p->image_data_raw, 6, p->image_width*p->image_height, out);
    fclose(out);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_save_image(Preview *p)
{
 FILE *out;
 int level=0;

  DBG(DBG_proc, "preview_save_image\n");

  if (!p->image_data_raw)
  {
    return;
  }

  if ( GROSSLY_EQUAL(p->max_scanner_surface[0], p->surface[0]) && /* full device surface */
       GROSSLY_EQUAL(p->max_scanner_surface[1], p->surface[1]) &&
       GROSSLY_EQUAL(p->max_scanner_surface[2], p->surface[2]) &&
       GROSSLY_EQUAL(p->max_scanner_surface[3], p->surface[3]) )
  {
    level = 0;
  }
  else if ( GROSSLY_EQUAL(p->scanner_surface[0], p->surface[0]) && /* user defined surface */
            GROSSLY_EQUAL(p->scanner_surface[1], p->surface[1]) &&
            GROSSLY_EQUAL(p->scanner_surface[2], p->surface[2]) &&
            GROSSLY_EQUAL(p->scanner_surface[3], p->surface[3]) )
  {
    level = 1;
  }
  else /* zoom area */
  {
    level = 2;
  }

  if (p->filename[level])
  {
    /* save preview image */
    out = fopen(p->filename[level], "wb"); /* b = binary mode for win32*/

    preview_save_image_file(p, out);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_delete_images(Preview *p)
{
 FILE *out;
 int level=0;

  DBG(DBG_proc, "preview_delete_images_file\n");

  for (level = 0; level<3; level++)
  {
    out = fopen(p->filename[level], "wb"); /* b = binary mode for win32*/
    if (out)
    fclose(out);
  }
  preview_update_surface(p, 1);

  xsane_batch_scan_update_icon_list();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_destroy(Preview *p)
{
 int level;

  DBG(DBG_proc, "preview_destroy\n");

  if (p->scanning)
  {
    preview_scan_done(p, 0);		/* don't save partial window */
  }

  for(level = 0; level <= 2; level++)
  {
    if (p->filename[level])
    {
      remove(p->filename[level]); /* remove existing preview */
    }
  }

  if (p->image_data_enh)
  {
    free(p->image_data_enh);
    p->image_data_enh = 0;
  }

  if (p->image_data_raw)
  {
    free(p->image_data_raw);
    p->image_data_raw = 0;
  }

  if (p->preview_row)
  {
    free(p->preview_row);
    p->preview_row = 0;
  }

  if (p->gc_selection)
  {
    gdk_gc_unref(p->gc_selection);
  }

  if (p->gc_selection_maximum)
  {
    gdk_gc_unref(p->gc_selection_maximum);
  }

  if (p->top)
  {
    gtk_widget_destroy(p->top);
  }
  free(p);

  p = 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_area(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 int i;

  DBG(DBG_proc, "preview_zoom_area\n");

  for (i=0; i<4; i++)
  {
    p->old_surface[i] = p->surface[i];
    p->surface[i]     = p->selection.coordinate[i];
  }

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, TRUE); /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_out, TRUE); /* allow zoom out */
  gtk_widget_set_sensitive(p->zoom_undo,TRUE); /* allow zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_not(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 int i;

  DBG(DBG_proc, "preview_zoom_not\n");

  for (i=0; i<4; i++)
  {
    p->surface[i] = p->scanner_surface[i];
  }

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, FALSE); /* forbid unzoom */
  gtk_widget_set_sensitive(p->zoom_out, FALSE); /* forbid zoom out */
  gtk_widget_set_sensitive(p->zoom_undo,TRUE);  /* allow zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_out(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 int i;
 float delta_width;
 float delta_height;

  DBG(DBG_proc, "preview_zoom_out\n");

  for (i=0; i<4; i++)
  {
    p->old_surface[i] = p->surface[i];
  }

  delta_width  = (p->surface[p->index_xmax] - p->surface[p->index_xmin]) * 0.2;
  delta_height = (p->surface[p->index_ymax] - p->surface[p->index_ymin]) * 0.2;

  p->surface[p->index_xmin] -= delta_width;
  p->surface[p->index_xmax] += delta_width;
  p->surface[p->index_ymin] -= delta_height;
  p->surface[p->index_ymax] += delta_height;

  if (p->surface[p->index_xmin] < p->scanner_surface[p->index_xmin])
  {
    p->surface[p->index_xmin] = p->scanner_surface[p->index_xmin];
  } 

  if (p->surface[p->index_ymin] < p->scanner_surface[p->index_ymin])
  {
    p->surface[p->index_ymin] = p->scanner_surface[p->index_ymin];
  } 

  if (p->surface[p->index_xmax] > p->scanner_surface[p->index_xmax])
  {
    p->surface[p->index_xmax] = p->scanner_surface[p->index_xmax];
  } 

  if (p->surface[p->index_ymax] > p->scanner_surface[p->index_ymax])
  {
    p->surface[p->index_ymax] = p->scanner_surface[p->index_ymax];
  } 

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, TRUE); /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_undo,TRUE); /* allow zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_select_zoom_point(Preview *p, int preview_x, int preview_y)
{
 int i;
 float device_x, device_y;

  DBG(DBG_proc, "preview_select_zoom_point(%d, %d)\n", preview_x, preview_y);

  preview_transform_coordinate_window_to_device(p, preview_x, preview_y, &device_x, &device_y);

  for (i=0; i<4; i++)
  {
    p->old_surface[i] = p->surface[i];
  }

  p->surface[0] = device_x + (p->surface[0] - device_x) * 0.8;
  p->surface[1] = device_y + (p->surface[1] - device_y) * 0.8;
  p->surface[2] = device_x + (p->surface[2] - device_x) * 0.8;
  p->surface[3] = device_y + (p->surface[3] - device_y) * 0.8;

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, TRUE); /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_out, TRUE); /* allow zoom out */
  gtk_widget_set_sensitive(p->zoom_undo,TRUE); /* allow zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_undo(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 int i;

  DBG(DBG_proc, "preview_zoom_undo\n");

  for (i=0; i<4; i++)
  {
    p->surface[i] = p->old_surface[i];
  }

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, TRUE);   /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_out, TRUE); /* allow zoom out */
  gtk_widget_set_sensitive(p->zoom_undo, FALSE); /* forbid zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_color(Preview *p, int x, int y, int range, int *red, int *green, int *blue)
{
 int image_x, image_y;
 int image_x_min, image_y_min;
 int image_x_max, image_y_max;
 int offset;
 int count = 0;

  DBG(DBG_proc, "preview_get_color\n");

  if (p->image_data_raw)
  {
    preview_transform_coordinate_window_to_image(p, x, y, &image_x, &image_y);

    if ( (image_x >= 0) && (image_x < p->image_width) && (image_y >=0) && (image_y < p->image_height) )
    {
      image_x_min = image_x - range/2;
      image_y_min = image_y - range/2;
      image_x_max = image_x + range/2;
      image_y_max = image_y + range/2;

      xsane_bound_int(&image_x_min, 0, p->image_width - 1);
      xsane_bound_int(&image_x_max, 0, p->image_width - 1);
      xsane_bound_int(&image_y_min, 0, p->image_height - 1);
      xsane_bound_int(&image_y_max, 0, p->image_height - 1);

      *red   = 0;
      *green = 0;
      *blue  = 0;

      for (image_x = image_x_min; image_x <= image_x_max; image_x++)
      {
        for (image_y = image_y_min; image_y <= image_y_max; image_y++)
        {
          count++;

          offset = 3 * (image_y * p->image_width + image_x);
 
          if (!xsane.negative) /* positive */
          {
            *red   += (p->image_data_raw[offset    ]) >> 8;
            *green += (p->image_data_raw[offset + 1]) >> 8;
            *blue  += (p->image_data_raw[offset + 2]) >> 8;
          }
          else /* negative */
          {
            *red   += 255 - (p->image_data_raw[offset    ] >> 8);
            *green += 255 - (p->image_data_raw[offset + 1] >> 8);
            *blue  += 255 - (p->image_data_raw[offset + 2] >> 8);
          }
        }
      }

      *red   /= count;
      *green /= count;
      *blue  /= count;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_add_batch(GtkWidget *window, Preview *data)
{
  DBG(DBG_proc, "preview_add_batch\n");

  xsane_batch_scan_add(); /* add active settings to batch list */

#if 0
  preview_draw_selection(p); /* read selection from backend: correct rotation */
  preview_establish_selection(p); /* read selection from backend: correct rotation */
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_pipette_white(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 GdkCursor *cursor;
 GdkColor fg;
 GdkColor bg;
 GdkPixmap *pixmap;
 GdkPixmap *mask;

  DBG(DBG_proc, "preview_pipette_white\n");

  p->mode = MODE_PIPETTE_WHITE;

  pixmap = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_white, CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);
  mask   = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_mask,  CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);

  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  cursor = gdk_cursor_new_from_pixmap(pixmap, mask, &fg, &bg, CURSOR_PIPETTE_HOT_X, CURSOR_PIPETTE_HOT_Y);

  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_pipette_gray(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 GdkCursor *cursor;
 GdkColor fg;
 GdkColor bg;
 GdkPixmap *pixmap;
 GdkPixmap *mask;

  DBG(DBG_proc, "preview_pipette_gray\n");

  p->mode = MODE_PIPETTE_GRAY;

  pixmap = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_gray, CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);
  mask   = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_mask, CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);

  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  cursor = gdk_cursor_new_from_pixmap(pixmap, mask, &fg, &bg, CURSOR_PIPETTE_HOT_X, CURSOR_PIPETTE_HOT_Y);

  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_pipette_black(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 GdkCursor *cursor;
 GdkColor fg;
 GdkColor bg;
 GdkPixmap *pixmap;
 GdkPixmap *mask;

  DBG(DBG_proc, "preview_pipette_black\n");

  p->mode = MODE_PIPETTE_BLACK;

  pixmap = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_black, CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);
  mask   = gdk_bitmap_create_from_data(p->top->window, cursor_pipette_mask , CURSOR_PIPETTE_WIDTH, CURSOR_PIPETTE_HEIGHT);

  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  cursor = gdk_cursor_new_from_pixmap(pixmap, mask, &fg, &bg, CURSOR_PIPETTE_HOT_X, CURSOR_PIPETTE_HOT_Y);

  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_init_autoraise_scan_area(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 GdkCursor *cursor;
 GdkColor fg;
 GdkColor bg;
 GdkPixmap *pixmap;
 GdkPixmap *mask;

  DBG(DBG_proc, "preview_init_autoraise_scan_area\n");

  p->mode = MODE_AUTORAISE_SCAN_AREA;

  pixmap = gdk_bitmap_create_from_data(p->top->window, cursor_autoraise_scan_area,      CURSOR_AUTORAISE_SCAN_AREA_WIDTH, CURSOR_AUTORAISE_SCAN_AREA_HEIGHT);
  mask   = gdk_bitmap_create_from_data(p->top->window, cursor_autoraise_scan_area_mask, CURSOR_AUTORAISE_SCAN_AREA_WIDTH, CURSOR_AUTORAISE_SCAN_AREA_HEIGHT);

  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  cursor = gdk_cursor_new_from_pixmap(pixmap, mask, &fg, &bg, CURSOR_AUTORAISE_SCAN_AREA_HOT_X, CURSOR_AUTORAISE_SCAN_AREA_HOT_Y);

  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = -1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_in(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 GdkCursor *cursor;
 GdkColor fg;
 GdkColor bg;
 GdkPixmap *pixmap;
 GdkPixmap *mask;

  DBG(DBG_proc, "preview_zoom\n");

  p->mode = MODE_ZOOM_IN;

  pixmap = gdk_bitmap_create_from_data(p->top->window, cursor_zoom,      CURSOR_ZOOM_WIDTH, CURSOR_ZOOM_HEIGHT);
  mask   = gdk_bitmap_create_from_data(p->top->window, cursor_zoom_mask, CURSOR_ZOOM_WIDTH, CURSOR_ZOOM_HEIGHT);

  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  cursor = gdk_cursor_new_from_pixmap(pixmap, mask, &fg, &bg, CURSOR_ZOOM_HOT_X, CURSOR_ZOOM_HOT_Y);

  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_unref(cursor);
  p->cursornr = -1;
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_select_full_preview_area(Preview *p)
{
 int i;

  DBG(DBG_proc, "preview_select_full_preview_area\n");

  p->selection.active = TRUE;

  for (i=0; i<4; i++)
  {
    p->selection.coordinate[i] = p->surface[i];
  }

  preview_update_maximum_output_size(p);
  preview_draw_selection(p);
  preview_establish_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_full_preview_area_callback(GtkWidget *widget, gpointer call_data)
{
 Preview *p = call_data;

  DBG(DBG_proc, "preview_full_preview_area_callback\n");

  preview_select_full_preview_area(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_delete_images_callback(GtkWidget *widget, gpointer call_data)
{
 Preview *p = call_data;

  DBG(DBG_proc, "preview_delete_images_callback\n");

  preview_delete_images(p);
  p->invalid = TRUE;
  preview_display_valid(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_rename_callback(GtkWidget *widget, GtkWidget *preset_area_widget)
{
 int selection;
 char *oldname;
 char *newname;
 Preview *p;
 GtkWidget *old_preset_area_menu;
 int old_selection;

  DBG(DBG_proc, "preview_preset_area_rename_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Selection");
  p = (Preview *) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Preview"); 

  DBG(DBG_info ,"rename %s\n", preferences.preset_area[selection]->name);

  /* set menu in correct state, is a bit strange this way but I do not have a better idea */
  old_preset_area_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(p->preset_area_option_menu));
  old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_preset_area_menu))), "Selection");  
  gtk_menu_popdown(GTK_MENU(old_preset_area_menu));
  gtk_option_menu_set_history(GTK_OPTION_MENU(p->preset_area_option_menu), old_selection);

  oldname = strdup(preferences.preset_area[selection]->name);

  if (!xsane_front_gtk_getname_dialog(WINDOW_PRESET_AREA_RENAME, DESC_PRESET_AREA_RENAME, oldname, &newname))
  {
    gtk_option_menu_remove_menu(GTK_OPTION_MENU(p->preset_area_option_menu));

    if (GTK_IS_WIDGET(old_preset_area_menu)) /* the menu normally is closed when we come here */
    {
      gtk_widget_destroy(old_preset_area_menu);
    }

    free(preferences.preset_area[selection]->name);
    preferences.preset_area[selection]->name = strdup(newname);
    DBG(DBG_info, "renaming %s to %s\n", oldname, newname);

    preview_create_preset_area_menu(p, old_selection);
  }
 
  free(oldname);
  free(newname);
 
  xsane_set_sensitivity(TRUE);

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_add_callback(GtkWidget *widget, GtkWidget *preset_area_widget)
{
 int selection, i, old_selection = 0;
 Preview *p;
 float coord[4];
 char suggested_name[PATH_MAX];
 char *newname;
 GtkWidget *old_preset_area_menu;

  DBG(DBG_proc, "preview_preset_area_add_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Selection");
  p = (Preview *) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Preview"); 

  /* set menu in correct state, is a bit strange this way but I do not have a better idea */
  old_preset_area_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(p->preset_area_option_menu));
  old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_preset_area_menu))), "Selection");  
  gtk_menu_popdown(GTK_MENU(old_preset_area_menu));
  gtk_option_menu_set_history(GTK_OPTION_MENU(p->preset_area_option_menu), old_selection);

  /* sugggest name = size in mm */ 
  preview_rotate_previewsurface_to_devicesurface(p->rotation, p->selection.coordinate, coord);
  snprintf(suggested_name, sizeof(suggested_name), "%d mm x %d mm", (int) (coord[2]-coord[0]), (int) (coord[3]-coord[1]));

  if (!xsane_front_gtk_getname_dialog(WINDOW_PRESET_AREA_ADD, DESC_PRESET_AREA_ADD, suggested_name, &newname))
  {
    preferences.preset_area = realloc(preferences.preset_area, (preferences.preset_area_definitions+1) * sizeof(void *));

    /* shift all items after selection */
    for (i = preferences.preset_area_definitions-1; i > selection; i--)
    {
      preferences.preset_area[i+1] = preferences.preset_area[i];
    }

    /* insert new item behind selected item */
    preferences.preset_area[selection+1] = calloc(sizeof(Preferences_preset_area_t), 1);
    preferences.preset_area[selection+1]->name    = strdup(newname);
    preferences.preset_area[selection+1]->xoffset = coord[0];
    preferences.preset_area[selection+1]->yoffset = coord[1];
    preferences.preset_area[selection+1]->width   = coord[2] - coord[0];
    preferences.preset_area[selection+1]->height  = coord[3] - coord[1];

    DBG(DBG_proc, "added %s\n", newname);

    preferences.preset_area_definitions++;

    preview_create_preset_area_menu(p, old_selection);
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_delete_callback(GtkWidget *widget, GtkWidget *preset_area_widget)
{
 int selection, i, old_selection = 0;
 Preview *p;
 GtkWidget *old_preset_area_menu;

  DBG(DBG_proc, "preview_preset_area_delete_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Selection");
  p = (Preview *) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Preview"); 


  if (selection) /* full size can not be deleted */
  {
    DBG(DBG_info ,"deleting %s\n", preferences.preset_area[selection]->name);

    free(preferences.preset_area[selection]);

    for (i=selection; i<preferences.preset_area_definitions-1; i++)
    {
      preferences.preset_area[i] = preferences.preset_area[i+1];
    }

    preferences.preset_area_definitions--;

    old_preset_area_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(p->preset_area_option_menu));

    gtk_option_menu_remove_menu(GTK_OPTION_MENU(p->preset_area_option_menu));
    old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_preset_area_menu))), "Selection");  

    if (old_selection == selection) /* we are deleting the selected surface */
    {
      old_selection = 0;
    }
    else if (old_selection > selection) /* we are deleting the selected surface */
    {
      old_selection--;
    }

    gtk_widget_destroy(old_preset_area_menu);

    preview_create_preset_area_menu(p, old_selection); /* build menu and set default to 0=full size */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_move_up_callback(GtkWidget *widget, GtkWidget *preset_area_widget)
{
 int selection, old_selection = 0;
 Preview *p;
 GtkWidget *old_preset_area_menu;

  DBG(DBG_proc, "preview_preset_area_move_up_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Selection");
  p = (Preview *) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Preview"); 

  if (selection > 1) /* make sure "full area" stays at top */
  {
   Preferences_preset_area_t *help_area;

    DBG(DBG_info ,"moving up %s\n", preferences.preset_area[selection]->name);

    help_area = preferences.preset_area[selection-1];
    preferences.preset_area[selection-1] = preferences.preset_area[selection];
    preferences.preset_area[selection] = help_area;

    old_preset_area_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(p->preset_area_option_menu));

    gtk_option_menu_remove_menu(GTK_OPTION_MENU(p->preset_area_option_menu));
    old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_preset_area_menu))), "Selection");  

    if (old_selection == selection)
    {
      old_selection--;
    }
    else if (old_selection == selection-1)
    {
      old_selection++;
    }

    gtk_widget_destroy(old_preset_area_menu);

    preview_create_preset_area_menu(p, old_selection); /* build menu and set default to 0=full size */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_move_down_callback(GtkWidget *widget, GtkWidget *preset_area_widget)
{
 int selection, old_selection = 0;
 Preview *p;
 GtkWidget *old_preset_area_menu;

  DBG(DBG_proc, "preview_preset_area_move_down_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Selection");
  p = (Preview *) gtk_object_get_data(GTK_OBJECT(preset_area_widget), "Preview"); 

  /* full size can not moved down */
  if ((selection) && (selection < preferences.preset_area_definitions-1))
  {
   Preferences_preset_area_t *help_area;

    DBG(DBG_info ,"moving down %s\n", preferences.preset_area[selection]->name);

    help_area = preferences.preset_area[selection];
    preferences.preset_area[selection] = preferences.preset_area[selection+1];
    preferences.preset_area[selection+1] = help_area;

    old_preset_area_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(p->preset_area_option_menu));

    gtk_option_menu_remove_menu(GTK_OPTION_MENU(p->preset_area_option_menu));
    old_selection = (int) gtk_object_get_data(GTK_OBJECT(gtk_menu_get_active(GTK_MENU(old_preset_area_menu))), "Selection");  

    if (old_selection == selection)
    {
      old_selection++;
    }
    else if (old_selection == selection+1)
    {
      old_selection--;
    }

    gtk_widget_destroy(old_preset_area_menu);

    preview_create_preset_area_menu(p, old_selection); /* build menu and set default to 0=full size */
  }

  return TRUE; /* event is handled */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_preset_area_context_menu_callback(GtkWidget *widget, GdkEvent *event)
{
 GtkWidget *menu;
 GtkWidget *menu_item;
 GdkEventButton *event_button;
 int selection;
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "preview_preset_area_context_menu_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  if (event->type == GDK_BUTTON_PRESS)
  {
    event_button = (GdkEventButton *) event;

    if (event_button->button == 3)
    {
      menu = gtk_menu_new();

      /** add selection */
      menu_item = gtk_menu_item_new_with_label(MENU_ITEM_PRESET_AREA_ADD_SEL);
      gtk_widget_show(menu_item);
      gtk_container_add(GTK_CONTAINER(menu), menu_item);
      g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) preview_preset_area_add_callback, widget);

      /* add separator */
      menu_item = gtk_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_container_add(GTK_CONTAINER(menu), menu_item);

      /* rename preset area */
      snprintf(buf, sizeof(buf), "%s: %s", preferences.preset_area[selection]->name, MENU_ITEM_RENAME);
      menu_item = gtk_menu_item_new_with_label(buf);
      gtk_widget_show(menu_item);
      gtk_container_add(GTK_CONTAINER(menu), menu_item);
      g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) preview_preset_area_rename_callback, widget);

      if (selection) /* not available for "full area" */
      {
        /* delete preset area */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.preset_area[selection]->name, MENU_ITEM_DELETE);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) preview_preset_area_delete_callback, widget);
      }

      if (selection>1) /* available from 3rd item */
      {
        /* move up */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.preset_area[selection]->name, MENU_ITEM_MOVE_UP);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) preview_preset_area_move_up_callback, widget);
      }

      if ((selection) && (selection < preferences.preset_area_definitions-1))
      {
        /* move down */
        snprintf(buf, sizeof(buf), "%s: %s", preferences.preset_area[selection]->name, MENU_ITEM_MOVE_DWN);
        menu_item = gtk_menu_item_new_with_label(buf);
        gtk_widget_show(menu_item);
        gtk_container_add(GTK_CONTAINER(menu), menu_item);
        g_signal_connect(GTK_OBJECT(menu_item), "activate", (GtkSignalFunc) preview_preset_area_move_down_callback, widget);
      }

/*      gtk_widget_show(menu); */
      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);

     return TRUE; /* event is handled */
    }
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_preset_area_callback(GtkWidget *widget, gpointer data)
{
 Preview *p = data;
 int selection;

  DBG(DBG_proc, "preview_preset_area_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  p->preset_surface[0] = preferences.preset_area[selection]->xoffset;
  p->preset_surface[1] = preferences.preset_area[selection]->yoffset;
  p->preset_surface[2] = preferences.preset_area[selection]->xoffset + preferences.preset_area[selection]->width;
  p->preset_surface[3] = preferences.preset_area[selection]->yoffset + preferences.preset_area[selection]->height;

  gtk_widget_set_sensitive(p->zoom_not,  TRUE); /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_undo, FALSE); /* forbid undo zoom */

  preview_update_surface(p, 0);
  preview_zoom_not(NULL, p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotation_callback(GtkWidget *widget, gpointer data)
{
 Preview *p = data;
 float rotated_surface[4];
 int rot;

  DBG(DBG_proc, "preview_rotation_callback\n");

  rot = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  switch (rot)
  {
    case 0: /* 0 degree */
    default:
      p->index_xmin = 0;
      p->index_xmax = 2;
      p->index_ymin = 1;
      p->index_ymax = 3;
     break;

    case 1: /* 90 degree */
      p->index_xmin = 2;
      p->index_xmax = 0;
      p->index_ymin = 1;
      p->index_ymax = 3;
     break;

    case 2: /* 180 degree */
      p->index_xmin = 2;
      p->index_xmax = 0;
      p->index_ymin = 3;
      p->index_ymax = 1;
     break;

    case 3: /* 270 degree */
      p->index_xmin = 0;
      p->index_xmax = 2;
      p->index_ymin = 3;
      p->index_ymax = 1;
     break;

    case 4: /* 0 degree, x mirror */
      p->index_xmin = 2;
      p->index_xmax = 0;
      p->index_ymin = 1;
      p->index_ymax = 3;
     break;

    case 5: /* 90 degree, x mirror */
      p->index_xmin = 0;
      p->index_xmax = 2;
      p->index_ymin = 1;
      p->index_ymax = 3;
     break;

    case 6: /* 180 degree, x mirror */
      p->index_xmin = 0;
      p->index_xmax = 2;
      p->index_ymin = 3;
      p->index_ymax = 1;
     break;

    case 7: /* 270 degree, x mirror */
      p->index_xmin = 2;
      p->index_xmax = 0;
      p->index_ymin = 3;
      p->index_ymax = 1;
  }

  /* at first undo mirror function, this is necessary because order does matter */
  if (p->rotation & 4)
  {
    rotated_surface[0] = p->surface[0];
    rotated_surface[1] = p->surface[1];
    rotated_surface[2] = p->surface[2];
    rotated_surface[3] = p->surface[3];
    preview_rotate_devicesurface_to_previewsurface(4, rotated_surface, p->surface);
  }

  /* now rotate the selection area and do mirror function (can be done in one step) */
  rotated_surface[0] = p->surface[0];
  rotated_surface[1] = p->surface[1];
  rotated_surface[2] = p->surface[2];
  rotated_surface[3] = p->surface[3];
  preview_rotate_devicesurface_to_previewsurface(( ( (rot & 3) - (p->rotation & 3) )  & 3 ) + /* rotation */ (rot & 4)/* x mirror */, rotated_surface, p->surface);

  p->rotation = rot;

  p->block_update_maximum_output_size_clipping = TRUE; /* necessary when in copy mode */
  preview_update_surface(p, 2); /* rotate surfaces */
  p->block_update_maximum_output_size_clipping = FALSE;
  preview_update_selection(p); /* read selection from backend: correct rotation */
  xsane_batch_scan_update_icon_list(); /* rotate batch scan icons */
  preview_establish_ratio(p); /* make sure ratio is like selected - when selected */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_establish_ratio(Preview *p)
{
 float width, height;

  DBG(DBG_proc, "preview_establish_ratio\n");

  if (p->ratio == 0.0)
  {
    return;
  }

  width  = fabs(p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]);
  height = fabs(p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]);

  if ( (0.99 < width / p->ratio / height) && (width / p->ratio / height < 1.01) )
  {
    return;
  }

  if ( (0.99 < width * p->ratio / height) && (width * p->ratio / height < 1.01) )
  {
    width = height;

    if (width > p->scanner_surface[p->index_xmax] - p->scanner_surface[p->index_xmin])
    {
      width = p->scanner_surface[p->index_xmax] - p->scanner_surface[p->index_xmin];
    }
  }

  height = width / p->ratio;
  if (height > p->scanner_surface[p->index_ymax] - p->scanner_surface[p->index_ymin])
  {
    height = p->scanner_surface[p->index_ymax] - p->scanner_surface[p->index_ymin];
    width = height * p->ratio;
  }

  p->selection.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + width;
  if (p->selection.coordinate[p->index_xmax] > p->scanner_surface[p->index_xmax])
  {
    p->selection.coordinate[p->index_xmax] = p->scanner_surface[p->index_xmax];
    p->selection.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmax] - width;
  }

  p->selection.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + height;
  if (p->selection.coordinate[p->index_ymax] > p->scanner_surface[p->index_ymax])
  {
    p->selection.coordinate[p->index_ymax] = p->scanner_surface[p->index_ymax];
    p->selection.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymax] - height;
  }

  preview_draw_selection(p); 
  preview_establish_selection(p); 
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_ratio_callback(GtkWidget *widget, gpointer data)
{
 Preview *p = data;
 float *ratio;

  DBG(DBG_proc, "preview_ratio_callback\n");

  ratio = (float *) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  p->ratio = *ratio;

  preview_establish_ratio(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_autoselect_scan_area_callback(GtkWidget *window, gpointer data)
{
 Preview *p=data;

  preview_autoselect_scan_area(p, p->selection.coordinate); /* get autoselection coordinates */
  preview_draw_selection(p); 
  preview_establish_selection(p); 
  xsane_update_histogram(TRUE /* update raw */); /* update histogram (necessary because overwritten by preview_update_surface */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_display_with_correction(Preview *p)
{
#ifdef HAVE_LIBLCMS
  if (xsane.enable_color_management)
  {
    preview_do_color_correction(p);
    gtk_widget_set_sensitive(p->pipette_white, FALSE); /* disable pipette buttons */
    gtk_widget_set_sensitive(p->pipette_gray,  FALSE); /* disable pipette buttons */
    gtk_widget_set_sensitive(p->pipette_black, FALSE); /* disable pipette buttons */

  }
  else
#endif
  {
    preview_do_gamma_correction(p);
    gtk_widget_set_sensitive(p->pipette_white, TRUE); /* enable pipette buttons */
    gtk_widget_set_sensitive(p->pipette_gray,  TRUE); /* enable pipette buttons */
    gtk_widget_set_sensitive(p->pipette_black, TRUE); /* enable pipette buttons */
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_do_gamma_correction(Preview *p)
{
 int x,y;
 int offset;
 u_char *image_data_enhp = NULL;
 guint16 *image_data_rawp = NULL;
 int rotate = 16 - preview_gamma_input_bits;

  DBG(DBG_proc, "preview_display_with_correction\n");

  if ((p->image_data_raw) && (p->params.depth > 1) && (preview_gamma_data_red))
  {
    if ( (xsane.param.format == SANE_FRAME_RGB) || /* color preview */
         (xsane.param.format == SANE_FRAME_RED) ||
         (xsane.param.format == SANE_FRAME_GREEN) ||
         (xsane.param.format == SANE_FRAME_BLUE) )
    {
      for (y=0; y < p->image_height; y++)
      {
        offset = 3 * (y * p->image_width);

        image_data_rawp = p->image_data_raw + offset;
        image_data_enhp = p->image_data_enh + offset;

        for (x=0; x < p->image_width; x++)
        {
          *image_data_enhp++ = preview_gamma_data_red  [(*image_data_rawp++) >> rotate];
          *image_data_enhp++ = preview_gamma_data_green[(*image_data_rawp++) >> rotate];
          *image_data_enhp++ = preview_gamma_data_blue [(*image_data_rawp++) >> rotate];
        }

        if (p->gamma_functions_interruptable)
        {
          while (gtk_events_pending())
          {
            DBG(DBG_info, "preview_display_with_correction: calling gtk_main_iteration\n");
            gtk_main_iteration();
          }
        }
      }
    }
    else /* grayscale preview */
    {
     int level;

      for (y=0; y < p->image_height; y++)
      {
        offset = 3 * (y * p->image_width);

        image_data_rawp = p->image_data_raw + offset;
        image_data_enhp = p->image_data_enh + offset;

        for (x=0; x < p->image_width; x++)
        {
          level = (*image_data_rawp++); /* red */
          level += (*image_data_rawp++); /* green */
          level += (*image_data_rawp++); /* blue */
          level /= 3;
          level >>= rotate;
          *image_data_enhp++ = preview_gamma_data_red  [level]; /* use 12 bit gamma table */
          *image_data_enhp++ = preview_gamma_data_green[level];
          *image_data_enhp++ = preview_gamma_data_blue [level];
        }

        if (p->gamma_functions_interruptable)
        {
          while (gtk_events_pending())
          {
            DBG(DBG_info, "preview_read_image_data (raw): calling gtk_main_iteration\n");
            gtk_main_iteration();
          }
        }
      }
    }
  }

  if (p->image_data_enh)
  {
    preview_display_partial_image(p);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
int preview_do_color_correction(Preview *p)
{
 int y;
 u_char *image_data_enhp = NULL;
 guint16 *image_data_rawp = NULL;
 cmsHPROFILE hInProfile = NULL;
 cmsHPROFILE hOutProfile = NULL;
 cmsHPROFILE hProofProfile = NULL;
 cmsHTRANSFORM hTransform = NULL;
 DWORD input_format, output_format;
 DWORD cms_flags = 0;
 int proof = 0;
 char *cms_proof_icm_profile = NULL;
 int linesize = 0;


  DBG(DBG_proc, "preview_do_color_correction\n");

  cmsErrorAction(LCMS_ERROR_SHOW);

  if (preferences.cms_bpc)
  {
    cms_flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
  }

  switch (p->cms_proofing)
  {
    default:
    case 0: /* display */
      proof = 0;
     break;

    case 1: /* proof printer */
      cms_proof_icm_profile  = preferences.printer[preferences.printernr]->icm_profile;
      proof = 1;
     break;

    case 2: /* proof custom proofing */
      cms_proof_icm_profile  = preferences.custom_proofing_icm_profile;
      proof = 1;
     break;
  }

  if ( (xsane.param.format == SANE_FRAME_RGB) || /* color preview */
       (xsane.param.format == SANE_FRAME_RED) ||
       (xsane.param.format == SANE_FRAME_GREEN) ||
       (xsane.param.format == SANE_FRAME_BLUE) )
  {
    input_format = TYPE_RGB_16;
    output_format = TYPE_RGB_8;
    linesize = p->image_width * 3;
  }
  else
  {
    input_format = TYPE_GRAY_16;
    output_format = TYPE_GRAY_8;
    linesize = p->image_width;
  }

  hInProfile  = cmsOpenProfileFromFile(xsane.scanner_default_color_icm_profile, "r");
  if (!hInProfile)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s\n%s %s: %s\n", ERR_CMS_CONVERSION, ERR_CMS_OPEN_ICM_FILE, CMS_SCANNER_ICM, xsane.scanner_default_color_icm_profile);
    xsane_back_gtk_error(buf, TRUE);
   return -1;
  }

  hOutProfile = cmsOpenProfileFromFile(preferences.display_icm_profile, "r");
  if (!hOutProfile)
  {
   char buf[TEXTBUFSIZE];

    cmsCloseProfile(hInProfile);

    snprintf(buf, sizeof(buf), "%s\n%s %s: %s\n", ERR_CMS_CONVERSION, ERR_CMS_OPEN_ICM_FILE, CMS_DISPLAY_ICM, preferences.display_icm_profile);
    xsane_back_gtk_error(buf, TRUE);
   return -1;
  }

  if (proof == 0)
  {
    hTransform = cmsCreateTransform(hInProfile, input_format,
                                    hOutProfile, output_format,
                                    preferences.cms_intent, cms_flags);
  }
  else /* proof */
  {
    cms_flags |= cmsFLAGS_SOFTPROOFING;

    if (p->cms_gamut_check)
    {
      cms_flags |= cmsFLAGS_GAMUTCHECK;
    }

    hProofProfile = cmsOpenProfileFromFile(cms_proof_icm_profile, "r");
    if (!hProofProfile)
    {
     char buf[TEXTBUFSIZE];

      cmsCloseProfile(hInProfile);
      cmsCloseProfile(hOutProfile);

      snprintf(buf, sizeof(buf), "%s\n%s %s: %s\n", ERR_CMS_CONVERSION, ERR_CMS_OPEN_ICM_FILE, CMS_PROOF_ICM, cms_proof_icm_profile);
      xsane_back_gtk_error(buf, TRUE);
     return -1;
    }

    hTransform = cmsCreateProofingTransform(hInProfile, input_format,
                                              hOutProfile, output_format,
                                              hProofProfile,
                                              preferences.cms_intent, p->cms_proofing_intent, cms_flags);
  }

  if (!hTransform)
  {
   char buf[TEXTBUFSIZE];

    cmsCloseProfile(hInProfile);
    cmsCloseProfile(hOutProfile);
    if (proof)
    {
      cmsCloseProfile(hProofProfile);
    }

    snprintf(buf, sizeof(buf), "%s\n%s\n", ERR_CMS_CONVERSION, ERR_CMS_CREATE_TRANSFORM);
    xsane_back_gtk_error(buf, TRUE);
   return -1;
  }


  for (y=0; y < p->image_height; y++)
  {
    image_data_rawp = p->image_data_raw + linesize * y;
    image_data_enhp = p->image_data_enh + linesize * y;

    cmsDoTransform(hTransform, image_data_rawp, image_data_enhp, p->image_width);

    if (p->gamma_functions_interruptable)
    {
      while (gtk_events_pending())
      {
        DBG(DBG_info, "preview_do_color_correction: calling gtk_main_iteration\n");
        gtk_main_iteration();
      }
    }
  }

  if (p->image_data_enh)
  {
    preview_display_partial_image(p);
  }

 return 0;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_calculate_raw_histogram(Preview *p, SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue)
{
 int x, y;
 int offset;
 SANE_Int red_raw, green_raw, blue_raw;
 SANE_Int min_x, max_x, min_y, max_y;
 float xscale, yscale;
 guint16 *image_data_rawp;
 
  DBG(DBG_proc, "preview_calculate_raw_histogram\n");

  preview_get_scale_device_to_image(p, &xscale, &yscale);

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      min_x = (p->selection.coordinate[0] - p->surface[0]) * xscale;
      min_y = (p->selection.coordinate[1] - p->surface[1]) * yscale;
      max_x = (p->selection.coordinate[2] - p->surface[0]) * xscale;
      max_y = (p->selection.coordinate[3] - p->surface[1]) * yscale;
     break;

    case 1: /* 90 degree */
      min_x = (p->selection.coordinate[1] - p->surface[1]) * xscale;
      min_y = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      max_x = (p->selection.coordinate[3] - p->surface[1]) * xscale;
      max_y = (p->selection.coordinate[0] - p->surface[2]) * xscale;
     break;

    case 2: /* 180 degree */
      min_x = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      min_y = (p->selection.coordinate[3] - p->surface[3]) * yscale;
      max_x = (p->selection.coordinate[0] - p->surface[2]) * xscale;
      max_y = (p->selection.coordinate[1] - p->surface[3]) * yscale;
     break;

    case 3: /* 270 degree */
      min_x = (p->selection.coordinate[3] - p->surface[3]) * xscale;
      min_y = (p->selection.coordinate[0] - p->surface[0]) * yscale;
      max_x = (p->selection.coordinate[1] - p->surface[3]) * xscale;
      max_y = (p->selection.coordinate[2] - p->surface[0]) * yscale;
     break;

    case 4: /* 0 degree, x mirror */
      min_x = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      min_y = (p->selection.coordinate[1] - p->surface[1]) * yscale;
      max_x = (p->selection.coordinate[0] - p->surface[2]) * xscale;
      max_y = (p->selection.coordinate[3] - p->surface[1]) * yscale;
     break;

    case 5: /* 90 degree, x mirror */
      min_x = (p->selection.coordinate[1] - p->surface[1]) * xscale;
      min_y = (p->selection.coordinate[0] - p->surface[0]) * yscale;
      max_x = (p->selection.coordinate[3] - p->surface[1]) * xscale;
      max_y = (p->selection.coordinate[2] - p->surface[0]) * yscale;
     break;

    case 6: /* 180 degree, x mirror */
      min_x = (p->selection.coordinate[0] - p->surface[0]) * xscale;
      min_y = (p->selection.coordinate[3] - p->surface[3]) * yscale;
      max_x = (p->selection.coordinate[2] - p->surface[0]) * xscale;
      max_y = (p->selection.coordinate[1] - p->surface[3]) * yscale;
     break;

    case 7: /* 270 degree, x mirror */
      min_x = (p->selection.coordinate[3] - p->surface[3]) * xscale;
      min_y = (p->selection.coordinate[2] - p->surface[2]) * yscale;
      max_x = (p->selection.coordinate[1] - p->surface[3]) * xscale;
      max_y = (p->selection.coordinate[0] - p->surface[2]) * yscale;
     break;
  }

  if (min_x < 0)
  {
    min_x = 0;
  }
   
  if (max_x >= p->image_width)
  {
    max_x = p->image_width-1;
  }
   
  if (min_y < 0)
  {
    min_y = 0;
  }
   
  if (max_y >= p->image_height)
  {
    max_y = p->image_height-1;
  }
   
  if ((p->image_data_raw) && (p->params.depth > 1) && (preview_gamma_data_red))
  {
    for (y = min_y; y <= max_y; y++)
    {
      offset = 3 * (y * p->image_width + min_x);
      image_data_rawp = p->image_data_raw + offset;

      if (!histogram_medium_gamma_data_red) /* no medium gamma table for histogran */
      {
        for (x = min_x; x <= max_x; x++)
        {
          red_raw   = (*image_data_rawp++) >> 8; /* reduce from 16 to 8 bits */
          green_raw = (*image_data_rawp++) >> 8;
          blue_raw  = (*image_data_rawp++) >> 8;

          count_raw      [(u_char) ((red_raw + green_raw + blue_raw)/3)]++;
          count_raw_red  [red_raw]++;
          count_raw_green[green_raw]++;
          count_raw_blue [blue_raw]++;
        }
      }
      else /* use medium gamma table for raw histogram */
      {
       int rotate = 16 - preview_gamma_input_bits;

        for (x = min_x; x <= max_x; x++)
        {
          red_raw   = histogram_medium_gamma_data_red  [(*image_data_rawp++) >> rotate];
          green_raw = histogram_medium_gamma_data_green[(*image_data_rawp++) >> rotate];
          blue_raw  = histogram_medium_gamma_data_blue [(*image_data_rawp++) >> rotate];

          count_raw      [(u_char) ((red_raw + green_raw + blue_raw)/3)]++;
          count_raw_red  [red_raw]++;
          count_raw_green[green_raw]++;
          count_raw_blue [blue_raw]++;
        }
      }

      if (p->gamma_functions_interruptable)
      {
        while (gtk_events_pending())
        {
          DBG(DBG_info, "preview_calculate_raw_histogram: calling gtk_main_iteration\n");
          gtk_main_iteration();
        }
      }
    }
  }
  else /* no preview image => all colors = 1 */
  {
   int i;

    for (i = 1; i <= 254; i++)
    {
      count_raw      [i] = 0;
      count_raw_red  [i] = 0;
      count_raw_green[i] = 0;
      count_raw_blue [i] = 0;
    }

    count_raw      [0] = 10;
    count_raw_red  [0] = 10;
    count_raw_green[0] = 10;
    count_raw_blue [0] = 10;

    count_raw      [255] = 10;
    count_raw_red  [255] = 10;
    count_raw_green[255] = 10;
    count_raw_blue [255] = 10;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_calculate_enh_histogram(Preview *p, SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue)
{
 int x, y;
 int offset;
 u_char red, green, blue;
 SANE_Int min_x, max_x, min_y, max_y;
 float xscale, yscale;
 guint16 *image_data_rawp;
 int rotate = 16 - preview_gamma_input_bits;
 
  DBG(DBG_proc, "preview_calculate_enh_histogram\n");

  preview_get_scale_device_to_image(p, &xscale, &yscale);

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      min_x = (p->selection.coordinate[0] - p->surface[0]) * xscale;
      min_y = (p->selection.coordinate[1] - p->surface[1]) * yscale;
      max_x = (p->selection.coordinate[2] - p->surface[0]) * xscale;
      max_y = (p->selection.coordinate[3] - p->surface[1]) * yscale;
     break;

    case 1: /* 90 degree */
      min_x = (p->selection.coordinate[1] - p->surface[1]) * xscale;
      min_y = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      max_x = (p->selection.coordinate[3] - p->surface[1]) * xscale;
      max_y = (p->selection.coordinate[0] - p->surface[2]) * xscale;
     break;

    case 2: /* 180 degree */
      min_x = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      min_y = (p->selection.coordinate[3] - p->surface[3]) * yscale;
      max_x = (p->selection.coordinate[0] - p->surface[2]) * xscale;
      max_y = (p->selection.coordinate[1] - p->surface[3]) * yscale;
     break;

    case 3: /* 270 degree */
      min_x = (p->selection.coordinate[3] - p->surface[3]) * xscale;
      min_y = (p->selection.coordinate[0] - p->surface[0]) * yscale;
      max_x = (p->selection.coordinate[1] - p->surface[3]) * xscale;
      max_y = (p->selection.coordinate[2] - p->surface[0]) * yscale;
     break;

    case 4: /* 0 degree, x mirror */
      min_x = (p->selection.coordinate[2] - p->surface[2]) * xscale;
      min_y = (p->selection.coordinate[1] - p->surface[1]) * yscale;
      max_x = (p->selection.coordinate[0] - p->surface[2]) * xscale;
      max_y = (p->selection.coordinate[3] - p->surface[1]) * yscale;
     break;

    case 5: /* 90 degree, x mirror */
      min_x = (p->selection.coordinate[1] - p->surface[1]) * xscale;
      min_y = (p->selection.coordinate[0] - p->surface[0]) * yscale;
      max_x = (p->selection.coordinate[3] - p->surface[1]) * xscale;
      max_y = (p->selection.coordinate[2] - p->surface[0]) * yscale;
     break;

    case 6: /* 180 degree, x mirror */
      min_x = (p->selection.coordinate[0] - p->surface[0]) * xscale;
      min_y = (p->selection.coordinate[3] - p->surface[3]) * yscale;
      max_x = (p->selection.coordinate[2] - p->surface[0]) * xscale;
      max_y = (p->selection.coordinate[1] - p->surface[3]) * yscale;
     break;

    case 7: /* 270 degree, x mirror */
      min_x = (p->selection.coordinate[3] - p->surface[3]) * xscale;
      min_y = (p->selection.coordinate[2] - p->surface[2]) * yscale;
      max_x = (p->selection.coordinate[1] - p->surface[3]) * xscale;
      max_y = (p->selection.coordinate[0] - p->surface[2]) * yscale;
     break;
  }

  if (min_x < 0)
  {
    min_x = 0;
  }
   
  if (max_x >= p->image_width)
  {
    max_x = p->image_width-1;
  }
   
  if (min_y < 0)
  {
    min_y = 0;
  }
   
  if (max_y >= p->image_height)
  {
    max_y = p->image_height-1;
  }
   
  if ((p->image_data_raw) && (p->params.depth > 1) && (preview_gamma_data_red))
  {
    for (y = min_y; y <= max_y; y++)
    {
      offset = 3 * (y * p->image_width + min_x);
      image_data_rawp = p->image_data_raw + offset;

      for (x = min_x; x <= max_x; x++)
      {
        red   = histogram_gamma_data_red  [(*image_data_rawp++) >> rotate];
        green = histogram_gamma_data_green[(*image_data_rawp++) >> rotate];
        blue  = histogram_gamma_data_blue [(*image_data_rawp++) >> rotate];

	count      [(u_char) ((red + green + blue)/3)]++;
	count_red  [red]++;
	count_green[green]++;
	count_blue [blue]++;
      }

      if (p->gamma_functions_interruptable)
      {
        while (gtk_events_pending())
        {
          DBG(DBG_info, "preview_calculate_enh_histogram: calling gtk_main_iteration\n");
          gtk_main_iteration();
        }
      }
    }
  }
  else /* no preview image => all colors = 1 */
  {
   int i;

    for (i = 1; i <= 254; i++)
    {
      count      [i] = 0;
      count_red  [i] = 0;
      count_green[i] = 0;
      count_blue [i] = 0;
    }

    count      [0] = 10;
    count_red  [0] = 10;
    count_green[0] = 10;
    count_blue [0] = 10;

    count      [255] = 10;
    count_red  [255] = 10;
    count_green[255] = 10;
    count_blue [255] = 10;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_gamma_correction(Preview *p, int gamma_input_bits, 
                              u_char *gamma_red, u_char *gamma_green, u_char *gamma_blue,
                              u_char *gamma_red_hist, u_char *gamma_green_hist, u_char *gamma_blue_hist,
                              u_char *medium_gamma_red_hist, u_char *medium_gamma_green_hist, u_char *medium_gamma_blue_hist)
{
  DBG(DBG_proc, "preview_gamma_correction\n");

  preview_gamma_data_red   = gamma_red;
  preview_gamma_data_green = gamma_green;
  preview_gamma_data_blue  = gamma_blue;

  histogram_gamma_data_red   = gamma_red_hist;
  histogram_gamma_data_green = gamma_green_hist;
  histogram_gamma_data_blue  = gamma_blue_hist;

  histogram_medium_gamma_data_red   = medium_gamma_red_hist;
  histogram_medium_gamma_data_green = medium_gamma_green_hist;
  histogram_medium_gamma_data_blue  = medium_gamma_blue_hist;

  preview_gamma_input_bits   = gamma_input_bits;

  preview_display_with_correction(p);
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_area_resize(Preview *p)
{
 float min_x, max_x, delta_x;
 float min_y, max_y, delta_y;
 float xscale, yscale;

  DBG(DBG_proc, "preview_area_resize\n");

  p->preview_window_width  = p->window->allocation.width;
  p->preview_window_height = p->window->allocation.height;

  p->preview_width         = p->window->allocation.width;
  p->preview_height        = p->window->allocation.height;

  preview_area_correct(p); /* set preview dimensions (with right aspect) that it fits into the window */

  if (p->preview_row) /* make sure preview_row is large enough for one line of the new size */
  {
    p->preview_row = realloc(p->preview_row, 3 * p->preview_window_width);
  }
  else
  {
    p->preview_row = malloc(3 * p->preview_window_width);
  }

  /* set the ruler ranges: */

  min_x = p->surface[xsane_back_gtk_TL_X];
  max_x = p->surface[xsane_back_gtk_BR_X];
  min_y = p->surface[xsane_back_gtk_TL_Y];
  max_y = p->surface[xsane_back_gtk_BR_Y]; 


  if (min_x <= -INF)
  {
    min_x = 0.0;
  }
  if (min_x >=  INF)
  {
    min_x = p->image_width - 1;
  }

  if (max_x <= -INF)
  {
    max_x = 0.0;
  }
  if (max_x >=  INF)
  {
    max_x = p->image_width - 1;
  }

  if (min_y <= -INF)
  {
    min_y = 0.0;
  }
  if (min_y >=  INF)
  {
    min_y = p->image_height - 1;
  }

  if (max_y <= -INF)
  {
    max_y = 0.0;
  }
  if (max_y >=  INF)
  {
    max_y = p->image_height - 1;
  }

  /* convert mm to inches if that's what the user wants: */

  if (p->surface_unit == SANE_UNIT_MM)
  {
   double factor = 1.0/preferences.length_unit;

    min_x *= factor;
    max_x *= factor;
    min_y *= factor;
    max_y *= factor;
  }

  preview_get_scale_window_to_image(p, &xscale, &yscale);

  delta_x = max_x - min_x;

  gtk_ruler_set_range(GTK_RULER(p->hruler), min_x, min_x + delta_x*p->preview_window_width/p->preview_width, min_x, /* max_size */ 20);

  delta_y = max_y - min_y;

  gtk_ruler_set_range(GTK_RULER(p->vruler), min_y, min_y + delta_y*p->preview_window_height/p->preview_height, min_y, /* max_size */ 20);

  gtk_label_set_text(GTK_LABEL(p->unit_label), xsane_back_gtk_unit_string(p->surface_unit));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

gint preview_area_resize_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
 Preview *p = (Preview *) data;
  DBG(DBG_proc, "preview_area_resize_handler\n");

  preview_area_resize(p);
  preview_paint_image(p);
 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_update_maximum_output_size(Preview *p)
{
 float xxx = 0.0;
 float yyy = 0.0;

 int paper_orientation = 0;

  if (p->block_update_maximum_output_size_clipping)
  {
    DBG(DBG_info, "preview_update_maximum_output_size: blocked\n");
   return;
  }

  DBG(DBG_proc, "preview_update_maximum_output_size\n");

  p->block_update_maximum_output_size_clipping = TRUE;

  if ( (p->maximum_output_width >= INF) || (p->maximum_output_height >= INF) )
  {
    if (p->selection_maximum.active)
    {
      p->selection_maximum.active = FALSE;
    }
  }
  else /* we have a maximum output size definition */
  {
    p->previous_selection_maximum = p->selection_maximum;
    p->selection_maximum.active = TRUE;

    if (p->paper_orientation & 4) /* center? */
    {
      paper_orientation = p->paper_orientation;
    }
    else /* not in center */
    {
      switch (p->rotation)
      {
        default:
        case 0: /* 0 degree */
          paper_orientation = p->paper_orientation & 3;
         break;

        case 1: /* 90 degree */
          paper_orientation = (1 - p->paper_orientation) & 3;
         break;

        case 2: /* 180 degree */
          paper_orientation = (2 + p->paper_orientation) & 3;
         break;

        case 3: /* 270 degree */
          paper_orientation = (3 - p->paper_orientation) & 3;
         break;
  
        case 4: /* 0 degree, x mirror */
          paper_orientation = (1 - p->paper_orientation) & 3;
         break;
 
        case 5: /* 90 degree, x mirror */
          paper_orientation = p->paper_orientation & 3;
         break;

        case 6: /* 180 degree, x mirror */
          paper_orientation = (3 - p->paper_orientation) & 3;
         break;

        case 7: /* 270 degree, x mirror */
          paper_orientation = (2 + p->paper_orientation) & 3;
         break;
      }
    }

    switch (paper_orientation)
    {
      default:
      case 0: /* top left portrait */
      case 8: /* top left landscape */
        xxx = 0.0;
        yyy = 0.0;
       break;

      case 1: /* top right portrait */
      case 9: /* top right landscape */
        xxx = 1.0;
        yyy = 0.0;
       break;

      case 2: /* bottom right portrait */
      case 10: /* bottom right landscape */
        xxx = 1.0;
        yyy = 1.0;
       break;

      case 3: /* bottom left portrait */
      case 11: /* bottom left landscape */
        xxx = 0.0;
        yyy = 1.0;
       break;

      case 4: /* center portrait */
      case 12: /* center landscape */
        xxx = 0.5;
        yyy = 0.5;
       break;
    }

    p->selection_maximum.coordinate[p->index_xmin] = p->selection.coordinate[p->index_xmin] + xxx * (p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]) - p->maximum_output_width  * xxx;
    p->selection_maximum.coordinate[p->index_ymin] = p->selection.coordinate[p->index_ymin] + yyy * (p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]) - p->maximum_output_height * yyy;
    p->selection_maximum.coordinate[p->index_xmax] = p->selection.coordinate[p->index_xmin] + xxx * (p->selection.coordinate[p->index_xmax] - p->selection.coordinate[p->index_xmin]) + p->maximum_output_width  * (1.0 - xxx);
    p->selection_maximum.coordinate[p->index_ymax] = p->selection.coordinate[p->index_ymin] + yyy * (p->selection.coordinate[p->index_ymax] - p->selection.coordinate[p->index_ymin]) + p->maximum_output_height * (1.0 - yyy);


    if ( (p->selection.coordinate[p->index_xmin] < p->selection_maximum.coordinate[p->index_xmin]) ||
         (p->selection.coordinate[p->index_ymin] < p->selection_maximum.coordinate[p->index_ymin]) ||
         (p->selection.coordinate[p->index_xmax] > p->selection_maximum.coordinate[p->index_xmax]) ||
         (p->selection.coordinate[p->index_ymax] > p->selection_maximum.coordinate[p->index_ymax]) )
    {
     int selection_changed = FALSE;

      if (p->selection.coordinate[p->index_xmin] < p->selection_maximum.coordinate[p->index_xmin])
      {
        p->selection.coordinate[p->index_xmin] = p->selection_maximum.coordinate[p->index_xmin];
        selection_changed = TRUE;
      }

      if (p->selection.coordinate[p->index_ymin] < p->selection_maximum.coordinate[p->index_ymin])
      {
        p->selection.coordinate[p->index_ymin] = p->selection_maximum.coordinate[p->index_ymin];
        selection_changed = TRUE;
      }

      if (p->selection.coordinate[p->index_xmax] > p->selection_maximum.coordinate[p->index_xmax])
      {
        p->selection.coordinate[p->index_xmax] = p->selection_maximum.coordinate[p->index_xmax];
        selection_changed = TRUE;
      }

      if (p->selection.coordinate[p->index_ymax] > p->selection_maximum.coordinate[p->index_ymax])
      {
        p->selection.coordinate[p->index_ymax] = p->selection_maximum.coordinate[p->index_ymax];
        selection_changed = TRUE;
      }

      preview_draw_selection(p);

      if (selection_changed)
      {
        preview_establish_selection(p); 
      }
    }
  }

  p->block_update_maximum_output_size_clipping=FALSE;
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_set_maximum_output_size(Preview *p, float width, float height, int paper_orientation)
{
 /* witdh and height in device units */
  DBG(DBG_proc, "preview_set_maximum_output_size\n");

  p->maximum_output_width  = width;
  p->maximum_output_height = height;
  p->paper_orientation     = paper_orientation;

  preview_update_maximum_output_size(p);
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#define AUTORAISE_ERROR 30
void preview_autoraise_scan_area(Preview *p, int preview_x, int preview_y, float *autoselect_coord)
{
 int x, y;
 int image_x, image_y;
 int offset;
 float average_color_r, average_color_g, average_color_b;
 int count;
 float error;
 int top, bottom, left, right;
 int top_ok, bottom_ok, left_ok, right_ok;
 float xscale, yscale;

  DBG(DBG_proc, "preview_autoraise_scan_area\n");

  preview_transform_coordinate_window_to_image(p, preview_x, preview_y, &image_x, &image_y);

  top_ok    = FALSE;
  bottom_ok = FALSE;
  left_ok   = FALSE;
  right_ok  = FALSE;

  top    = image_y - 5;
  bottom = image_y + 5;
  left   = image_x - 5;
  right  = image_x + 5;


  while (!(top_ok && bottom_ok && left_ok && right_ok))
  {
    /* search top */
    if (!top_ok)
    {
      top--;
    }

    top_ok = TRUE;

    if (top <= 0)
    {
      top = 0;
    }
    else
    {
      average_color_r = average_color_g = average_color_b = 0;
      count = 0;

      for (x = left; x < right; x++)
      {
        offset = 3 * (top * p->image_width + x);
        average_color_r += p->image_data_enh[offset + 0];
        average_color_g += p->image_data_enh[offset + 1];
        average_color_b += p->image_data_enh[offset + 2];
        count++;
      }

      average_color_r /= count;
      average_color_g /= count;
      average_color_b /= count;

      for (x = left; x < right; x++)
      {
        offset = 3 * (top * p->image_width + x);

        error = fabs(p->image_data_enh[offset + 0] - average_color_r) +
                fabs(p->image_data_enh[offset + 1] - average_color_g) +
                fabs(p->image_data_enh[offset + 2] - average_color_b);

        if (error > AUTORAISE_ERROR)
        {
          top_ok = FALSE;
          break;
        }
      }
    }

    /* search bottom */
    if (!bottom_ok)
    {
      bottom++;
    }

    bottom_ok = TRUE;

    if (bottom >= p->image_height-1)
    {
      bottom = p->image_height-1;
    }
    else
    {
      average_color_r = average_color_g = average_color_b = 0;
      count = 0;

      for (x = left; x < right; x++)
      {
        offset = 3 * (bottom * p->image_width + x);
        average_color_r += p->image_data_enh[offset + 0];
        average_color_g += p->image_data_enh[offset + 1];
        average_color_b += p->image_data_enh[offset + 2];
        count++;
      }

      average_color_r /= count;
      average_color_g /= count;
      average_color_b /= count;

      for (x = left; x < right; x++)
      {
        offset = 3 * (bottom * p->image_width + x);

        error = fabs(p->image_data_enh[offset + 0] - average_color_r) +
                fabs(p->image_data_enh[offset + 1] - average_color_g) +
                fabs(p->image_data_enh[offset + 2] - average_color_b);

        if (error > AUTORAISE_ERROR)
        {
          bottom_ok = FALSE;
          break;
        }
      }
    }

    /* search left */
    if (!left_ok)
    {
      left--;
    }

    left_ok = TRUE;

    if (left <= 0)
    {
      left = 0;
    }
    else
    {
      average_color_r = average_color_g = average_color_b = 0;
      count = 0;

      for (y = top; y < bottom; y++)
      {
        offset = 3 * (left + y * p->image_width);
        average_color_r += p->image_data_enh[offset + 0];
        average_color_g += p->image_data_enh[offset + 1];
        average_color_b += p->image_data_enh[offset + 2];
        count++;
      }

      average_color_r /= count;
      average_color_g /= count;
      average_color_b /= count;

      for (y = top; y < bottom; y++)
      {
        offset = 3 * (left + y * p->image_width);

        error = fabs(p->image_data_enh[offset + 0] - average_color_r) +
                fabs(p->image_data_enh[offset + 1] - average_color_g) +
                fabs(p->image_data_enh[offset + 2] - average_color_b);

        if (error > AUTORAISE_ERROR)
        {
          left_ok = FALSE;
          break;
        }
      }
    }

    /* search right */
    if (!right_ok)
    {
      right++;
    }

    right_ok = TRUE;

    if (right >= p->image_width-1)
    {
      right = p->image_width-1;
    }
    else
    {
      average_color_r = average_color_g = average_color_b = 0;
      count = 0;

      for (y = top; y < bottom; y++)
      {
        offset = 3 * (right + y * p->image_width);
        average_color_r += p->image_data_enh[offset + 0];
        average_color_g += p->image_data_enh[offset + 1];
        average_color_b += p->image_data_enh[offset + 2];
        count++;
      }

      average_color_r /= count;
      average_color_g /= count;
      average_color_b /= count;

      for (y = top; y < bottom; y++)
      {
        offset = 3 * (right + y * p->image_width);

        error = fabs(p->image_data_enh[offset + 0] - average_color_r) +
                fabs(p->image_data_enh[offset + 1] - average_color_g) +
                fabs(p->image_data_enh[offset + 2] - average_color_b);

        if (error > AUTORAISE_ERROR)
        {
          right_ok = FALSE;
          break;
        }
      }
    }
  }


  preview_get_scale_device_to_image(p, &xscale, &yscale);

  if (((p->rotation & 3) == 0) || ((p->rotation & 3) == 2)) /* 0 or 180 degree */
  {
    *(autoselect_coord+0) = p->image_surface[0] + left / xscale;
    *(autoselect_coord+2) = p->image_surface[0] + right / xscale;
    *(autoselect_coord+1) = p->image_surface[1] + top / yscale;
    *(autoselect_coord+3) = p->image_surface[1] + bottom / yscale;
  }
  else /* 90 or 270 degree */
  {
    *(autoselect_coord+1) = p->image_surface[1] + left / xscale;
    *(autoselect_coord+3) = p->image_surface[1] + right / xscale;
    *(autoselect_coord+0) = p->image_surface[0] + top / yscale;
    *(autoselect_coord+2) = p->image_surface[0] + bottom / yscale;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_autoselect_scan_area(Preview *p, float *autoselect_coord)
{
 int x, y;
 int offset;
 float color;
 int top, bottom, left, right;
 float xscale, yscale;
 long bright_sum = 0;
 int brightness;
 int background_white;

  DBG(DBG_proc, "preview_autoselect_scan_area\n");

  /* try to find out background color */
  /* add color values at the margins */
  /* and see if it is more black or more white */

  /* upper line */
  for (x = 0; x < p->image_width; x++)
  {
    offset = 3 * x;
    bright_sum += (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
  }

  /* lower line */
  for (x = 0; x < p->image_width; x++)
  {
    offset = 3 * ( (p->image_height-1) * p->image_width + x);
    bright_sum += (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
  }

  /* left line */
  for (y = 0; y < p->image_height; y++)
  {
    offset = 3 * y * p->image_width;
    bright_sum += (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
  }

  /* right line */
  for (y = 0; y < p->image_height; y++)
  {
    offset = 3 * (y * p->image_width + p->image_width - 1);
    bright_sum += (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
  }

  brightness = bright_sum / (2 * (p->image_width + p->image_height) );
  DBG(DBG_info, "preview_autoselect_scan_area: average margin brightness is %d\n", brightness);

  if ( brightness > 128 )
  {
    DBG(DBG_info, "preview_autoselect_scan_area: background is white\n");
    background_white = 1;
  }
  else
  {
    DBG(DBG_info, "preview_autoselect_scan_area: background is black\n");
    background_white = 0;
  }
  

  /* search top */
  top = 0;
  for (y = 0; y < p->image_height; y++)
  {
    for (x = 0; x < p->image_width; x++)
    {
      offset = 3 * (y * p->image_width + x);
      color = (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
      if (background_white)
      {
        if (color < 200)
        {
          top = y;
          break;
        }
      }

      else if (color > 55 )
      {
        top = y;
        break;
      }
    }
    if (top)
    {
      break;
    }
  }


  /* search bottom */
  bottom = 0;
  for (y = p->image_height-1; y > top; y--)
  {
    for (x = 0; x < p->image_width; x++)
    {
      offset = 3 * (y * p->image_width + x);
      color = (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
      if (background_white)
      {
        if (color < 200)
        {
          bottom = y;
          break;
        }
      }
      else if (color > 55 )
      {
        bottom = y;
        break;
      }
    }
    if (bottom)
    {
      break;
    }
  }


  /* search left */
  left = 0;
  for (x = 0; x < p->image_width; x++)
  {
    for (y = 0; y < p->image_height; y++)
    {
      offset = 3 * (y * p->image_width + x);
      color = (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
      if (background_white)
      {
        if (color < 200)
        {
          left = x;
          break;
        }
      }
      else if (color > 55 )
      {
        left = x;
        break;
      }
    }
    if (left)
    {
      break;
    }
  }


  /* search right */
  right = 0;
  for (x = p->image_width-1; x > left; x--)
  {
    for (y = 0; y < p->image_height; y++)
    {
      offset = 3 * (y * p->image_width + x);
      color = (p->image_data_enh[offset + 0] + p->image_data_enh[offset + 1] + p->image_data_enh[offset + 2]) / 3.0; 
      if (background_white)
      {
        if (color < 200)
        {
          right = x;
          break;
        }
      }
      else if (color > 55 )
      {
        right = x;
        break;
      }
    }
    if (right)
    {
      break;
    }
  }

  if ( (top >= bottom) || (right <= left) ) /* empty selection: use complete image */
  {
    DBG(DBG_info, "autoselect_scan_area: empty selection: using complete area\n");
    top    = 0;
    bottom = p->image_height -1;
    left   = 0;
    right  = p->image_width -1;
  }

  preview_get_scale_device_to_image(p, &xscale, &yscale);

  if (((p->rotation & 3) == 0) || ((p->rotation & 3) == 2)) /* 0 or 180 degree */
  {
    *(autoselect_coord+0) = p->image_surface[0] + left / xscale;
    *(autoselect_coord+2) = p->image_surface[0] + right / xscale;
    *(autoselect_coord+1) = p->image_surface[1] + top / yscale;
    *(autoselect_coord+3) = p->image_surface[1] + bottom / yscale;
  }
  else /* 90 or 270 degree */
  {
    *(autoselect_coord+1) = p->image_surface[1] + left / xscale;
    *(autoselect_coord+3) = p->image_surface[1] + right / xscale;
    *(autoselect_coord+0) = p->image_surface[0] + top / yscale;
    *(autoselect_coord+2) = p->image_surface[0] + bottom / yscale;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_display_valid(Preview *p)
{
  DBG(DBG_proc, "preview_display_valid\n");

  if (p->scanning)/* we are just scanning the preview */
  {
    DBG(DBG_info, "preview scanning\n");

    gtk_widget_show(p->scanning_pixmap);
    gtk_widget_hide(p->incomplete_pixmap);
    gtk_widget_hide(p->valid_pixmap);
    gtk_widget_hide(p->invalid_pixmap);
  }
  else if ((xsane.medium_changed) || (xsane.xsane_channels != p->preview_channels) || (p->invalid) ) /* preview is not valid */
  {
    DBG(DBG_info, "preview not vaild\n");

    gtk_widget_show(p->invalid_pixmap);
    gtk_widget_hide(p->scanning_pixmap);
    gtk_widget_hide(p->incomplete_pixmap);
    gtk_widget_hide(p->valid_pixmap);
  }
  else if (p->scan_incomplete)/* preview scan has been cancled */
  {
    DBG(DBG_info, "preview incomplete\n");

    gtk_widget_show(p->incomplete_pixmap);
    gtk_widget_hide(p->scanning_pixmap);
    gtk_widget_hide(p->valid_pixmap);
    gtk_widget_hide(p->invalid_pixmap);
  }
  else /* preview is valid */
  {
    DBG(DBG_info, "preview vaild\n");

    gtk_widget_show(p->valid_pixmap);
    gtk_widget_hide(p->scanning_pixmap);
    gtk_widget_hide(p->incomplete_pixmap);
    gtk_widget_hide(p->invalid_pixmap);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */
