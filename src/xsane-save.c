/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-save.c

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

#include "xsane.h"
#include "xsane-preview.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

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
#include <time.h>
#endif

#ifdef HAVE_MMAP
#include <unistd.h>
#include <sys/mman.h>
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
 
#include <libgimp/gimp.h>
 
static void xsane_gimp_query(void);
static void xsane_gimp_run(char *name, int nparams, GimpParam * param, int *nreturn_vals, GimpParam ** return_vals);
 
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,                         /* init_proc */
  NULL,                         /* quit_proc */
  xsane_gimp_query,             /* query_proc */
  xsane_gimp_run,               /* run_proc */
};


static int xsane_decode_devname(const char *encoded_devname, int n,
char *buf);
static int xsane_encode_devname(const char *devname, int n, char *buf);
void null_print_func(gchar *msg);
 
#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

static int cancel_save;

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel_save()
{
  DBG(DBG_proc, "xsane_cancel_save\n");
  cancel_save = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_convert_text_to_filename(char **text)
{
  DBG(DBG_proc, "xsane_convert_text_to_filename\n");

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
    DBG(DBG_info, "filename = \"%s\"\n", *text);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_counter_in_filename(char **filename, int skip, int step, int min_counter_len)
{
 FILE *testfile;
 char *position_point = NULL;
 char *position_counter;
 char buf[PATH_MAX];
 int counter;
 int counter_len;
 int set_counter_len = min_counter_len;

  DBG(DBG_proc, "xsane_update_counter_in_filename\n");

  while (1) /* may  be we have to skip existing files */
  {
    if (!xsane.filetype) /* no filetype: serach "." */
    {
      position_point = strrchr(*filename, '.');
    }

    if (!position_point) /* nothing usable ? */
    {
      position_point = *filename + strlen(*filename); /* here is no point, but position - 1 is last character */
    }

    if (position_point)
    {
      position_counter = position_point-1; /* go to last number of counter (if counter exists) */

      /* search non numeric char */
      while ( (position_counter >= *filename) && (*position_counter >= '0') && (*position_counter <='9') )
      {
        position_counter--; /* search fisrt numeric character */
      }

      position_counter++; /* go to first numeric charcter */

      counter_len = position_point - position_counter;

      if (counter_len) /* we have a counter */
      {
        sscanf(position_counter, "%d", &counter);
        counter += step; /* update counter */

        if (counter < 0)
        {
          counter = 0;
          xsane_back_gtk_warning(WARN_COUNTER_UNDERRUN, TRUE);
          break; /* last available number ("..999") */
        }

        *position_counter = 0; /* set end of string mark to counter start */

        if (set_counter_len == 0)
        {
           set_counter_len = counter_len;
        }

        snprintf(buf, sizeof(buf), "%s%0*d%s", *filename, set_counter_len,  counter, position_point);

        DBG(DBG_info, "filename = \"%s\"\n", buf);

        free(*filename);
        *filename = strdup(buf);

        if (skip) /* test if filename already used */
        {
          if (xsane.filetype) /* add filetype to filename */
          {
            snprintf(buf, sizeof(buf), "%s%s", *filename, xsane.filetype);
            testfile = fopen(buf, "rb"); /* read binary (b for win32) */
          }
          else /* filetype in filename */
          {
            testfile = fopen(*filename, "rb"); /* read binary (b for win32) */
          }

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
      else /* no counter */
      {
        break; /* no counter */
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_write_pnm_header(FILE *outfile, int pixel_width, int pixel_height, int bits)
{
  rewind(outfile);

  switch (xsane.param.format)
  {
    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
      switch (bits)
      {
         case 8: /* color 8 bit mode, write ppm header */
           fprintf(outfile, "P6\n"
                            "# XSane settings:\n"
                            "#  gamma      IRGB = %3.2f %3.2f %3.2f %3.2f\n"
                            "#  brightness IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                            "#  contrast   IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                            "# XSANE data follows\n"
                            "%05d %05d\n255\n",
                             xsane.gamma,      xsane.gamma_red,      xsane.gamma_green,      xsane.gamma_blue,
                             xsane.brightness, xsane.brightness_red, xsane.brightness_green, xsane.brightness_blue,
                             xsane.contrast,   xsane.contrast_red,   xsane.contrast_green,   xsane.contrast_blue,
                             pixel_width, pixel_height);
          break;

         default: /* color, but not 8 bit mode, write as raw data because this is not defined in pnm */
           fprintf(outfile, "P6\n"
                            "# This file is in a not public defined data format.\n"
                            "# It is a 16 bit RGB binary format.\n"
                            "# Some programs can read this as pnm/ppm format.\n"
                            "# File created by XSane.\n"
                            "# XSane settings:\n"
                            "#  gamma      IRGB = %3.2f %3.2f %3.2f %3.2f\n"
                            "#  brightness IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                            "#  contrast   IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                            "# XSANE data follows.\n"
                            "%05d %05d\n"
                            "65535\n",
                            xsane.gamma,      xsane.gamma_red,      xsane.gamma_green,      xsane.gamma_blue,
                            xsane.brightness, xsane.brightness_red, xsane.brightness_green, xsane.brightness_blue,
                            xsane.contrast,   xsane.contrast_red,   xsane.contrast_green,   xsane.contrast_blue,
                            pixel_width, pixel_height);
          break;
      }
      break;
 
    case SANE_FRAME_GRAY:
      switch (bits)
      {
         case 1: /* 1 bit lineart mode, write pbm header */
           fprintf(outfile, "P4\n"
                            "# XSane settings:\n"
                            "#  threshold = %4.1f\n"
                            "# XSANE data follows\n"
                            "%05d %05d\n",
                            xsane.threshold,
                            pixel_width, pixel_height);
          break;

         case 8: /* 8 bit grayscale mode, write pgm header */
           fprintf(outfile, "P5\n"
                            "# XSane settings:\n"
                            "#  gamma      = %3.2f\n"
                            "#  brightness = %4.1f\n"
                            "#  contrast   = %4.1f\n"
                            "# XSANE data follows\n"
                            "%05d %05d\n"
                            "255\n",
                            xsane.gamma,
                            xsane.brightness,
                            xsane.contrast,
                            pixel_width, pixel_height);
          break;

         default: /* grayscale mode but not 1 or 8 bit, write as raw data because this is not defined in pnm */
           fprintf(outfile, "P5\n"
                            "# This file is in a not public defined data format.\n"
                            "# It is a 16 bit gray binary format.\n"
                            "# Some programs can read this as pnm/pgm format.\n"
                            "# XSane settings:\n"
                            "#  gamma      = %3.2f\n"
                            "#  brightness = %4.1f\n"
                            "#  contrast   = %4.1f\n"
                            "# XSANE data follows.\n"
                            "%05d %05d\n"
                            "65535\n",
                            xsane.gamma,
                            xsane.brightness,
                            xsane.contrast,
                            pixel_width, pixel_height);
          break;
      }
      break;

#ifdef SUPPORT_RGBA
    case SANE_FRAME_RGBA:
      switch (bits)
      {
         case 8: /* 8 bit RGBA mode */
           fprintf(outfile, "SANE_RGBA\n%d %d\n255\n", pixel_width, pixel_height);
          break;

         default: /* 16 bit RGBA mode */
           fprintf(outfile, "SANE_RGBA\n%d %d\n65535\n", pixel_width, pixel_height);
          break;
      }
      break;
#endif
 
     default:
     /* unknown file format, do not write header */
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_grayscale_image_as_lineart(FILE *outfile, FILE *imagefile, int pixel_width, int pixel_height)
{
 int x, y, bit;
 u_char bitval, packed;

  cancel_save = 0;

  xsane_write_pnm_header(outfile, pixel_width, pixel_height, 1 /* bits */);
  xsane.header_size = ftell(outfile);
  xsane.depth = 1; /* our new depth is 1 bit/pixel */

  for (y = 0; y < pixel_height; y++)
  {
    bit = 128;
    packed = 0;

    for (x = 0; x < pixel_width; x++)
    {
      bitval = fgetc(imagefile);

      if (!bitval) /* white gets 0 bit, black gets 1 bit */
      {
        packed |= bit;
      }

      if (bit == 1)
      {
        fputc(packed, outfile);
        bit = 128;
        packed = 0;
      }
      else
      {
        bit >>= 1;
      }
    }

    if (bit != 128)
    {
      fputc(packed, outfile);
      bit = 128;
      packed = 0;
    }

    xsane_progress_update((float) y / pixel_height); /* update progress bar */
    while (gtk_events_pending()) /* give gtk the chance to display the changes */
    {
      gtk_main_iteration();
    }

    if (cancel_save)
    {
      break;
    }
  }
 
 return (cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_rotate_image(FILE *outfile, FILE *imagefile, int color, int bits,
                            int *pixel_width_ptr, int *pixel_height_ptr, int rotation)
/* returns true if operation was cancelled */
{
 int x, y, pos0, bytespp, i;
 int pixel_width  = *pixel_width_ptr;
 int pixel_height = *pixel_height_ptr;
#ifdef HAVE_MMAP
 char *mmaped_imagefile = NULL;
#endif

  DBG(DBG_proc, "xsane_save_rotate_image\n");

  cancel_save = 0;

  pos0 = xsane.header_size; /* mark position to skip header */

  bytespp = 1;

  if (color)
  {
    bytespp *= 3;
  }

  if (bits > 8)
  {
    bytespp *= 2;
  }

  if (bits < 8) /* lineart images are expanded to grayscale until transformation is done */
  {
    bits = 8; /* so we have at least 8 bits/pixel here */
  }

#ifdef HAVE_MMAP
  mmaped_imagefile = mmap(NULL, pixel_width * pixel_height * bytespp + pos0, PROT_READ, MAP_PRIVATE, fileno(imagefile), 0);
  if (mmaped_imagefile == (void *) -1) /* mmap failed */
  {
    DBG(DBG_info, "xsane_save_rotate_image: unable to memory map image file, using standard file access\n");
    mmaped_imagefile = NULL;
  }
  else
  {
    DBG(DBG_info, "xsane_save_rotate_image: using memory mapped image file\n");
  }
#endif

  switch (rotation)
  {
    case 0: /* 0 degree */
    default:
     break;

    case 1: /* 90 degree */
      *pixel_width_ptr  = pixel_height;
      *pixel_height_ptr = pixel_width;

      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (x=0; x<pixel_width; x++)
      {
        xsane_progress_update((float) x / pixel_width);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (y=pixel_height-1; y>=0; y--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width); /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }

     break;

    case 2: /* 180 degree */
      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (y=pixel_height-1; y>=0; y--)
      {
        xsane_progress_update((float) (pixel_height - y) / pixel_height);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (x=pixel_width-1; x>=0; x--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }
     break;

    case 3: /* 270 degree */
      *pixel_width_ptr  = pixel_height;
      *pixel_height_ptr = pixel_width;

      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (x=pixel_width-1; x>=0; x--)
      {
        xsane_progress_update((float) (pixel_width - x) / pixel_width);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (y=0; y<pixel_height; y++)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }
     break;

    case 4: /* 0 degree, x mirror */
      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (y=0; y<pixel_height; y++)
      {
        xsane_progress_update((float) y / pixel_height);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (x=pixel_width-1; x>=0; x--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }
     break;
     break;

    case 5: /* 90 degree, x mirror */
      *pixel_width_ptr  = pixel_height;
      *pixel_height_ptr = pixel_width;

      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (x=0; x<pixel_width; x++)
      {
        xsane_progress_update((float) x / pixel_width);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (y=0; y<pixel_height; y++)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width); /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }

     break;

    case 6: /* 180 degree, x mirror */
      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (y=pixel_height-1; y>=0; y--)
      {
        xsane_progress_update((float) (pixel_height - y) / pixel_height);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (x=0; x<pixel_width; x++)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }
     break;

    case 7: /* 270 degree, x mirror */
      *pixel_width_ptr  = pixel_height;
      *pixel_height_ptr = pixel_width;

      xsane_write_pnm_header(outfile, *pixel_width_ptr, *pixel_height_ptr, bits);
      xsane.header_size = ftell(outfile);

      for (x=pixel_width-1; x>=0; x--)
      {
        xsane_progress_update((float) (pixel_width - x) / pixel_width);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        for (y=pixel_height-1; y>=0; y--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i=0; i<bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i=0; i<bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (cancel_save)
        {
          break;
        }
      }
     break;
  }

#ifdef HAVE_MMAP
  if (mmaped_imagefile)
  {
    munmap(mmaped_imagefile, pos0 + pixel_width * pixel_height * bytespp);
  }
#endif

  return (cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_create_header(FILE *outfile, int color, int bits, int pixel_width, int pixel_height,
                                        int left, int bottom, float width, float height,
                                        int paperwidth, int paperheight, int rotate)
{
 int degree, position_left, position_bottom, box_left, box_bottom, box_right, box_top;

  DBG(DBG_proc, "xsane_save_ps_create_header\n");

  if (rotate) /* rotate with 90 degrees - eg for landscape mode */
  {
    degree = 90;
    position_left   = left;
    position_bottom = bottom - paperwidth;
    box_left        = paperwidth - bottom - height * 72.0;
    box_bottom      = left;
    box_right       = (int) (box_left   + height * 72.0);
    box_top         = (int) (box_bottom + width  * 72.0);
  }
  else /* do not rotate, eg for portrait mode */
  {
    degree = 0;
    position_left   = left;
    position_bottom = bottom;
    box_left        = left;
    box_bottom      = bottom;
    box_right       = (int) (box_left   + width  * 72.0);
    box_top         = (int) (box_bottom + height * 72.0);
  }

  if (bits == 16)
  {
    bits = 8; /* 16 bits/color are already reduced to 8 bits/color */
  }

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: xsane version %s (sane %d.%d)\n", VERSION,
                                                                   SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                                                   SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n", box_left, box_bottom, box_right, box_top);
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");

  if (bits == 1)
  {
    fprintf(outfile, "/pix %d string def\n", (pixel_width+7)/8);
    fprintf(outfile, "/grays %d string def\n", pixel_width);
    fprintf(outfile, "/npixels 0 def\n");
    fprintf(outfile, "/rgbindx 0 def\n");
  }
  else
  {
    fprintf(outfile, "/pix %d string def\n", pixel_width);
  }


  fprintf(outfile, "%d rotate\n", degree);
  fprintf(outfile, "%d %d translate\n", position_left, position_bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d %d\n", pixel_width, pixel_height, bits);
  fprintf(outfile, "[%d %d %d %d %d %d]\n", pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");

  if (color)
  {
    fprintf(outfile, "false 3 colorimage\n");
    fprintf(outfile, "\n");
  }
  else
  {
    fprintf(outfile, "image\n");
    fprintf(outfile, "\n");
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_bw(FILE *outfile, FILE *imagefile, int pixel_width, int pixel_height)
{
 int x, y, count;
 int bytes_per_line = (pixel_width+7)/8;

  DBG(DBG_proc, "xsane_save_ps_bw\n");

  cancel_save = 0;

  count = 0;
  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update((float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

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
    if (cancel_save)
    {
      break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_gray(FILE *outfile, FILE *imagefile, int pixel_width, int pixel_height)
{
 int x, y, count;

  DBG(DBG_proc, "xsane_save_ps_gray\n");

  cancel_save = 0;

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
    xsane_progress_update((float)y/pixel_height);
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

static void xsane_save_ps_color(FILE *outfile, FILE *imagefile, int pixel_width, int pixel_height)
{
 int x, y, count;

  DBG(DBG_proc, "xsane_save_ps_color\n");

  cancel_save = 0;
 
  count = 0;
  for (y=0; y<pixel_height; y++)
  {
    xsane_progress_update((float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

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

    if (cancel_save)
    {
      break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_ps(FILE *outfile, FILE *imagefile,
                   int color, int bits,
                   int pixel_width, int pixel_height,
                   int left, int bottom,
                   float width, float height,
                   int paperheight, int paperwidth, int rotate)
{
  DBG(DBG_proc, "xsane_save_ps\n");

  xsane_save_ps_create_header(outfile, color, bits, pixel_width, pixel_height,
                              left, bottom, width, height, paperheight, paperwidth, rotate);

  if (color == 0) /* lineart, halftone, grayscale */
  {
    if (bits == 1) /* lineart, halftone */
    {
      xsane_save_ps_bw(outfile, imagefile, pixel_width, pixel_height);
    }
    else /* grayscale */
    {
      xsane_save_ps_gray(outfile, imagefile, pixel_width, pixel_height);
    }
  }
  else /* color */
  {
    xsane_save_ps_color(outfile, imagefile, pixel_width, pixel_height);
  }

  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBJPEG
void xsane_save_jpeg(FILE *outfile, FILE *imagefile,
                     int color, int bits,
                     int pixel_width, int pixel_height,
                     int quality)
{
 unsigned char *data;
 char buf[256];
 int x,y;
 int components = 1;
 struct jpeg_compress_struct cinfo;
 struct jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];

  DBG(DBG_proc, "xsane_save_jpeg\n");

  cancel_save = 0;

  if (color)
  {
    components = 3;
  }

  data = malloc(pixel_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
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
    xsane_progress_update((float)y/pixel_height);
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
                     int pixel_width, int pixel_height, int quality)
{
 TIFF *tiffile;
 char *data;
 char buf[256];
 int y, w;
 int components;
 int compression;
 int bytes;
 struct tm *ptm;
 time_t now;

  DBG(DBG_proc, "xsane_save_tiff\n");

  cancel_save = 0;

  if (bits == 1)
  {
    compression = preferences.tiff_compression1_nr;
  }
  else if (bits == 8)
  {
    compression = preferences.tiff_compression8_nr;
  }
  else
  {
    compression = preferences.tiff_compression16_nr;
  }


  if (color)
  {
    components = 3;
  }
  else
  {
    components = 1;
  }

  if (bits <= 8)
  {
    bytes = 1;
  }
  else
  {
    bytes = 2;
  }

  tiffile = TIFFOpen(outfilename, "w");
  if (!tiffile)
  {
    snprintf(buf, sizeof(buf), "%s %s %s\n",ERR_DURING_SAVE, ERR_OPEN_FAILED, outfilename);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

#if 0
  data = malloc(pixel_width * components * bytes);
#else
  data = (char *)_TIFFmalloc(pixel_width * components * bytes);
#endif

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }
  
  TIFFSetField(tiffile, TIFFTAG_IMAGEWIDTH, pixel_width);
  TIFFSetField(tiffile, TIFFTAG_IMAGELENGTH, pixel_height);
  TIFFSetField(tiffile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE, bits); 
  TIFFSetField(tiffile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tiffile, TIFFTAG_COMPRESSION, compression);
  TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL, components);
  TIFFSetField(tiffile, TIFFTAG_SOFTWARE, "xsane");

  time(&now);
  ptm = localtime(&now);
  sprintf(buf, "%04d:%02d:%02d %02d:%02d:%02d", 1900+ptm->tm_year, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  TIFFSetField(tiffile, TIFFTAG_DATETIME, buf);

  TIFFSetField(tiffile, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
  TIFFSetField(tiffile, TIFFTAG_XRESOLUTION, xsane.resolution_x);
  TIFFSetField(tiffile, TIFFTAG_YRESOLUTION, xsane.resolution_y);   

  if (compression == COMPRESSION_JPEG)
  {
    TIFFSetField(tiffile, TIFFTAG_JPEGQUALITY, quality);
    TIFFSetField(tiffile, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RAW); /* should be default, but to be sure */
  }

  if (color)
  {
    TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  }
  else
  {
    if (bits == 1) /* lineart */
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    }
    else /* grayscale */
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    }
  }

  TIFFSetField(tiffile, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiffile, -1));

  w = TIFFScanlineSize(tiffile);

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update((float) y / pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
    
    fread(data, 1, w, imagefile);

    TIFFWriteScanline(tiffile, data, y, 0);

    if (cancel_save)
    {
      break;
    }
  }

  TIFFClose(tiffile);
#if 0
  free(data);
#else
  _TIFFfree(data);
#endif
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
 unsigned char *data;
 char buf[256];
 int colortype, components, byte_width;
 int y;

  DBG(DBG_proc, "xsane_save_png\n");

  cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBTIFF);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBTIFF);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  byte_width = pixel_width;

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

  if (color >=3)
  {
    sig_bit.red   = bits;
    sig_bit.green = bits;
    sig_bit.blue  = bits;

    if (color ==4)
    {
      sig_bit.alpha = bits;
    }

  }
  else
  {
    sig_bit.gray  = bits;

    if (bits == 1)
    {
      byte_width = pixel_width/8;
      png_set_invert_mono(png_ptr);
    }
  }

  png_set_sBIT(png_ptr, png_info_ptr, &sig_bit);
  png_write_info(png_ptr, png_info_ptr);
  png_set_shift(png_ptr, &sig_bit);

  data = malloc(pixel_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update((float) y / pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    fread(data, components, byte_width, imagefile);

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
 unsigned char *data;
 char buf[256];
 int colortype, components;
 int x,y;
 guint16 val;

  DBG(DBG_proc, "xsane_save_png16\n");

  cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
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
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return;
  }

  for (y = 0; y < pixel_height; y++)
  {
    xsane_progress_update((float)y/pixel_height);
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

  DBG(DBG_proc, "xsane_save_pnm_16_gray\n");

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

    xsane_progress_update((float)y/pixel_height);
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

  DBG(DBG_proc, "xsane_save_pnm_16_color\n");

  cancel_save = 0;

  /* write ppm ascii > 8 bpp */
  fprintf(outfile, "P3\n# SANE data follows\n%d %d\n65535\n", pixel_width, pixel_height);

  for (y=0; y<pixel_height; y++)
  {
    xsane_progress_update((float)y/pixel_height);
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

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

    if (cancel_save)
    {
      break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pnm_16(FILE *outfile, FILE *imagefile, int color, int bits, int pixel_width, int pixel_height)
{
  DBG(DBG_proc, "xsane_save_pnm_16\n");

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
/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
static int xsane_decode_devname(const char *encoded_devname, int n, char *buf)
{
 char *dst, *limit;
 const char *src;
 char ch, val;

  DBG(DBG_proc, "xsane_decode_devname\n");

  limit = buf + n;
  for (src = encoded_devname, dst = buf; *src; ++dst)
  {
    if (dst >= limit)
    {
      return -1;
    }

    ch = *src++;
    /* don't use the ctype.h macros here since we don't want to allow anything non-ASCII here... */
    if (ch != '-')
    {
      *dst = ch;
    }
    else /* decode */
    {
      ch = *src++;
      if (ch == '-')
      {
        *dst = ch;
      }
      else
      {
        if (ch >= 'a' && ch <= 'f')
        {
          val = (ch - 'a') + 10;
        }
        else
        {
          val = (ch - '0');
        }
        val <<= 4;

        ch = *src++;
        if (ch >= 'a' && ch <= 'f')
        {
          val |= (ch - 'a') + 10;
        }
        else
        {
          val |= (ch - '0');
        }

        *dst = val;

        ++src;    /* simply skip terminating '-' for now... */
      }
    }
  }

  if (dst >= limit)
  {
    return -1;
  }

  *dst = '\0';
 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_encode_devname(const char *devname, int n, char *buf)
{
 static const char hexdigit[] = "0123456789abcdef";
 char *dst, *limit;
 const char *src;
 char ch;

  DBG(DBG_proc, "xsane_encode_devname\n");

  limit = buf + n;
  for (src = devname, dst = buf; *src; ++src)
  {
    if (dst >= limit)
    {
      return -1;
    }

    ch = *src;
    /* don't use the ctype.h macros here since we don't want to allow anything non-ASCII here... */
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
    {
      *dst++ = ch;
    }
    else /* encode */
    {
      if (dst + 4 >= limit)
      {
        return -1;
      }

      *dst++ = '-';
      if (ch == '-')
      {
        *dst++ = '-';
      }
      else
      {
        *dst++ = hexdigit[(ch >> 4) & 0x0f];
        *dst++ = hexdigit[(ch >> 0) & 0x0f];
        *dst++ = '-';
      }
    }
  }

  if (dst >= limit)
  {
    return -1;
  }

  *dst = '\0';
 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gimp_query(void)
{
 static GimpParamDef args[] =
 {
     {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
 };
 static GimpParamDef *return_vals = NULL;
 static int nargs = sizeof(args) / sizeof(args[0]);
 static int nreturn_vals = 0;
 char mpath[1024];
 char name[1024];
 size_t len;
 int i, j;

  DBG(DBG_proc, "xsane_gimp_query\n");

  snprintf(name, sizeof(name), "%s", xsane.prog_name);
#ifdef GIMP_CHECK_VERSION
# if GIMP_CHECK_VERSION(1,1,9)
  snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU_DIALOG);
# else
  snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU_DIALOG_OLD);
# endif
#else
  snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU_DIALOG_OLD);
#endif
  gimp_install_procedure(name,
			 XSANE_GIMP_INSTALL_BLURB,
			 XSANE_GIMP_INSTALL_HELP,
			 XSANE_AUTHOR,
			 XSANE_COPYRIGHT,
			 XSANE_DATE,
			 mpath,
			 0, /* "RGB, GRAY", */
			 GIMP_EXTENSION,
			 nargs, nreturn_vals,
			 args, return_vals);

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);
  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    DBG(DBG_error0, "\n\n"
                    "%s %s:\n"
                    "  %s\n"
                    "  %s %d\n"
                    "  %s %d\n"
                    "%s\n\n",
                     xsane.prog_name, ERR_ERROR,
                     ERR_MAJOR_VERSION_NR_CONFLICT,
                     ERR_XSANE_MAJOR_VERSION, SANE_V_MAJOR,
                     ERR_BACKEND_MAJOR_VERSION, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                     ERR_PROGRAM_ABORTED);      
    return;
  }

  sane_get_devices(&xsane.devlist, SANE_FALSE);

  for (i = 0; xsane.devlist[i]; ++i)
  {
    snprintf(name, sizeof(name), "%s-", xsane.prog_name);
    if (xsane_encode_devname(xsane.devlist[i]->name, sizeof(name) - 6, name + 6) < 0)
    {
      continue;	/* name too long... */
    }

#ifdef GIMP_CHECK_VERSION
# if GIMP_CHECK_VERSION(1,1,9)
    snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU);
# else
    snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU_OLD);
# endif
#else
    snprintf(mpath, sizeof(mpath), "%s", XSANE_GIMP_MENU_OLD);
#endif
    len = strlen(mpath);
    for (j = 0; xsane.devlist[i]->name[j]; ++j)
    {
      if (xsane.devlist[i]->name[j] == '/')
      {
        mpath[len++] = '\'';
      }
      else
      {
        mpath[len++] = xsane.devlist[i]->name[j];
      }
    }
    mpath[len++] = '\0';

    gimp_install_procedure(name,
                           XSANE_GIMP_INSTALL_BLURB,
                           XSANE_GIMP_INSTALL_HELP,
                           XSANE_AUTHOR,
                           XSANE_COPYRIGHT,
                           XSANE_DATE,
                           mpath,
                           0, /* "RGB, GRAY", */
                           GIMP_EXTENSION,
                           nargs, nreturn_vals,
                           args, return_vals);
  }

  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gimp_run(char *name, int nparams, GimpParam * param, int *nreturn_vals, GimpParam ** return_vals)
{
 static GimpParam values[2];
 GimpRunModeType run_mode;
 char devname[1024];
 char *args[2];
 int nargs;

  DBG(DBG_proc, "xsane_gimp_run\n");

  run_mode = param[0].data.d_int32;
  xsane.mode = XSANE_GIMP_EXTENSION;
  xsane.xsane_mode = XSANE_SCAN;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_CALLING_ERROR;

  nargs = 0;
  args[nargs++] = "xsane";

  xsane.selected_dev = -1;
  if (strncmp(name, "xsane-", 6) == 0)
  {
    if (xsane_decode_devname(name + 6, sizeof(devname), devname) < 0)
    {
      return;			/* name too long */
    }
    args[nargs++] = devname;
  }

  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      xsane_interface(nargs, args);
      values[0].data.d_status = GIMP_PDB_SUCCESS;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      break;

    default:
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void null_print_func(gchar *msg)
{
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_transfer_to_gimp(FILE *imagefile, int color, int bits, int pixel_width, int pixel_height)
{
 int remaining;
 size_t tile_size;
 GimpImageType image_type    = GIMP_GRAY;
 GimpImageType drawable_type = GIMP_GRAY_IMAGE;
 gint32 layer_ID;
 int i;

  DBG(DBG_info, "xsane_transer_to_gimp(color=%d, bits=%d, pixel_width=%d, pixel_height=%d)\n",
                 color, bits, pixel_width, pixel_height);

  cancel_save = 0;

  xsane.x = xsane.y = 0; 
  xsane.tile_offset = 0;
  tile_size = pixel_width * gimp_tile_height();

  if (color == 3) /* RGB */
  {
    tile_size *= 3;  /* 24 bits/pixel RGB */
    image_type    = GIMP_RGB;
    drawable_type = GIMP_RGB_IMAGE;
  }
  else if (color == 4) /* RGBA */
  {
    tile_size *= 4;  /* 32 bits/pixel RGBA */
    image_type    = GIMP_RGB;
    drawable_type = GIMP_RGBA_IMAGE; /* interpret infrared as alpha */
  }
  /* color == 0 is predefined */
 
  xsane.image_ID = gimp_image_new(pixel_width, pixel_height, image_type);
 
/* the following is supported since gimp-1.1.? */
#ifdef GIMP_HAVE_RESOLUTION_INFO
  if (xsane.resolution_x > 0)
  {
    gimp_image_set_resolution(xsane.image_ID, xsane.resolution_x ,xsane.resolution_y);
  }
/*          gimp_image_set_unit(xsane.image_ID, unit?); */
#endif
 
  layer_ID = gimp_layer_new(xsane.image_ID, "Background", pixel_width, pixel_height, drawable_type, 100.0, GIMP_NORMAL_MODE);
  gimp_image_add_layer(xsane.image_ID, layer_ID, 0);
  xsane.drawable = gimp_drawable_get(layer_ID);
  gimp_pixel_rgn_init(&xsane.region, xsane.drawable, 0, 0, xsane.drawable->width, xsane.drawable->height, TRUE, FALSE);
  xsane.tile = g_new(guchar, tile_size);
 

  if (!color) /* gray */
  {
    switch (bits)
    {
      case 1: /* 1 bit gray => conversion to 8 bit gray */
        for (i = 0; i < ((pixel_width+7)/8)*pixel_height; ++i)
        {
         u_char mask;
         int j;

          mask = fgetc(imagefile);
          for (j = 7; j >= 0; --j)
          {
            u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
            xsane.tile[xsane.tile_offset++] = gl;

            xsane.x++;

            if (xsane.x >= pixel_width)
            {
             int tile_height = gimp_tile_height();

              xsane.x = 0;
              xsane.y++;

              if (xsane.y % tile_height == 0)
              {
                gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height, pixel_width, tile_height);
                xsane.tile_offset = 0;
              }

              xsane_progress_update((float) xsane.y / pixel_height); /* update progress bar */
              while (gtk_events_pending())
              {
                gtk_main_iteration();
              }

              break; /* leave for j loop */
            }
          }

          if (cancel_save)
          {
            break;
          }
        }
       break; /* leave switch depth 1 */

      case 8: /* 8 bit gray */
      case 16: /* 16 bit gray already has been reduced to 8 bit */
        for (i = 0; i < pixel_width*pixel_height; ++i)
        {
          xsane.tile[xsane.tile_offset++] = fgetc(imagefile);
          xsane.x++;

          if (xsane.x >= pixel_width)
          {
           int tile_height = gimp_tile_height();

            xsane.x = 0;
            xsane.y++;

            if (xsane.y % tile_height == 0)
            {
              gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height, pixel_width, tile_height);
              xsane.tile_offset = 0;
            }

            xsane_progress_update((float) xsane.y / pixel_height); /* update progress bar */
            while (gtk_events_pending())
            {
              gtk_main_iteration();
            }
          }

          if (cancel_save)
          {
            break;
          }
        }
       break; /* leave switch depth */

      default: /* bad depth */
       break; /* default */
    }
  }
  else if (color == 3) /* RGB */
  {
    switch (bits)
    {
      case 8: /* 8 bit RGB */
      case 16: /* 16 bit RGB already has been reduced to 8 bit */
        for (i = 0; i < pixel_width*pixel_height*3; ++i)
        {
          xsane.tile[xsane.tile_offset++] = fgetc(imagefile);
          if (xsane.tile_offset % 3 == 0)
          {
            xsane.x++;

            if (xsane.x >= pixel_width)
            {
             int tile_height = gimp_tile_height();

              xsane.x = 0;
              xsane.y++;

              if (xsane.y % tile_height == 0)
              {
                gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height, pixel_width, tile_height);
                xsane.tile_offset = 0;
              }

              xsane_progress_update((float) xsane.y / pixel_height); /* update progress bar */
              while (gtk_events_pending())
              {
                gtk_main_iteration();
              }
            }
          }

          if (cancel_save)
          {
            break;
          }
        }
       break; /* case 8 */

      default: /* bad depth */
       break; /* default */
    }
  }
#ifdef SUPPORT_RGBA
  else if (color == 4) /* RGBA */
  {
   int i;

    switch (bits)
    {
      case 8: /* 8 bit RGBA */
      case 16: /* 16 bit RGBA already has been reduced to 8 bit */
        for (i = 0; i < pixel_width*pixel_height*4; ++i)
        {
          xsane.tile[xsane.tile_offset++] = fgetc(imagefile);
          if (xsane.tile_offset % 4 == 0)
          {
            xsane.x++;

            if (xsane.x >= pixel_width)
            {
             int tile_height = gimp_tile_height();

              xsane.x = 0;
              xsane.y++;

              if (xsane.y % tile_height == 0)
              {
                gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height, pixel_width, tile_height);
                xsane.tile_offset = 0;
              }

              xsane_progress_update((float) xsane.y / pixel_height); /* update progress bar */
              while (gtk_events_pending())
              {
                gtk_main_iteration();
              }
            }
          }

          if (cancel_save)
          {
            break;
          }
        }
       break;

      default: /* bad depth */
       break;
    }
  }
#endif

/* scan_done part */
  if (xsane.y > pixel_height)
  {
    xsane.y = pixel_height;
  }
 
  remaining = xsane.y % gimp_tile_height();
  if (remaining)
  {
    gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - remaining, pixel_width, remaining);
  }
  gimp_drawable_flush(xsane.drawable);
  gimp_display_new(xsane.image_ID);
  gimp_drawable_detach(xsane.drawable);
  g_free(xsane.tile);
  xsane.tile = 0;
}
#endif /* HAVE_LIBGIMP_GIMP_H */ 

/* ---------------------------------------------------------------------------------------------------------------------- */
#ifdef XSANE_ACTIVATE_MAIL

/* character base of base64 coding */
static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ---------------------------------------------------------------------------------------------------------------------- */

static void write_3chars_as_base64(unsigned char c1, unsigned char c2, unsigned char c3, int pads, int fd_socket)
{
 char buf[4];

  buf[0] = base64[c1>>2]; /* wirte bits 7-2 of first char */
  buf[1] = base64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)]; /* write bits 1,0 of first and bits 7-4 of second char */

  if (pads == 2) /* only one byte used */
  {
    buf[2] = '='; /* char not used */
    buf[3] = '='; /* char not used */
  }
  else if (pads) /* only two bytes used */
  {
    buf[2] = base64[((c2 & 0xF) << 2)]; /* write bits 3-0 of second char */
    buf[3] = '='; /* char not used */
  }
  else
  {
    buf[2] = base64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)]; /* write bits 3-0 of second and bits 7,6 of third char */
    buf[3] = base64[c3 & 0x3F]; /* write bits 5-0 of third char as lsb */
  }

  write(fd_socket, buf, 4);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_base64(int fd_socket, FILE *infile) 
{
 int c1, c2, c3;
 int pos=0;

  while ((c1 = getc(infile)) != EOF)
  {
    c2 = getc(infile);
    if (c2 == EOF)
    {
      write_3chars_as_base64(c1, 0, 0, 2, fd_socket);
    }
    else
    {
      c3 = getc(infile);
      if (c3 == EOF)
      {
        write_3chars_as_base64(c1, c2, 0, 1, fd_socket);
      }
      else
      {
        write_3chars_as_base64(c1, c2, c3, 0, fd_socket);
      }
    }

    pos += 4;
    if (pos > 71)
    {
      write(fd_socket, "\n", 1);
      
      pos = 0;
    }
  }

  if (pos)
  {
    write(fd_socket, "\n", 1);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_header(int fd_socket, char *from, char *reply_to, char *to, char *subject, char *boundary, int related)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "From: %s\n", from);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Reply-To: %s\n", reply_to);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "To: %s\n", to);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Subject: %s\n", subject);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "MIME-Version: 1.0\n");
  write(fd_socket, buf, strlen(buf));

  if (related) /* related means that we need a special link in the html part to display the image */
  {
    snprintf(buf, sizeof(buf), "Content-Type: multipart/related;\n");
    write(fd_socket, buf, strlen(buf));
  }
  else
  {
    snprintf(buf, sizeof(buf), "Content-Type: multipart/mixed;\n");
    write(fd_socket, buf, strlen(buf));
  }

  snprintf(buf, sizeof(buf), " boundary=\"%s\"\n\n", boundary);
  write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_footer(int fd_socket, char *boundary)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "--%s--\n", boundary);
  write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_mime_ascii(int fd_socket, char *boundary)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "--%s\n", boundary);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: text/plain;\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        charset=\"iso-8859-1\"\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: 8bit\n\n");
  write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_mime_html(int fd_socket, char *boundary)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "--%s\n", boundary);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: text/html;\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        charset=\"us-ascii\"\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: 7bit\n\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "<html>\n");
  write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_attach_image_png(int fd_socket, char *boundary, char *content_id, FILE *infile, char *filename)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "--%s\n", boundary);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: image/png\n");
  write(fd_socket, buf, strlen(buf));

  if (content_id)
  {
    snprintf(buf, sizeof(buf), "Content-ID: <%s>\n", content_id);
    write(fd_socket, buf, strlen(buf));
  }

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: base64\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Disposition: inline;\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        filename=\"%s\"\n", filename);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "\n");
  write(fd_socket, buf, strlen(buf));

  write_base64(fd_socket, infile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_mail_attach_file(int fd_socket, char *boundary, FILE *infile, char *filename)
{
 char buf[1024];

  snprintf(buf, sizeof(buf), "--%s\n", boundary);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: application/octet-stream\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        name=\"%s\"\n", filename);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: base64\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Disposition: attachment;\n");
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        filename=\"%s\"\n", filename);
  write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "\n");
  write(fd_socket, buf, strlen(buf));

  write_base64(fd_socket, infile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* returns fd_socket if sucessfull, < 0 when error occured */
int open_socket(char *server, int port)
{
 int fd_socket;
 struct sockaddr_in sin;
 struct hostent *he;

  he = gethostbyname(server);
  if (!he)
  {
    DBG(DBG_error, "open_socket: Could not get hostname of \"%s\"\n", server);
   return -1;
  }
  else
  {
    DBG(DBG_info, "open_socket: connecting to \"%s\" = %d.%d.%d.%d\n",
        he->h_name,
        (unsigned char) he->h_addr_list[0][0],
        (unsigned char) he->h_addr_list[0][1],
        (unsigned char) he->h_addr_list[0][2],
        (unsigned char) he->h_addr_list[0][3]);
  }
 
  if (he->h_addrtype != AF_INET)
  {
    DBG(DBG_error, "open_socket: Unknown address family: %d\n", he->h_addrtype);
   return -1;
  }

  fd_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (fd_socket < 0)
  {
    DBG(DBG_error, "open_socket: Could not create socket: %s\n", strerror(errno));
   return -1;
  }

/*  setsockopt (dev->ctl, level, TCP_NODELAY, &on, sizeof (on)); */

  sin.sin_port = htons(port);
  sin.sin_family = AF_INET;
  memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);

  if (connect(fd_socket, &sin, sizeof(sin)))
  {
    DBG(DBG_error, "open_socket: Could not connect with port %d of socket: %s\n", ntohs(sin.sin_port), strerror(errno));
   return -1;
  }

  DBG(DBG_info, "open_socket: Connected with port %d\n", ntohs(sin.sin_port));

 return fd_socket;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* returns 0 if success */
/* not only a write routine, also reads data */
int pop3_login(int fd_socket, char *user, char *passwd)
{
 char buf[1024];
 int len;

  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "USER %s\r\n", user);
  DBG(DBG_info2, "> USER xxx\n");
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);
  if (buf[0] != '+')
  {
    return -1;
  }

  snprintf(buf, sizeof(buf), "PASS %s\r\n", passwd);
  DBG(DBG_info2, "> PASS xxx\n");
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);
  if (buf[0] != '+')
  {
    return -1;
  }

  snprintf(buf, sizeof(buf), "QUIT\r\n");
  DBG(DBG_info2, "> QUIT\n");
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* not only a write routine, also reads data */
int write_smtp_header(int fd_socket, char *from, char *to)
{
 char buf[1024];
 int len;

  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "helo localhost\r\n");
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "MAIL FROM: %s\r\n", from);
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "RCPT TO: %s\r\n", to);
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "DATA\r\n");
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* not only a write routine, also reads data */
int write_smtp_footer(int fd_socket)
{
 char buf[1024];
 int len;

  snprintf(buf, sizeof(buf), ".\r\n");
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "QUIT\r\n");
  DBG(DBG_info2, "> %s", buf);
  write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

 return 0;
}

#endif
/* ---------------------------------------------------------------------------------------------------------------------- */
