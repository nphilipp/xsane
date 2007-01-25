/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-gamma.h

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

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <sane/sane.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_clear_histogram(XsanePixmap *hist);    
extern void xsane_draw_slider_level(XsaneSlider *slider);
extern void xsane_update_slider(XsaneSlider *slider);
extern void xsane_update_sliders(void);
extern void xsane_create_slider(XsaneSlider *slider);
extern void xsane_create_histogram(GtkWidget *parent, const char *title, int width, int height, XsanePixmap *hist);
extern void xsane_get_free_gamma_curve(gfloat *free_color_gamma_data, SANE_Int *gammadata,
                                       int negative, double gamma, double brightness, double contrast,
                                       int len, int maxout);
extern void xsane_calculate_raw_histogram(void);
extern void xsane_calculate_end_histogram(void);
extern void xsane_update_histogram(int update_raw);
extern void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data);
extern void xsane_create_gamma_curve(SANE_Int *gammadata,
                                     int negative, double gamma, double brightness, double contrast,
                                     double medium_shadow, double medium_highlight, double medium_gamma,
                                     int numbers, int maxout);
extern void xsane_update_gamma_curve(int update_raw);
extern void xsane_enhancement_by_gamma(void);
extern void xsane_enhancement_restore_default(void);
extern void xsane_enhancement_restore(void);
extern void xsane_enhancement_store(void);
extern void xsane_enhancement_by_histogram(int update_gamma);
extern void xsane_create_histogram_dialog(const char *devicetext);
extern void xsane_create_gamma_dialog(const char *devicetext);
extern void xsane_update_gamma_dialog(void);
extern void xsane_set_auto_enhancement(void);
extern void xsane_apply_medium_definition_as_enhancement(Preferences_medium_t *medium);
extern void xsane_set_medium(Preferences_medium_t *medium);

/* ---------------------------------------------------------------------------------------------------------------------- */
