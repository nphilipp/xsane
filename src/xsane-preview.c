/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preview.c

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

*/

/* ---------------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
/* #include <sys/param.h> */
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
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

#define PRESET_AREA_ITEMS 10
typedef struct
{
  char *name;
  float width;
  float height;
} Preset_area;

static const Preset_area preset_area[] =
{
 { "full size",	INF,	INF },
 { "DIN A3P",	296.98,	420.0 },
 { "DIN A4P",	210.0,	296.98 },
 { "DIN A4L",	296.98,	210.0 },
 { "DIN A5P",	148.5,	210.0 },
 { "DIN A5L",	210.0,	148.5 },
 { "9x13 cm",	90.0,	130.0 },
 { "13x9 cm",	130.0,	90.0 },
 { "legal",	215.9,	355.6 },
 { "letter",	215.9,	279.4 }
};

/* ---------------------------------------------------------------------------------------------------------------------- */

static SANE_Int *preview_gamma_data_red   = 0;
static SANE_Int *preview_gamma_data_green = 0;
static SANE_Int *preview_gamma_data_blue  = 0;

static SANE_Int *histogram_gamma_data_red   = 0;
static SANE_Int *histogram_gamma_data_green = 0;
static SANE_Int *histogram_gamma_data_blue  = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations */
static void preview_rotate_dsurface_to_psurface(int rotation, float dsurface[4], float *psurface);
static void preview_rotate_psurface_to_dsurface(int rotation, float psurface[4], float *dsurface);
static void preview_transform_coordinate_preview_to_window(Preview *p, float dcoordinate[4], float *pcoordinate);
static void preview_order_selection(Preview *p);
static void preview_bound_selection(Preview *p);
static void preview_draw_rect(Preview *p, GdkWindow *win, GdkGC *gc, float coord[4]);
static void preview_draw_selection(Preview *p);
static void preview_update_selection(Preview *p);
static void preview_establish_selection(Preview *p);
/* static void preview_update_batch_selection(Preview *p); */
static void preview_get_scale_device_to_image(Preview *p, float *xscalep, float *yscalep);
static void preview_get_scale_device_to_preview(Preview *p, float *xscalep, float *yscalep);
static void preview_get_scale_preview_to_image(Preview *p, float *xscalep, float *yscalep);
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
static void preview_scan_done(Preview *p);
static void preview_scan_start(Preview *p);
static int preview_make_image_path(Preview *p, size_t filename_size, char *filename, int level);
static void preview_restore_image(Preview *p);
static gint preview_expose_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_hold_event_handler(gpointer data);
static gint preview_motion_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_button_press_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static gint preview_button_release_event_handler(GtkWidget *window, GdkEvent *event, gpointer data);
static void preview_start_button_clicked(GtkWidget *widget, gpointer data);
static void preview_cancel_button_clicked(GtkWidget *widget, gpointer data);
static void preview_area_correct(Preview *p);
static void preview_save_image(Preview *p);
static void preview_zoom_not(GtkWidget *window, gpointer data);
static void preview_zoom_out(GtkWidget *window, gpointer data);
static void preview_zoom_in(GtkWidget *window, gpointer data);
static void preview_zoom_undo(GtkWidget *window, gpointer data);
static void preview_get_color(Preview *p, int x, int y, int *red, int *green, int *blue);
static void preview_pipette_white(GtkWidget *window, gpointer data);
static void preview_pipette_gray(GtkWidget *window, gpointer data);
static void preview_pipette_black(GtkWidget *window, gpointer data);
void preview_select_full_preview_area(Preview *p);
static void preview_full_preview_area_callback(GtkWidget *widget, gpointer call_data);
static void preview_preset_area_callback(GtkWidget *widget, gpointer call_data);
static void preview_rotation_callback(GtkWidget *widget, gpointer call_data);

void preview_do_gamma_correction(Preview *p);
void preview_calculate_histogram(Preview *p,
  SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue,
  SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue);
void preview_gamma_correction(Preview *p,
                              SANE_Int *gamma_red, SANE_Int *gamma_green, SANE_Int *gamma_blue,
                              SANE_Int *gamma_red_hist, SANE_Int *gamma_green_hist, SANE_Int *gamma_blue_hist);
