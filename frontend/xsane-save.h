/* xsane
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998,1999 Oliver Rauch

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

void xsane_cancel_save();

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_convert_text_to_filename(char **filename);

/* ---------------------------------------------------------------------------------------------------------------------- */


void xsane_increase_counter_in_filename(char *filename, int skip);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_ps(FILE *outfile, FILE *imagefile,
                   int color, int bits,
                   int pixel_width, int pixel_height,
                   int left, int bottom,
                   float width, float height);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_jpeg(FILE *outfile, FILE *imagefile,
                     int color, int bits,
                     int pixel_width, int pixel_height,
                     int quality);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_tiff(const char *outfilename, FILE *imagefile,
                     int color, int bits,
                     int pixel_width, int pixel_height,
                     int compression);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_png(FILE *outfile, FILE *imagefile,
                    int color, int bits,
                    int pixel_width, int pixel_height,
                    int compression);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_png_16(FILE *outfile, FILE *imagefile,
                       int color, int bits,
                       int pixel_width, int pixel_height,
                       int compression);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pnm_16(FILE *outfile, FILE *imagefile,
                       int color, int bits,
                       int pixel_width, int pixel_height);

/* ---------------------------------------------------------------------------------------------------------------------- */
