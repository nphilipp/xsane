/* xsane preview
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

/* ------------------------------------------------------------------------------------------------------ */

#ifndef xsanepreview_h
#define xsanepreview_h

/* ------------------------------------------------------------------------------------------------------ */

#include <sys/types.h>

#include <sane/config.h>
#include <sane/sane.h>

#define SELECTION_RANGE 15
#define XSANE_CURSOR_PREVIEW GDK_LEFT_PTR

/* ------------------------------------------------------------------------------------------------------ */

enum
{
  MODE_NORMAL,
  MODE_PIPETTE_WHITE,
  MODE_PIPETTE_GRAY,
  MODE_PIPETTE_BLACK
};

/* ------------------------------------------------------------------------------------------------------ */

typedef struct Batch_selection
{
  float coordinate[4]; /* batch selection coordinate (device coord) */
  struct Batch_selection *next;
} Batch_selection;

/* ------------------------------------------------------------------------------------------------------ */

typedef struct
{
  int mode;
  GSGDialog *dialog;	/* the dialog for this preview */

  SANE_Value_Type surface_type;
  SANE_Unit surface_unit;
  float surface[4];		/* the corners of the selected surface (device coords) */
  float old_surface[4];		/* the corners of the old selected surface (device coords) */
  float max_scanner_surface[4];	/* the scanner defined corners of the scanner surface (device coords) */
  float scanner_surface[4];	/* the user defined corners of the scanner surface (device coords) */
  float image_surface[4];	/* the corners of the surface (device coords) of the scanned image */
  float aspect;			/* the aspect ratio of the scan surface */

  float preset_width;		/* user selected maximum scan width */
  float preset_height;		/* user selected maximum scan height */

  int saved_dpi_valid;
  SANE_Word saved_dpi;
  int saved_coord_valid[4];
  SANE_Word saved_coord[4];
  int saved_custom_gamma_valid;
  SANE_Word saved_custom_gamma;
  int saved_bit_depth_valid;
  SANE_Word saved_bit_depth;

    /* desired/user-selected preview-window size: */
  int preview_width;		/* used with for displaying the preview image */
  int preview_height;		/* used height for displaying the preview image */
  int preview_window_width;	/* width of the preview window */
  int preview_window_height;	/* height of the preview window */
  u_char *preview_row;

  int scanning;
  time_t image_last_time_updated;
  gint input_tag;
  SANE_Parameters params;
  int image_offset;
  int image_x;
  int image_y;
  int image_width;		/* width of preview image in pixels */
  int image_height;		/* height of preview image in pixel lines */
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
    float coordinate[4]; /* selection coordinate (device coord) */
  } selection, previous_selection;

  Batch_selection *batch_selection;

  GtkWidget *top;		/* top-level widget */
  GtkWidget *hruler;
  GtkWidget *vruler;
  GtkWidget *viewport;
  GtkWidget *window;		/* the preview window */
/*  GtkWidget *zoom_view; */ 	/* the zoom view */
  GtkWidget *start;		/* the start button */
  GtkWidget *cancel;		/* the cancel button */

  GtkWidget *button_box;	/* hbox for the following buttons */
  GtkWidget *pipette_white;	/* pipette white button */
  GtkWidget *pipette_gray;	/* pipette gray button */
  GtkWidget *pipette_black;	/* pipette black button */
  GtkWidget *zoom_not;		/* zoom not button */
  GtkWidget *zoom_out;		/* zoom out button */
  GtkWidget *zoom_in;		/* zoom in button */
  GtkWidget *zoom_undo;		/* zoom undo button */
  GtkWidget *preset_area_option_menu;	/* menu for selection of preview area */
}
Preview;

/* ------------------------------------------------------------------------------------------------------ */

extern Preview *preview_new (GSGDialog *dialog);   /* Create a new preview based on the info in DIALOG.  */

extern void preview_gamma_correction(Preview *p,		  /* Do gamma correction on preview data */
                                     int gamma_red[], int gamma_green[], int gamma_blue[],
                                     int gamma_red_hist[], int gamma_green_hist[], int gamma_blue_hist[]);

extern void preview_update_surface(Preview *p, int surface_changed);   /* params changed: update preview */

extern void preview_scan(Preview *p);			     /* Acquire a preview image and display it.  */

extern void preview_destroy(Preview *p);					  /* Destroy a preview.  */

extern void preview_calculate_histogram(Preview *p,				  /* calculate histogram */
	SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue,
	SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue);

extern void preview_area_resize(GtkWidget *widget);				/* redraw preview rulers */

/* ------------------------------------------------------------------------------------------------------ */

#endif /* preview_h */
