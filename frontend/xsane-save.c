/* xsane
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

#include <sane/config.h>
#include <xsane-back-gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xsane.h"
#include "xsane-preview.h"
#include "xsane-front-gtk.h"

#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static int cancel_save;

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel_save()
{
  cancel_save = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_convert_text_to_filename(char **text)
{
  if (text)
  {
   char *filename = *text;
   char buf[256];
   int buflen=0;
   int txtlen=0;

    while((filename[txtlen] != 0) && (buflen<253))
    {
      switch (filename[txtlen])
      {
        case ' ':
          buf[buflen++] = ':';
          buf[buflen++] = '_';
          txtlen++;
          break;

        case '/':
          buf[buflen++] = ':';
          buf[buflen++] = '%';
          txtlen++;
          break;

        case '*':
          buf[buflen++] = ':';
          buf[buflen++] = '#';
          txtlen++;
          break;

        case '?':
          buf[buflen++] = ':';
          buf[buflen++] = 'q';
          txtlen++;
          break;

        case '\\':
          buf[buflen++] = ':';
          buf[buflen++] = '=';
          txtlen++;
          break;

        case ';':
          buf[buflen++] = ':';
          buf[buflen++] = '!';
          txtlen++;
          break;

        case '&':
          buf[buflen++] = ':';
          buf[buflen++] = '+';
          txtlen++;
          break;

        case '<':
          buf[buflen++] = ':';
          buf[buflen++] = 's';
          txtlen++;
          break;

        case '>':
          buf[buflen++] = ':';
          buf[buflen++] = 'g';
          txtlen++;
          break;

        case '|':
          buf[buflen++] = ':';
          buf[buflen++] = 'p';
          txtlen++;
          break;

        case ':':
          buf[buflen++] = ':';
          buf[buflen++] = ':';
          txtlen++;
          break;

        default:
          buf[buflen++] = filename[txtlen++];
          break;
      }
    }
    buf[buflen] = 0;
    free(filename);
    *text = strdup(buf);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_increase_counter_in_filename(char *filename, int skip)
{
 char *position_point;
 char *position_counter;
 char counter;
 FILE *testfile;

  while (1)
  { 
    position_point = strrchr(filename, '.');
    if (position_point)
    {
      position_counter = position_point-1;
    }
    else
    {
      position_counter = filename + strlen(filename) - 1;
    }
  
    if (!( (*position_counter >= '0') && (*position_counter <='9') ))
    {
      break; /* no counter found */
    }

    while ( (position_counter > filename) && (*position_counter >= '0') && (*position_counter <='9') )
    {
      counter = ++(*position_counter);
      if (counter !=  ':')
      {
        break;
      }
      *position_counter = '0';
      position_counter--;
    }

    if (!( (*position_counter >= '0') && (*position_counter <='9') )) /* overflow */
    {
     char buf[256];

      snprintf(buf, sizeof(buf), "Filename counter overflow\n");
      gsg_warning(buf);
      break; /* last available number ("999") */
    }

    if (skip) /* test if filename already used */
    {
      testfile = fopen(filename, "r");
      if (testfile) /* filename used: skip */
      {
        fclose(testfile);
      }
      else
      {
        break; /* filename not used, ok */
      }
    }
    else /* do not test if filename already used */
    {
      break; /* filename ok */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_bw(FILE *outfile, FILE *imagefile,
                            int pixel_width, int pixel_height,
                            int left, int bottom,
                            float width, float height)
{
 int x, y, count;
 int bytes_per_line = (pixel_width+7)/8;

  cancel_save = 0;

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: xsane version %s (sane %d.%d)\n", VERSION,
                                                                   SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                                                   SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
          left, bottom,(int) (left + width * 72.0),(int) (bottom + height * 72.0));
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");
  fprintf(outfile, "/pix %d string def\n", (pixel_width+7)/8);
  fprintf(outfile, "/grays %d string def\n", pixel_width);
  fprintf(outfile, "/npixels 0 def\n");
  fprintf(outfile, "/rgbindx 0 def\n");
  fprintf(outfile, "%d %d translate\n", left, bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d 1\n", pixel_width, pixel_height);
  fprintf(outfile, "[%d %d %d %d %d %d]\n",pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");
  fprintf(outfile, "image\n");
  fprintf(outfile, "\n");
 
  count = 0;
  for (y = 0; y < pixel_height; y++)
  {
    for (x = 0; x < bytes_per_line; x++)
    {
      fprintf(outfile, "%02x", (fgetc(imagefile) ^ 255));
      if (++count >= 40)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;
    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    if (cancel_save)
    {
      break;
    }
  }
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_gray(FILE *outfile, FILE *imagefile,
                               int bits,
                               int pixel_width, int pixel_height,
                               int left, int bottom,
                               float width, float height)
{
 int x, y, count;

  cancel_save = 0;

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: xsane version %s (sane %d.%d)\n", VERSION,
                                                                   SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                                                   SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
          left, bottom,(int) (left + width * 72.0),(int) (bottom + height * 72.0));
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");
  fprintf(outfile, "/pix %d string def\n", pixel_width);
  fprintf(outfile, "%d %d translate\n", left, bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d %d\n", pixel_width, pixel_height, bits);
  fprintf(outfile, "[%d %d %d %d %d %d]\n",pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");
  fprintf(outfile, "image\n");
  fprintf(outfile, "\n");
 
  count = 0;
  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fprintf(outfile, "%02x", fgetc(imagefile));
      if (++count >=40)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;
    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    if (cancel_save)
    {
      break;
    }
  }
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_color(FILE *outfile, FILE *imagefile,
                                int bits,
                                int pixel_width, int pixel_height,
                                int left, int bottom,
                                float width, float height)
{
 int x, y, count;

  cancel_save = 0;

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: xsane version %s (sane %d.%d)\n", VERSION,
                                                                   SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                                                   SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
          left, bottom,(int) (left + width * 72.0),(int) (bottom + height * 72.0));
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");
  fprintf(outfile, "/pix %d string def\n", pixel_width * 3);
  fprintf(outfile, "%d %d translate\n", left, bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d %d\n", pixel_width, pixel_height, bits);
  fprintf(outfile, "[%d %d %d %d %d %d]\n",pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");
  fprintf(outfile, "false 3 colorimage\n");
  fprintf(outfile, "\n");
 
  count = 0;
  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fprintf(outfile, "%02x", fgetc(imagefile));
      fprintf(outfile, "%02x", fgetc(imagefile));
      fprintf(outfile, "%02x", fgetc(imagefile));
      if (++count >=10)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;

    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    if (cancel_save)
    {
      break;
    }
  }
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_ps(FILE *outfile, FILE *imagefile,
                   int color, int bits,
                   int pixel_width, int pixel_height,
                   int left, int bottom,
                   float width, float height)
{
  if (color == 0) /* lineart, halftone, grayscale */
  {
    if (bits == 1) /* lineart, halftone */
    {
      xsane_save_ps_bw(outfile, imagefile, pixel_width, pixel_height, left, bottom, width, height);
    }
    else /* grayscale */
    {
      xsane_save_ps_gray(outfile, imagefile, bits, pixel_width, pixel_height, left, bottom, width, height);
    }
  }
  else /* color */
  {
    xsane_save_ps_color(outfile, imagefile, bits, pixel_width, pixel_height, left, bottom, width, height);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBJPEG
void xsane_save_jpeg(FILE *outfile, FILE *imagefile,
                     int color, int bits,
                     int pixel_width, int pixel_height,
                     int quality)
{
 char *data;
 char buf[256];
 int x,y;
 int components = 1;
 struct jpeg_compress_struct cinfo;
 struct jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];

  cancel_save = 0;

  if (color)
  {
    components = 3;
  }

  data = malloc(pixel_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "Error during save: out of memory\n");
    gsg_error(buf);
    return;
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);
  cinfo.image_width      = pixel_width;
  cinfo.image_height     = pixel_height;
  cinfo.input_components = components;
  if (color)
  {
    cinfo.in_color_space   = JCS_RGB;
  }
  else
  {
    cinfo.in_color_space   = JCS_GRAYSCALE;
  }
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  for (y=0; y<pixel_height; y++)
  {
    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    if (bits == 1)
    {
     int byte = 0;
     int mask = 128;

      for (x = 0; x < pixel_width; x++)
      {

        if ( (x % 8) == 0)
	{
          byte = fgetc(imagefile);
          mask = 128;
	}

        if (byte & mask)
        {
          data[x] = 0;
        }
        else
        {
          data[x] = 255;
        }
        mask >>= 1;
      }
    }
    else
    {
      fread(data, components, pixel_width, imagefile);
    }
    row_pointer[0] = data;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
    if (cancel_save)
    {
      break;
    }
  }

  jpeg_finish_compress(&cinfo);
  free(data);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBTIFF
void xsane_save_tiff(const char *outfilename, FILE *imagefile,
                     int color, int bits,
                     int pixel_width, int pixel_height,
                     int compression)
{
 TIFF *tiffile;
 char *data;
 char buf[256];
 int x, y, w;
 int components;

  cancel_save = 0;

  if (color)
  {
    components = 3;
  }
  else
  {
    components = 1;
  }

  tiffile = TIFFOpen(outfilename, "w");
  if (!tiffile)
  {
    snprintf(buf, sizeof(buf), "Error during save: TIFFOpen did not open file %s\n", outfilename);
    gsg_error(buf);
    return;
  }

  data = malloc(pixel_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "Error during save: out of memory\n");
    gsg_error(buf);
    return;
  }
  
  TIFFSetField(tiffile, TIFFTAG_IMAGEWIDTH, pixel_width);
  TIFFSetField(tiffile, TIFFTAG_IMAGELENGTH, pixel_height);
  TIFFSetField(tiffile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE, 8); /* change this if >8 bits is supported */
  TIFFSetField(tiffile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tiffile, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
  TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL, components);

  if (color)
  {
    TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  }
  else
  {
    TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  }

  w = TIFFScanlineSize(tiffile);
  TIFFSetField(tiffile, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiffile, -1));

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update(xsane.progress, (float) y / pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    
    if (bits == 1)
    {
     int byte = 0;
     int mask = 128;

      for (x = 0; x < pixel_width; x++)
      {

        if ( (x % 8) == 0)
	{
          byte = fgetc(imagefile);
          mask = 128;
	}

        if (byte & mask)
        {
          data[x] = 0;
        }
        else
        {
          data[x] = 255;
        }
        mask >>= 1;
      }
    }
    else
    {
      fread(data, components, pixel_width, imagefile);
    }
    TIFFWriteScanline(tiffile, data, y, 0);
    if (cancel_save)
    {
      break;
    }
  }

  TIFFClose(tiffile);
  free(data);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
void xsane_save_png(FILE *outfile, FILE *imagefile,
                    int color, int bits,
                    int pixel_width, int pixel_height,
                    int compression)
{
 png_structp png_ptr;
 png_infop png_info_ptr;
 png_bytep row_ptr;
 png_color_8 sig_bit;
 char *data;
 char buf[256];
 int colortype, components;
 int y;

  cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "Error during save: could not create png write structure\n");
    gsg_error(buf);
    return;
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "Error during save: could not create png info structure\n");
    gsg_error(buf);
    return;
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "Error during save: could not setjmp for png routine\n");
    gsg_error(buf);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  if (color == 4) /* RGBA */
  {
    components = 4;
    colortype = PNG_COLOR_TYPE_RGB_ALPHA;
  }
  else if (color) /* RGB */
  {
    components = 3;
    colortype = PNG_COLOR_TYPE_RGB;
  }
  else /* gray or black/white */
  {
    components = 1;
    colortype = PNG_COLOR_TYPE_GRAY;
  }
  
  png_init_io(png_ptr, outfile);
  png_set_compression_level(png_ptr, compression);
  png_set_IHDR(png_ptr, png_info_ptr, pixel_width, pixel_height, bits,
               colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  sig_bit.red   = bits;
  sig_bit.green = bits;
  sig_bit.blue  = bits;
  sig_bit.alpha = bits;
  sig_bit.gray  = bits;

  png_set_sBIT(png_ptr, png_info_ptr, &sig_bit);
  png_write_info(png_ptr, png_info_ptr);
  png_set_shift(png_ptr, &sig_bit);
  png_set_packing(png_ptr);

  data = malloc(pixel_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "Error during save: out of memory\n");
    gsg_error(buf);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update(xsane.progress, (float) y / pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    fread(data, components, pixel_width, imagefile);

    row_ptr = data;
    png_write_rows(png_ptr, &row_ptr, 1);
    if (cancel_save)
    {
      break;
    }
  }

  free(data);
  png_write_end(png_ptr, png_info_ptr);
  png_destroy_write_struct(&png_ptr, (png_infopp) 0);

}
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
void xsane_save_png_16(FILE *outfile, FILE *imagefile,
                       int color, int bits,
                       int pixel_width, int pixel_height,
                       int compression)
{
 png_structp png_ptr;
 png_infop png_info_ptr;
 png_bytep row_ptr;
 png_color_8 sig_bit; /* should be 16, but then I get a warning about wrong type */
 char *data;
 char buf[256];
 int colortype, components;
 int x,y;
 guint16 val;

  cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "Error during save: could not create png write structure\n");
    gsg_error(buf);
    return;
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "Error during save: could not create png info structure\n");
    gsg_error(buf);
    return;
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "Error during save: could not setjmp for png routine\n");
    gsg_error(buf);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  if (color == 4) /* RGBA */
  {
    components = 4;
    colortype = PNG_COLOR_TYPE_RGB_ALPHA;
  }
  else if (color) /* RGB */
  {
    components = 3;
    colortype = PNG_COLOR_TYPE_RGB;
  }
  else /* gray or black/white */
  {
    components = 1;
    colortype = PNG_COLOR_TYPE_GRAY;
  }
  
  png_init_io(png_ptr, outfile);
  png_set_compression_level(png_ptr, compression);
  png_set_IHDR(png_ptr, png_info_ptr, pixel_width, pixel_height, 16,
               colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  sig_bit.red   = bits;
  sig_bit.green = bits;
  sig_bit.blue  = bits;
  sig_bit.alpha = bits;
  sig_bit.gray  = bits;

  png_set_sBIT(png_ptr, png_info_ptr, &sig_bit);
  png_write_info(png_ptr, png_info_ptr);
  png_set_shift(png_ptr, &sig_bit);
  png_set_packing(png_ptr);

  data = malloc(pixel_width * components * 2);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "Error during save: out of memory\n");
    gsg_error(buf);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    for (x = 0; x < pixel_width * components; x++) /* this must be changed in dependance of endianess */
    {
      fread(&val, 2, 1, imagefile);	/* get data in machine order */
      data[x*2+0] = val/256;		/* write data in network order (MSB first) */
      data[x*2+1] = val & 255;
    }

    row_ptr = data;
    png_write_rows(png_ptr, &row_ptr, 1);
    if (cancel_save)
    {
      break;
    }
  }

  free(data);
  png_write_end(png_ptr, png_info_ptr);
  png_destroy_write_struct(&png_ptr, (png_infopp) 0);
}
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pnm_16_gray(FILE *outfile, FILE *imagefile, int bits, int pixel_width, int pixel_height)
{
 int x,y;
 guint16 val;
 int count = 0;

  cancel_save = 0;

  /* write pgm ascii > 8 bpp */
  fprintf(outfile, "P2\n# SANE data follows\n%d %d\n65535\n", pixel_width, pixel_height);

  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fread(&val, 2, 1, imagefile); /* get data in machine order */
      fprintf(outfile, "%d ", val);

      if (++count >= 10)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;

    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    if (cancel_save)
    {
      break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pnm_16_color(FILE *outfile, FILE *imagefile, int bits, int pixel_width, int pixel_height)
{
 int x,y;
 guint16 val;
 int count = 0;

  cancel_save = 0;

  /* write ppm ascii > 8 bpp */
  fprintf(outfile, "P3\n# SANE data follows\n%d %d\n65535\n", pixel_width, pixel_height);

  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fread(&val, 2, 1, imagefile); /* get data in machine order */
      fprintf(outfile, "%d ", val);

      fread(&val, 2, 1, imagefile);
      fprintf(outfile, "%d ", val);

      fread(&val, 2, 1, imagefile);
      fprintf(outfile, "%d  ", val);

      if (++count >= 3)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;

    xsane_progress_update(xsane.progress, (float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    if (cancel_save)
    {
      break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pnm_16(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height)
{
  if (color)
  {
    xsane_save_pnm_16_color(outfile, imagefile, bits, pixel_width, pixel_height);
  }
  else
  {
    xsane_save_pnm_16_gray(outfile, imagefile, bits, pixel_width, pixel_height);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */
