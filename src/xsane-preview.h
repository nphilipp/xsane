/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preview.h

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

/* ------------------------------------------------------------------------------------------------------ */

#ifndef xsanepreview_h
#define xsanepreview_h

/* ------------------------------------------------------------------------------------------------------ */

#include <sys/types.h>
#include <sane/sane.h>

#define SELECTION_RANGE_IN  4
#define SELECTION_RANGE_OUT 8
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

typedef struct
{
  int active;
  float coordinate[4]; /* selection coordinate (device coord) */
} Tselection;

/* ------------------------------------------------------------------------------------------------------ */

typedef struct
{
  int mode;
  int calibration;
  int startimage;

  int cursornr;

  guint hold_timer;

  char *filename[3];		/* filenames for preview level 0,1,2 */

  SANE_Value_Type surface_type;
  SANE_Unit surface_unit;
  float orig_scanner_surface[4];/* the scanner defined corners of the scanner surface (device coords) */
  float image_surface[4];	/* the corners of the surface (device coords) of the scanned image */
  float max_scanner_surface[4];	/* rotated corners of the scanner surface (window coords) */
  float preset_surface[4];	/* the corners of the reduced (by user) surface (window coords) */
  float scanner_surface[4];	/* the user defined corners of the scanner surface (window coords) */
  float surface[4];		/* the corners of the selected surface (window coords) */
  float old_surface[4];		/* the corners of the old selected surface (window coords) */
  float aspect;			/* the aspect ratio of the scan surface */

  float maximum_output_width;	/* maximum output width (photocopy) */
  float maximum_output_height;	/* maximum output height (photocopy) */

  int index_xmin, index_xmax, index_ymin, index_ymax; /* index numbers in dependance of p->rotation */

  int saved_dpi_valid;
  int saved_dpi_x_valid;
  int saved_dpi_y_valid;
  SANE_Word saved_dpi;
  SANE_Word saved_dpi_x;
  SANE_Word saved_dpi_y;
  int saved_coord_valid[4];
  SANE_Word saved_coord[4];
  int saved_custom_gamma_valid;
  SANE_Word saved_custom_gamma;
  int saved_bit_depth_valid;
  SANE_Word saved_bit_depth;
  int saved_scanmode_valid;
  char saved_scanmode[64]; /* I hope that is enough or we will get segaults or strange effects */

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
  int rotation;			/* rotation: 0=0, 1=90, 2=180, 3=270 degree, 4-7= rotation + mirror in x direction */
  u_char *image_data_raw;	/* 3 * image_width * image_height bytes */
  u_char *image_data_enh;	/* 3 * image_width * image_height bytes */

  GdkGC *gc_selection;
  GdkGC *gc_selection_maximum;
  int selection_drag;
  int selection_drag_edge;
  int selection_xpos;
  int selection_ypos;
  int selection_xedge;
  int selection_yedge;

  Tselection selection;				/* selected area to scan */
  Tselection previous_selection;		/* previous ... */
  Tselection selection_maximum;			/* maximum selection size (photocopy) */
  Tselection previous_selection_maximum;	/* previous ... */

  int show_selection;

  Batch_selection *batch_selection;

  GtkWidget *top;		/* top-level widget */
  GtkWidget *hruler;
  GtkWidget *vruler;
  GtkWidget *viewport;
  GtkWidget *window;		/* the preview window */
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
  GtkWidget *full_area;		/* select full scanarea */
  GtkWidget *autoselect;	/* autoselect scanarea */
  GtkWidget *preset_area_option_menu;	/* menu for selection of preview area */
  GtkWidget *rotation_option_menu;	/* menu for selection of rotation */
}
Preview;

/* ------------------------------------------------------------------------------------------------------ */

extern Preview *preview_new(void);   /* Create a new preview based on the info in DIALOG.  */

extern void preview_gamma_correction(Preview *p,		  /* Do gamma correction on preview data */
                                     int gamma_red[], int gamma_green[], int gamma_blue[],
                                     int gamma_red_hist[], int gamma_green_hist[], int gamma_blue_hist[]);

extern void preview_update_surface(Preview *p, int surface_changed);   /* params changed: update preview */

extern void preview_scan(Preview *p);			     /* Acquire a preview image and display it.  */

extern void preview_destroy(Preview *p);					  /* Destroy a preview.  */

extern void preview_calculate_histogram(Preview *p,				  /* calculate histogram */
	SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue,
	SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue);

extern void preview_area_resize(Preview *p);					/* redraw preview rulers */
extern void preview_set_maximum_output_size(Preview *p, float width, float height);   /* set maximum outut size */
extern void preview_select_full_preview_area(Preview *p);

/* ------------------------------------------------------------------------------------------------------ */

#endif /* preview_h */
