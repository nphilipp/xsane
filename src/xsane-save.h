/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-save.h

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

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_cancel_save();
extern void xsane_convert_text_to_filename(char **filename);
extern void xsane_update_counter_in_filename(char **filename, int skip, int step, int min_counter_len);
extern void xsane_increase_counter_in_filename(char *filename, int skip);
extern void xsane_write_pnm_header(FILE *outfile, int pixel_width, int pixel_height, int bits);
extern int xsane_save_grayscale_image_as_lineart(FILE *outfile, FILE *imagefile, int pixel_width, int pixel_height);
extern int xsane_save_rotate_image(FILE *outfile, FILE *imagefile, int color, int bits,
                                   int *pixel_width_ptr, int *pixel_height_ptr, int rotation);
extern void xsane_save_ps(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height,
                          int left, int bottom, float width, float height, int paperwidth, int paperheight, int landscape);
extern void xsane_save_jpeg(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height, int quality);
extern void xsane_save_tiff(const char *outfilename, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height, int quality);
extern void xsane_save_png(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height, int compression);
extern void xsane_save_png_16(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height, int compression);
extern void xsane_save_pnm_16(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height);

/* ---------------------------------------------------------------------------------------------------------------------- */
