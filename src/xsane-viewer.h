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

typedef enum
{
  VIEWER_NO_MODIFICATION = 0,
  VIEWER_NO_NAME_AND_SIZE_MODIFICATION,
  VIEWER_NO_NAME_MODIFICATION,
  VIEWER_FULL_MODIFICATION
} viewer_modification;

typedef struct Viewer
{
  struct Viewer *next_viewer;

  char *filename;
  char *output_filename;
  char *last_saved_filename;
  char *undo_filename;

  int reduce_to_lineart;
  float zoom;
  int image_saved;
  int cancel_save;
  viewer_modification allow_modification;

  int despeckle_radius;
  float blur_radius;

  int bind_scale;
  double x_scale_factor;
  double y_scale_factor;

  GtkWidget *top;
  GtkWidget *button_box;

  GtkWidget *file_button_box;
  GtkWidget *filters_button_box;
  GtkWidget *geometry_button_box;

  GtkWidget *file_menu;
  GtkWidget *edit_menu;
  GtkWidget *filters_menu;
  GtkWidget *geometry_menu;

  GtkWidget *viewport;
  GtkWidget *window;

  GtkWidget *save_menu_item;
  GtkWidget *ocr_menu_item;
  GtkWidget *clone_menu_item;

  GtkWidget *undo_menu_item;

  GtkWidget *despeckle_menu_item;
  GtkWidget *blur_menu_item;

  GtkWidget *save;
  GtkWidget *ocr;
  GtkWidget *undo;
  GtkWidget *clone;

  GtkWidget *despeckle;
  GtkWidget *blur;

  GtkWidget *image_info_label;

  GtkProgressBar *progress_bar;

  GtkWidget *active_dialog;

  int block_actions;
}
Viewer;

extern Viewer *xsane_viewer_new(char *filename, int reduce_to_lineart, char *output_filename, viewer_modification allow_modification);

#endif
