/* xsane
   Copyright (C) 1997 David Mosberger-Tang
   Copyright (C) 1999 Oliver Rauch
   This file is part of the XSANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef xsanepreview_h
#define xsanepreview_h

#include <sys/types.h>

#include <sane/config.h>
#include <sane/sane.h>

#define SELECTION_RANGE 15

typedef struct
  {
    GSGDialog *dialog;	/* the dialog for this preview */

    SANE_Value_Type surface_type;
    SANE_Unit surface_unit;
    float surface[4];		/* the corners of the selected surface (device coords) */
    float old_surface[4];	/* the corners of the old selected surface (device coords) */
    float scanner_surface[4];	/* the corners of the scanner surface (device coords) */
    float aspect;	/* the aspect ratio of the scan surface */

    int saved_dpi_valid;
    SANE_Word saved_dpi;
    int saved_coord_valid[4];
    SANE_Word saved_coord[4];
    int saved_custom_gamma_valid;
    SANE_Word saved_custom_gamma;
    int saved_bit_depth_valid;
    SANE_Word saved_bit_depth;

    /* desired/user-selected preview-window size: */
    int preview_width;
    int preview_height;
    u_char *preview_row;

    int scanning;
    time_t image_last_time_updated;
    gint input_tag;
    SANE_Parameters params;
    int image_offset;
    int image_x;
    int image_y;
    int image_width;
    int image_height;
    u_char *image_data_raw;	/* 3 * image_width * image_height bytes */
    u_char *image_data_enh;	/* 3 * image_width * image_height bytes */

    GdkGC *gc;
    int selection_drag;
    int selection_drag_edge;
    int selection_xpos;
    int selection_ypos;
    int selection_xedge;
    int selection_yedge;
    struct
      {
	int active;
	int coord[4];
      }
    selection, previous_selection;

    GtkWidget *top;	/* top-level widget */
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *viewport;
    GtkWidget *window;	/* the preview window */
    GtkWidget *cancel;	/* the cancel button */
    GtkWidget *pipette_white;
    GtkWidget *pipette_gray;
    GtkWidget *pipette_black;
    GtkWidget *zoom_not;
    GtkWidget *zoom_out;
    GtkWidget *zoom_in;
  }
Preview;

/* Create a new preview based on the info in DIALOG.  */
extern Preview *preview_new (GSGDialog *dialog);

/* Do gamma correction on preview data */
extern void preview_gamma_correction(Preview *p,
                                     int gamma_red[], int gamma_green[], int gamma_blue[],
                                     int gamma_red_hist[], int gamma_green_hist[], int gamma_blue_hist[]);

/* Some of the parameters may have changed---update the preview.  */
extern void preview_update(Preview *p);

/* Acquire a preview image and display it.  */
extern void preview_scan(Preview *p);

/* Destroy a preview.  */
extern void preview_destroy(Preview *p);

/* calculate histogram */
extern void preview_calculate_histogram(Preview *p,
SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue,
SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
SANE_Int left_x, SANE_Int top_y, SANE_Int right_x, SANE_Int bottom_y);

/* redraw preview rulers */
extern void preview_area_resize(GtkWidget *widget);

#endif /* preview_h */
