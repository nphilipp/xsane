/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-scan.c

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
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

static int xsane_generate_dummy_filename(int conversion_level);
static void xsane_read_image_data(gpointer data, gint source, GdkInputCondition cond);
static RETSIGTYPE xsane_sigpipe_handler(int signal);
static int xsane_test_multi_scan(void);
void xsane_scan_done(SANE_Status status);
void xsane_cancel(void);
static void xsane_start_scan(void);
void xsane_scan_dialog(GtkWidget * widget, gpointer call_data);
static void xsane_create_internal_gamma_tables(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_generate_dummy_filename(int conversion_level)
/* conversion levels: */
/* 0 = scan */
/* 1 = rotate */
/* 2 = pack */
{
 char filename[PATH_MAX];
 int tempfile = FALSE; /* returns TRUE if file is a temporary file */

  DBG(DBG_proc, "xsane_generate_dummy_filename(conversion_level=%d)\n", conversion_level);

  if (xsane.dummy_filename)
  {
    free(xsane.dummy_filename);
  }

  if ( (conversion_level == 0) && (xsane.preview->rotation) ) /* scan level with rotation */
  {
    tempfile = TRUE;
  }

  if ( (conversion_level == 1) && (xsane.expand_lineart_to_grayscale) ) /* rotation level and expanded lineart*/
  {
    tempfile = TRUE;
  }

  if ( (xsane.xsane_mode == XSANE_COPY) ||
       (xsane.xsane_mode == XSANE_FAX) ||
       ( (xsane.xsane_mode == XSANE_SCAN)  && (xsane.xsane_output_format != XSANE_PNM) &&
         (xsane.xsane_output_format != XSANE_RAW16) && (xsane.xsane_output_format != XSANE_RGBA) ) )
  {
    tempfile = TRUE;
  }

  if (tempfile) /* save to temporary file */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, 0, 0, "conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);
    xsane.dummy_filename = strdup(filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

    return TRUE;
  }
  else /* no conversion following, save directly to the selected filename */
  {
    xsane.dummy_filename = strdup(xsane.output_filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

    return FALSE;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_read_image_data(gpointer data, gint source, GdkInputCondition cond)
{
 SANE_Handle dev = xsane.dev;
 SANE_Status status;
 SANE_Int len;
 int i, j;
 char buf[255];

  DBG(DBG_proc, "xsane_read_image_data\n");

  if ( (xsane.param.depth == 1) || (xsane.param.depth == 8) )
  {
   unsigned char buf8[2*32768];
   unsigned char *buf8ptr;

    DBG(DBG_info, "depth = 1 or 8 bit\n");

    while (1)
    {
      status = sane_read(dev, (SANE_Byte *) buf8, sizeof(buf8), &len);

      DBG(DBG_info, "sane_read returned with status %s\n", XSANE_STRSTATUS(status));
      DBG(DBG_info, "sane_read: len = %d\n", len);

      if (status == SANE_STATUS_EOF)
      {
        if (!xsane.param.last_frame)
        {
          DBG(DBG_info, "not last frame\n");

          if (xsane.input_tag >= 0)
          {
            gdk_input_remove(xsane.input_tag);
            xsane.input_tag = -1;
          }
          xsane_start_scan();
          break; /* leave while loop */
        }

        xsane_scan_done(SANE_STATUS_EOF); /* image complete, stop scanning */
        return;
      }

      if (status == SANE_STATUS_CANCELLED)
      {
        xsane_scan_done(status); /* status = return of sane_read */
        snprintf(buf, sizeof(buf), "%s.", XSANE_STRSTATUS(status));
        xsane_back_gtk_warning(buf, TRUE);
        return;
      }

      if (status != SANE_STATUS_GOOD)
      {
        xsane_scan_done(status); /* status = return of sane_read */
        snprintf(buf, sizeof(buf), "%s %s.", ERR_DURING_READ, XSANE_STRSTATUS(status));
        xsane_back_gtk_error(buf, TRUE);
        return;
      }

      if (!len)
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.bytes_read / (gfloat) xsane.num_bytes);

      if (xsane.input_tag < 0)
      {
        while (gtk_events_pending())
        {
          DBG(DBG_info, "calling gtk_main_iteration\n");
          gtk_main_iteration();
        }
      }

      switch (xsane.param.format)
      {
        case SANE_FRAME_GRAY:
          {
           int i;
           u_char val;

            DBG(DBG_info, "grayscale\n");

            if ((!xsane.scanner_gamma_gray) && (xsane.param.depth > 1))
            {
              buf8ptr = buf8;

              for (i=0; i < len; ++i) /* do gamma correction by xsane */
              {
                *buf8ptr = xsane.gamma_data[(int) (*buf8ptr)];
                buf8ptr++;
              }

              fwrite(buf8, 1, len, xsane.out); /* write gamma corrected data */
            }
            else if ((xsane.param.depth == 1) && (xsane.expand_lineart_to_grayscale)) 
            {
             unsigned char *expanded_buf8;
             unsigned char *expanded_buf8ptr;
 
              /* if we want to do any postprocessing (e.g. rotation) */
              /* we save lineart images in grayscale mode */
              /* to speed up transformation and saving the transformed  expanded (1bit->1byte) */
              /* is written in a buffer and saved as full buffer */
 
              expanded_buf8 = malloc(len * 8); /* one byte for each pixel (bit) */
              if (!expanded_buf8)
              {
                xsane_scan_done(-1); /* -1 = error */
                snprintf(buf, sizeof(buf), "%s", ERR_NO_MEM);
                xsane_back_gtk_error(buf, TRUE);
               return;
              }
 
              expanded_buf8ptr = expanded_buf8;
              buf8ptr = buf8;
 
              for (i = 0; i < len; ++i)
              {
                val = *buf8ptr;
                for (j = 7; j >= 0; --j)
                {
                  *expanded_buf8ptr = (val & (1 << j)) ? 0x00 : 0xff;
                  expanded_buf8ptr++;
                }
                buf8ptr++;
              }
              fwrite(expanded_buf8, 1, len*8, xsane.out);
              free(expanded_buf8);
            }
            else /* save direct to the file */
            {
              fwrite(buf8, 1, len, xsane.out);
            }
          }
         break; /* SANE_FRAME_GRAY */

        case SANE_FRAME_RGB:
          {
           int i;

            DBG(DBG_info, "1 pass color\n");

            if (!xsane.scanner_gamma_color) /* do gamma correction by xsane */
            {
              buf8ptr = buf8;
              for (i=0; i < len; ++i)
              {
                if (xsane.pixelcolor == 0)
                {
                  *buf8ptr = xsane.gamma_data_red[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor++;
                }
                else if (xsane.pixelcolor == 1)
                {
                  *buf8ptr = xsane.gamma_data_green[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor++;
                }
                else
                {
                  *buf8ptr = xsane.gamma_data_blue[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor = 0;
                }
              }
              fwrite(buf8, 1, len, xsane.out); /* write buffer */
            }
            else /* gamma correction has been done by scanner */
            {
              fwrite(buf8, 1, len, xsane.out); /* write buffer */
            }
          }
         break;

        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          {
            DBG(DBG_info, "3 pass color\n");

            if (!xsane.scanner_gamma_color) /* gamma correction by xsane */
            {
             char val;
             SANE_Int *gamma;

              if (xsane.param.format == SANE_FRAME_RED)
              {
                gamma = xsane.gamma_data_red;
              }
              else if (xsane.param.format == SANE_FRAME_GREEN)
              {
                gamma = xsane.gamma_data_green;
              }
              else
              {
                gamma = xsane.gamma_data_blue;
              }

              for (i = 0; i < len; ++i)
              {
                val = gamma[(int) buf8[i]];
#if 0
                fwrite(&val, 1, 1, xsane.out);
#else
                fputc(val, xsane.out);
#endif
                fseek(xsane.out, 2, SEEK_CUR);
              }
            }
            else /* gamma correction by scanner */
            {
              for (i = 0; i < len; ++i)
              {
#if 0
                fwrite(&buf8[i], 1, 1, xsane.out);
#else
                fputc(buf8[i], xsane.out);
#endif
                fseek(xsane.out, 2, SEEK_CUR);
              }
            }
          }
         break;

#ifdef SUPPORT_RGBA
        case SANE_FRAME_RGBA: /* Scanning including Infrared channel */
          {
           int i;
           char val;

            DBG(DBG_info, "1 pass color+alpha (RGBA)\n");

            if (!xsane.scanner_gamma_color) /* gamma correction by xsane */
            {
              buf8ptr = buf8;

              for (i=0; i < len; ++i)
              {
                if (xsane.pixelcolor == 0)
                {
                  *buf8ptr = xsane.gamma_data_red[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor++;
                }
                else if (xsane.pixelcolor == 1)
                {
                  *buf8ptr = xsane.gamma_data_green[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor++;
                }
                else if (xsane.pixelcolor == 2)
                {
                  *buf8ptr = xsane.gamma_data_blue[(int) (*buf8ptr)];
                  buf8ptr++;
                  xsane.pixelcolor++;
                }
                else
                {
                  /* no gamma table for infrared channel */
                  buf8ptr++;
                  xsane.pixelcolor = 0;
                }
              }

              fwrite(buf8, 1, len, xsane.out);
            }
            else /* gamma correction has been done by scanner */
            {
              fwrite(buf8, 1, len, xsane.out);
            }
          }
         break;
#endif

        default:
          xsane_scan_done(-1); /* -1 = error */
          DBG(DBG_error, "xsane_read_image_data: %s %d\n", ERR_BAD_FRAME_FORMAT, xsane.param.format);
          return;
         break;
      }
    }
  }
  else if ( xsane.param.depth == 16 )
  {
   guint16 buf16[32768];
   guint16 *buf16ptr;
   unsigned char *buf8 = (unsigned char *) buf16;
   unsigned char *buf8ptr;
   char buf[255];
   char last = 0;
   int offset = 0;

    DBG(DBG_info, "depth = 16 bit\n");

    while (1)
    {
      if (offset) /* if we have had an odd number of bytes */
      {
        buf16[0] = last;  /* ATTENTION: that is wrong! */
        status = sane_read(dev, ((SANE_Byte *) buf16) + 1, sizeof(buf16) - 1, &len);
        if (len)
        {
          len++;
        }
      }
      else /* last read we had an even number of bytes */
      {
        status = sane_read(dev, (SANE_Byte *) buf16, sizeof(buf16), &len);
      }

      if (len % 2) /* odd number of bytes */
      {
        len--;
        last = buf16[len];
        offset = 1;
      }
      else /* even number of bytes */
      {
        offset = 0;
      }

      if (status == SANE_STATUS_EOF)
      {
        if (!xsane.param.last_frame)
        {
          DBG(DBG_info, "not last frame\n");
          if (xsane.input_tag >= 0)
          {
            gdk_input_remove(xsane.input_tag);
            xsane.input_tag = -1;
          }
          xsane_start_scan();
          break; /* leave while loop */
        }

        xsane_scan_done(SANE_STATUS_EOF); /* image complete, stop scanning */
        return;
      }

      if (status == SANE_STATUS_CANCELLED)
      {
        xsane_scan_done(status); /* status = return of sane_read */
        snprintf(buf, sizeof(buf), "%s.", XSANE_STRSTATUS(status));
        xsane_back_gtk_warning(buf, TRUE);
        return;
      }

      if (status != SANE_STATUS_GOOD)
      {
        xsane_scan_done(status); /* status = return of sane_read */
        snprintf(buf, sizeof(buf), "%s %s.", ERR_DURING_READ, XSANE_STRSTATUS(status));
        xsane_back_gtk_error(buf, TRUE);
        return;
      }

      if (!len) /* nothing read */
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.bytes_read / (gfloat) xsane.num_bytes);

      if (xsane.input_tag < 0)
      {
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }
      }

      switch (xsane.param.format)
      {
        case SANE_FRAME_GRAY:
          {
           int i;

            if (xsane.reduce_16bit_to_8bit) /* reduce 16 bit image to 8 bit */
            {
              DBG(DBG_info, "reducing 16 bit image to 8 bit\n");

              if (!xsane.scanner_gamma_gray) /* gamma correction by xsane */
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  *buf8ptr = (xsane.gamma_data[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                  buf8ptr++;
                  buf16ptr++;
                }

                fwrite(buf8, 1, len/2, xsane.out);
              }
              else /* gamma correction by scanner */
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  *buf8ptr = (*buf16ptr) >> 8; /* reduce to 8 bit */
                  buf8ptr++;
                  buf16ptr++;
                }

                fwrite(buf8, 1, len/2, xsane.out);
              }
            }
            else /* save as 16 bit image */
            {
              if (!xsane.scanner_gamma_gray) /* gamma correction by xsane */
              {
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  *buf16ptr = xsane.gamma_data[(*buf16ptr)];
                  buf16ptr++;
                }
                fwrite(buf16, 2, len/2, xsane.out);
              }
              else /* gamma correction by scanner */
              {
                fwrite(buf16, 2, len/2, xsane.out);
              }
            }
          }
         break;

        case SANE_FRAME_RGB:
          {
           int i;

            if (xsane.reduce_16bit_to_8bit) /* reduce 16 bit image to 8 bit */
            {
              DBG(DBG_info, "reducing 16 bit image to 8 bit\n");

              if (!xsane.scanner_gamma_color) /* gamma correction by xsane */
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  if (xsane.pixelcolor == 0)
                  {
                    *buf8ptr = (xsane.gamma_data_red[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 1)
                  {
                    *buf8ptr = (xsane.gamma_data_green[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else
                  {
                    *buf8ptr = (xsane.gamma_data_blue[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor = 0;
                  }
                }
                fwrite(buf8, 1, len/2, xsane.out);
              }
              else /* gamma correction by scanner */
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  *buf8ptr = (*buf16ptr) >> 8; /* reduce to 8 bit */
                  buf8ptr++;
                  buf16ptr++;
                  xsane.pixelcolor++;
                }
                fwrite(buf8, 1, len/2, xsane.out);
              }
            }
            else /* save as 16 bit image */
            {
              if (!xsane.scanner_gamma_color) /* gamma correction by xsane */
              {
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  if (xsane.pixelcolor == 0)
                  {
                    *buf16ptr = xsane.gamma_data_red[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 1)
                  {
                    *buf16ptr = xsane.gamma_data_green[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else
                  {
                    *buf16ptr = xsane.gamma_data_blue[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor = 0;
                  }
                }
                fwrite(buf16, 2, len/2, xsane.out);
              }
              else /* gamma correction by scanner */
              {
                fwrite(buf16, 2, len/2, xsane.out);
              }
            }
          }
         break;

        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          /* this is incomplete:
             - missing: gamma correction by xsane
             - missing: reduction to 8 bit
             but I do not think there are 3 pass scanners with more
             than 24 bits/pixel */
          {
            for (i = 0; i < len/2; ++i)
            {
              fwrite(buf16 + i*2, 2, 1, xsane.out);
              fseek(xsane.out, 4, SEEK_CUR);
            }
          }
         break;

#ifdef SUPPORT_RGBA
        case SANE_FRAME_RGBA:
          {
           int i;
           guint16 val;

            if (xsane.reduce_16bit_to_8bit) /* reduce 16 bit image to 8 bit */
            {
              DBG(DBG_info, "reducing 16 bit image to 8 bit\n");

              if (!xsane.scanner_gamma_color)
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  if (xsane.pixelcolor == 0)
                  {
                    *buf8ptr = (xsane.gamma_data_red[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 1)
                  {
                    *buf8ptr = (xsane.gamma_data_green[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 2)
                  {
                    *buf8ptr = (xsane.gamma_data_blue[(*buf16ptr)]) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else
                  {
                    /* no gamma table for infrared channel */
                    *buf8ptr = (*buf16ptr) >> 8; /* reduce to 8 bit */
                    buf8ptr++;
                    buf16ptr++;
                    xsane.pixelcolor = 0;
                  }
                }

                fwrite(buf8, 1, len, xsane.out);
              }
              else /* gamma correction done by scanner */
              {
                buf8ptr = buf8;
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  *buf8ptr = (*buf16ptr) >> 8; /* reduce to 8 bit */
                  buf8ptr++;
                  buf16ptr++;
                }

                fwrite(buf8, 1, len, xsane.out);
              }
            }
            else /* save as 16 bit image */
            {
              if (!xsane.scanner_gamma_color)
              {
                buf16ptr = buf16;

                for (i=0; i < len/2; ++i)
                {
                  if (xsane.pixelcolor == 0)
                  {
                    *buf16ptr = xsane.gamma_data_red[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 1)
                  {
                    *buf16ptr = xsane.gamma_data_green[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else if (xsane.pixelcolor == 2)
                  {
                    *buf16ptr = xsane.gamma_data_blue[(*buf16ptr)];
                    buf16ptr++;
                    xsane.pixelcolor++;
                  }
                  else
                  {
                    /* no gamma table for infrared channel */
                    buf16ptr++;
                    xsane.pixelcolor = 0;
                  }
                }

                fwrite(buf16, 2, len/2, xsane.out);
              }
              else /* gamma correction done by scanner */
              {
                fwrite(buf16, 2, len/2, xsane.out);
              }
            }
          }
         break;
#endif

        default:
          xsane_scan_done(-1); /* -1 = error */
          DBG(DBG_error, "xsane_read_image_data: %s %d\n", ERR_BAD_FRAME_FORMAT, xsane.param.format);
          return;
         break;
      }
    }
  }
  else
  {
    xsane_scan_done(-1); /* -1 = error */
    snprintf(buf, sizeof(buf), "%s %d.", ERR_BAD_DEPTH, xsane.param.depth);
    xsane_back_gtk_error(buf, TRUE);
    return;
  }

  return;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_sigpipe_handler(int signal)
/* this is to catch a broken pipe while writing to printercommand */
{
  DBG(DBG_proc, "xsane_sigpipe_handler\n");

  xsane_cancel_save(0);
  xsane.broken_pipe = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_test_multi_scan(void)
{
 char *set;
 SANE_Status status;
 const SANE_Option_Descriptor *opt;

  DBG(DBG_proc, "xsane_test_multi_scan\n");

  opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.scansource);
  if (opt)
  {
    if (SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      if (opt->constraint_type == SANE_CONSTRAINT_STRING_LIST)
      {
        set = malloc(opt->size);
        status = xsane_control_option(xsane.dev, xsane.well_known.scansource, SANE_ACTION_GET_VALUE, set, 0);

        if (status == SANE_STATUS_GOOD)
        {
          if (xsane.adf_scansource)
          {
            if (!strcmp(set, xsane.adf_scansource))
            {
              free(set);
              return TRUE;
            }
          }
        }
        free(set);
      }
    }
  }

#if 0 /* this is planned for the next sane-standard */
  if (xsane.param.bitfield & XSANE_PARAM_STATUS_MORE_IMAGES)
  {
    return TRUE;
  }
#endif

  return FALSE;
}
                                    
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scan_done(SANE_Status status)
{
 FILE *outfile;
 FILE *infile;
 char buf[256];
 int pixel_width  = xsane.param.pixels_per_line;
 int pixel_height = xsane.param.lines;

  DBG(DBG_proc, "xsane_scan_done\n");

  if (xsane.input_tag >= 0)
  {
    gdk_input_remove(xsane.input_tag);
    xsane.input_tag = -1;
  }

  xsane_progress_clear(); /* clear progress bar and reset cancel callback */

  while(gtk_events_pending())	/* let gtk remove the progress bar and update everything that needs it */
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }


  /* we have to free the gamma tables if we used software gamma correction */
  
  if (xsane.gamma_data) 
  {
    DBG(DBG_info, "freeing gray gamma table\n");

    free(xsane.gamma_data);
    xsane.gamma_data = 0;
  }

  if (xsane.gamma_data_red)
  {
    DBG(DBG_info, "freeing color gamma tables\n");

    free(xsane.gamma_data_red);
    free(xsane.gamma_data_green);
    free(xsane.gamma_data_blue);

    xsane.gamma_data_red   = 0;
    xsane.gamma_data_green = 0;
    xsane.gamma_data_blue  = 0;
  }

  if (xsane.out) /* close file - this is dummy_file but if there is no conversion it is the wanted file */
  {
    /* correct image height if necessary, e.g. when scanner with hand scanner */

    pixel_height = xsane.bytes_read / xsane.param.bytes_per_line;
    if (xsane.param.lines != pixel_height)
    {
      DBG(DBG_info, "correcting image height to %d lines\n", pixel_height);
      xsane.param.lines = pixel_height;
      xsane_write_pnm_header(xsane.out, xsane.param.pixels_per_line, xsane.param.lines, xsane.depth);
    }

    DBG(DBG_info, "closing output file\n");

    fclose(xsane.out);
    xsane.out = 0;
  }

  if ( (status == SANE_STATUS_GOOD) || (status == SANE_STATUS_EOF) ) /* no error, do conversion etc. */
  {
    /* do we have to rotate the image ? */
    if (xsane.preview->rotation)
    {
     char *old_dummy_filename;
     int abort = 0;

      infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */
      if (infile != 0)
      {
        fseek(infile, xsane.header_size, SEEK_SET);

        /* open progressbar */
        xsane_progress_new(PROGRESS_ROTATING_DATA, PROGRESS_TRANSFERING_DATA, (GtkSignalFunc) xsane_cancel_save);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        /* on some filesystems it is not allowed to erase an opened file and access the
           file after that, so we must wait until the file is closed */
        old_dummy_filename = strdup(xsane.dummy_filename);

        if (xsane_generate_dummy_filename(1)) /* create filename for rotation */
        {
          /* temporary file */
          umask(0177); /* creare temporary file with "-rw-------" permissions */   
        }
        else
        {
          /* no temporary file */
          umask((mode_t) preferences.image_umask); /* define image file permissions */   
        }

        /* rotate image */
        outfile = fopen(xsane.dummy_filename, "wb"); /* read binary (b for win32) */
        umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   

        if (outfile)
        {
          if (xsane_save_rotate_image(outfile, infile, xsane.xsane_color, xsane.depth,
                                      &pixel_width, &pixel_height, xsane.preview->rotation))
          {
            abort = 1;
          }
        }
        else
        {
         char buf[256];
          DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
          snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
          xsane_back_gtk_error(buf, TRUE);
          abort = 1;
        }

        fclose(infile);
        fclose(outfile);
        remove(old_dummy_filename); /* remove the unrotated image file */

        free(old_dummy_filename); /* release memory */
        xsane_progress_clear();
      }
      else
      {
       char buf[256];
        DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
        snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
        xsane_back_gtk_error(buf, TRUE);
        abort = 1;
      }

      if (abort)
      {
        xsane_set_sensitivity(TRUE);		/* reactivate buttons etc */
        sane_cancel(xsane.dev); /* stop scanning */
        xsane_update_histogram(TRUE /* update raw */);
        xsane_update_param(0);
        xsane.header_size = 0;
        return;
      }


      /* when we are scanning in lineart mode and we are transforming the image */
      /* it is saved as grayscale while scanning so we can use the standard transformations */
      /* but now we have to save the lineart image as packed lineart again */
      if (xsane.expand_lineart_to_grayscale) /* we have to pack the lineart image again */
      {
        infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */
        if (infile != 0)
        {
          fseek(infile, xsane.header_size, SEEK_SET);

          /* open progressbar */
          xsane_progress_new(PROGRESS_PACKING_DATA, PROGRESS_TRANSFERING_DATA, (GtkSignalFunc) xsane_cancel_save);
          while (gtk_events_pending())
          {
            gtk_main_iteration();
          }

          /* on some filesystems it is not allowed to erase an opened file and access the
             file after that, so we must wait until the file is closed */
           old_dummy_filename = strdup(xsane.dummy_filename);

          if (xsane_generate_dummy_filename(2)) /* create filename for packing */
          {
            /* temporary file */
            umask(0177); /* creare temporary file with "-rw-------" permissions */   
          }
          else
          {
            /* no temporary file */
            umask((mode_t) preferences.image_umask); /* define image file permissions */   
          }

          /* pack lineart image */
          outfile = fopen(xsane.dummy_filename, "wb"); /* read binary (b for win32) */
          umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   

          if (outfile)
          {
            if (xsane_save_grayscale_image_as_lineart(outfile, infile, pixel_width, pixel_height))
            {
              abort = 1;
            }
          }
          else
          {
           char buf[256];
            DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
            snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
            xsane_back_gtk_error(buf, TRUE);
            abort = 1;
          }

          fclose(infile);
          fclose(outfile);
          remove(old_dummy_filename); /* remove the unrotated image file */
          free(old_dummy_filename); /* release memory */
          xsane_progress_clear();
        }
        else
        {
         char buf[256];
          DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
          snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
          xsane_back_gtk_error(buf, TRUE);
          abort = 1;
        }

        if (abort)
        {
          xsane_set_sensitivity(TRUE);		/* reactivate buttons etc */
          sane_cancel(xsane.dev); /* stop scanning */
          xsane_update_histogram(TRUE /* update raw */);
          xsane_update_param(0);
          xsane.header_size = 0;
          return;
        }
      }
    }

    if (xsane.xsane_mode == XSANE_SCAN)
    {
      if ( ( (status == SANE_STATUS_GOOD) || (status == SANE_STATUS_EOF) ) && (xsane.print_filenames) )
      {
        if (xsane.output_filename[0] != '/') /* relative path */
        {
         char pathname[512];
          getcwd(pathname, sizeof(pathname));
          printf("XSANE_IMAGE_FILENAME: %s/%s\n", pathname, xsane.output_filename);
          fflush(stdout);
        }
        else /* absolute path */
        {
          printf("XSANE_IMAGE_FILENAME: %s\n", xsane.output_filename);
          fflush(stdout);
        }
      }

      if ( ( (xsane.xsane_output_format != XSANE_PNM) && /* these files do not need any transformation */
             (xsane.xsane_output_format != XSANE_RAW16) &&
             (xsane.xsane_output_format != XSANE_RGBA) ) ||
           (xsane.mode == XSANE_GIMP_EXTENSION) )
      {
        infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */
        if (infile != 0)
        {
          fseek(infile, xsane.header_size, SEEK_SET);

          /* open progressbar */
          xsane_progress_new(PROGRESS_CONVERTING_DATA, PROGRESS_TRANSFERING_DATA, (GtkSignalFunc) xsane_cancel_save);
          while (gtk_events_pending())
          {
            gtk_main_iteration();
          }


#ifdef HAVE_LIBGIMP_GIMP_H
          if (xsane.mode == XSANE_GIMP_EXTENSION)	/* xsane runs as gimp plugin */
          {
            xsane_transfer_to_gimp(infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height);
          }
          else
#endif /* HAVE_LIBGIMP_GIMP_H */
#ifdef HAVE_LIBTIFF
          if (xsane.xsane_output_format == XSANE_TIFF)		/* routines that want to have filename  for saving */
          {
            remove(xsane.output_filename);
            umask((mode_t) preferences.image_umask); /* define image file permissions */   
            xsane_save_tiff(xsane.output_filename, infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height,
                            preferences.jpeg_quality);
            umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   
          }
          else							/* routines that want to have filedescriptor for saving */
#endif /* HAVE_LIBTIFF */
          {
            remove(xsane.output_filename);
            umask((mode_t) preferences.image_umask); /* define image file permissions */   
            outfile = fopen(xsane.output_filename, "wb"); /* b = binary mode for win32 */
            umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   

            if (outfile != 0)
            {
              switch(xsane.xsane_output_format)
              {
#ifdef HAVE_LIBJPEG
                case XSANE_JPEG:
                  xsane_save_jpeg(outfile, infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height,
                                  preferences.jpeg_quality);
                 break; /* switch format == XSANE_JPEG */
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
                case XSANE_PNG:
                  if (xsane.depth <= 8)
                  {
                    xsane_save_png(outfile, infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height,
                                   preferences.png_compression);
                  }
                  else
                  {
                    xsane_save_png_16(outfile, infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height,
                                      preferences.png_compression);
                  }
                 break; /* switch format == XSANE_PNG */
#endif
#endif

                case XSANE_PNM16:
                  xsane_save_pnm_16(outfile, infile, xsane.xsane_color, xsane.depth, pixel_width, pixel_height);
                 break; /* switch fomat == XSANE_PNM16 */

                case XSANE_PS: /* save postscript, use original size */
                { 
                 float imagewidth  = pixel_width/xsane.resolution_x; /* width in inch */
                 float imageheight = pixel_height/xsane.resolution_y; /* height in inch */

                  if (preferences.psrotate) /* rotate: landscape */
                  {
                    xsane_save_ps(outfile, infile,
                                  xsane.xsane_color /* gray, color */,
                                  xsane.depth /* bits */,
                                  pixel_width, pixel_height,
                                  (preferences.psfile_bottomoffset + preferences.psfile_height) * 36.0/MM_PER_INCH - imagewidth * 36.0, /* left edge */
                                  (preferences.psfile_leftoffset   + preferences.psfile_width)  * 36.0/MM_PER_INCH - imageheight * 36.0, /* bottom edge */
                                   imagewidth, imageheight,
                                  (preferences.psfile_leftoffset   + preferences.psfile_width ) * 72.0/MM_PER_INCH,  /* paperwidth */
                                  (preferences.psfile_bottomoffset + preferences.psfile_height) * 72.0/MM_PER_INCH, /* paperheight */
                                  1 /* landscape */);
                  }
                  else /* do not rotate: portrait */
                  {
                    xsane_save_ps(outfile, infile,
                                  xsane.xsane_color /* gray, color */,
                                  xsane.depth /* bits */,
                                  pixel_width, pixel_height,
                                  (preferences.psfile_leftoffset   + preferences.psfile_width)  * 36.0/MM_PER_INCH - imagewidth * 36.0,
                                  (preferences.psfile_bottomoffset + preferences.psfile_height) * 36.0/MM_PER_INCH - imageheight * 36.0,
                                  imagewidth, imageheight,
                                  (preferences.psfile_leftoffset   + preferences.psfile_width ) * 72.0/MM_PER_INCH, /* paperwidth */
                                  (preferences.psfile_bottomoffset + preferences.psfile_height) * 72.0/MM_PER_INCH, /* paperheight */
                                  0 /* portrait */);
                  }
                }
                break; /* switch format == XSANE_PS */


                default:
                  snprintf(buf, sizeof(buf),"%s", ERR_UNKNOWN_SAVING_FORMAT);
                  xsane_back_gtk_error(buf, TRUE);
                 break; /* switch format == default */
              }
              fclose(outfile);
             }
             else
             {
              char buf[256];

               snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.output_filename, strerror(errno));
               xsane_back_gtk_error(buf, TRUE);
             }
           }
           fclose(infile);
           remove(xsane.dummy_filename);
           xsane_progress_clear();
        }
        else
        {
         char buf[256];
          snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.output_filename, strerror(errno));
          xsane_back_gtk_error(buf, TRUE);
        }

        while (gtk_events_pending())
        { 
          gtk_main_iteration();
        }
      }
    }
    else if (xsane.xsane_mode == XSANE_COPY)
    {
     FILE *outfile;
     FILE *infile;
     char buf[256];

       DBG(DBG_info, "XSANE_COPY\n");

       xsane_update_int(xsane.copy_number_entry, &xsane.copy_number); /* get number of copies */
       if (xsane.copy_number < 1)
       {
         xsane.copy_number = 1;
       }

       /* open progressbar */
       xsane_progress_new(PROGRESS_CONVERTING_DATA, PROGRESS_TRANSFERING_DATA, (GtkSignalFunc) xsane_cancel_save);

       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       xsane.broken_pipe = 0;
       infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */

       snprintf(buf, sizeof(buf), "%s %s%d", preferences.printer[preferences.printernr]->command,
                                             preferences.printer[preferences.printernr]->copy_number_option,
                                             xsane.copy_number);
       outfile = popen(buf, "w");
/*       outfile = popen(preferences.printer[preferences.printernr]->command, "w"); */
       if ((outfile != 0) && (infile != 0)) /* copy mode, use zoom size */
       {
        struct SIGACTION act;
        float imagewidth, imageheight;
        int printer_resolution;

         switch (xsane.param.format)
         {
           case SANE_FRAME_GRAY:
             if (xsane.depth == 1)
             {
               printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
             }
             else
             {
               printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
             }
           break; /* switch format == SANE_FRAME_GRAY */

           case SANE_FRAME_RGB:
           case SANE_FRAME_RED:
           case SANE_FRAME_GREEN:
           case SANE_FRAME_BLUE:
           default:
             printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
           break; /* switch format == SANE_FRAME_{color} */
         }        

         imagewidth  = pixel_width/(float)printer_resolution; /* width in inch */
         imageheight = pixel_height/(float)printer_resolution; /* height in inch */

         memset (&act, 0, sizeof (act)); /* define broken pipe handler */
         act.sa_handler = xsane_sigpipe_handler;
         sigaction (SIGPIPE, &act, 0);


         fseek(infile, xsane.header_size, SEEK_SET);

         if (preferences.psrotate) /* rotate: landscape */
         {
           xsane_save_ps(outfile, infile,
                         xsane.xsane_color /* gray, color */,
                         xsane.depth /* bits */,
                         pixel_width, pixel_height,
                         (preferences.printer[preferences.printernr]->bottomoffset +
                          preferences.printer[preferences.printernr]->height) * 36.0/MM_PER_INCH - imagewidth * 36.0, /* left edge */
                         (preferences.printer[preferences.printernr]->leftoffset +
                          preferences.printer[preferences.printernr]->width) * 36.0/MM_PER_INCH - imageheight * 36.0, /* bottom edge */
                          imagewidth, imageheight,
                         (preferences.printer[preferences.printernr]->leftoffset +
                          preferences.printer[preferences.printernr]->width ) * 72.0/MM_PER_INCH,  /* paperwidth */
                         (preferences.printer[preferences.printernr]->bottomoffset +
                          preferences.printer[preferences.printernr]->height) * 72.0/MM_PER_INCH, /* paperheight */
                         1 /* landscape */);
         }
         else /* do not rotate: portrait */
         {
           xsane_save_ps(outfile, infile,
                         xsane.xsane_color /* gray, color */,
                         xsane.depth /* bits */,
                         pixel_width, pixel_height,
                         (preferences.printer[preferences.printernr]->leftoffset +
                          preferences.printer[preferences.printernr]->width) * 36.0/MM_PER_INCH - imagewidth * 36.0, /* left edge */
                         (preferences.printer[preferences.printernr]->bottomoffset +
                          preferences.printer[preferences.printernr]->height) * 36.0/MM_PER_INCH - imageheight * 36.0, /* bottom edge */
                         imagewidth, imageheight,
                         (preferences.printer[preferences.printernr]->leftoffset +
                          preferences.printer[preferences.printernr]->width ) * 72.0/MM_PER_INCH,  /* paperwidth */
                         (preferences.printer[preferences.printernr]->bottomoffset +
                          preferences.printer[preferences.printernr]->height) * 72.0/MM_PER_INCH, /* paperheight */
                         0 /* portrait */);
         }
       }
       else
       {
        char buf[256];

         if (!infile)
         {
           snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.output_filename, strerror(errno));
           xsane_back_gtk_error(buf, TRUE);
         }
         else if (!outfile)
         {
           xsane_back_gtk_error(ERR_FAILED_PRINTER_PIPE, TRUE);
         }
       }

       if (xsane.broken_pipe)
       {
         snprintf(buf, sizeof(buf), "%s \"%s\"", ERR_FAILED_EXEC_PRINTER_CMD, preferences.printer[preferences.printernr]->command);
         xsane_back_gtk_error(buf, TRUE);
       }

       xsane_progress_clear();
       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       if (infile)
       {
         fclose(infile);
         remove(xsane.dummy_filename);
       }

       if (outfile)
       {
         pclose(outfile);
       }
    }
    else if (xsane.xsane_mode == XSANE_FAX)
    {
     FILE *outfile;
     FILE *infile;

       /* open progressbar */
       xsane_progress_new(PROGRESS_CONVERTING_DATA, PROGRESS_TRANSFERING_DATA, (GtkSignalFunc) xsane_cancel_save);

       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */
       if (infile != 0)
       {
         fseek(infile, xsane.header_size, SEEK_SET);

         umask((mode_t) preferences.image_umask); /* define image file permissions */   
         outfile = fopen(xsane.fax_filename, "wb"); /* b = binary mode for win32 */
         umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   
         if (outfile != 0)
         {
          float imagewidth, imageheight;

           imagewidth  = pixel_width/xsane.resolution_x; /* width in inch */
           imageheight = pixel_height/xsane.resolution_y; /* height in inch */

           DBG(DBG_info, "imagewidth  = %f\n", imagewidth);
           DBG(DBG_info, "imageheight = %f\n", imageheight);

/* disabled ( 0 * ...) in the moment */
           if (0 * preferences.psrotate) /* rotate: landscape */
           {
             xsane_save_ps(outfile, infile,
                           xsane.xsane_color /* gray, color */,
                           xsane.depth /* bits */,
                           pixel_width, pixel_height,
                           (preferences.fax_bottomoffset + preferences.fax_height) * 36.0/MM_PER_INCH - imagewidth * 36.0, /* left edge */
                           (preferences.fax_leftoffset   + preferences.fax_width)  * 36.0/MM_PER_INCH - imageheight * 36.0, /* bottom edge */
                            imagewidth, imageheight,
                           (preferences.fax_leftoffset   + preferences.fax_width ) * 72.0/MM_PER_INCH, /* paperwidth */
                           (preferences.fax_bottomoffset + preferences.fax_height) * 72.0/MM_PER_INCH, /* paperheight */
                           1 /* landscape */);
           }
           else /* do not rotate: portrait */
           {
             xsane_save_ps(outfile, infile,
                           xsane.xsane_color /* gray, color */,
                           xsane.depth /* bits */,
                           pixel_width, pixel_height,
                           (preferences.fax_leftoffset   + preferences.fax_width)  * 36.0/MM_PER_INCH - imagewidth * 36.0,
                           (preferences.fax_bottomoffset + preferences.fax_height) * 36.0/MM_PER_INCH - imageheight * 36.0,
                           imagewidth, imageheight,
                           (preferences.fax_leftoffset   + preferences.fax_width ) * 72.0/MM_PER_INCH, /* paperwidth */
                           (preferences.fax_bottomoffset + preferences.fax_height) * 72.0/MM_PER_INCH, /* paperheight */
                           0 /* portrait */);
           }
           fclose(outfile);
         }
         else
         {
          char buf[256];

           DBG(DBG_info, "open of faxfile `%s'failed : %s\n", xsane.fax_filename, strerror(errno));

           snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.fax_filename, strerror(errno));
           xsane_back_gtk_error(buf, TRUE);
         }

         fclose(infile);
         remove(xsane.dummy_filename);
       }
       else
       {
        char buf[256];

         DBG(DBG_info, "open of faxfile `%s'failed : %s\n", xsane.fax_filename, strerror(errno));

         snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.fax_filename, strerror(errno));
         xsane_back_gtk_error(buf, TRUE);
       }
       xsane_progress_clear();

       while (gtk_events_pending())
       {
         DBG(DBG_info, "calling gtk_main_iteration");
         gtk_main_iteration();
       }
    }

    if ( (xsane.xsane_mode == XSANE_SCAN) && (xsane.mode == XSANE_STANDALONE) )
    {
      if (!xsane.force_filename) /* user filename selection active */
      {
        if (preferences.filename_counter_step) /* increase filename counter ? */
        {
          xsane_update_counter_in_filename(&preferences.filename, preferences.skip_existing_numbers,
                                           preferences.filename_counter_step, preferences.filename_counter_len);
          gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), (char *) preferences.filename); /* update filename in entry */
          gtk_entry_set_position(GTK_ENTRY(xsane.outputfilename_entry), strlen(preferences.filename)); /* set cursor to right position of filename */
        }
      }
      else /* external filename */
      {
        xsane_update_counter_in_filename(&xsane.external_filename, TRUE, 1, 0);
      }
    }
    else if (xsane.xsane_mode == XSANE_FAX)
    {
     GtkWidget *list_item;
     char *page;
     char *extension;

      page = strdup(strrchr(xsane.fax_filename,'/')+1);
      extension = strrchr(page, '.');
      if (extension)
      {
        *extension = 0;
      }
      list_item = gtk_list_item_new_with_label(page);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
      gtk_container_add(GTK_CONTAINER(xsane.fax_list), list_item);
      gtk_widget_show(list_item);

      xsane_update_counter_in_filename(&xsane.fax_filename, TRUE, 1, preferences.filename_counter_len);
      xsane_fax_project_save();
      free(page);
    }
  }
  else /* an error occured, remove the dummy_file */
  {
    if (xsane.dummy_filename) /* remove corrupt file */
    {
      remove(xsane.dummy_filename);
    }
  }

  free(xsane.dummy_filename); /* no dummy_filename, needed if an error occurs */
  xsane.dummy_filename = 0;

  if (xsane.output_filename)
  {
    free(xsane.output_filename);
    xsane.output_filename = 0;
  }

  xsane.header_size = 0;

  if ( ( (status == SANE_STATUS_GOOD) || (status == SANE_STATUS_EOF) ) && (xsane_test_multi_scan()) )
  {
    /* multi scan (eg ADF): scan again			*/
    /* stopped when:					*/
    /*  a) xsane_test_multi_scan returns false		*/
    /*  b) sane_start returns SANE_STATUS_NO_DOCS	*/
    /*  c) an error occurs 				*/

    xsane.adf_page_counter += 1;
    gtk_signal_emit_by_name(xsane.start_button, "clicked"); /* press START button */
  }
  else /* last scan: update histogram */
  {
    xsane_set_sensitivity(TRUE);		/* reactivate buttons etc */
    sane_cancel(xsane.dev); /* stop scanning */
    xsane_update_histogram(TRUE /* update raw */);
    xsane_update_param(0);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel(void)
{
  DBG(DBG_proc, "xsane_cancel\n");
  sane_cancel(xsane.dev);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_start_scan(void)
{
 SANE_Status status;
 SANE_Handle dev = xsane.dev;
 const char *frame_type = 0;
 char buf[256];
 int fd;

  DBG(DBG_proc, "xsane_start_scan\n");

  if ( (xsane.mode == XSANE_STANDALONE) && (xsane.xsane_mode == XSANE_SCAN) )
  {
    /* correct length of filename counter if it is shorter than minimum length */
    if (!xsane.force_filename)
    {
      xsane_update_counter_in_filename(&preferences.filename, FALSE, 0, preferences.filename_counter_len);
      gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), preferences.filename); 
    }
  }

  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh);    
  xsane_set_sensitivity(FALSE);

  status = sane_start(dev);
  DBG(DBG_info, "sane_start returned with status %s\n", XSANE_STRSTATUS(status));

  if ((status == SANE_STATUS_NO_DOCS) && (xsane.adf_page_counter>0)) /* ADF out of docs but not first page */
  {
    xsane_scan_done(status); /* ok, stop multi image scan */
    snprintf(buf, sizeof(buf), "%s %d", TEXT_ADF_PAGES_SCANNED, xsane.adf_page_counter);
    xsane_back_gtk_info(buf, FALSE);
    xsane.adf_page_counter = 0;
    return;
  }
  else if (status != SANE_STATUS_GOOD) /* error */
  {
    xsane_scan_done(status);
    snprintf(buf, sizeof(buf), "%s %s", ERR_FAILED_START_SCANNER, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
    xsane.adf_page_counter = 0;
    return;
  }

  status = sane_get_parameters(dev, &xsane.param);
  if (status != SANE_STATUS_GOOD)
  {
    xsane_scan_done(status);
    snprintf(buf, sizeof(buf), "%s %s", ERR_FAILED_GET_PARAMS, XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, TRUE);
    xsane.adf_page_counter = 0;
    return;
  }

  xsane.depth = xsane.param.depth; /* bit depth for saving, can be changed: 1->8, 16->8 */

  xsane.num_bytes = xsane.param.lines * xsane.param.bytes_per_line;
  xsane.bytes_read = 0;
  xsane.expand_lineart_to_grayscale = 0;

  switch (xsane.param.format)
  {
    case SANE_FRAME_RGB:	frame_type = "RGB"; break;
    case SANE_FRAME_RED:	frame_type = "red"; break;
    case SANE_FRAME_GREEN:	frame_type = "green"; break;
    case SANE_FRAME_BLUE:	frame_type = "blue"; break;
    case SANE_FRAME_GRAY:	frame_type = "gray"; break;
#ifdef SUPPORT_RGBA
    case SANE_FRAME_RGBA:	frame_type = "RGBA"; break;
#endif
    default:			frame_type = "unknown"; break;
  }

  if ( (xsane.param.depth == 1) && (xsane.preview->rotation) )
  {
    xsane.expand_lineart_to_grayscale = 1; /* We want to do transformation with lineart scan, so we save it as grayscale */
  }

  if (!xsane.header_size) /* first pass of multi pass scan or single pass scan */
  {
    xsane_create_internal_gamma_tables(); /* create gamma tables that are not supported by scanner */

    if (xsane_generate_dummy_filename(0)) /* create filename the scanned data is saved to */
    {
      /* temporary file */
      umask(0177); /* creare temporary file with "-rw-------" permissions */   
    }
    else
    {
      /* no temporary file */
      umask((mode_t) preferences.image_umask); /* define image file permissions */   
    }

    remove(xsane.dummy_filename); /* remove existing file */
    xsane.out = fopen(xsane.dummy_filename, "wb"); /* b = binary mode for win32 */
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   

    if (!xsane.out) /* error while opening the dummy_file for writing */
    {
      xsane_scan_done(-1); /* -1 = error */
      DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
      return;
    }

    if ( (xsane.expand_lineart_to_grayscale) || (xsane.reduce_16bit_to_8bit) )
    {
      xsane.depth = 8;
    }

    xsane_write_pnm_header(xsane.out, xsane.param.pixels_per_line, xsane.param.lines, xsane.depth);

    fflush(xsane.out);
    xsane.header_size = ftell(xsane.out);
  }

  if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
  {
    fseek(xsane.out, xsane.header_size + xsane.param.format - SANE_FRAME_RED, SEEK_SET);
  }

  xsane.pixelcolor = 0;

  snprintf(buf, sizeof(buf), PROGRESS_RECEIVING_FRAME_DATA, _(frame_type));
  xsane_progress_new(buf, PROGRESS_SCANNING, (GtkSignalFunc) xsane_cancel);

  xsane.input_tag = -1;

#ifndef BUGGY_GDK_INPUT_EXCEPTION
  /* for unix */
  if (sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD && sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD)
  {
    DBG(DBG_info, "gdk_input_add\n");
    xsane.input_tag = gdk_input_add(fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, xsane_read_image_data, 0);
  }
  else
#else
  /* for win32 */
  sane_set_io_mode(dev, SANE_FALSE);
#endif
  {
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the scan button is pressed */
/* or by scan_done if automatic document feeder is selected */
void xsane_scan_dialog(GtkWidget * widget, gpointer call_data)
{
 char buf[256];
 const SANE_Option_Descriptor *opt;

  DBG(DBG_proc, "xsane_scan_dialog\n");

  xsane.reduce_16bit_to_8bit = preferences.reduce_16bit_to_8bit; /* reduce 16 bit image to 8 bit ? */

  sane_get_parameters(xsane.dev, &xsane.param); /* update xsane.param */

  xsane_define_output_filename(); /* make xsane.output_filename up to date */

  /* test if file exists, may be we have to output an overwrite warning */
  if (xsane.mode == XSANE_STANDALONE) /* We are running in standalone mode */
  {
   char *extension;

    if ( (xsane.xsane_mode == XSANE_SCAN) && (preferences.overwrite_warning) ) /* test if filename already used */
    {
     FILE *testfile;

      testfile = fopen(xsane.output_filename, "rb"); /* read binary (b for win32) */
      if (testfile) /* filename used: skip */
      {
       char buf[256];

        fclose(testfile);
        snprintf(buf, sizeof(buf), WARN_FILE_EXISTS, xsane.output_filename);
        if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_OVERWRITE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
        {
          return;
        }
      }
    }

    xsane_identify_output_format(&extension); /* set xsane.xsane_output_format */

    if (xsane.xsane_mode == XSANE_SCAN)
    {
      if (xsane.xsane_output_format == XSANE_UNKNOWN)
      {
        if (extension)
        {
          snprintf(buf, sizeof(buf), ERR_UNSUPPORTED_OUTPUT_FORMAT, xsane.param.depth, extension);
        }
        else
        {
          snprintf(buf, sizeof(buf), "%s", ERR_NO_OUTPUT_FORMAT);
        }
        xsane_back_gtk_error(buf, TRUE);
        return;
      }
      else if ( ( ( (xsane.xsane_output_format == XSANE_JPEG) && xsane.param.depth == 16) ||
                  ( (xsane.xsane_output_format == XSANE_PS)   && xsane.param.depth == 16) ) &&
                ( !xsane.reduce_16bit_to_8bit) )
                
      {
        snprintf(buf, sizeof(buf), TEXT_REDUCE_16BIT_TO_8BIT);
        if (xsane_back_gtk_decision(ERR_HEADER_INFO, (gchar **) info_xpm, buf, BUTTON_REDUCE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
        {
          return;
        }
        xsane.reduce_16bit_to_8bit = TRUE;
      }
      
#ifdef SUPPORT_RGBA
      else if ((xsane.xsane_output_format == XSANE_RGBA) && (xsane.param.format != SANE_FRAME_RGBA))
      {
        snprintf(buf, sizeof(buf), "No RGBA data format !!!"); /* user selected output format RGBA, scanner uses other format */
        xsane_back_gtk_error(buf, TRUE);
        return;
      }
#endif
    }
#ifdef SUPPORT_RGBA
    else if (xsane.param.format == SANE_FRAME_RGBA) /* no scanmode but format=rgba */
    {
      snprintf(buf, sizeof(buf), "Special format RGBA only supported in scan mode !!!");
      xsane_back_gtk_error(buf, TRUE);
      return;
    }
#endif

#ifdef SUPPORT_RGBA
    if (xsane.param.format == SANE_FRAME_RGBA)
    {
      if ( (xsane.xsane_output_format != XSANE_RGBA) && (xsane.xsane_output_format != XSANE_PNG) )
      {
        snprintf(buf, sizeof(buf), "Image data of type SANE_FRAME_RGBA\ncan only be saved in rgba or png format");
        xsane_back_gtk_error(buf, TRUE);
        return;
      }
    }
#endif

    if (xsane.xsane_mode == XSANE_FAX)
    {
      mkdir(preferences.fax_project, 7*64 + 0*8 + 0);
    }

    if (extension)
    {
      free(extension);
    }
  }
#ifdef HAVE_LIBGIMP_GIMP_H
  else	/* We are running in gimp mode */
  {
    if ((xsane.param.depth != 1) && (xsane.param.depth != 8)) /* not support bit depth ? */
    {
      if (!xsane.reduce_16bit_to_8bit) /* ask if reduce 16 to 8 bit */
      {
        if (xsane.param.depth == 16)
        {
          snprintf(buf, sizeof(buf), TEXT_GIMP_REDUCE_16BIT_TO_8BIT);
          if (xsane_back_gtk_decision(ERR_HEADER_INFO, (gchar **) info_xpm, buf, BUTTON_REDUCE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
          {
            return;
          }
          xsane.reduce_16bit_to_8bit = TRUE;
        }
        else /* unsupported bit depth */
        {
          snprintf(buf, sizeof(buf), ERR_GIMP_BAD_DEPTH, xsane.param.depth);
          xsane_back_gtk_error(buf, TRUE);
         return;
        }
      }
    }
  }
#endif

  if (xsane.dummy_filename) /* no dummy filename defined - necessary if an error occurs */
  {
    free(xsane.dummy_filename);
    xsane.dummy_filename = 0;
  }

  /* create scanner gamma tables, xsane internal gamma tables are created after sane_start */
  if ( xsane.xsane_color && /* color scan */
       xsane.scanner_gamma_color ) /* gamma table for red, green and blue available */
  {
   double gamma_red, gamma_green, gamma_blue;
   int gamma_red_size, gamma_green_size, gamma_blue_size;
   int gamma_red_max, gamma_green_max, gamma_blue_max;

    /* ok, scanner color gamma function is supported, so we do all conversions about that */
    /* we do not need any gamma tables while scanning, so we can free them after sending */
    /* the data to the scanner */

    /* if also gray gamma function is supported, set this to 1.0 to get the right colors */
    if (xsane.scanner_gamma_gray)
    {
     int gamma_gray_size;
     int gamma_gray_max;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);
      gamma_gray_size = opt->size / sizeof(opt->type);
      gamma_gray_max  = opt->constraint.range->max;

      xsane.gamma_data = malloc(gamma_gray_size  * sizeof(SANE_Int));
      xsane_create_gamma_curve(xsane.gamma_data, 0, 1.0, 0.0, 0.0, gamma_gray_size, gamma_gray_max);
      xsane_back_gtk_update_vector(xsane.well_known.gamma_vector, xsane.gamma_data);
      free(xsane.gamma_data);
      xsane.gamma_data = 0;
    }

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_r);
    gamma_red_size = opt->size / sizeof(opt->type);
    gamma_red_max  = opt->constraint.range->max;

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_g);
    gamma_green_size = opt->size / sizeof(opt->type);
    gamma_green_max  = opt->constraint.range->max;

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector_b);
    gamma_blue_size = opt->size / sizeof(opt->type);
    gamma_blue_max  = opt->constraint.range->max;

    xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
    xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
    xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

    if (xsane.xsane_mode == XSANE_COPY)
    {
      gamma_red   = xsane.gamma * xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_red;
      gamma_green = xsane.gamma * xsane.gamma_green * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_green;
      gamma_blue  = xsane.gamma * xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_blue;
    }
    else
    {
      gamma_red   = xsane.gamma * xsane.gamma_red;
      gamma_green = xsane.gamma * xsane.gamma_green;
      gamma_blue  = xsane.gamma * xsane.gamma_blue;
    }

    xsane_create_gamma_curve(xsane.gamma_data_red, xsane.negative,
		 	       gamma_red,
			       xsane.brightness + xsane.brightness_red,
			       xsane.contrast + xsane.contrast_red, gamma_red_size, gamma_red_max);

    xsane_create_gamma_curve(xsane.gamma_data_green, xsane.negative,
			       gamma_green,
			       xsane.brightness + xsane.brightness_green,
			       xsane.contrast + xsane.contrast_green, gamma_green_size, gamma_green_max);

    xsane_create_gamma_curve(xsane.gamma_data_blue, xsane.negative,
			       gamma_blue,
			       xsane.brightness + xsane.brightness_blue,
			       xsane.contrast + xsane.contrast_blue , gamma_blue_size, gamma_blue_max);

    xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_r, xsane.gamma_data_red);
    xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_g, xsane.gamma_data_green);
    xsane_back_gtk_update_vector(xsane.well_known.gamma_vector_b, xsane.gamma_data_blue);

    free(xsane.gamma_data_red);
    free(xsane.gamma_data_green);
    free(xsane.gamma_data_blue);

    xsane.gamma_data_red   = 0;
    xsane.gamma_data_green = 0;
    xsane.gamma_data_blue  = 0;
  }
  else if (xsane.scanner_gamma_gray) /* only scanner gray gamma function available */
  {
   /* ok, the scanner only supports gray gamma function */
   /* if we are doing a grayscale scan everyting is ok, */
   /* for a color scan the software has to do the gamma correction set by the component slider */

   double gamma;
   int gamma_gray_size;
   int gamma_gray_max;

    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.gamma_vector);
    gamma_gray_size = opt->size / sizeof(opt->type);
    gamma_gray_max  = opt->constraint.range->max;

    if (xsane.xsane_mode == XSANE_COPY)
    {
      gamma = xsane.gamma * preferences.printer[preferences.printernr]->gamma;
    }
    else
    {
      gamma = xsane.gamma;
    }

    xsane.gamma_data = malloc(gamma_gray_size * sizeof(SANE_Int));
    xsane_create_gamma_curve(xsane.gamma_data, xsane.negative,
                             gamma, xsane.brightness, xsane.contrast,
                             gamma_gray_size, gamma_gray_max);

    xsane_back_gtk_update_vector(xsane.well_known.gamma_vector, xsane.gamma_data);
    free(xsane.gamma_data);
    xsane.gamma_data = 0;
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_start_scan();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_create_internal_gamma_tables(void)
{
 int size, maxval;

  size = (int) pow(2, xsane.param.depth);
  maxval = size-1;

  if (xsane.xsane_color) /* color scan */
  {
    if ( (!xsane.scanner_gamma_color) && (xsane.scanner_gamma_gray) )
    {
      /* we have to create color gamma table for software conversion */
	/* but we only have to use color slider values, because gray slider value */
	/* is used by scanner gray gamma */

     double gamma_red, gamma_green, gamma_blue;

      DBG(DBG_info, "creating xsane internal color gamma tables with size %d\n", size);

      xsane.gamma_data_red   = malloc(size * sizeof(SANE_Int));
      xsane.gamma_data_green = malloc(size * sizeof(SANE_Int));
      xsane.gamma_data_blue  = malloc(size * sizeof(SANE_Int));

      if (xsane.xsane_mode == XSANE_COPY)
      {
        gamma_red   = xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma_red;
        gamma_green = xsane.gamma_green * preferences.printer[preferences.printernr]->gamma_green;
        gamma_blue  = xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma_blue;
      }
      else
      {
        gamma_red   = xsane.gamma_red;
        gamma_green = xsane.gamma_green;
        gamma_blue  = xsane.gamma_blue;
      }
    
      xsane_create_gamma_curve(xsane.gamma_data_red, 0, gamma_red, xsane.brightness_red, xsane.contrast_red, size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_green, 0, gamma_green, xsane.brightness_green, xsane.contrast_green, size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_blue, 0, gamma_blue, xsane.brightness_blue, xsane.contrast_blue, size, maxval);

      /* gamma tables are freed after scan */
    }
    else if ( (!xsane.scanner_gamma_color) && (!xsane.scanner_gamma_gray) ) /* no scanner gamma table */
    {
     double gamma_red, gamma_green, gamma_blue;
      /* ok, we have to combin gray and color slider values */

      DBG(DBG_info, "creating xsane internal complete gamma tables with size %d\n", size);

      xsane.gamma_data_red   = malloc(size * sizeof(SANE_Int));
      xsane.gamma_data_green = malloc(size * sizeof(SANE_Int));
      xsane.gamma_data_blue  = malloc(size * sizeof(SANE_Int));

      if (xsane.xsane_mode == XSANE_COPY)
      {
        gamma_red   = xsane.gamma * xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_red;
        gamma_green = xsane.gamma * xsane.gamma_green * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_green;
        gamma_blue  = xsane.gamma * xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_blue;
      }
      else
      {
        gamma_red   = xsane.gamma * xsane.gamma_red;
        gamma_green = xsane.gamma * xsane.gamma_green;
        gamma_blue  = xsane.gamma * xsane.gamma_blue;
      }

      xsane_create_gamma_curve(xsane.gamma_data_red, xsane.negative,
                               gamma_red,
                               xsane.brightness + xsane.brightness_red,
                               xsane.contrast + xsane.contrast_red, size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_green, xsane.negative,
                               gamma_green,
                               xsane.brightness + xsane.brightness_green,
                               xsane.contrast + xsane.contrast_green, size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_blue, xsane.negative,
                               gamma_blue,
                               xsane.brightness + xsane.brightness_blue,
                               xsane.contrast + xsane.contrast_blue , size, maxval);

      /* gamma tables are freed after scan */
    }
  }
  else /* grayscale scan */
  {
    if (!xsane.scanner_gamma_gray) /* no gray scanner gamma table */
    {
     double gamma;

      DBG(DBG_info, "creating xsane internal gray gamma table with size %d\n", size);

      if (xsane.xsane_mode == XSANE_COPY)
      {
        gamma = xsane.gamma * preferences.printer[preferences.printernr]->gamma;
      }
      else
      {
        gamma = xsane.gamma;
      }

      xsane.gamma_data = malloc(size * sizeof(SANE_Int));
      xsane_create_gamma_curve(xsane.gamma_data, xsane.negative,
                               gamma, xsane.brightness, xsane.contrast, size, maxval);

      /* gamma table is freed after scan */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

