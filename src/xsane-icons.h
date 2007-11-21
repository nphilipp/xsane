/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-icons.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2007 Oliver Rauch
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

/* ------------------------------------------------------------------------- */

#ifndef XSANE_ICONS_H
#define XSANE_ICONS_H    

#define CURSOR_PIPETTE_WIDTH 16
#define CURSOR_PIPETTE_HEIGHT 16
#define CURSOR_PIPETTE_HOT_X 1
#define CURSOR_PIPETTE_HOT_Y 14
#define CURSOR_AUTORAISE_SCAN_AREA_WIDTH 16
#define CURSOR_AUTORAISE_SCAN_AREA_HEIGHT 16
#define CURSOR_AUTORAISE_SCAN_AREA_HOT_X 7
#define CURSOR_AUTORAISE_SCAN_AREA_HOT_Y 7
#define CURSOR_ZOOM_WIDTH 16
#define CURSOR_ZOOM_HEIGHT 16
#define CURSOR_ZOOM_HOT_X 6
#define CURSOR_ZOOM_HOT_Y 6

extern const char *xsane_window_icon_xpm[];
extern const char *error_xpm[];
extern const char *warning_xpm[];
extern const char *info_xpm[];
extern const char *device_xpm[];
extern const char *no_device_xpm[];
extern const char *file_xpm[];
extern const char *load_xpm[];
extern const char *save_xpm[];
extern const char *ocr_xpm[];
extern const char *scale_xpm[];
extern const char *despeckle_xpm[];
extern const char *blur_xpm[];
extern const char *undo_xpm[];
extern const char *clone_xpm[];
extern const char *rotate90_xpm[];
extern const char *rotate180_xpm[];
extern const char *rotate270_xpm[];
extern const char *mirror_x_xpm[];
extern const char *mirror_y_xpm[];
extern const char *fax_xpm[];
extern const char *faxreceiver_xpm[];
extern const char *email_xpm[];
extern const char *emailreceiver_xpm[];
extern const char *subject_xpm[];
extern const char *adf_xpm[];
extern const char *target_xpm[];
extern const char *multipage_xpm[];
extern const char *colormode_xpm[];
extern const char *step_xpm[];
extern const char *medium_xpm[];
extern const char *medium_edit_xpm[];
extern const char *medium_delete_xpm[];
extern const char *Gamma_xpm[];
extern const char *Gamma_red_xpm[];
extern const char *Gamma_green_xpm[];
extern const char *Gamma_blue_xpm[];
extern const char *brightness_xpm[];
extern const char *brightness_red_xpm[];
extern const char *brightness_green_xpm[];
extern const char *brightness_blue_xpm[];
extern const char *contrast_xpm[];
extern const char *contrast_red_xpm[];
extern const char *contrast_green_xpm[];
extern const char *contrast_blue_xpm[];
extern const char *threshold_xpm[];
extern const char *rgb_default_xpm[];
extern const char *negative_xpm[];
extern const char *enhance_xpm[];
extern const char *store_enhancement_xpm[];
extern const char *restore_enhancement_xpm[];
extern const char *default_enhancement_xpm[];
extern const char *add_batch_xpm[];
extern const char *del_batch_xpm[];
extern const char *empty_batch_xpm[];
extern const char *ascii_xpm[];

extern const char *pipette_white_xpm[];
extern const char *pipette_gray_xpm[];
extern const char *pipette_black_xpm[];
extern const char *zoom_not_xpm[];
extern const char *zoom_out_xpm[];
extern const char *zoom_in_xpm[];
extern const char *zoom_area_xpm[];
extern const char *zoom_undo_xpm[];
extern const char *full_preview_area_xpm[];
extern const char *auto_select_preview_area_xpm[];
extern const char *auto_raise_preview_area_xpm[];
extern const char *delete_images_xpm[];
extern const char *size_xpm[];
extern const char *size_x_xpm[];
extern const char *size_y_xpm[];
extern const char *rotation_xpm[];
extern const char *aspect_ratio_xpm[];
extern const char *printer_xpm[];
extern const char *zoom_xpm[];
extern const char *zoom_x_xpm[];
extern const char *zoom_y_xpm[];
extern const char *resolution_xpm[];
extern const char *resolution_x_xpm[];
extern const char *resolution_y_xpm[];
extern const char *cms_xpm[];
extern const char *scanner_xpm[];
extern const char *intensity_xpm[];
extern const char *red_xpm[];
extern const char *green_xpm[];
extern const char *blue_xpm[];
extern const char *pixel_xpm[];
extern const char *log_xpm[];
extern const char *move_up_xpm[];
extern const char *move_down_xpm[];
extern const char *scanning_xpm[];
extern const char *invalid_xpm[];
extern const char *valid_xpm[];
extern const char *incomplete_xpm[];
extern const char *paper_orientation_portrait_center_xpm[];
extern const char *paper_orientation_portrait_top_left_xpm[];
extern const char *paper_orientation_portrait_top_right_xpm[];
extern const char *paper_orientation_portrait_bottom_left_xpm[];
extern const char *paper_orientation_portrait_bottom_right_xpm[];
extern const char *paper_orientation_landscape_center_xpm[];
extern const char *paper_orientation_landscape_top_left_xpm[];
extern const char *paper_orientation_landscape_top_right_xpm[];
extern const char *paper_orientation_landscape_bottom_left_xpm[];
extern const char *paper_orientation_landscape_bottom_right_xpm[];
extern const char cursor_pipette_white[];
extern const char cursor_pipette_gray[];
extern const char cursor_pipette_black[];
extern const char cursor_pipette_mask[];
extern const char cursor_autoraise_scan_area[];
extern const char cursor_autoraise_scan_area_mask[];
extern const char cursor_zoom[];
extern const char cursor_zoom_mask[];
#endif