void preview_area_resize(Preview *p);
gint preview_area_resize_handler(GtkWidget *widget, GdkEvent *event, gpointer data);
void preview_update_maximum_output_size(Preview *p);
void preview_set_maximum_output_size(Preview *p, float width, float height);

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotate_dsurface_to_psurface(int rotation, float dsurface[4], float *psurface)
{
  DBG(DBG_proc, "preview_rotate_dsurface_to_psurface(rotation = %d)\n", rotation);

  switch (rotation)
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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotate_psurface_to_dsurface(int rotation, float psurface[4], float *dsurface)
{
  DBG(DBG_proc, "preview_rotate_psurface_to_dsurface(rotation = %d)\n", rotation);

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
      *(dsurface+0) = psurface[3];
      *(dsurface+1) = psurface[0];
      *(dsurface+2) = psurface[1];
      *(dsurface+3) = psurface[2];
     break;

    case 2: /* 180 degree */
      *(dsurface+0) = psurface[2];
      *(dsurface+1) = psurface[3];
      *(dsurface+2) = psurface[0];
      *(dsurface+3) = psurface[1];
     break;

    case 3: /* 270 degree */
      *(dsurface+0) = psurface[1];
      *(dsurface+1) = psurface[2];
      *(dsurface+2) = psurface[3];
      *(dsurface+3) = psurface[0];
     break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_transform_coordinate_preview_to_window(Preview *p, float dcoordinate[4], float *pcoordinate)
{
 float minx, maxx, miny, maxy;
 float xscale, yscale;

  DBG(DBG_proc, "preview_transform_coordinate_preview_to_window\n");

  preview_get_scale_device_to_preview(p, &xscale, &yscale);

  minx = dcoordinate[0];
  miny = dcoordinate[1];
  maxx = dcoordinate[2];
  maxy = dcoordinate[3];

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
      *(pcoordinate+0) = xscale * (minx - p->surface[0]);
      *(pcoordinate+1) = yscale * (miny - p->surface[1]);
      *(pcoordinate+2) = xscale * (maxx - p->surface[0]);
      *(pcoordinate+3) = yscale * (maxy - p->surface[1]);
     break;

    case 1: /* 90 degree */
      *(pcoordinate+0) = xscale * (minx - p->surface[0]);
      *(pcoordinate+1) = yscale * (miny - p->surface[1]);
      *(pcoordinate+2) = xscale * (maxx - p->surface[0]);
      *(pcoordinate+3) = yscale * (maxy - p->surface[1]);
     break;

    case 2: /* 180 degree */
      *(pcoordinate+0) = xscale * (p->surface[0] - maxx);
      *(pcoordinate+1) = yscale * (p->surface[1] - maxy);
      *(pcoordinate+2) = xscale * (p->surface[0] - minx);
      *(pcoordinate+3) = yscale * (p->surface[1] - miny);
     break;

    case 3: /* 270 degree */
      *(pcoordinate+0) = xscale * (minx - p->surface[0]);
      *(pcoordinate+1) = yscale * (miny - p->surface[1]);
      *(pcoordinate+2) = xscale * (maxx - p->surface[0]);
      *(pcoordinate+3) = yscale * (maxy - p->surface[1]);
     break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_transform_coordinate_window_to_preview(Preview *p, float winx, float winy, float *previewx, float *previewy)
{
 float xscale, yscale;

  DBG(DBG_proc, "preview_transform_coordinate_window_to_preview\n");

  preview_get_scale_device_to_preview(p, &xscale, &yscale);

  switch (p->rotation)
  {
    case 0: /* 0 degree */
    default:
      *previewx = p->surface[0] + winx / xscale;
      *previewy = p->surface[1] + winy / yscale;
     break;

    case 1: /* 90 degree */
      *previewx = p->surface[0] + winx / xscale;
      *previewy = p->surface[1] + winy / yscale;
     break;

    case 2: /* 180 degree */
      *previewx = p->surface[0] + winx / xscale;
      *previewy = p->surface[1] + winy / yscale;
     break;

    case 3: /* 270 degree */
      *previewx = p->surface[0] + winx / xscale;
      *previewy = p->surface[1] + winy / yscale;
     break;
  }
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
    if (p->selection.coordinate[0] > p->selection.coordinate[2])
    {
      tmp_coordinate = p->selection.coordinate[0];
      p->selection.coordinate[0] = p->selection.coordinate[2];
      p->selection.coordinate[2] = tmp_coordinate;

      p->selection_xedge = (p->selection_xedge + 2) & 3;
    }

    if (p->selection.coordinate[1] > p->selection.coordinate[3])
    {
      tmp_coordinate = p->selection.coordinate[1];
      p->selection.coordinate[1] = p->selection.coordinate[3];
      p->selection.coordinate[3] = tmp_coordinate;

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
    if (p->selection.coordinate[0] < p->scanner_surface[0])
    {
      p->selection.coordinate[0] = p->scanner_surface[0];
    }

    if (p->selection.coordinate[1] < p->scanner_surface[1])
    {
      p->selection.coordinate[1] =  p->scanner_surface[1];
    }

    if (p->selection.coordinate[2] > p->scanner_surface[2])
    {
      p->selection.coordinate[2] = p->scanner_surface[2];
    }

    if (p->selection.coordinate[3] > p->scanner_surface[3])
    {
      p->selection.coordinate[3] = p->scanner_surface[3];
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_draw_rect(Preview *p, GdkWindow *win, GdkGC *gc, float coordinate[4])
{
 float pcoord[4];

  DBG(DBG_proc, "preview_draw_rect\n");

  preview_transform_coordinate_preview_to_window(p, coordinate, pcoord);
  gdk_draw_rectangle(win, gc, FALSE, pcoord[0], pcoord[1], pcoord[2]-pcoord[0] + 1, pcoord[3] - pcoord[1] + 1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_draw_selection(Preview *p)
{
  DBG(DBG_proc, "preview_draw_selection\n");

  if (!p->gc_selection) /* window isn't mapped yet */
  {
    return;
  }

  if (p->show_selection == FALSE)
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
    else /* backend does not use scanarea options */
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

  preview_rotate_dsurface_to_psurface(p->rotation, coord, p->selection.coordinate);

#if 0
  for (i = 0; i < 2; ++i)
  {
    if (p->selection.coordinate[i + 2] < p->selection.coordinate[i])
    {
      p->selection.coordinate[i + 2] = p->selection.coordinate[i];
    }
  }
#endif

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

  preview_rotate_psurface_to_dsurface(p->rotation, p->selection.coordinate, coord);

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

  DBG(DBG_proc, "preview_get_scale_device_to_image\n");

  device_width  = fabs(p->image_surface[2] - p->image_surface[0]);
  device_height = fabs(p->image_surface[3] - p->image_surface[1]);

  if ( (device_width >0) && (device_width < INF) )
  {
    xscale = p->image_width / device_width;
  }

  if ( (device_height >0) && (device_height < INF) )
  {
    yscale = p->image_height / device_height;
  }

  if (p->surface_unit == SANE_UNIT_PIXEL)
  {
    if (xscale > yscale)
    {
      yscale = xscale;
    }
    else
    {
      xscale = yscale;
    }
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info2, "preview_get_scale_device_to_image: xscale = %f\n", xscale);
  DBG(DBG_info2, "preview_get_scale_device_to_image: yscale = %f\n", yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_scale_device_to_preview(Preview *p, float *xscalep, float *yscalep)
{
 float device_width, device_height;
 float xscale = 1.0;
 float yscale = 1.0;

  DBG(DBG_proc, "preview_get_scale_device_to_preview\n");

  device_width  = fabs(p->image_surface[2] - p->image_surface[0]);
  device_height = fabs(p->image_surface[3] - p->image_surface[1]);

  if ( (device_width >0) && (device_width < INF) )
  {
    xscale = p->preview_width / device_width;
  }

  if ( (device_height >0) && (device_height < INF) )
  {
    yscale = p->preview_height / device_height;
  }

  if (p->surface_unit == SANE_UNIT_PIXEL)
  {
    if (xscale > yscale)
    {
      yscale = xscale;
    }
    else
    {
      xscale = yscale;
    }
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info2, "preview_get_scale_device_to_preview: xscale = %f\n", xscale);
  DBG(DBG_info2, "preview_get_scale_device_to_preview: yscale = %f\n", yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_get_scale_preview_to_image(Preview *p, float *xscalep, float *yscalep)
{
 float xscale = 1.0;
 float yscale = 1.0;

  DBG(DBG_proc, "preview_get_scale_preview_to_image\n");

  if (p->image_width > 0)
  {
    xscale = p->image_width / (float) p->preview_width;
  }

  if (p->image_height > 0)
  {
    yscale = p->image_height / (float) p->preview_height;
  }

  if (p->surface_unit == SANE_UNIT_PIXEL)
  {
    if (xscale > yscale)
    {
      yscale = xscale;
    }
    else
    {
      xscale = yscale;
    }
  }

  *xscalep = xscale;
  *yscalep = yscale;

  DBG(DBG_info2, "preview_get_scale_preview_to_image: xscale = %f\n", xscale);
  DBG(DBG_info2, "preview_get_scale_preview_to_image: yscale = %f\n", yscale);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_paint_image(Preview *p)
{
 float xscale, yscale, src_x, src_y;
 int dst_x, dst_y, height, x, y, old_y, src_offset;

  DBG(DBG_proc, "preview_paint_image\n");

  if (!p->image_data_enh)
  {
    return; /* no image data */
  }

  memset(p->preview_row, 0x80, 3 * p->preview_window_width);

  old_y = -1;
  height = 0;

  if (p->calibration)
  {
    xscale=1.0;
    yscale=1.0;
  }
  else
  {
    preview_get_scale_preview_to_image(p, &xscale, &yscale);
  }

  switch (p->rotation)
  {
    case 0: /* do not rotate - 0 degree */
    default:

      /* don't draw last line unless it's complete: */
      height = p->image_y;

      if (p->image_x == 0 && height < p->image_height)
      {
        ++height;
      }

      src_y = 0.0;

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= height)
        {
          break;
        }
        src_offset = y * 3 * p->image_width;

        if (old_y != y) /* create new line ? - not necessary if the same line is used several times */
        {
          old_y = y;
          src_x = 0.0;

          for (dst_x = 0; dst_x < p->preview_width; ++dst_x)
          {
            x = (int) (src_x + 0.5);
            if (x >= p->image_width)
            {
              break;
            }

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + 3*x + 0];
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + 3*x + 1];
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + 3*x + 2];
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


    case 1: /* 90 degree */
      /* because we run in x direction we have to draw all rows all the time */

      src_y = 0.0;

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= p->image_width)
        {
          break;
        }
        src_offset = y * 3;

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

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + 3*x*p->image_width + 0];
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + 3*x*p->image_width + 1];
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + 3*x*p->image_width + 2];
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
      height = p->image_y;

      if (p->image_x == 0 && height < p->image_height)
      {
        ++height;
      }

      src_y = 0;

      for (dst_y = p->preview_height-1; dst_y >0; --dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y <= height)
        {
//          break;
        }

        src_offset = y * 3 * p->image_width;

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

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + 3*x + 0];
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + 3*x + 1];
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + 3*x + 2];
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

      for (dst_y = 0; dst_y < p->preview_height; ++dst_y)
      {
        y = (int) (src_y + 0.5);
        if (y >= p->image_width)
        {
          break;
        }
        src_offset = (p->image_width - y - 1) * 3;

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

            p->preview_row[3*dst_x + 0] = p->image_data_enh[src_offset + 3*x*p->image_width + 0];
            p->preview_row[3*dst_x + 1] = p->image_data_enh[src_offset + 3*x*p->image_width + 1];
            p->preview_row[3*dst_x + 2] = p->image_data_enh[src_offset + 3*x*p->image_width + 2];
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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_display_partial_image(Preview *p)
{
  DBG(DBG_proc, "preview_display_partial_image\n");

  p->previous_selection.active = FALSE; /* ok, old selections are overpainted */
  p->previous_selection_maximum.active = FALSE;

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
    p->image_data_raw = realloc(p->image_data_raw, 3 * p->image_width * p->image_height);
    p->image_data_enh = realloc(p->image_data_enh, 3 * p->image_width * p->image_height);
    assert(p->image_data_raw);
    assert(p->image_data_enh);
  }

  memcpy(p->image_data_raw, p->image_data_enh, 3 * p->image_width * p->image_height); /* store scanned preview ind raw */

  preview_do_gamma_correction(p);
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
    char buf[256];
    opt = xsane_get_option_descriptor(dev, option);
    snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_SET_OPTION, opt->name, XSANE_STRSTATUS(status));
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
    word = SANE_FIX(value) + 0.5;
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

static int preview_increment_image_y(Preview *p)
{
 size_t extra_size, offset;
 char buf[256];

  DBG(DBG_proc, "preview_increment_image_y\n");

  p->image_x = 0;
  ++p->image_y;
  if (p->params.lines <= 0 && p->image_y >= p->image_height)
  {
    offset = 3 * p->image_width*p->image_height;
    extra_size = 3 * 32 * p->image_width;
    p->image_height += 32;
    p->image_data_raw = realloc(p->image_data_raw, offset + extra_size);
    p->image_data_enh = realloc(p->image_data_enh, offset + extra_size);
    if ( (!p->image_data_enh) || (!p->image_data_raw) )
    {
      snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_ALLOCATE_IMAGE, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
      preview_scan_done(p);
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
 u_char buf[8192];
 guint16 *buf16 = (guint16 *) buf;
 SANE_Handle dev;
 SANE_Int len;
 int i, j;
 int offset = 0;
 char last = 0;

  DBG(DBG_proc, "preview_read_image_data\n");

  dev = xsane.dev;
  while (1)
  {
    if ((p->params.depth == 1) || (p->params.depth == 8))
    {
      status = sane_read(dev, buf, sizeof(buf), &len);
    }
    else if (p->params.depth == 16)
    {
      if (offset)
      {
        buf16[0] = last; /* ATTENTION: that is wrong! */
        status = sane_read(dev, ((SANE_Byte *) buf16) + 1, sizeof(buf16) - 1, &len);
      }
      else
      {
        status = sane_read(dev, (SANE_Byte *) buf16, sizeof(buf16), &len);
      }

      if (len % 2) /* odd number of bytes */
      {
        len--;
        last = buf16[len];
        offset = 1;
      }
      else /* even number of bytes */
      {
        offset = 0;
      }
    }
    else
    {
      goto bad_depth;
    }


    if (status != SANE_STATUS_GOOD)
    {
      if (status == SANE_STATUS_EOF)
      {
        if (p->params.last_frame) /* got all preview image data */
        {
          preview_update_surface(p, 0);
          preview_display_image(p); /* must come after preview_update_surface */
          preview_save_image(p);    /* save preview image */
          preview_scan_done(p);     /* scan is done */
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
          break;
        }
      }
      else
      {
        snprintf(buf, sizeof(buf), "%s %s.", ERR_DURING_READ, XSANE_STRSTATUS(status));
        xsane_back_gtk_error(buf, TRUE);
      }
      preview_scan_done(p);
      return;
    }

    if (!len)
    {
      break;			/* out of data for now */
    }

    switch (p->params.format)
    {
      case SANE_FRAME_RGB:
        switch (p->params.depth)
        {
          case 8:
            for (i = 0; i < len; ++i)
            {
              p->image_data_enh[p->image_offset++] = buf[i];
              if (p->image_offset%3 == 0)
              {
                if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                {
                  return;
                }
              }
            }
            break;

          case 16:
            for (i = 0; i < len/2; ++i)
            {
              p->image_data_enh[p->image_offset++] = (u_char) (buf16[i]/256);
              if (p->image_offset%3 == 0)
              {
                if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                {
                  return;
                }
              }
            }
            break;

          default:
            goto bad_depth;
        }
       break;

      case SANE_FRAME_GRAY:
        switch (p->params.depth)
        {
          case 1:
            for (i = 0; i < len; ++i)
            {
              u_char mask = buf[i];

              for (j = 7; j >= 0; --j)
              {
                u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
                p->image_data_enh[p->image_offset++] = gl;
                p->image_data_enh[p->image_offset++] = gl;
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
              u_char gray = buf[i];
              p->image_data_enh[p->image_offset++] = gray;
              p->image_data_enh[p->image_offset++] = gray;
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
              u_char gray = buf16[i]/256;
              p->image_data_enh[p->image_offset++] = gray;
              p->image_data_enh[p->image_offset++] = gray;
              p->image_data_enh[p->image_offset++] = gray;
              if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
	      {
                return;
              }
            }
           break;

          default:
            goto bad_depth;
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
                  u_char mask = buf[i];

                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;
                    p->image_data_enh[p->image_offset++] = gl;
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
                  p->image_data_enh[p->image_offset] = buf[i];
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
                  p->image_data_enh[p->image_offset] = (u_char) (buf16[i]/256);
                  p->image_offset += 3;
                  if (++p->image_x >= p->image_width && preview_increment_image_y(p) < 0)
                  {
                    return;
                  }
                }
               break;

              default:
                goto bad_depth;
            }
           break;

          default:
            fprintf(stderr, "preview_read_image_data: %s %d\n", ERR_BAD_FRAME_FORMAT, p->params.format);
            preview_scan_done(p);
            return;
    }

    if (p->input_tag < 0)
    {
      preview_display_maybe(p);
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }
  }
  preview_display_maybe(p);
  return;

bad_depth:
  snprintf(buf, sizeof(buf), "%s %d.", ERR_PREVIEW_BAD_DEPTH, p->params.depth);
  xsane_back_gtk_error(buf, TRUE);
  preview_scan_done(p);
  return;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_scan_done(Preview *p)
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

  preview_update_surface(p, 0); /* if surface was not defined it's necessary to redefine it now */

  preview_update_selection(p);
  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_get_memory(Preview *p)
{
 char buf[256];

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

  p->image_data_enh = malloc(3 * p->image_width * (p->image_height));
  p->image_data_raw = malloc(3 * p->image_width * (p->image_height));
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

  memset(p->image_data_enh, 0xff, 3*p->image_width*p->image_height); /* clean memory */

  return 0; /* ok */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_scan_start(Preview *p)
{
 SANE_Handle dev = xsane.dev;
 SANE_Status status;
 char buf[256];
 int fd, y, i;
 int gamma_gray_size  = 256; /* set this values to image depth for more than 8bpp input support!!! */
 int gamma_red_size   = 256;
 int gamma_green_size = 256;
 int gamma_blue_size  = 256;
 int gamma_gray_max   = 255; /* set this to to image depth for more than 8bpp output support */
 int gamma_red_max    = 255;
 int gamma_green_max  = 255;
 int gamma_blue_max   = 255;

  DBG(DBG_proc, "preview_scan_start\n");

  for (i=0; i<4; i++)
  {
    p->image_surface[i] = p->surface[i];
  }

  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh);
  gtk_widget_set_sensitive(p->cancel, TRUE);
  xsane_set_sensitivity(FALSE);
  /* clear old preview: */
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
      xsane_create_gamma_curve(gamma_data, 0, 1.0, 0.0, 0.0, gamma_gray_size, gamma_gray_max);
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

      xsane_create_gamma_curve(gamma_data_red,   0, 1.0, 0.0, 0.0, gamma_red_size,   gamma_red_max);
      xsane_create_gamma_curve(gamma_data_green, 0, 1.0, 0.0, 0.0, gamma_green_size, gamma_green_max);
      xsane_create_gamma_curve(gamma_data_blue,  0, 1.0, 0.0, 0.0, gamma_blue_size,  gamma_blue_max);

      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_r, gamma_data_red);
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_g, gamma_data_green);
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_b, gamma_data_blue);

      free(gamma_data_red);
      free(gamma_data_green);
      free(gamma_data_blue);
    }
  }

  status = sane_start(dev);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_START_SCANNER, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
    preview_scan_done(p);
    return;
  }

  status = sane_get_parameters(dev, &p->params);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_GET_PARAMS, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
    preview_scan_done(p);
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
      preview_scan_done(p); /* error */
      return;
    }
  }

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* THIS IS A BIT STRANGE HERE */
  p->selection.active = FALSE;
  p->previous_selection_maximum.active = FALSE;
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

  p->scanning = TRUE;

#ifndef BUGGY_GDK_INPUT_EXCEPTION
  /* for unix */
  if (sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD && sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD)
  {
    p->input_tag = gdk_input_add(fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, preview_read_image_data, p);
  }
  else
#else
  /* for win32 */
  sane_set_io_mode(dev, SANE_FALSE);
#endif
  {
    preview_read_image_data(p, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_make_image_path(Preview *p, size_t filename_size, char *filename, int level)
{
 char buf[256];

  DBG(DBG_proc, "preview_make_image_path\n");

  snprintf(buf, sizeof(buf), "preview-level-%d-", level);
  return xsane_back_gtk_make_path(filename_size, filename, 0, 0, buf, xsane.dev_name, ".ppm", XSANE_PATH_TMP);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int preview_restore_image_from_file(Preview *p, FILE *in, int min_quality)
{
 u_int psurface_type, psurface_unit;
 int image_width, image_height;
 int xoffset, yoffset, width, height;
 int bit_depth;
 int quality = 0;
 int y;
 float psurface[4];
 float dsurface[4];
 size_t nread;
 char *imagep;
 char buf[255];

  DBG(DBG_proc, "preview_restore_image_from_file\n");

  if (!in)
  {
    return min_quality;
  }

  /* See whether there is a saved preview and load it if present: */
  if (fscanf(in, "P6\n# surface: %g %g %g %g %u %u\n%d %d\n%d",
	      psurface + 0, psurface + 1, psurface + 2, psurface + 3,
	      &psurface_type, &psurface_unit,
	      &image_width, &image_height,
              &bit_depth) != 9)
  {
    return min_quality;
  }

  fgets(buf, sizeof(buf), in); /* skip newline character. this made a lot of problems in the past, so I skip it this way */


  if (min_quality >= 0) /* read real preview */
  {
    if ((psurface_type != p->surface_type) || (psurface_unit != p->surface_unit))
    {
      return min_quality;
    }

    preview_rotate_psurface_to_dsurface(p->rotation, p->surface, dsurface);

    xoffset = (dsurface[0] - psurface[0])/(psurface[2] - psurface[0]) * image_width;
    yoffset = (dsurface[1] - psurface[1])/(psurface[3] - psurface[1]) * image_height;
    width   = (dsurface[2] - dsurface[0])/(psurface[2] - psurface[0]) * image_width;
    height  = (dsurface[3] - dsurface[1])/(psurface[3] - psurface[1]) * image_height;
    quality = width;

    if ((xoffset < 0) || (yoffset < 0) ||
        (xoffset+width > image_width) || (yoffset+height > image_height) ||
        (width == 0) || (height == 0))
    {
      return min_quality;
    }

    if (quality < min_quality)
    {
      return min_quality;
    }
  }
  else /* read startimage */
  {
    xoffset = 0;
    yoffset = 0;
    width   = image_width;
    height  = image_height;
  }

  p->params.depth   = 8;
  p->image_width    = width;
  p->image_height   = height;

  if (preview_get_memory(p))
  {
    return min_quality; /* error allocating memory */
  }

  fseek(in, yoffset * 3 * image_width, SEEK_CUR); /* skip unused lines */

  imagep = p->image_data_enh;

  for (y = yoffset; y < yoffset + height; y++)
  {
    fseek(in, xoffset * 3, SEEK_CUR); /* skip unused pixel left of area */

    nread = fread(imagep, 3, width, in);
    imagep += width * 3;

    fseek(in, (image_width - width - xoffset) * 3, SEEK_CUR); /* skip unused pixel right of area */
  }

  p->image_x = width;
  p->image_y = height;

  p->image_surface[0] = p->surface[0];
  p->image_surface[1] = p->surface[1];
  p->image_surface[2] = p->surface[2];
  p->image_surface[3] = p->surface[3];

 return quality;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_restore_image(Preview *p)
{
 FILE *in;
 int quality = 0;
 int level;

  DBG(DBG_proc, "preview_restore_image\n");

  if (p->calibration)
  {
   char filename[PATH_MAX];

    DBG(DBG_proc, "calibration mode\n");
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-calibration", 0, ".pnm", XSANE_PATH_SYSTEM);
    in = fopen(filename, "rb"); /* read binary (b for win32) */
    if (in)
    {
      quality = preview_restore_image_from_file(p, in, -1);
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
          quality = preview_restore_image_from_file(p, in, quality);
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
        quality = preview_restore_image_from_file(p, in, -1);
        fclose(in);
      }
    }
  }

  memcpy(p->image_data_raw, p->image_data_enh, 3 * p->image_width * p->image_height);

/* the following commands may be removed because they are done because a event is emmited */
  preview_do_gamma_correction(p);
  xsane_update_histogram();
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint preview_hold_event_handler(gpointer data)
{
 Preview *p = data;

  DBG(DBG_proc, "preview_hold_event_handler\n");

  gtk_timeout_remove(p->hold_timer);
  p->hold_timer = 0;

  preview_draw_selection(p);
  preview_establish_selection(p);

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
  preview_transform_coordinate_preview_to_window(p, p->selection.coordinate, preview_selection);

  /* cursor-prosition (window) -> preview coordinate (device) */
  preview_transform_coordinate_window_to_preview(p, event->button.x, event->button.y, &preview_x, &preview_y);

  preview_get_scale_device_to_preview(p, &xscale, &yscale);

  if (!p->scanning)
  {
    if (p->hold_timer) /* hold timer active? then remove it, we had a motion */
    {
      gtk_timeout_remove(p->hold_timer);
      p->hold_timer = 0;
    }

    switch (((GdkEventMotion *)event)->state &
            GDK_Num_Lock & GDK_Caps_Lock & GDK_Shift_Lock & GDK_Scroll_Lock) /* mask all Locks */
    {
      case 256: /* left button */

        DBG(DBG_info2, "left button\n");

        if ( (p->selection_drag) || (p->selection_drag_edge) )
        {
          p->selection.active = TRUE;
          p->selection.coordinate[p->selection_xedge] = preview_x;
          p->selection.coordinate[p->selection_yedge] = preview_y;

          preview_order_selection(p);
          preview_bound_selection(p);

          if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
          {
            preview_draw_selection(p);
            preview_establish_selection(p);
          }
          else if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
          {
            /* call preview_hold_event_hanlder if mouse is not moved for ??? ms */
            p->hold_timer = gtk_timeout_add(XSANE_HOLD_TIME, preview_hold_event_handler, (gpointer *) p);
            preview_update_maximum_output_size(p);
            preview_draw_selection(p);
          }
          else /* discontinous */
          {
            preview_update_maximum_output_size(p);
            preview_draw_selection(p); /* only draw selection, do not update backend geometry options */
          }
        }

        cursornr = p->cursornr;

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

        if (cursornr != p->cursornr)
        {
          cursor = gdk_cursor_new(cursornr);	/* set curosr */
          gdk_window_set_cursor(p->window->window, cursor);
          gdk_cursor_destroy(cursor);
          p->cursornr = cursornr;
        }
       break;

      case 512: /* middle button */
      case 1024: /* right button */
        DBG(DBG_info2, "middle or right button\n");

        if (p->selection_drag)
        {
         double dx, dy;

          dx = (p->selection_xpos - event->motion.x) / xscale;
          dy = (p->selection_ypos - event->motion.y) / yscale;

          p->selection_xpos = event->motion.x;
          p->selection_ypos = event->motion.y;

          if (dx > p->selection.coordinate[0] - p->scanner_surface[0]) 
          {
            dx = p->selection.coordinate[0] - p->scanner_surface[0];
          }

          if (dy > p->selection.coordinate[1] - p->scanner_surface[1])
          {
            dy = p->selection.coordinate[1] - p->scanner_surface[1];
          }

          if (dx < p->selection.coordinate[2] - p->scanner_surface[2])
          { 
            dx = p->selection.coordinate[2] - p->scanner_surface[2];
          }

          if (dy < p->selection.coordinate[3] - p->scanner_surface[3])
          {
            dy = p->selection.coordinate[3] - p->scanner_surface[3];
          }

          p->selection.active = TRUE;
          p->selection.coordinate[0] -= dx;
          p->selection.coordinate[1] -= dy;
          p->selection.coordinate[2] -= dx;
          p->selection.coordinate[3] -= dy;

          if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
          {
            preview_draw_selection(p);
            preview_establish_selection(p);
          }
          else if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
          {
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
        else
        {
          cursornr = XSANE_CURSOR_PREVIEW;
        }

        if ((cursornr != p->cursornr) && (p->cursornr != -1))
        {
          cursor = gdk_cursor_new(cursornr);	/* set curosr */
          gdk_window_set_cursor(p->window->window, cursor);
          gdk_cursor_destroy(cursor);
          p->cursornr = cursornr;
        }
       break;
    }
  }

  while (gtk_events_pending()) /* make sure all later events are handled */
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
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
  preview_transform_coordinate_preview_to_window(p, p->selection.coordinate, preview_selection);

  /* cursor-prosition (window) -> preview coordinate (device) */
  preview_transform_coordinate_window_to_preview(p, event->button.x, event->button.y, &preview_x, &preview_y);

  if (!p->scanning)
  {
    switch (p->mode)
    {
      case MODE_PIPETTE_WHITE:
      {
        DBG(DBG_info, "pipette white mode\n");
        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) && (p->image_data_raw) ) /* left or middle button */
        {
         int r,g,b;

          preview_get_color(p, event->button.x, event->button.y, &r, &g, &b);

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
        gdk_cursor_destroy(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_PIPETTE_GRAY:
      {
        DBG(DBG_info, "pipette gray mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) && (p->image_data_raw) ) /* left or middle button */
        {
         int r,g,b;

          preview_get_color(p, event->button.x, event->button.y, &r, &g, &b);

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
        gdk_cursor_destroy(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_PIPETTE_BLACK:
      {
        DBG(DBG_info, "pipette black mode\n");

        if ( ( (((GdkEventButton *)event)->button == 1) || (((GdkEventButton *)event)->button == 2) ) &&
             (p->image_data_raw) ) /* left or middle button */
        {
         int r,g,b;

          preview_get_color(p, event->button.x, event->button.y, &r, &g, &b);

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
        gdk_cursor_destroy(cursor);
        p->cursornr = XSANE_CURSOR_PREVIEW;
      }
      break;

      case MODE_NORMAL:
      {
        DBG(DBG_info, "normal mode\n");

        if (p->show_selection)
        {
          switch (((GdkEventButton *)event)->button)
          {
            case 1: /* left button */
              DBG(DBG_info, "left button\n");

              p->selection_xedge = -1;
              if ( (preview_selection[0] - SELECTION_RANGE_OUT < event->button.x) &&
                   (event->button.x < preview_selection[0] + SELECTION_RANGE_IN) ) /* left */
              {
                p->selection_xedge = 0;
              }
              else if ( (preview_selection[2] - SELECTION_RANGE_IN < event->button.x) &&
                        (event->button.x < preview_selection[2] + SELECTION_RANGE_OUT) ) /* right */
              {
                p->selection_xedge = 2;
              }

              p->selection_yedge = -1;
              if ( (preview_selection[1] - SELECTION_RANGE_OUT < event->button.y) &&
                   (event->button.y < preview_selection[1] + SELECTION_RANGE_IN) ) /* top */
              {
                p->selection_yedge = 1;
              }
              else if ( (preview_selection[3] - SELECTION_RANGE_IN < event->button.y) &&
                        (event->button.y < preview_selection[3] + SELECTION_RANGE_OUT) ) /* bottom */
              {
                p->selection_yedge = 3;
              }

              if ( (p->selection_xedge != -1) && (p->selection_yedge != -1) ) /* move edge */
              {
                p->selection_drag_edge = TRUE;
                p->selection.coordinate[p->selection_xedge] = preview_x;
                p->selection.coordinate[p->selection_yedge] = preview_y;
                preview_draw_selection(p);
              }
              else /* select new area */
              {
                p->selection_xedge = 2;
                p->selection_yedge = 3;
                p->selection.coordinate[0] = preview_x;
                p->selection.coordinate[1] = preview_y;
                p->selection_drag = TRUE;

                cursornr = GDK_CROSS;
                cursor = gdk_cursor_new(cursornr);	/* set curosr */
                gdk_window_set_cursor(p->window->window, cursor);
                gdk_cursor_destroy(cursor);
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
                p->selection_drag = TRUE;
                p->selection_xpos = event->button.x;
                p->selection_ypos = event->button.y;

                cursornr = GDK_HAND2;
                cursor = gdk_cursor_new(cursornr);	/* set curosr */
                gdk_window_set_cursor(p->window->window, cursor);
                gdk_cursor_destroy(cursor);
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
  preview_transform_coordinate_preview_to_window(p, p->selection.coordinate, preview_selection);

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
            cursornr = XSANE_CURSOR_PREVIEW;
            cursor = gdk_cursor_new(cursornr);	/* set curosr */
            gdk_window_set_cursor(p->window->window, cursor);
            gdk_cursor_destroy(cursor);
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

static gint preview_expose_event_handler(GtkWidget *window, GdkEvent *event, gpointer data)
{
 Preview *p = data;
 GdkColor color;
 GdkColormap *colormap; 

  DBG(DBG_proc, "preview_expose_event_handler\n");

  if (event->type == GDK_EXPOSE)
  {
    if (!p->gc_selection)
    {
      DBG(DBG_info, "defining line styles for selection and page frames\n");
      colormap = gdk_window_get_colormap(p->window->window);

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
      p->previous_selection.active = FALSE; /* ok, old selections are overpainted */
      p->previous_selection_maximum.active = FALSE;
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
  DBG(DBG_proc, "preview_cancel_button_clicked\n");

  preview_scan_done(data);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

Preview *preview_new(void)
{
 GtkWidget *table, *frame;
 GtkSignalFunc signal_func;
 GtkWidgetClass *class;
 GtkWidget *vbox, *hbox;
 GdkCursor *cursor;
 GtkWidget *preset_area_option_menu, *preset_area_menu, *preset_area_item;
 GtkWidget *rotation_option_menu, *rotation_menu, *rotation_item;
 Preview *p;
 int i;
 char buf[256];
 char filename[PATH_MAX];

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

  gtk_preview_set_gamma(1.0);
  gtk_preview_set_install_cmap(preferences.preview_own_cmap);


  for(i=0; i<=2; i++) /* create random filenames for previews */
  {
    if (preview_make_image_path(p, sizeof(filename), filename, i)>=0)
    {
      umask(0177);			/* create temporary file with "-rw-------" permissions */
      fclose(fopen(filename, "wb"));	/* make sure file exists, b = binary mode for win32 */
      umask(XSANE_DEFAULT_UMASK);	/* define new file permissions */
      p->filename[i] = strdup(filename);/* store filename */
    }
    else
    {
      fprintf(stderr, "could not create filename for preview-level %d\n", i);
      p->filename[i] = NULL;
    }
  }

  p->preset_width  = INF;	/* use full scanarea */
  p->preset_height = INF;	/* use full scanarea */

  p->maximum_output_width  = INF; /* full output with */
  p->maximum_output_height = INF; /* full output height */

#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_push_visual(gtk_preview_get_visual());
#endif
  gtk_widget_push_colormap(gtk_preview_get_cmap());

  snprintf(buf, sizeof(buf), "%s %s", WINDOW_PREVIEW, xsane.device_text);
  p->top = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(p->top), buf);
  xsane_set_window_icon(p->top, 0);

  /* set the main vbox */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
  gtk_container_add(GTK_CONTAINER(p->top), vbox);
  gtk_widget_show(vbox);     

  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(vbox, 2);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);


  /* top hbox for pipette buttons */
  p->button_box = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(p->button_box), 4);
  gtk_box_pack_start(GTK_BOX(vbox), p->button_box, FALSE, FALSE, 0);

  /* White, gray and black pipette button */
  p->pipette_white = xsane_button_new_with_pixmap(p->button_box, pipette_white_xpm, DESC_PIPETTE_WHITE, (GtkSignalFunc) preview_pipette_white, p);
  p->pipette_gray  = xsane_button_new_with_pixmap(p->button_box, pipette_gray_xpm,  DESC_PIPETTE_GRAY,  (GtkSignalFunc) preview_pipette_gray,  p);
  p->pipette_black = xsane_button_new_with_pixmap(p->button_box, pipette_black_xpm, DESC_PIPETTE_BLACK, (GtkSignalFunc) preview_pipette_black, p);

  /* Zoom not, zoom out and zoom in button */
  p->zoom_not  = xsane_button_new_with_pixmap(p->button_box, zoom_not_xpm,  DESC_ZOOM_FULL, (GtkSignalFunc) preview_zoom_not,   p);
  p->zoom_out  = xsane_button_new_with_pixmap(p->button_box, zoom_out_xpm,  DESC_ZOOM_OUT,  (GtkSignalFunc) preview_zoom_out,   p);
  p->zoom_in   = xsane_button_new_with_pixmap(p->button_box, zoom_in_xpm,   DESC_ZOOM_IN,   (GtkSignalFunc) preview_zoom_in,    p);
  p->zoom_undo = xsane_button_new_with_pixmap(p->button_box, zoom_undo_xpm, DESC_ZOOM_UNDO, (GtkSignalFunc) preview_zoom_undo,  p);

  gtk_widget_set_sensitive(p->zoom_not, FALSE); /* no zoom at this point, so no zoom not */
  gtk_widget_set_sensitive(p->zoom_out, FALSE); /* no zoom at this point, so no zoom out */
  gtk_widget_set_sensitive(p->zoom_undo, FALSE); /* no zoom at this point, so no zoom undo */



  xsane_button_new_with_pixmap(p->button_box, full_preview_area_xpm, DESC_FULL_PREVIEW_AREA,
                               (GtkSignalFunc) preview_full_preview_area_callback, p);

  /* select maximum scanarea */
  preset_area_menu = gtk_menu_new();

  for (i = 0; i < PRESET_AREA_ITEMS; ++i)
  {
    preset_area_item = gtk_menu_item_new_with_label(preset_area[i].name);
    gtk_container_add(GTK_CONTAINER(preset_area_menu), preset_area_item);
    gtk_signal_connect(GTK_OBJECT(preset_area_item), "activate", (GtkSignalFunc) preview_preset_area_callback, p);
    gtk_object_set_data(GTK_OBJECT(preset_area_item), "Selection", (void *) i);  

    gtk_widget_show(preset_area_item);
  }                  

  preset_area_option_menu = gtk_option_menu_new();
  gtk_box_pack_start(GTK_BOX(p->button_box), preset_area_option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(preset_area_option_menu), preset_area_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(preset_area_option_menu), 0); /* full area */
/*  xsane_back_gtk_set_tooltip(tooltips, preset_area_option_menu, desc); */

  gtk_widget_show(preset_area_option_menu);
  p->preset_area_option_menu = preset_area_option_menu;


  /* select rotation */
  rotation_menu = gtk_menu_new();

  for (i = 0; i < 4; ++i)
  {
   char buffer[256];

    snprintf(buffer, sizeof(buffer), "%d°", i*90);  
    rotation_item = gtk_menu_item_new_with_label(buffer);
    gtk_container_add(GTK_CONTAINER(rotation_menu), rotation_item);
    gtk_signal_connect(GTK_OBJECT(rotation_item), "activate", (GtkSignalFunc) preview_rotation_callback, p);
    gtk_object_set_data(GTK_OBJECT(rotation_item), "Selection", (void *) i);  

    gtk_widget_show(rotation_item);
  }                  

  rotation_option_menu = gtk_option_menu_new();
  gtk_box_pack_start(GTK_BOX(p->button_box), rotation_option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(rotation_option_menu), rotation_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(rotation_option_menu), p->rotation); /* set rotation */
/*  xsane_back_gtk_set_tooltip(tooltips, rotation_option_menu, desc); */

#ifdef ORAUCH
  gtk_widget_show(rotation_option_menu);
#endif
  p->rotation_option_menu = rotation_option_menu;


  gtk_widget_show(p->button_box);



  /* construct the preview area (table with sliders & preview window) */
  table = gtk_table_new(2, 2, /* homogeneous */ FALSE);
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 1);
  gtk_container_set_border_width(GTK_CONTAINER(table), 2);
  gtk_box_pack_start(GTK_BOX(vbox), table, /* expand */ TRUE, /* fill */ TRUE, /* padding */ 0);

  /* the empty box in the top-left corner */
  frame = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_table_attach(GTK_TABLE(table), frame, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  /* the horizontal ruler */
  p->hruler = gtk_hruler_new();
  gtk_table_attach(GTK_TABLE(table), p->hruler, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

  /* the vertical ruler */
  p->vruler = gtk_vruler_new();
  gtk_table_attach(GTK_TABLE(table), p->vruler, 0, 1, 1, 2, 0, GTK_FILL, 0, 0);

  /* the preview area */
  p->window = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_set_expand(GTK_PREVIEW(p->window), TRUE);

  gtk_widget_set_events(p->window, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  /* the first expose_event is responsible to draw the selection frame when the user moved/changed it */
  gtk_signal_connect(GTK_OBJECT(p->window), "expose_event", (GtkSignalFunc) preview_expose_event_handler, p);
  gtk_signal_connect(GTK_OBJECT(p->window), "button_press_event",   (GtkSignalFunc) preview_button_press_event_handler, p);
  gtk_signal_connect(GTK_OBJECT(p->window), "motion_notify_event",  (GtkSignalFunc) preview_motion_event_handler, p);
  gtk_signal_connect(GTK_OBJECT(p->window), "button_release_event", (GtkSignalFunc) preview_button_release_event_handler, p);
  gtk_signal_connect_after(GTK_OBJECT(p->window), "size_allocate", (GtkSignalFunc) preview_area_resize_handler, p);
  /* the second expose_event is responsible to draw the selection frame when the window was covered */
  gtk_signal_connect_after(GTK_OBJECT(p->window), "expose_event",  (GtkSignalFunc) preview_expose_event_handler, p);

  /* Connect the motion-notify events of the preview area with the rulers.  Nifty stuff!  */
  class = GTK_WIDGET_CLASS(GTK_OBJECT(p->hruler)->klass);
  signal_func = (GtkSignalFunc) class->motion_notify_event;
  gtk_signal_connect_object(GTK_OBJECT(p->window), "motion_notify_event", signal_func, GTK_OBJECT(p->hruler));
  class = GTK_WIDGET_CLASS(GTK_OBJECT(p->vruler)->klass);
  signal_func = (GtkSignalFunc) class->motion_notify_event;
  gtk_signal_connect_object(GTK_OBJECT(p->window), "motion_notify_event", signal_func, GTK_OBJECT(p->vruler));


  p->viewport = gtk_frame_new(/* label */ 0);
  gtk_frame_set_shadow_type(GTK_FRAME(p->viewport), GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(p->viewport), p->window);

  gtk_table_attach(GTK_TABLE(table), p->viewport, 1, 2, 1, 2,
		   GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

  preview_update_surface(p, 0);

  /* fill in action area: */

  /* Start button */
  p->start = gtk_button_new_with_label(BUTTON_PREVIEW_ACQUIRE);
  gtk_signal_connect(GTK_OBJECT(p->start), "clicked", (GtkSignalFunc) preview_start_button_clicked, p);
  gtk_box_pack_start(GTK_BOX(hbox), p->start, TRUE, TRUE, 0);

  /* Cancel button */
  p->cancel = gtk_button_new_with_label(BUTTON_PREVIEW_CANCEL);
  gtk_signal_connect(GTK_OBJECT(p->cancel), "clicked", (GtkSignalFunc) preview_cancel_button_clicked, p);
  gtk_box_pack_start(GTK_BOX(hbox), p->cancel, TRUE, TRUE, 0);
  gtk_widget_set_sensitive(p->cancel, FALSE);

  gtk_widget_show(p->cancel);
  gtk_widget_show(p->start);
  gtk_widget_show(p->viewport);
  gtk_widget_show(p->window);
  gtk_widget_show(p->hruler);
  gtk_widget_show(p->vruler);
  gtk_widget_show(frame);
  gtk_widget_show(table);
  gtk_widget_show(p->top);

  cursor = gdk_cursor_new(XSANE_CURSOR_PREVIEW);	/* set default curosr */
  gdk_window_set_cursor(p->window->window, cursor);
  gdk_cursor_destroy(cursor);
  p->cursornr = XSANE_CURSOR_PREVIEW;

  gtk_widget_pop_colormap();
#ifndef XSERVER_WITH_BUGGY_VISUALS
  gtk_widget_pop_visual();
#endif

  preview_update_surface(p, 0);
  return p;
}


/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_area_correct(Preview *p)
{
 float width, height, max_width, max_height;

  DBG(DBG_proc, "preview_area_correct\n");

  width      = p->preview_width;
  height     = p->preview_height;
  max_width  = p->preview_window_width;
  max_height = p->preview_window_height;

  width  = max_width;
  height = width / p->aspect;

  if (height > max_height)
  {
    height = max_height;
    width  = height * p->aspect;
  }

  p->preview_width  = width + 0.5;
  p->preview_height = height + 0.5;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_update_surface(Preview *p, int surface_changed)
{
 float val;
 float width, height;
 float max_width, max_height;
 float preset_width, preset_height;
 const SANE_Option_Descriptor *opt;
 int i;
 SANE_Value_Type type;
 SANE_Unit unit;
 double min, max;

  DBG(DBG_proc, "preview_update_surface\n");

  unit = SANE_UNIT_PIXEL;
  type = SANE_TYPE_INT;

  p->show_selection = FALSE; /* at first let's say we have no corrdinate selection */

  for (i = 0; i < 4; ++i) /* test if surface (max vals of scanarea) has changed */
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
       surface_changed = 2;
       p->orig_scanner_surface[i] = val;
    }
  }

  if (surface_changed == 2) /* redefine all surface subparts */
  {
    preview_rotate_dsurface_to_psurface(p->rotation, p->orig_scanner_surface, p->max_scanner_surface);

    for (i = 0; i < 4; i++)
    {
       val = p->max_scanner_surface[i];
       p->scanner_surface[i]     = val;
       p->surface[i]             = val;
       p->image_surface[i]       = val;
    }
  } 

  max_width  = p->max_scanner_surface[xsane_back_gtk_BR_X] - p->max_scanner_surface[xsane_back_gtk_TL_X];
  max_height = p->max_scanner_surface[xsane_back_gtk_BR_Y] - p->max_scanner_surface[xsane_back_gtk_TL_Y];

  width  = p->scanner_surface[xsane_back_gtk_BR_X] - p->scanner_surface[xsane_back_gtk_TL_X];
  height = p->scanner_surface[xsane_back_gtk_BR_Y] - p->scanner_surface[xsane_back_gtk_TL_Y];

  preset_width  = p->preset_width;
  preset_height = p->preset_height;

  if (preset_width > max_width)
  {
    preset_width = max_width;
  }

  if (preset_height > max_height)
  {
    preset_height = max_height;
  }

  if ( (width != preset_width) || (height != preset_height) )
  {
    p->scanner_surface[xsane_back_gtk_TL_X] = p->scanner_surface[xsane_back_gtk_TL_X];
    p->surface[xsane_back_gtk_TL_X]         = p->scanner_surface[xsane_back_gtk_TL_X];
    p->image_surface[xsane_back_gtk_TL_X]   = p->scanner_surface[xsane_back_gtk_TL_X];

    p->scanner_surface[xsane_back_gtk_BR_X] = p->scanner_surface[xsane_back_gtk_TL_X] + preset_width;
    p->surface[xsane_back_gtk_BR_X]         = p->scanner_surface[xsane_back_gtk_TL_X] + preset_width;
    p->image_surface[xsane_back_gtk_BR_X]   = p->scanner_surface[xsane_back_gtk_TL_X] + preset_width;

    p->scanner_surface[xsane_back_gtk_TL_Y] = p->scanner_surface[xsane_back_gtk_TL_Y];
    p->surface[xsane_back_gtk_TL_Y]         = p->scanner_surface[xsane_back_gtk_TL_Y];
    p->image_surface[xsane_back_gtk_TL_Y]   = p->scanner_surface[xsane_back_gtk_TL_Y];

    p->scanner_surface[xsane_back_gtk_BR_Y] = p->scanner_surface[xsane_back_gtk_TL_Y] + preset_height;
    p->surface[xsane_back_gtk_BR_Y]         = p->scanner_surface[xsane_back_gtk_TL_Y] + preset_height;
    p->image_surface[xsane_back_gtk_BR_Y]   = p->scanner_surface[xsane_back_gtk_TL_Y] + preset_height;

    surface_changed = 1;
  }

  if (p->surface_unit != unit)
  {
    surface_changed = 1;
    p->surface_unit = unit;
  }

  if (p->surface_unit == SANE_UNIT_MM)
  {
    gtk_widget_set_sensitive(p->preset_area_option_menu, TRUE); /* enable preset area */
  }
  else
  {
    gtk_widget_set_sensitive(p->preset_area_option_menu, FALSE); /* disable preset area */
  }

  if (p->surface_type != type)
  {
    surface_changed = 1;
    p->surface_type = type;
  }

  if (surface_changed)
  {
    DBG(DBG_info, "surface_changed\n");
    /* guess the initial preview window size: */

    width  = p->surface[xsane_back_gtk_BR_X] - p->surface[xsane_back_gtk_TL_X];
    height = p->surface[xsane_back_gtk_BR_Y] - p->surface[xsane_back_gtk_TL_Y];

    if (p->surface_type == SANE_TYPE_INT)
    {
      width  += 1.0;
      height += 1.0;
    }
    else
    {
      width  += SANE_UNFIX(1.0);
      height += SANE_UNFIX(1.0);
    }

    if (p->calibration)
    {
      p->aspect = p->image_width/(float) p->image_height;
    }
    else if (width >= INF || height >= INF)
    {
      p->aspect = 1.0;
    }
    else
    {
      p->aspect = width/height;
    }
  }
  else if ( (p->image_height) && (p->image_width) )
  {
    p->aspect = p->image_width/(float) p->image_height;
  }

  if ( (surface_changed) && (p->preview_window_width == 0) )
  {
    DBG(DBG_info, "defining size of preview window\n");

    p->preview_window_width  = 0.5 * gdk_screen_width();
    p->preview_window_height = 0.5 * gdk_screen_height();
    preview_area_correct(p);
    gtk_widget_set_usize(GTK_WIDGET(p->window), p->preview_width, p->preview_height);
  }
  else
  {
    preview_area_correct(p);
  }

  if (surface_changed)
  {
    preview_area_resize(p);	/* correct rulers */
    preview_bound_selection(p);	/* make sure selection is not larger than surface */
    preview_restore_image(p);	/* draw selected surface of the image */
  }
  else
  {
    preview_update_selection(p);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_scan(Preview *p)
{
 double min, max, swidth, sheight, width, height, dpi = 0;
 const SANE_Option_Descriptor *opt;
 gint gwidth, gheight;
 int i;
 float dsurface[4];

  DBG(DBG_proc, "preview_scan\n");

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
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.dpi);

    gwidth  = p->preview_width;
    gheight = p->preview_height;

    height = gheight;
    width  = height * p->aspect;

    if (width > gwidth)
    {
      width  = gwidth;
      height = width / p->aspect;
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

  preview_rotate_psurface_to_dsurface(p->rotation, p->surface, dsurface);

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

  xsane.block_update_param = FALSE;

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

    preview_rotate_psurface_to_dsurface(p->rotation, p->surface, dsurface);

    /* always save it as a PPM image: */
    fprintf(out, "P6\n# surface: %g %g %g %g %u %u\n%d %d\n255\n",
                 dsurface[0], dsurface[1], dsurface[2], dsurface[3],
                 p->surface_type, p->surface_unit, p->image_width, p->image_height);

    fwrite(p->image_data_raw, 3, p->image_width*p->image_height, out);
    fclose(out);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_save_image(Preview *p)
{
 FILE *out;
 int level=0;

  DBG(DBG_proc, "preview_save_image\n");

  if (!p->image_data_enh)
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
//    remove(p->filename[level]); /* remove existing preview */
    umask(0177); /* create temporary file with "-rw-------" permissions */
    out = fopen(p->filename[level], "wb"); /* b = binary mode for win32*/
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */

    preview_save_image_file(p, out);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_destroy(Preview *p)
{
 int level;

  DBG(DBG_proc, "preview_destroy\n");

  if (p->scanning)
  {
    preview_scan_done(p);		/* don't save partial window */
  }
  else
  {
    preview_save_image(p);
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
    gdk_gc_destroy(p->gc_selection);
  }

  if (p->gc_selection_maximum)
  {
    gdk_gc_destroy(p->gc_selection_maximum);
  }

  if (p->top)
  {
    gtk_widget_destroy(p->top);
  }
  free(p);

  p = 0;
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
 float delta_width  = (p->surface[2] - p->surface[0]) * 0.2;
 float delta_height = (p->surface[3] - p->surface[1]) * 0.2;

  DBG(DBG_proc, "preview_zoom_out\n");

  for (i=0; i<4; i++)
  {
    p->old_surface[i] = p->surface[i];
  }

  p->surface[0] -= delta_width;
  p->surface[1] -= delta_height;
  p->surface[2] += delta_width;
  p->surface[3] += delta_height;

  if (p->surface[0] < p->scanner_surface[0])
  {
    p->surface[0] = p->scanner_surface[0];
  } 

  if (p->surface[1] < p->scanner_surface[1])
  {
    p->surface[1] = p->scanner_surface[1];
  } 

  if (p->surface[2] > p->scanner_surface[2])
  {
    p->surface[2] = p->scanner_surface[2];
  } 

  if (p->surface[3] > p->scanner_surface[3])
  {
    p->surface[3] = p->scanner_surface[3];
  } 

  preview_update_surface(p, 1);
  gtk_widget_set_sensitive(p->zoom_not, TRUE); /* allow unzoom */
  gtk_widget_set_sensitive(p->zoom_undo,TRUE); /* allow zoom undo */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_zoom_in(GtkWidget *window, gpointer data)
{
 Preview *p=data;
 int i;

  DBG(DBG_proc, "preview_zoom_in\n");

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

static void preview_get_color(Preview *p, int x, int y, int *red, int *green, int *blue)
{
 int image_x, image_y;
 float xscale_p2i, yscale_p2i;
 int offset;

  DBG(DBG_proc, "preview_get_color\n");

  if (p->image_data_raw)
  {
    preview_get_scale_preview_to_image(p, &xscale_p2i, &yscale_p2i);

    image_x = x * xscale_p2i;
    image_y = y * yscale_p2i;

    offset = 3 * (image_y * p->image_width + image_x);

    if (!xsane.negative) /* positive */
    {
      *red   = p->image_data_raw[offset    ];
      *green = p->image_data_raw[offset + 1];
      *blue  = p->image_data_raw[offset + 2];
    }
    else /* negative */
    {
      *red   = 255 - p->image_data_raw[offset    ];
      *green = 255 - p->image_data_raw[offset + 1];
      *blue  = 255 - p->image_data_raw[offset + 2];
    }
  }
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
  gdk_cursor_destroy(cursor);
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
  gdk_cursor_destroy(cursor);
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
  gdk_cursor_destroy(cursor);
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

static void preview_preset_area_callback(GtkWidget *widget, gpointer call_data)
{
 Preview *p = call_data;
 int selection;

  DBG(DBG_proc, "preview_preset_area_callback\n");

  selection = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  p->preset_width  = preset_area[selection].width;
  p->preset_height = preset_area[selection].height;

  preview_update_surface(p, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_rotation_callback(GtkWidget *widget, gpointer call_data)
{
 Preview *p = call_data;
 int rot;

  DBG(DBG_proc, "preview_rotation_callback\n");

  rot = (int) gtk_object_get_data(GTK_OBJECT(widget), "Selection");

  p->rotation = rot;
  preview_update_surface(p, 2);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_do_gamma_correction(Preview *p)
{
 int x,y;
 int offset;

  DBG(DBG_proc, "preview_do_gamma_correction\n");

  if ((p->image_data_raw) && (p->params.depth > 1) && (preview_gamma_data_red))
  {
    if ( (xsane.param.format == SANE_FRAME_RGB) ||
         (xsane.param.format == SANE_FRAME_RED) ||
         (xsane.param.format == SANE_FRAME_GREEN) ||
         (xsane.param.format == SANE_FRAME_BLUE) )
    {
      for (y=0; y < p->image_height; y++)
      {
        for (x=0; x < p->image_width; x++)
        {
          offset = 3 * (y * p->image_width + x);
          p->image_data_enh[offset    ] = preview_gamma_data_red  [p->image_data_raw[offset    ]];
          p->image_data_enh[offset + 1] = preview_gamma_data_green[p->image_data_raw[offset + 1]];
          p->image_data_enh[offset + 2] = preview_gamma_data_blue [p->image_data_raw[offset + 2]];
        }
      }
    }
    else
    {
     int level;

      for (y=0; y < p->image_height; y++)
      {
        for (x=0; x < p->image_width; x++)
        {
          offset = 3 * (y * p->image_width + x);
          level = (p->image_data_raw[offset] + p->image_data_raw[offset+1] + p->image_data_raw[offset+2]) / 3;
          p->image_data_enh[offset    ] = preview_gamma_data_red  [level];
          p->image_data_enh[offset + 1] = preview_gamma_data_green[level];
          p->image_data_enh[offset + 2] = preview_gamma_data_blue [level];
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

void preview_calculate_histogram(Preview *p,
  SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue,
  SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue)
{
 int x, y;
 int offset;
 SANE_Int red_raw, green_raw, blue_raw;
 SANE_Int red, green, blue;
 SANE_Int min_x, max_x, min_y, max_y;
 float xscale, yscale;
 
  DBG(DBG_proc, "preview_calculate_histogram\n");

  preview_get_scale_device_to_image(p, &xscale, &yscale);

  min_x = (p->selection.coordinate[0] - p->surface[0]) * xscale;
  min_y = (p->selection.coordinate[1] - p->surface[1]) * yscale;
  max_x = (p->selection.coordinate[2] - p->surface[0]) * xscale;
  max_y = (p->selection.coordinate[3] - p->surface[1]) * yscale;

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
      for (x = min_x; x <= max_x; x++)
      {
        offset = 3 * (y * p->image_width + x);
        red_raw   = p->image_data_raw[offset    ];
        green_raw = p->image_data_raw[offset + 1];
        blue_raw  = p->image_data_raw[offset + 2];

        red   = histogram_gamma_data_red  [red_raw];
        green = histogram_gamma_data_green[green_raw];
        blue  = histogram_gamma_data_blue [blue_raw];

/*	count_raw      [(int) sqrt((red_raw*red_raw + green_raw*green_raw + blue_raw*blue_raw)/3.0)]++; */
	count_raw      [(int) ((red_raw + green_raw + blue_raw)/3)]++;
	count_raw_red  [red_raw]++;
	count_raw_green[green_raw]++;
	count_raw_blue [blue_raw]++;

/*	count      [(int) sqrt((red*red + green*green + blue*blue)/3.0)]++; */
	count      [(int) ((red + green + blue)/3)]++;
	count_red  [red]++;
	count_green[green]++;
	count_blue [blue]++;
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

      count      [i] = 0;
      count_red  [i] = 0;
      count_green[i] = 0;
      count_blue [i] = 0;
    }

    count_raw      [0] = 10;
    count_raw_red  [0] = 10;
    count_raw_green[0] = 10;
    count_raw_blue [0] = 10;

    count      [0] = 10;
    count_red  [0] = 10;
    count_green[0] = 10;
    count_blue [0] = 10;

    count_raw      [255] = 10;
    count_raw_red  [255] = 10;
    count_raw_green[255] = 10;
    count_raw_blue [255] = 10;

    count      [255] = 10;
    count_red  [255] = 10;
    count_green[255] = 10;
    count_blue [255] = 10;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_gamma_correction(Preview *p,
                              SANE_Int *gamma_red, SANE_Int *gamma_green, SANE_Int *gamma_blue,
                              SANE_Int *gamma_red_hist, SANE_Int *gamma_green_hist, SANE_Int *gamma_blue_hist)
{
  DBG(DBG_proc, "preview_gamma_correction\n");

  preview_gamma_data_red   = gamma_red;
  preview_gamma_data_green = gamma_green;
  preview_gamma_data_blue  = gamma_blue;

  histogram_gamma_data_red   = gamma_red_hist;
  histogram_gamma_data_green = gamma_green_hist;
  histogram_gamma_data_blue  = gamma_blue_hist;

  preview_do_gamma_correction(p);
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_area_resize(Preview *p)
{
 float min_x, max_x, delta_x;
 float min_y, max_y, delta_y;
 float xscale, yscale, f;

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
  if (min_x <= -INF)
  {
    min_x = 0.0;
  }

  max_x = p->surface[xsane_back_gtk_BR_X];
  if (max_x >=  INF)
  {
    max_x = p->image_width - 1;
  }

  min_y = p->surface[xsane_back_gtk_TL_Y];
  if (min_y <= -INF)
  {
    min_y = 0.0;
  }

  max_y = p->surface[xsane_back_gtk_BR_Y];
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

  preview_get_scale_preview_to_image(p, &xscale, &yscale);

  if (p->image_width > 0)
  {
    f = xscale * p->preview_width / p->image_width;
  }
  else
  {
    f = 1.0;
  }

  min_x *= f;
  max_x *= f;
  delta_x = max_x - min_x;

  gtk_ruler_set_range(GTK_RULER(p->hruler), min_x, min_x + delta_x*p->preview_window_width/p->preview_width, min_x, /* max_size */ 20);

  if (p->image_height > 0)
  {
    f = yscale * p->preview_height / p->image_height;
  }
  else
  {
    f = 1.0;
  }

  min_y *= f;
  max_y *= f;
  delta_y = max_y - min_y;

  gtk_ruler_set_range(GTK_RULER(p->vruler), min_y, min_y + delta_y*p->preview_window_height/p->preview_height, min_y, /* max_size */ 20);

  preview_paint_image(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

gint preview_area_resize_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  DBG(DBG_proc, "preview_area_resize_handler\n");

  preview_area_resize((Preview *) data);
 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
void preview_update_maximum_output_size(Preview *p)
{
  DBG(DBG_proc, "preview_update_maximum_output_size\n");

  if ( (p->maximum_output_width >= INF) || (p->maximum_output_height >= INF) )
  {
    if (p->selection_maximum.active)
    {
      p->selection_maximum.active = FALSE;
    }
  }
  else
  {
    p->previous_selection_maximum = p->selection_maximum;

    p->selection_maximum.active = TRUE;
    p->selection_maximum.coordinate[0] = (p->selection.coordinate[0] + p->selection.coordinate[2] - p->maximum_output_width )/2.0;
    p->selection_maximum.coordinate[1] = (p->selection.coordinate[1] + p->selection.coordinate[3] - p->maximum_output_height)/2.0;
    p->selection_maximum.coordinate[2] = (p->selection.coordinate[0] + p->selection.coordinate[2] + p->maximum_output_width )/2.0;
    p->selection_maximum.coordinate[3] = (p->selection.coordinate[1] + p->selection.coordinate[3] + p->maximum_output_height)/2.0;

    if (p->selection_maximum.coordinate[0] < p->max_scanner_surface[0])
    {
      p->selection_maximum.coordinate[0] = p->max_scanner_surface[0];
    }

    if (p->selection_maximum.coordinate[1] < p->max_scanner_surface[1])
    {
      p->selection_maximum.coordinate[1] = p->max_scanner_surface[1];
    }

    if (p->selection_maximum.coordinate[2] > p->max_scanner_surface[2])
    {
      p->selection_maximum.coordinate[2] = p->max_scanner_surface[2];
    }

    if (p->selection_maximum.coordinate[3] > p->max_scanner_surface[3])
    {
      p->selection_maximum.coordinate[3] = p->max_scanner_surface[3];
    }

    if ( (p->selection.coordinate[0] < p->selection_maximum.coordinate[0]) ||
         (p->selection.coordinate[1] < p->selection_maximum.coordinate[1]) ||
         (p->selection.coordinate[2] > p->selection_maximum.coordinate[2]) ||
         (p->selection.coordinate[3] > p->selection_maximum.coordinate[3]) )
    {
      if (p->selection.coordinate[0] < p->selection_maximum.coordinate[0])
      {
        p->selection.coordinate[0] = p->selection_maximum.coordinate[0];
      }

      if (p->selection.coordinate[1] < p->selection_maximum.coordinate[1])
      {
        p->selection.coordinate[1] = p->selection_maximum.coordinate[1];
      }

      if (p->selection.coordinate[2] > p->selection_maximum.coordinate[2])
      {
        p->selection.coordinate[2] = p->selection_maximum.coordinate[2];
      }

      if (p->selection.coordinate[3] > p->selection_maximum.coordinate[3])
      {
        p->selection.coordinate[3] = p->selection_maximum.coordinate[3];
      }
      preview_draw_selection(p);
      preview_establish_selection(p);
    }
  }
}
#endif

void preview_update_maximum_output_size(Preview *p)
{
  if ( (p->maximum_output_width >= INF) || (p->maximum_output_height >= INF) )
  {
    if (p->selection_maximum.active)
    {
      p->selection_maximum.active = FALSE;
    }
  }
  else
  {
    p->previous_selection_maximum = p->selection_maximum;

    p->selection_maximum.active = TRUE;
    p->selection_maximum.coordinate[0] = p->selection.coordinate[0];
    p->selection_maximum.coordinate[1] = p->selection.coordinate[1];
    p->selection_maximum.coordinate[2] = p->selection.coordinate[0] + p->maximum_output_width;
    p->selection_maximum.coordinate[3] = p->selection.coordinate[1] + p->maximum_output_height;

    if (p->selection_maximum.coordinate[2] > p->max_scanner_surface[2])
    {
      p->selection_maximum.coordinate[2] = p->max_scanner_surface[2];
    }

    if (p->selection_maximum.coordinate[3] > p->max_scanner_surface[3])
    {
      p->selection_maximum.coordinate[3] = p->max_scanner_surface[3];
    }

    if ( (p->selection.coordinate[0] < p->selection_maximum.coordinate[0]) ||
         (p->selection.coordinate[1] < p->selection_maximum.coordinate[1]) ||
         (p->selection.coordinate[2] > p->selection_maximum.coordinate[2]) ||
         (p->selection.coordinate[3] > p->selection_maximum.coordinate[3]) )
    {
      if (p->selection.coordinate[2] > p->selection_maximum.coordinate[2])
      {
        p->selection.coordinate[2] = p->selection_maximum.coordinate[2];
      }

      if (p->selection.coordinate[3] > p->selection_maximum.coordinate[3])
      {
        p->selection.coordinate[3] = p->selection_maximum.coordinate[3];
      }
      preview_draw_selection(p);
      preview_establish_selection(p);
    }
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void preview_set_maximum_output_size(Preview *p, float width, float height)
{
 /* witdh and height in device units */
  DBG(DBG_proc, "preview_set_maximum_output_size\n");

  p->maximum_output_width  = width;
  p->maximum_output_height = height;

  preview_update_maximum_output_size(p);
  preview_draw_selection(p);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
