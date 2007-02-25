/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
 
   xsane-viewer.h
 
   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 2001-2007 Oliver Rauch
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
  char *selection_filetype;

  int cms_function;

  int allow_reduction_to_lineart;
  int keep_viewer_pnm_format;
  float zoom;
  int image_saved;
  int cancel_save;
  viewer_modification allow_modification;

  int despeckle_radius;
  float blur_radius;

  int enable_color_management;
  int cms_enable;
  int cms_bpc;
  int cms_proofing;
  int cms_intent;
  int cms_proofing_intent;
  int cms_gamut_check;
  int cms_gamut_alarm_color;

  int bind_scale;
  double x_scale_factor;
  double y_scale_factor;

  GtkWidget *top;
  GtkWidget *button_box;

  GtkWidget *file_button_box;
  GtkWidget *edit_button_box;
  GtkWidget *filters_button_box;
  GtkWidget *geometry_button_box;

  GtkWidget *file_menu;
  GtkWidget *edit_menu;
  GtkWidget *filters_menu;
  GtkWidget *geometry_menu;
  GtkWidget *color_management_menu;

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

  GtkWidget *cms_proofing_widget[3];
  GtkWidget *cms_intent_widget[4];
  GtkWidget *cms_proofing_intent_widget[2];
  GtkWidget *cms_gamut_alarm_color_widget[6];

  GtkWidget *image_info_label;

  GtkProgressBar *progress_bar;

  GtkWidget *active_dialog;

  int block_actions;
}
Viewer;

extern Viewer *xsane_viewer_new(char *filename, char *selection_filetype, int allow_reduction_to_lineart,
                                char *output_filename, viewer_modification allow_modification, int image_saved);

#endif
