/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998,1999 Oliver Rauch
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
extern void xsane_create_histogram(GtkWidget *parent, char *title, int width, int height, XsanePixmap *hist);
extern void xsane_destroy_histogram(void);
extern void xsane_calculate_histogram(void);
extern void xsane_update_histogram(void);
extern void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data);
extern void xsane_create_gamma_curve(SANE_Int *gammadata, int negative, double gamma, 
                                     double brightness, double contrast, int numbers, int maxout);
extern void xsane_update_gamma(void);
extern void xsane_enhancement_by_gamma(void);
extern void xsane_enhancement_restore_default(void);
extern void xsane_enhancement_restore_saved(void);
extern void xsane_enhancement_save(void);
extern void xsane_enhancement_by_histogram(void);
extern void xsane_create_histogram_dialog(char *devicetext);

/* ---------------------------------------------------------------------------------------------------------------------- */
