/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
 
   xsane-viewer.h
 
   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 2001-2004 Oliver Rauch
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

#ifndef XSANE_VIEWER_H
#define XSANE_VIEWER_H

#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct Viewer
{
  struct Viewer *next_viewer;

  char *filename;
  char *output_filename;

  int reduce_to_lineart;
  float zoom;
  int image_saved;
  int cancel_save;

  int despeckle_radius;
  float blur_radius;

  int bind_scale;
  double x_scale_factor;
  double y_scale_factor;

  GtkWidget *top;
  GtkWidget *button_box;

  GtkWidget *viewport;
  GtkWidget *window;

  GtkWidget *save;
  GtkWidget *ocr;
  GtkWidget *clone;
  GtkWidget *scale;

  GtkWidget *despeckle;
  GtkWidget *blur;

  GtkWidget *rotate90;
  GtkWidget *rotate180;
  GtkWidget *rotate270;

  GtkWidget *mirror_x;
  GtkWidget *mirror_y;

  GtkWidget *image_info_label;

  GtkProgressBar *progress_bar;

  GtkWidget *active_dialog;

  int block_actions;
}
Viewer;

extern Viewer *xsane_viewer_new(char *filename, int reduce_to_lineart, char *output_filename);

#endif
