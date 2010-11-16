/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-scan.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2010 Oliver Rauch
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
#include "xsane-viewer.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"
#include "xsane-multipage-project.h"
#include "xsane-fax-project.h"
#include "xsane-email-project.h"

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
gint xsane_scan_dialog(gpointer *data);
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

  if ( (conversion_level == 0) && (xsane.scan_rotation) ) /* scan level with rotation */
  {
    tempfile = TRUE;
  }

  if ( (conversion_level == 1) && (xsane.expand_lineart_to_grayscale) ) /* rotation level and expanded lineart*/
  {
    tempfile = TRUE;
  }

  if ( (xsane.mode == XSANE_GIMP_EXTENSION) ||
       (xsane.xsane_mode == XSANE_COPY) ||
       (xsane.xsane_mode == XSANE_VIEWER) ||
       ( (xsane.xsane_mode == XSANE_SAVE)  &&
         (xsane.xsane_output_format != XSANE_PNM) &&
         (xsane.xsane_output_format != XSANE_RGBA) ) )
  {
    tempfile = TRUE;
  }

  if (tempfile) /* save to temporary file */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, 0, 0, "xsane-conversion-", xsane.dev_name, ".ppm", XSANE_PATH_TMP);
    xsane.dummy_filename = strdup(filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

   return TRUE;
  }
  else if (xsane.xsane_mode == XSANE_MULTIPAGE) /* no conversion following, save directly to the selected filename */
  {
    xsane.dummy_filename = strdup(xsane.multipage_filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

   return FALSE;
  }
  else if (xsane.xsane_mode == XSANE_FAX) /* no conversion following, save directly to the selected filename */
  {
    xsane.dummy_filename = strdup(xsane.fax_filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

   return FALSE;
  }
  else if (xsane.xsane_mode == XSANE_EMAIL) /* no conversion following, save directly to the selected filename */
  {
    xsane.dummy_filename = strdup(xsane.email_filename);
    DBG(DBG_info, "xsane.dummy_filename = %s\n", xsane.dummy_filename);

   return FALSE;
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
 char buf[TEXTBUFSIZE];
 size_t bytes_read;

  DBG(DBG_proc, "xsane_read_image_data\n");

  xsane.reading_data = TRUE;

  if ( (xsane.param.depth == 1) || (xsane.param.depth == 8) )
  {
   unsigned char buf8[2*32768];
   unsigned char *buf8ptr;

    DBG(DBG_info, "depth = 1 or 8 bit\n");

    while (1)
    {
      if (xsane.cancel_scan)
      {
        break; /* leave while loop */
      }

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

      if (!len) /* nothing read */
      {
        if (xsane.input_tag >= 0)
        {
          break; /* leave xsane_read_image_data, will be called by gdk when select_fd event occurs */
        }
        else /* no select fd available */
        {
          while (gtk_events_pending())
          {
            DBG(DBG_info, "calling gtk_main_iteration\n");
            gtk_main_iteration();
          }
          continue; /* we have to keep this loop running because it will never be called again */
        }
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.bytes_read / (gfloat) xsane.num_bytes);

      /* it is not allowed to call gtk_main_iteration when we have gdk_input active */
      /* because xsane_read_image_data will be called several times */
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

                  xsane.lineart_to_grayscale_x--;
                  if (xsane.lineart_to_grayscale_x <= 0)
                  {
                    xsane.lineart_to_grayscale_x = xsane.param.pixels_per_line;
                    break;
                  }
                }
                buf8ptr++;
              }
              fwrite(expanded_buf8, 1, (size_t) (expanded_buf8ptr - expanded_buf8), xsane.out);
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
           unsigned char rgbbuf[3 * XSANE_3PASS_BUFFER_RGB_SIZE];
           int pos;

            DBG(DBG_info, "3 pass color\n");

            if (!xsane.scanner_gamma_color) /* gamma correction by xsane */
            {
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
                buf8[i] = gamma[(int) buf8[i]];
              }
            }

            buf8ptr = buf8;
            pos = 0;

            while(pos < len)
            {
             int cnt, bytes;

              cnt = len - pos;

              if (cnt > XSANE_3PASS_BUFFER_RGB_SIZE)
              {
                cnt = XSANE_3PASS_BUFFER_RGB_SIZE;
              }

              bytes = 3 * cnt - 2;

              /* if there already is data: read block of already scanned colors */
              if( (xsane.param.format > SANE_FRAME_RED) && (cnt > 1) )
              {
               long fpos = ftell(xsane.out);

                fseek(xsane.out, 0, SEEK_CUR); /* sync between write and read */
                bytes_read = fread(rgbbuf, 1, bytes - 1, xsane.out);
                fseek(xsane.out, fpos, SEEK_SET);
              }

              /* add just scanned color to block */
              for(j = 0; j < cnt; j++)
              {
                rgbbuf[3 * j] = buf8ptr[j];
              }

              /* write block back to disk */
              fwrite(rgbbuf, 1, bytes, xsane.out);

              pos += cnt;
              buf8ptr += cnt;

              /* skip the bytes for the two other colors */
              fseek(xsane.out, 2, SEEK_CUR);
            } /* while(pos < len) */
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
   char buf[TEXTBUFSIZE];

    DBG(DBG_info, "depth = 16 bit\n");

    while (1)
    {
      if (xsane.cancel_scan)
      {
        break; /* leave while loop */
      }

      if (xsane.read_offset_16) /* if we have had an odd number of bytes */
      {
        buf8[0] = xsane.last_offset_16_byte;
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

      DBG(DBG_info, "sane_read returned with status %s\n", XSANE_STRSTATUS(status));
      DBG(DBG_info, "sane_read: len = %d\n", len);


      if (!xsane.scanning) /* scan may have been canceled while sane_read was executed */
      {
        return; /* ok, the scan has been canceled */
      }


      if (len % 2) /* odd number of bytes */
      {
        len--;
        xsane.last_offset_16_byte = buf16[len];
        xsane.read_offset_16 = 1;
      }
      else /* even number of bytes */
      {
        xsane.read_offset_16 = 0;
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
        if (xsane.input_tag >= 0)
        {
          break; /* leave xsane_read_image_data, will be called by gdk when select_fd event occurs */
        }
        else /* no select fd available */
        {
          while (gtk_events_pending())
          {
            DBG(DBG_info, "calling gtk_main_iteration\n");
            gtk_main_iteration();
          }
          continue; /* we have to keep this loop running because it will never be called again */
        }
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
              fwrite(buf16 + i, 2, 1, xsane.out);
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

  if (xsane.cancel_scan)
  {
    xsane.cancel_scan = FALSE; /* make sure we do not get an infinite loop */
    
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }

  xsane.reading_data = FALSE;

 return;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_sigpipe_handler(int signal)
/* this is to catch a broken pipe while writing to printercommand */
{
  DBG(DBG_proc, "xsane_sigpipe_handler\n");

  xsane_cancel_save(&xsane.cancel_save);
  xsane.broken_pipe = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_test_multi_scan(void)
{
  DBG(DBG_proc, "xsane_test_multi_scan\n");

  if (xsane.mode == XSANE_GIMP_EXTENSION)
  {
    return FALSE;
  }

  if (xsane.batch_loop)
  {
    return FALSE;
  }

  if (xsane.adf_page_counter+1 < preferences.adf_pages_max)
  {
    return TRUE;
  }
  else
  {
    xsane.adf_page_counter = 0;
    return FALSE;
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

static int xsane_reduce_to_lineart()
/* returns 1 on abort, 0 on success */
{
 char *old_dummy_filename;
 int abort = 0;

  /* open progressbar */
  xsane_progress_new(PROGRESS_PACKING_DATA, PROGRESS_TRANSFERRING_DATA, (GtkSignalFunc) xsane_cancel_save, &xsane.cancel_save);
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  /* on some filesystems it is not allowed to erase an opened file and access the
     file after that, so we must wait until the file is closed */
  old_dummy_filename = strdup(xsane.dummy_filename);

  /* temporary file is created with permission 0600 in xsane_generate_dummy_filename */
  if (!xsane_generate_dummy_filename(2)) /* create filename for packing */
  {
    /* no temporary file */
    if (xsane_create_secure_file(xsane.dummy_filename)) /* remove possibly existing symbolic links for security */
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, xsane.dummy_filename);
      xsane_back_gtk_error(buf, TRUE);
      abort = 1; /* abort scanning */
    }
  }

  if (!abort)
  {
    abort = xsane_save_image_as_lineart(xsane.dummy_filename, old_dummy_filename, xsane.progress_bar, &xsane.cancel_save);
  }

  if (abort)
  {
    xsane_set_sensitivity(TRUE);		/* reactivate buttons etc */
    sane_cancel(xsane.dev); /* stop scanning */
    xsane_update_histogram(TRUE /* update raw */);
    xsane_update_param(0);
    xsane.header_size = 0;
   return 1;
  }

  remove(old_dummy_filename); /* remove the unrotated image file */
  free(old_dummy_filename); /* release memory */
  xsane_progress_clear();

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scan_done(SANE_Status status)
{
 Image_info image_info;

  DBG(DBG_proc, "xsane_scan_done\n");

  if (!xsane.scanning)
  {
    return;
  }

  xsane.reading_data = FALSE;
  xsane.scanning = FALSE; /* set marker that sane_start is called */

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
   int pixel_height = xsane.bytes_read / xsane.param.bytes_per_line;

    /* correct image height if necessary, e.g. when scanning with hand scanner */

    if (xsane.param.lines != pixel_height)
    {
      DBG(DBG_info, "correcting image height to %d lines\n", pixel_height);
      xsane.param.lines = pixel_height;

      image_info.image_width       = xsane.param.pixels_per_line;
      image_info.image_height      = pixel_height;
      image_info.depth             = xsane.depth;
      image_info.channels          = xsane.xsane_channels;

      image_info.resolution_x      = xsane.resolution_x;
      image_info.resolution_y      = xsane.resolution_y;

      image_info.gamma             = xsane.gamma;
      image_info.gamma_red         = xsane.gamma_red;
      image_info.gamma_green       = xsane.gamma_green;
      image_info.gamma_blue        = xsane.gamma_blue;

      image_info.brightness        = xsane.brightness;
      image_info.brightness_red    = xsane.brightness_red;
      image_info.brightness_green  = xsane.brightness_green;
      image_info.brightness_blue   = xsane.brightness_blue;
 
      image_info.contrast          = xsane.contrast;
      image_info.contrast_red      = xsane.contrast_red;
      image_info.contrast_green    = xsane.contrast_green;
      image_info.contrast_blue     = xsane.contrast_blue;
 
      image_info.threshold         = xsane.threshold;

      image_info.reduce_to_lineart = xsane.expand_lineart_to_grayscale;

      image_info.enable_color_management = xsane.enable_color_management;
      image_info.cms_function            = preferences.cms_function;
      image_info.cms_intent              = preferences.cms_intent;
      image_info.cms_bpc                 = preferences.cms_bpc;

      image_info.icm_profile[0] = 0; /* empty string */

      if (image_info.channels == 1)
      {
        if (xsane.scanner_default_gray_icm_profile)
        {
          strncpy(image_info.icm_profile, xsane.scanner_default_gray_icm_profile, sizeof(image_info.icm_profile));
        }
      }
      else
      {
        if (xsane.scanner_default_color_icm_profile)
        {
          strncpy(image_info.icm_profile, xsane.scanner_default_color_icm_profile, sizeof(image_info.icm_profile));
        }
      }


      xsane_write_pnm_header(xsane.out, &image_info, 0);
    }

    DBG(DBG_info, "closing output file\n");

    fflush(xsane.out);
    fclose(xsane.out);
    xsane.out = 0;
  }

  if ( (status == SANE_STATUS_GOOD) || (status == SANE_STATUS_EOF) ) /* no error, do conversion etc. */
  {
    /* do we have to rotate the image ? */
    if (xsane.scan_rotation)
    {
     char *old_dummy_filename;
     int abort = 0;
     FILE *outfile;
     FILE *infile;

      infile = fopen(xsane.dummy_filename, "rb"); /* read binary (b for win32) */
      if (infile != 0)
      {
        xsane_read_pnm_header(infile, &image_info);

        /* open progressbar */
        xsane_progress_new(PROGRESS_ROTATING_DATA, PROGRESS_TRANSFERRING_DATA, (GtkSignalFunc) xsane_cancel_save, &xsane.cancel_save);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }

        /* on some filesystems it is not allowed to erase an opened file and access the
           file after that, so we must wait until the file is closed */
        old_dummy_filename = strdup(xsane.dummy_filename);

        /* temporary file is created with permission 0600 in xsane_generate_dummy_filename */
        if (!xsane_generate_dummy_filename(1)) /* create filename for rotation */
        {
          /* no temporary file */
          if (xsane_create_secure_file(xsane.dummy_filename)) /* remove possibly existing symbolic links for security */
          {
           char buf[TEXTBUFSIZE];

            snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, xsane.dummy_filename);
            xsane_back_gtk_error(buf, TRUE);
            abort = 1; /* abort scanning */
          }
        }

        if (!abort)
        {
          /* rotate image */
          outfile = fopen(xsane.dummy_filename, "wb"); /* read binary (b for win32) */

          if (outfile)
          {
            if (xsane_save_rotate_image(outfile, infile, &image_info, xsane.scan_rotation, xsane.progress_bar, &xsane.cancel_save))
            {
              abort = 1;
            }
          }
          else
          {
           char buf[TEXTBUFSIZE];
            DBG(DBG_info, "open of file `%s'failed : %s\n", xsane.dummy_filename, strerror(errno));
            snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, xsane.dummy_filename, strerror(errno));
            xsane_back_gtk_error(buf, TRUE);
            abort = 1;
          }

          fclose(outfile);
        }

        fclose(infile);
        remove(old_dummy_filename); /* remove the unrotated image file */

        free(old_dummy_filename); /* release memory */
        xsane_progress_clear();
      }
      else
      {
       char buf[TEXTBUFSIZE];
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

    if (xsane.xsane_mode == XSANE_VIEWER)
    {
      if (!xsane.force_filename) /* user filename selection active */
      {
        xsane_viewer_new(xsane.dummy_filename, preferences.filetype, TRUE, preferences.filename, VIEWER_FULL_MODIFICATION, IMAGE_NOT_SAVED);
      }
      else
      {
        xsane_viewer_new(xsane.dummy_filename, NULL, TRUE, xsane.external_filename, VIEWER_NO_NAME_MODIFICATION, IMAGE_NOT_SAVED);
      }

      xsane.expand_lineart_to_grayscale = 0;
    }

    if ((xsane.xsane_mode == XSANE_MULTIPAGE) || (xsane.xsane_mode == XSANE_FAX) || (xsane.xsane_mode == XSANE_EMAIL))
    {
      xsane.expand_lineart_to_grayscale = 0;
    }

    /* when we are scanning in lineart mode and we are transforming the image */
    /* it is saved as grayscale while scanning so we can use the standard transformations */
    /* but now we have to save the lineart image as packed lineart again */
    if ((xsane.param.depth == 1) && (xsane.expand_lineart_to_grayscale)) 
    {
      if (xsane_reduce_to_lineart())
      {
        return; /* aborted */
      }
    }

    if (xsane.xsane_mode == XSANE_SAVE)
    {
      if ( ( (xsane.xsane_output_format != XSANE_PNM) && /* these files do not need any transformation */
             (xsane.xsane_output_format != XSANE_RGBA) ) ||
           (xsane.mode == XSANE_GIMP_EXTENSION) )
      { /* ok, we have to do a transformation */

        /* open progressbar */
        xsane_progress_new(PROGRESS_CONVERTING_DATA, PROGRESS_TRANSFERRING_DATA, (GtkSignalFunc) xsane_cancel_save, &xsane.cancel_save);
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }


#ifdef HAVE_ANY_GIMP
        if (xsane.mode == XSANE_GIMP_EXTENSION)	/* xsane runs as gimp plugin */
        {
          xsane_transfer_to_gimp(xsane.dummy_filename, xsane.enable_color_management, preferences.cms_function, xsane.progress_bar, &xsane.cancel_save);
        }
        else
#endif /* HAVE_ANY_GIMP */
        {
          xsane_save_image_as(xsane.output_filename, xsane.dummy_filename, xsane.xsane_output_format, xsane.enable_color_management, preferences.cms_function, preferences.cms_intent, preferences.cms_bpc, xsane.progress_bar, &xsane.cancel_save);
        }

        xsane_progress_clear();

        while (gtk_events_pending())
        { 
          gtk_main_iteration();
        }

        remove(xsane.dummy_filename);
      }

      if (xsane.print_filenames) /* print created filenames to stdout? */
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
    }
    else if (xsane.xsane_mode == XSANE_COPY)
    {
     FILE *outfile;
     FILE *infile;
     char buf[TEXTBUFSIZE];

      DBG(DBG_info, "XSANE_COPY\n");

      xsane_update_int(xsane.copy_number_entry, &xsane.copy_number); /* get number of copies */
      if (xsane.copy_number < 1)
      {
        xsane.copy_number = 1;
      }

      /* open progressbar */
      xsane_progress_new(PROGRESS_CONVERTING_DATA, PROGRESS_TRANSFERRING_DATA, (GtkSignalFunc) xsane_cancel_save, &xsane.cancel_save);

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

        xsane_read_pnm_header(infile, &image_info);

        imagewidth  = 72.0 * image_info.image_width /image_info.resolution_x * xsane.zoom; /* desired width in 1/72 inch */
        imageheight = 72.0 * image_info.image_height/image_info.resolution_y * xsane.zoom; /* desired height in 1/72 inch */

        memset (&act, 0, sizeof (act)); /* define broken pipe handler */
        act.sa_handler = xsane_sigpipe_handler;
        sigaction (SIGPIPE, &act, 0);

        DBG(DBG_info, "imagewidth  = %f 1/72 inch\n", imagewidth);
        DBG(DBG_info, "imageheight = %f 1/72 inch\n", imageheight);
        DBG(DBG_info, "zoom        = %f\n", xsane.zoom);

        xsane_save_ps(outfile, infile,
                      &image_info,
                      imagewidth, imageheight,
                      preferences.printer[preferences.printernr]->leftoffset   * 72.0/MM_PER_INCH, /* paper_left_margin */
                      preferences.printer[preferences.printernr]->bottomoffset * 72.0/MM_PER_INCH, /* paper_bottom_margin */
                      preferences.printer[preferences.printernr]->width  * 72.0/MM_PER_INCH, /* usable paper_width */
                      preferences.printer[preferences.printernr]->height * 72.0/MM_PER_INCH, /* usable paper_height */
                      preferences.paper_orientation,
                      preferences.printer[preferences.printernr]->ps_flatedecoded, /* ps level 3 */
                      NULL /* hTransform */, xsane.enable_color_management,
                      preferences.printer[preferences.printernr]->embed_csa, xsane.scanner_default_color_icm_profile,
                      preferences.printer[preferences.printernr]->embed_crd, preferences.printer[preferences.printernr]->icm_profile, preferences.printer[preferences.printernr]->cms_bpc,
                      0 /* intent */,
                      xsane.progress_bar,
                      &xsane.cancel_save);
      }
      else
      {
       char buf[TEXTBUFSIZE];

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

    if ( ( (xsane.xsane_mode == XSANE_SAVE) || (xsane.xsane_mode == XSANE_VIEWER) ) && (xsane.mode == XSANE_STANDALONE) )
    {
      if (!xsane.force_filename) /* user filename selection active */
      {
        if (preferences.filename_counter_step) /* increase filename counter ? */
        {
          xsane_update_counter_in_filename(&preferences.filename, preferences.skip_existing_numbers,
                                           preferences.filename_counter_step, preferences.filename_counter_len);
          xsane_set_filename(preferences.filename); /* update filename in entry */
          gtk_entry_set_position(GTK_ENTRY(xsane.outputfilename_entry), strlen(preferences.filename)); /* set cursor to right position of filename */
        }
      }
      else /* external filename */
      {
        xsane_update_counter_in_filename(&xsane.external_filename, TRUE, 1, 0);
      }
    }
    else if (xsane.xsane_mode == XSANE_MULTIPAGE)
    {
     GtkWidget *list_item;
     char *page;
     char *type;
     char *extension;

      page = strdup(strrchr(xsane.multipage_filename,'/')+1);
      extension = strrchr(page, '.');
      if (extension)
      {
        type = strdup(extension);
        *extension = 0;
      }
      else
      {
        type = "";
      }

      list_item = gtk_list_item_new_with_label(page);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
      gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
      gtk_widget_show(list_item);

      xsane_update_counter_in_filename(&xsane.multipage_filename, TRUE, 1, preferences.filename_counter_len);
      xsane_multipage_project_save();
      free(page);
      free(type);

      if (xsane.multipage_status)
      {
        free(xsane.multipage_status);
      }
      xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CHANGED);

      xsane_multipage_project_save();

      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
    }
    else if (xsane.xsane_mode == XSANE_FAX)
    {
     GtkWidget *list_item;
     char *page;
     char *type;
     char *extension;

      page = strdup(strrchr(xsane.fax_filename,'/')+1);
      extension = strrchr(page, '.');
      if (extension)
      {
        type = strdup(extension);
        *extension = 0;
      }
      else
      {
        type = "";
      }

      list_item = gtk_list_item_new_with_label(page);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
      gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
      gtk_widget_show(list_item);

      xsane_update_counter_in_filename(&xsane.fax_filename, TRUE, 1, preferences.filename_counter_len);
      xsane_fax_project_save();
      free(page);
      free(type);

      if (xsane.fax_status)
      {
        free(xsane.fax_status);
      }
      xsane.fax_status = strdup(TEXT_PROJECT_STATUS_CHANGED);

      xsane_fax_project_save();

      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
    }
#ifdef XSANE_ACTIVATE_EMAIL
    else if (xsane.xsane_mode == XSANE_EMAIL)
    {
     GtkWidget *list_item;
     char *page;
     char *type;
     char *extension;

      page = strdup(strrchr(xsane.email_filename,'/')+1);
      extension = strrchr(page, '.');
      if (extension)
      {
        type = strdup(extension);
        *extension = 0;
      }
      else
      {
        type = "";
      }

      list_item = gtk_list_item_new_with_label(page);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
      gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
      gtk_widget_show(list_item);

      xsane_update_counter_in_filename(&xsane.email_filename, TRUE, 1, preferences.filename_counter_len);
      xsane_email_project_save();
      free(page);
      free(type);

      if (xsane.email_status)
      {
        free(xsane.email_status);
      }
      xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);

      xsane_email_project_save();

      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.email_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
    }
#endif
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

    DBG(DBG_info, "ADF mode end of scan: increment page counter and restart scan\n");
    xsane.adf_page_counter += 1;
    gtk_timeout_add(100, (GtkFunction)xsane_scan_dialog, NULL); /* wait 100ms then call xsane_scan_dialog(); */
  }
  else if ( ( (status == SANE_STATUS_GOOD) || (status == SANE_STATUS_EOF) ) && (xsane.batch_loop  == BATCH_MODE_LOOP) )
  {
    /* batch scan loop, this is not the last scan */
    DBG(DBG_info, "Batch mode end of scan\n");
    sane_cancel(xsane.dev); /* we have to call sane_cancel otherwise we are not able to set new parameters */
  }
/*  else if ( ( (status != SANE_STATUS_GOOD) && (status != SANE_STATUS_EOF) ) || (!xsane.batch_loop) ) */ /* last scan: update histogram */
  else
  {
    DBG(DBG_info, "Normal end of scan\n");
    xsane.adf_page_counter = 0;
    xsane_set_sensitivity(TRUE);		/* reactivate buttons etc */
    sane_cancel(xsane.dev); /* stop scanning */
    xsane_update_histogram(TRUE /* update raw */);
    xsane_update_param(0);
  }

  xsane.status_of_last_scan = status;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel(void)
{
  DBG(DBG_proc, "xsane_cancel\n");

  if (!xsane.scanning)
  {
    return;
  }

  sane_cancel(xsane.dev);

  /* we have to make sure that xsane does detect that the scan has been cancled */
  /* but the select_fd does not make sure that preview_read_image_data is called */
  /* when the select_fd is closed by the backend, so we have to make sure that */
  /* preview_read_image_data is called */

  if (xsane.reading_data) /* we are still reading data, set flag for cancel */
  {
    xsane.cancel_scan = TRUE;
  }
  else /* we are not reading image data, so we have to call it now */
  {
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* xsane_start_scan is called 3 times in 3 pass scanning mode */
static void xsane_start_scan(void)
{
 SANE_Status status;
 SANE_Handle dev = xsane.dev;
 const char *frame_type = 0;
 char buf[TEXTBUFSIZE];
 int fd;
 Image_info image_info;

  DBG(DBG_proc, "xsane_start_scan\n");

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane.read_offset_16 = 0; /* no last byte of old 16 bit data */

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

  if ((xsane.param.depth == 1) && ((xsane.scan_rotation) ||
                                   (xsane.xsane_mode == XSANE_VIEWER) ||
                                   (xsane.xsane_mode == XSANE_MULTIPAGE) ||
                                   (xsane.xsane_mode == XSANE_FAX) ||
                                   (xsane.xsane_mode == XSANE_EMAIL))
     ) /* We want to do a transformation with a lineart scan */
       /* or use the viewer to display a lineart scan, */
       /* so we save it as grayscale */
  {
    DBG(DBG_proc, "lineart scan will be expanded to grayscale\n");
    xsane.expand_lineart_to_grayscale = 1;
  }

  if (!xsane.header_size) /* first pass of multi pass scan or single pass scan */
  {
    xsane_create_internal_gamma_tables(); /* create gamma tables that are not supported by scanner */

    /* temporary file is created with permission 0600 in xsane_generate_dummy_filename */
    if (!xsane_generate_dummy_filename(0)) /* create filename the scanned data is saved to */
    {
      /* no temporary file */
      if (xsane_create_secure_file(xsane.dummy_filename)) /* remove possibly existing symbolic links for security */
      {
        snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, xsane.dummy_filename);
        xsane_scan_done(-1); /* -1 = error */
        xsane_back_gtk_error(buf, TRUE);
       return;
      }
    }

    /* create file: + = also allow read for blocked rgb multiplexing,  b = binary mode for win32 */
    xsane.out = fopen(xsane.dummy_filename, "wb+");

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

    image_info.image_width      = xsane.param.pixels_per_line;
    image_info.image_height     = xsane.param.lines;
    image_info.depth            = xsane.depth;
    image_info.channels         = xsane.xsane_channels;

    image_info.resolution_x     = xsane.resolution_x;
    image_info.resolution_y     = xsane.resolution_y;

    image_info.gamma            = xsane.gamma;
    image_info.gamma_red        = xsane.gamma_red;
    image_info.gamma_green      = xsane.gamma_green;
    image_info.gamma_blue       = xsane.gamma_blue;

    image_info.brightness       = xsane.brightness;
    image_info.brightness_red   = xsane.brightness_red;
    image_info.brightness_green = xsane.brightness_green;
    image_info.brightness_blue  = xsane.brightness_blue;
 
    image_info.contrast         = xsane.contrast;
    image_info.contrast_red     = xsane.contrast_red;
    image_info.contrast_green   = xsane.contrast_green;
    image_info.contrast_blue    = xsane.contrast_blue;
 
    image_info.threshold        = xsane.threshold;

    image_info.reduce_to_lineart = xsane.expand_lineart_to_grayscale;

    image_info.enable_color_management = xsane.enable_color_management;
    image_info.cms_function            = preferences.cms_function;
    image_info.cms_intent              = preferences.cms_intent;
    image_info.cms_bpc                 = preferences.cms_bpc;


    image_info.icm_profile[0] = 0; /* empty string */

    if (image_info.channels == 1)
    {
      if (xsane.scanner_default_gray_icm_profile)
      {
        strncpy(image_info.icm_profile, xsane.scanner_default_gray_icm_profile, sizeof(image_info.icm_profile));
      }
    }
    else
    {
      if (xsane.scanner_default_color_icm_profile)
      {
        strncpy(image_info.icm_profile, xsane.scanner_default_color_icm_profile, sizeof(image_info.icm_profile));
      }
    }

    xsane_write_pnm_header(xsane.out, &image_info, 0);

    fflush(xsane.out);
    xsane.header_size = ftell(xsane.out); /* store header size for 3 pass scan */
  }

  if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
  {
/* correct this using read_pnm_header */
    fseek(xsane.out, xsane.header_size + xsane.param.format - SANE_FRAME_RED, SEEK_SET);
  }

  xsane.pixelcolor = 0;

  snprintf(buf, sizeof(buf), PROGRESS_RECEIVING_FRAME_DATA, _(frame_type));
  
  if (preferences.adf_pages_max > 1)
  {
   char buf2[TEXTBUFSIZE];
    if (preferences.adf_pages_max > 1)
    {
      snprintf(buf2, sizeof(buf2), "%s (%d/%d)", PROGRESS_SCANNING, xsane.adf_page_counter+1, preferences.adf_pages_max);
    }
    else
    {
      snprintf(buf2, sizeof(buf2), "%s (%d)", PROGRESS_SCANNING, xsane.adf_page_counter+1);
    }
    xsane_progress_new(buf, buf2, (GtkSignalFunc) xsane_cancel, NULL);
  }
  else
  {
    xsane_progress_new(buf, PROGRESS_SCANNING, (GtkSignalFunc) xsane_cancel, NULL);
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane.input_tag = -1;

  xsane.lineart_to_grayscale_x = xsane.param.pixels_per_line;
#ifndef BUGGY_GDK_INPUT_EXCEPTION
  if ((sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD) && (sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD))
  {
    DBG(DBG_info, "gdk_input_add\n");
    xsane.input_tag = gdk_input_add(fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, xsane_read_image_data, 0);
  }
  else
#endif
  {
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the scan button is pressed */
/* or by scan_done if automatic document feeder is selected */
/* always returns 0 beacause ADF function calls it as timeout function */
/* and return value 0 is used to tell the timeout handler to stop timer */
gint xsane_scan_dialog(gpointer *data)
{
 char buf[TEXTBUFSIZE];
 const SANE_Option_Descriptor *opt;

  DBG(DBG_proc, "xsane_scan_dialog\n");

  xsane_set_sensitivity(FALSE);

  xsane.reduce_16bit_to_8bit = preferences.reduce_16bit_to_8bit; /* reduce 16 bit image to 8 bit ? */

  sane_get_parameters(xsane.dev, &xsane.param); /* update xsane.param */

  if ( ( (xsane.xsane_mode == XSANE_SAVE) || (xsane.xsane_mode == XSANE_VIEWER) ) && (xsane.mode == XSANE_STANDALONE) )
  {
    /* correct length of filename counter if it is shorter than minimum length */
    if (!xsane.force_filename)
    {
      /* when we are doing an adf or batch scan then we need a counter in the filename */
      if ((xsane.batch_loop) || (preferences.adf_pages_max > 1))
      {
        xsane_ensure_counter_in_filename(&preferences.filename, preferences.filename_counter_len);
      }
      xsane_update_counter_in_filename(&preferences.filename, FALSE, 0, preferences.filename_counter_len);
      xsane_set_filename(preferences.filename); 
    }
  }

  xsane_define_output_filename(); /* make xsane.output_filename up to date */

  /* test if file exists, may be we have to output an overwrite warning */
  if (xsane.mode == XSANE_STANDALONE) /* We are running in standalone mode */
  {
   char *extension;

    if ( (xsane.xsane_mode == XSANE_SAVE) && (preferences.overwrite_warning) ) /* test if filename already used */
    {
     FILE *testfile;

      testfile = fopen(xsane.output_filename, "rb"); /* read binary (b for win32) */
      if (testfile) /* filename used: skip */
      {
       char buf[TEXTBUFSIZE];

        fclose(testfile);

        snprintf(buf, sizeof(buf), WARN_FILE_EXISTS, xsane.output_filename);
        if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_OVERWRITE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
        {
          xsane_set_sensitivity(TRUE);
          return FALSE;
        }
      }
    }

    xsane.xsane_output_format = xsane_identify_output_format(xsane.output_filename, preferences.filetype, &extension);

    if (xsane.xsane_mode == XSANE_SAVE)
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
        xsane_set_sensitivity(TRUE);
       return FALSE;
      }
#if 0 /* this is not necessary any more because the saving routines do the conversion */
      else if ( ( ( (xsane.xsane_output_format == XSANE_JPEG) && xsane.param.depth == 16) ||
                  ( (xsane.xsane_output_format == XSANE_PS)   && xsane.param.depth == 16) ) &&
                ( !xsane.reduce_16bit_to_8bit) )
                
      {
        snprintf(buf, sizeof(buf), TEXT_REDUCE_16BIT_TO_8BIT);
        if (xsane_back_gtk_decision(ERR_HEADER_INFO, (gchar **) info_xpm, buf, BUTTON_REDUCE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
        {
          xsane_set_sensitivity(TRUE);
         return FALSE;
        }
        xsane.reduce_16bit_to_8bit = TRUE;
      }
#endif
      
#ifdef SUPPORT_RGBA
      else if ((xsane.xsane_output_format == XSANE_RGBA) && (xsane.param.format != SANE_FRAME_RGBA))
      {
        snprintf(buf, sizeof(buf), "No RGBA data format !!!"); /* user selected output format RGBA, scanner uses other format */
        xsane_back_gtk_error(buf, TRUE);
        xsane_set_sensitivity(TRUE);
       return FALSE;
      }
#endif
    }
#ifdef SUPPORT_RGBA
    else if (xsane.param.format == SANE_FRAME_RGBA) /* no scanmode but format=rgba */
    {
      snprintf(buf, sizeof(buf), "Special format RGBA only supported in scan mode !!!");
      xsane_back_gtk_error(buf, TRUE);
      xsane_set_sensitivity(TRUE);
     return FALSE;
    }
#endif

#ifdef SUPPORT_RGBA
    if (xsane.param.format == SANE_FRAME_RGBA)
    {
      if ( (xsane.xsane_output_format != XSANE_RGBA) && (xsane.xsane_output_format != XSANE_PNG) )
      {
        snprintf(buf, sizeof(buf), "Image data of type SANE_FRAME_RGBA\ncan only be saved in rgba or png format");
        xsane_back_gtk_error(buf, TRUE);
        xsane_set_sensitivity(TRUE);
       return FALSE;
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

  if (xsane.dummy_filename) /* no dummy filename defined - necessary if an error occurs */
  {
    free(xsane.dummy_filename);
    xsane.dummy_filename = 0;
  }

  /* create scanner gamma tables, xsane internal gamma tables are created after sane_start */
  if ( (xsane.xsane_channels > 1) && /* color scan */
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
      xsane_create_gamma_curve(xsane.gamma_data, 0, 1.0, 0.0, 0.0, 0.0, 100.0, 1.0, gamma_gray_size, gamma_gray_max);
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

    xsane_create_gamma_curve(xsane.gamma_data_red, xsane.negative != xsane.medium_negative,
		 	       gamma_red,
			       xsane.brightness + xsane.brightness_red,
			       xsane.contrast + xsane.contrast_red,
                               xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red, 
                               gamma_red_size, gamma_red_max);

    xsane_create_gamma_curve(xsane.gamma_data_green, xsane.negative != xsane.medium_negative,
			       gamma_green,
			       xsane.brightness + xsane.brightness_green,
			       xsane.contrast + xsane.contrast_green,
                               xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green, 
                               gamma_green_size, gamma_green_max);

    xsane_create_gamma_curve(xsane.gamma_data_blue, xsane.negative != xsane.medium_negative,
			       gamma_blue,
			       xsane.brightness + xsane.brightness_blue,
			       xsane.contrast + xsane.contrast_blue,
                               xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue, 
                               gamma_blue_size, gamma_blue_max);

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
  else if (xsane.scanner_gamma_gray) /* only scanner gray gamma function available or grayscale scan */
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

    if (xsane.xsane_channels > 1) /* color scan */
    {
      xsane_create_gamma_curve(xsane.gamma_data, xsane.negative,
                               gamma, xsane.brightness, xsane.contrast,
                               0.0, 100.0, 1.0, /* medium correction is done by xsane internal gamma correction */
                               gamma_gray_size, gamma_gray_max);
    }
    else
    {
      xsane_create_gamma_curve(xsane.gamma_data, xsane.negative != xsane.medium_negative,
                               gamma, xsane.brightness, xsane.contrast,
                               xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray, 
                               gamma_gray_size, gamma_gray_max);
    }

    xsane_back_gtk_update_vector(xsane.well_known.gamma_vector, xsane.gamma_data);
    free(xsane.gamma_data);
    xsane.gamma_data = 0;
  }

#if 0
  xsane_clear_histogram(&xsane.histogram_raw);
  xsane_clear_histogram(&xsane.histogram_enh);    
  xsane_set_sensitivity(FALSE);
#endif

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane.reading_data = FALSE;

  xsane.scanning = TRUE; /* set marker that scan has been initiated */

  xsane_start_scan();
 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_create_internal_gamma_tables(void)
{
 int size, maxval;

  size = (int) pow(2, xsane.param.depth);
  maxval = size-1;

  if (xsane.xsane_channels > 1) /* color scan */
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
    
      xsane_create_gamma_curve(xsane.gamma_data_red, xsane.medium_negative,
                               gamma_red, xsane.brightness_red, xsane.contrast_red,
                               xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red, 
                               size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_green, xsane.medium_negative,
                               gamma_green, xsane.brightness_green, xsane.contrast_green,
                               xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green, 
                               size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_blue, xsane.medium_negative,
                               gamma_blue, xsane.brightness_blue, xsane.contrast_blue,
                               xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue, 
                               size, maxval);

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

      xsane_create_gamma_curve(xsane.gamma_data_red, xsane.negative != xsane.medium_negative,
                               gamma_red,
                               xsane.brightness + xsane.brightness_red,
                               xsane.contrast + xsane.contrast_red,
                               xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red, 
                               size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_green, xsane.negative != xsane.medium_negative,
                               gamma_green,
                               xsane.brightness + xsane.brightness_green,
                               xsane.contrast + xsane.contrast_green,
                               xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green, 
                               size, maxval);

      xsane_create_gamma_curve(xsane.gamma_data_blue, xsane.negative != xsane.medium_negative,
                               gamma_blue,
                               xsane.brightness + xsane.brightness_blue,
                               xsane.contrast + xsane.contrast_blue,
                               xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue, 
                               size, maxval);

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
      xsane_create_gamma_curve(xsane.gamma_data, xsane.negative != xsane.medium_negative,
                               gamma, xsane.brightness, xsane.contrast,
                               xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray, 
                               size, maxval);

      /* gamma table is freed after scan */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

