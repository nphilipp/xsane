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

#ifdef _AIX
# include <lalloca.h>   /* MUST come first for AIX! */
#endif

#include "sane/config.h"

#include <lalloca.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/stat.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>

#include "sane/sanei_signal.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-text.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H

#include <libgimp/gimp.h>

static void xsane_gimp_query(void);
static void xsane_gimp_run(char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals);

GPlugInInfo PLUG_IN_INFO =
{
  NULL,                         /* init_proc */
  NULL,                         /* quit_proc */
  xsane_gimp_query,             /* query_proc */
  xsane_gimp_run,               /* run_proc */
};

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

static int xsane_decode_devname(const char *encoded_devname, int n, char *buf);
static int encode_devname(const char *devname, int n, char *buf);
void null_print_func(gchar *msg);
static void xsane_gimp_advance(void);
static void xsane_read_image_data(gpointer data, gint source, GdkInputCondition cond);
static RETSIGTYPE xsane_sigpipe_handler(int signal);
static void xsane_scan_done(void);
void xsane_cancel(void);
static void xsane_start_scan(void);
void xsane_scan_dialog(GtkWidget * widget, gpointer call_data);

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_decode_devname(const char *encoded_devname, int n, char *buf)
{
  char *dst, *limit;
  const char *src;
  char ch, val;

  limit = buf + n;
  for (src = encoded_devname, dst = buf; *src; ++dst)
    {
      if (dst >= limit)
        return -1;

      ch = *src++;
      /* don't use the ctype.h macros here since we don't want to
         allow anything non-ASCII here... */
      if (ch != '-')
        *dst = ch;
      else
        {
          /* decode */

          ch = *src++;
          if (ch == '-')
            *dst = ch;
          else
            {
              if (ch >= 'a' && ch <= 'f')
                val = (ch - 'a') + 10;
              else
                val = (ch - '0');
              val <<= 4;

              ch = *src++;
              if (ch >= 'a' && ch <= 'f')
                val |= (ch - 'a') + 10;
              else
                val |= (ch - '0');

              *dst = val;

              ++src;    /* simply skip terminating '-' for now... */
            }
        }
    }
  if (dst >= limit)
    return -1;
  *dst = '\0';
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
static int encode_devname(const char *devname, int n, char *buf)
{
  static const char hexdigit[] = "0123456789abcdef";
  char *dst, *limit;
  const char *src;
  char ch;

  limit = buf + n;
  for (src = devname, dst = buf; *src; ++src)
    {
      if (dst >= limit)
	return -1;

      ch = *src;
      /* don't use the ctype.h macros here since we don't want to
	 allow anything non-ASCII here... */
      if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
	*dst++ = ch;
      else
	{
	  /* encode */

	  if (dst + 4 >= limit)
	    return -1;

	  *dst++ = '-';
	  if (ch == '-')
	    *dst++ = '-';
	  else
	    {
	      *dst++ = hexdigit[(ch >> 4) & 0x0f];
	      *dst++ = hexdigit[(ch >> 0) & 0x0f];
	      *dst++ = '-';
	    }
	}
    }
  if (dst >= limit)
    return -1;
  *dst = '\0';
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gimp_query(void)
{
  static GParamDef args[] =
  {
      {PARAM_INT32, "run_mode", "Interactive, non-interactive"},
  };
  static GParamDef *return_vals = NULL;
  static int nargs = sizeof(args) / sizeof(args[0]);
  static int nreturn_vals = 0;
  char mpath[1024];
  char name[1024];
  size_t len;
  int i, j;

  gimp_install_procedure(
      "xsane",
      "Front-end to the SANE interface",
      "This function provides access to scanners and other image acquisition "
      "devices through the SANE (Scanner Access Now Easy) interface.",
       "Oliver Rauch",
       "Oliver Rauch",
      "1998/1999",
      "<Toolbox>/Xtns/XSane/Device dialog...",
      "RGB, GRAY",
      PROC_EXTENSION,
      nargs, nreturn_vals,
      args, return_vals);

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);
  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    fprintf(stderr, "\n\n"
                    "%s warning:\n"
                    "  Sane major version number mismatch!\n"
                    "  xsane major version = %d\n"
                    "  backend major version = %d\n",
                    prog_name, SANE_V_MAJOR, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode));
    return;
  }

  sane_get_devices(&devlist, SANE_FALSE);

  for (i = 0; devlist[i]; ++i)
    {
      strcpy(name, "xsane-");
      if (encode_devname(devlist[i]->name, sizeof(name) - 6, name + 6) < 0)
	continue;	/* name too long... */

      strncpy(mpath, "<Toolbox>/Xtns/XSane/", sizeof(mpath));
      len = strlen(mpath);
      for (j = 0; devlist[i]->name[j]; ++j)
	{
	  if (devlist[i]->name[j] == '/')
	    mpath[len++] = '\'';
	  else
	    mpath[len++] = devlist[i]->name[j];
	}
      mpath[len++] = '\0';

      gimp_install_procedure
	(name, "Front-end to the SANE interface",
	 "This function provides access to scanners and other image "
	 "acquisition devices through the SANE (Scanner Access Now Easy) "
	 "interface.",
         "Oliver Rauch",
         "Oliver Rauch",
	 "1998/1999", mpath, "RGB, GRAY", PROC_EXTENSION,
	 nargs, nreturn_vals, args, return_vals);
    }
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gimp_run(char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals)
{
  static GParam values[2];
  GRunModeType run_mode;
  char devname[1024];
  char *args[2];
  int nargs;

  run_mode = param[0].data.d_int32;
  xsane.mode = GIMP_EXTENSION;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = STATUS_CALLING_ERROR;

  nargs = 0;
  args[nargs++] = "xssane";

  seldev = -1;
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
    case RUN_INTERACTIVE:
      xsane_interface(nargs, args);
      values[0].data.d_status = STATUS_SUCCESS;
      break;

    case RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      break;

    case RUN_WITH_LAST_VALS:
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

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
static void xsane_gimp_advance(void)
{
  if (++xsane.x >= xsane.param.pixels_per_line)
    {
      int tile_height = gimp_tile_height();

      xsane.x = 0;
      ++xsane.y;
      if (xsane.y % tile_height == 0)
	{
	  gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height,
				  xsane.param.pixels_per_line, tile_height);
	  if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
	    {
	      int height;

	      xsane.tile_offset %= 3;
	      if (!xsane.first_frame)
		{
		  /* get the data for the existing tile: */
		  height = tile_height;
		  if (xsane.y + height >= xsane.param.lines)
                  {
		    height = xsane.param.lines - xsane.y;
                  }
		  gimp_pixel_rgn_get_rect(&xsane.region, xsane.tile, 0, xsane.y, xsane.param.pixels_per_line, height);
		}
	    }
	  else
          {
	    xsane.tile_offset = 0;
          }
	}
    }
}

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_read_image_data(gpointer data, gint source, GdkInputCondition cond)
{
  SANE_Handle dev = gsg_dialog_get_device (dialog);
  SANE_Status status;
  SANE_Int len;
  int i;
  char buf[255];

  if ( (xsane.param.depth == 1) || (xsane.param.depth == 8) )
  {
    static unsigned char buf8[32768];

    while (1)
    {
      status = sane_read(dev, (SANE_Byte *) buf8, sizeof(buf8), &len);
      if (status != SANE_STATUS_GOOD)
      {
        if (status == SANE_STATUS_EOF)
        {
          if (!xsane.param.last_frame)
          {
            xsane_start_scan();
            break; /* leave while loop */
          }
        }
        else
        {
          snprintf(buf, sizeof(buf), "Error during read: %s.", sane_strstatus(status));
          gsg_error(buf);
        }
        xsane_scan_done();
        return;
      }

      if (!len)
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.progress, xsane.bytes_read / (gfloat) xsane.num_bytes);

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
          if (xsane.mode == STANDALONE)
          {
           int i;
           char val;

            if ((!xsane.scanner_gamma_gray) && (xsane.param.depth > 1))
            {
              for (i=0; i < len; ++i)
              {
                val = xsane.gamma_data[(int) buf8[i]];
                fwrite(&val, 1, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf8, 1, len, xsane.out);
            }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
          else
          {
            switch (xsane.param.depth)
            {
              case 1:
                for (i = 0; i < len; ++i)
                {
                 u_char mask;
                 int j;

                  mask = buf8[i];
                  for (j = 7; j >= 0; --j)
                  {
                    u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
                    xsane.tile[xsane.tile_offset++] = gl;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                    {
                      break;
                    }
                  }
                }
               break;

              case 8:
                if (!xsane.scanner_gamma_gray)
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = xsane.gamma_data[(int) buf8[i]];
                    xsane_gimp_advance();
                  }
                }
                else
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = buf8[i];
                    xsane_gimp_advance();
                  }
                }
               break;
            }
          }
#endif /* HAVE_LIBGIMP_GIMP_H */
         break;

        case SANE_FRAME_RGB:
          if (xsane.mode == STANDALONE)
          {
           int i;
           char val;

            if (!xsane.scanner_gamma_color)
            {
              for (i=0; i < len; ++i)
              {
                if (dialog->pixelcolor == 0)
                {
                  val = xsane.gamma_data_red[(int) buf8[i]];
                  dialog->pixelcolor++;
                }
                else if (dialog->pixelcolor == 1)
                {
                  val = xsane.gamma_data_green[(int) buf8[i]];
                  dialog->pixelcolor++;
                }
                else
                {
                  val = xsane.gamma_data_blue[(int) buf8[i]];
                  dialog->pixelcolor = 0;
                }
                fwrite(&val, 1, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf8, 1, len, xsane.out);
            }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
          else
          {
            switch (xsane.param.depth)
            {
              case 1:
                if (xsane.param.format == SANE_FRAME_RGB)
                {
                  goto bad_depth;
                }
                for (i = 0; i < len; ++i)
                {
                 u_char mask;
                 int j;

                  mask = buf8[i];
                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;
                    xsane.tile[xsane.tile_offset++] = gl;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                   break;
                  }
                }
               break;

              case 8:
                if (!xsane.scanner_gamma_color)
                {
                  for (i = 0; i < len; ++i)
                  {
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_red[(int) buf8[i]];
                    }
                    else if (xsane.tile_offset % 3 == 1)
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_green[(int) buf8[i]];
                    }
                    else
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_blue[(int) buf8[i]];
                    }
 
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane_gimp_advance();
                    }
                  }
                }
                else
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = buf8[i];
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane_gimp_advance();
                    }
                  }
                }
               break;
            }
          }
#endif /* HAVE_LIBGIMP_GIMP_H */
         break;

        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          if (xsane.mode == STANDALONE)
          {
            for (i = 0; i < len; ++i)
            {
              fwrite(buf8 + i, 1, 1, xsane.out);
              fseek(xsane.out, 2, SEEK_CUR);
            }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
          else
          {
            switch (xsane.param.depth)
            {
              case 1:
                for (i = 0; i < len; ++i)
                {
                 u_char mask;
                 int j;

                  mask = buf8[i];
                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;
                    xsane.tile[xsane.tile_offset] = gl;
                    xsane.tile_offset += 3;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                    {
                      break;
                    }
                  }
                }
               break;

              case 8:
                for (i = 0; i < len; ++i)
                {
                  xsane.tile[xsane.tile_offset] = buf8[i];
                  xsane.tile_offset += 3;
                  xsane_gimp_advance();
                }
               break;
            }
          }
#endif /* HAVE_LIBGIMP_GIMP_H */
         break;

        default:
          fprintf(stderr, "%s.xsane_read_image_data: bad frame format %d\n", prog_name, xsane.param.format);
          xsane_scan_done();
          return;
      }
    }
  }
  else if ( xsane.param.depth == 16 )
  {
    static guint16 buf16[32768];
    char buf[255];
    char last = 0;
    int offset = 0;

    while (1)
    {
      if (offset) /* if we have had an odd number of bytes */
      {
        buf16[0] = last;
        status = sane_read(dev, (SANE_Byte *) (buf16 + 1), sizeof(buf16) - 1, &len);
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


      if (status != SANE_STATUS_GOOD)
      {
        if (status == SANE_STATUS_EOF)
        {
          if (!xsane.param.last_frame)
          {
            xsane_start_scan();
            break; /* leave while loop */
          }
        }
        else
        {
          snprintf(buf, sizeof(buf), "Error during read: %s.", sane_strstatus(status));
          gsg_error(buf);
        }
        xsane_scan_done();
        return;
      }

      if (!len)
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.progress, xsane.bytes_read / (gfloat) xsane.num_bytes);

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
          if (xsane.mode == STANDALONE)
          {
           int i;
           guint16 val;

            if (!xsane.scanner_gamma_gray)
            {
              for (i=0; i < len/2; ++i)
              {
                val = xsane.gamma_data[buf16[i]];
                fwrite(&val, 2, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf16, 2, len/2, xsane.out);
            }
          }
         break;

        case SANE_FRAME_RGB:
          if (xsane.mode == STANDALONE)
          {
           int i;
           guint16 val;

            if (!xsane.scanner_gamma_color)
            {
              for (i=0; i < len/2; ++i)
              {
                if (dialog->pixelcolor == 0)
                {
                  val = xsane.gamma_data_red[buf16[i]];
                  dialog->pixelcolor++;
                }
                else if (dialog->pixelcolor == 1)
                {
                  val = xsane.gamma_data_green[buf16[i]];
                  dialog->pixelcolor++;
                }
                else
                {
                  val = xsane.gamma_data_blue[buf16[i]];
                  dialog->pixelcolor = 0;
                }
                fwrite(&val, 2, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf16, 2, len/2, xsane.out);
            }
          }
         break;

        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          if (xsane.mode == STANDALONE)
          {
            for (i = 0; i < len/2; ++i)
            {
              fwrite(buf16 + i*2, 2, 1, xsane.out);
              fseek(xsane.out, 4, SEEK_CUR);
            }
          }
         break;

        default:
          fprintf(stderr, "%s.xsane_read_image_data: bad frame format %d\n", prog_name, xsane.param.format);
          xsane_scan_done();
          return;
      }
    }
  }
  else
  {
    snprintf(buf, sizeof(buf), "Cannot handle depth %d.", xsane.param.depth);
    gsg_error(buf);
    xsane_scan_done();
  }

  return;

#ifdef HAVE_LIBGIMP_GIMP_H
bad_depth:

  snprintf(buf, sizeof(buf), "Cannot handle depth %d.", xsane.param.depth);
  gsg_error(buf);
  xsane_scan_done();
  return;
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_sigpipe_handler(int signal)
/* this is to catch a broken pipe while writing to printercommand */
{
  xsane_cancel_save(0);
  xsane.broken_pipe = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scan_done(void)
{
  gtk_widget_set_sensitive(xsane.shell, TRUE);
  gtk_widget_set_sensitive(xsane.histogram_dialog, TRUE);
  gsg_set_sensitivity(dialog, TRUE);

  if (xsane.input_tag >= 0)
  {
    gdk_input_remove(xsane.input_tag);
    xsane.input_tag = -1;
  }

  if (!xsane.progress)
  {
    return;
  }

  /* remove progressbar */
  xsane_progress_free(xsane.progress);
  xsane.progress = 0;
  while(gtk_events_pending())
  {
    gtk_main_iteration();
  }


  /* we have to free the gamma tables if we used software gamma correction */
  
  if (xsane.gamma_data) 
  {
    free(xsane.gamma_data);
    xsane.gamma_data = 0;
  }

  if (xsane.gamma_data_red)
  {
    free(xsane.gamma_data_red);
    free(xsane.gamma_data_green);
    free(xsane.gamma_data_blue);

    xsane.gamma_data_red   = 0;
    xsane.gamma_data_green = 0;
    xsane.gamma_data_blue  = 0;
  }

  if (xsane.mode == STANDALONE)
  {
    fclose(xsane.out);
    xsane.out = 0;


    if ( (xsane.xsane_mode == XSANE_SCAN) && (xsane.xsane_output_format != XSANE_PNM) 
                                            && (xsane.xsane_output_format != XSANE_RAW16) )
    {
     FILE *outfile;
     FILE *infile;
     char copyfilename[PATH_MAX];
     char buf[256];

       /* open progressbar */
       snprintf(buf, sizeof(buf), "Saving image");
       xsane.progress = xsane_progress_new("Converting data....", buf, (GtkSignalFunc) xsane_cancel_save, 0);
       xsane_progress_update(xsane.progress, 0);
       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", 0, "conversion", dialog->dev_name, ".tmp");
       infile = fopen(copyfilename, "r");
       if (infile != 0)
       {
         fseek(infile, xsane.header_size, SEEK_SET);

#ifdef HAVE_LIBTIFF
         if (xsane.xsane_output_format == XSANE_TIFF)		/* routines that want to have filename  for saving */
         {
           xsane_save_tiff(preferences.filename, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                           xsane.param.lines, preferences.png_compression);
         }
         else							/* routines that want to have filedescriptor for saving */
#endif
         {
           outfile = fopen(preferences.filename, "w");
           if (outfile != 0)
           {
             switch(xsane.xsane_output_format)
             {
#ifdef HAVE_LIBJPEG
               case XSANE_JPEG:
                 xsane_save_jpeg(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                 xsane.param.lines, preferences.jpeg_quality);
                break;
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
               case XSANE_PNG:
                 if (xsane.param.depth <= 8)
                 {
                   xsane_save_png(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                  xsane.param.lines, preferences.png_compression);
                 }
                 else
                 {
                   xsane_save_png_16(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                     xsane.param.lines, preferences.png_compression);
                 }
                break;

               case XSANE_PNM16:
                 xsane_save_pnm_16(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                   xsane.param.lines);
                break;
#endif
#endif

               case XSANE_PS: /* save postscript, use original size */
               { 
                 float imagewidth  = xsane.param.pixels_per_line/(float)xsane.resolution; /* width in inch */
                 float imageheight = xsane.param.lines/(float)xsane.resolution; /* height in inch */

                  xsane_save_ps(outfile, infile,
                                xsane.xsane_color /* gray, color */,
                                xsane.param.depth /* bits */,
                                xsane.param.pixels_per_line, xsane.param.lines,
                                preferences.printer[preferences.printernr]->leftoffset + preferences.printer[preferences.printernr]->width/2 - imagewidth*36,
                                preferences.printer[preferences.printernr]->bottomoffset + preferences.printer[preferences.printernr]->height/2 - imageheight*36,
                                imagewidth, imageheight);
               }
               break;


               default:
                 snprintf(buf, sizeof(buf),"Unknown file format for saving!\n" );
                 gsg_error(buf);
             }
             fclose(outfile);
           }
           else
           {
            char buf[256];

             snprintf(buf, sizeof(buf), "Failed to open `%s': %s", preferences.filename, strerror(errno));
             gsg_error(buf);
           }
         }
         fclose(infile);
         remove(copyfilename);
       }
       else
       {
        char buf[256];
         snprintf(buf, sizeof(buf), "Failed to open imagefile(%s) for conversion: %s", preferences.filename, strerror(errno));
         gsg_error(buf);
       }
       xsane_progress_free(xsane.progress);
       xsane.progress = 0;

       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }
    }
    else if (xsane.xsane_mode == XSANE_COPY)
    {
     FILE *outfile;
     FILE *infile;
     char copyfilename[PATH_MAX];
     char buf[256];

       xsane_update_int(xsane.copy_number_entry, &xsane.copy_number); /* get number of copies */
       if (xsane.copy_number < 1)
       {
         xsane.copy_number = 1;
       }

       /* open progressbar */
       snprintf(buf, sizeof(buf), "converting to postscript");
       xsane.progress = xsane_progress_new("Converting data....", buf, (GtkSignalFunc) xsane_cancel_save, 0);
       xsane_progress_update(xsane.progress, 0);
       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       xsane.broken_pipe = 0;
       gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", 0, "conversion", dialog->dev_name, ".tmp");
       infile = fopen(copyfilename, "r");

       snprintf(buf, sizeof(buf), "%s %s%d", preferences.printer[preferences.printernr]->command,
                                             preferences.printer[preferences.printernr]->copy_number_option,
                                             xsane.copy_number);
       outfile = popen(buf, "w");
/*       outfile = popen(preferences.printer[preferences.printernr]->command, "w"); */
       if ((outfile != 0) && (infile != 0)) /* copy mode, use zoom size */
       {
        struct SIGACTION act;
        float imagewidth  = xsane.param.pixels_per_line/(float)preferences.printer[preferences.printernr]->resolution; /* width in inch */
        float imageheight = xsane.param.lines/(float)preferences.printer[preferences.printernr]->resolution; /* height in inch */

         memset (&act, 0, sizeof (act)); /* define broken pipe handler */
         act.sa_handler = xsane_sigpipe_handler;
         sigaction (SIGPIPE, &act, 0);


         fseek(infile, xsane.header_size, SEEK_SET);
         xsane_save_ps(outfile, infile,
                       xsane.xsane_color /* gray, color */,
                       xsane.param.depth /* bits */,
                       xsane.param.pixels_per_line, xsane.param.lines,
                       preferences.printer[preferences.printernr]->leftoffset + preferences.printer[preferences.printernr]->width/2 - imagewidth*36,
                       preferences.printer[preferences.printernr]->bottomoffset + preferences.printer[preferences.printernr]->height/2 - imageheight*36,
                       imagewidth, imageheight);
       }
       else
       {
        char buf[256];

         if (!infile)
         {
           snprintf(buf, sizeof(buf), "Failed to open imagefile(%s) for conversion: %s", preferences.filename, strerror(errno));
           gsg_error(buf);
         }
         else if (!outfile)
         {
           snprintf(buf, sizeof(buf), "Failed to open pipe for executing printercommand");
           gsg_error(buf);
         }
       }

       if (xsane.broken_pipe)
       {
         snprintf(buf, sizeof(buf), "Failed to execute printercommand \"%s\"",
                  preferences.printer[preferences.printernr]->command);
         gsg_error(buf);
       }

       xsane_progress_free(xsane.progress);
       xsane.progress = 0;
       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       if (infile)
       {
         fclose(infile);
         remove(copyfilename);
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
     char copyfilename[PATH_MAX];
     char buf[256];

       /* open progressbar */
       snprintf(buf, sizeof(buf), "Saving fax image");
       xsane.progress = xsane_progress_new("Converting data....", buf, (GtkSignalFunc) xsane_cancel_save, 0);
       xsane_progress_update(xsane.progress, 0);
       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }

       gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", 0, "conversion", dialog->dev_name, ".tmp");
       infile = fopen(copyfilename, "r");
       if (infile != 0)
       {
         fseek(infile, xsane.header_size, SEEK_SET);

         {
           outfile = fopen(xsane.fax_filename, "w");
           if (outfile != 0)
           {
            float imagewidth  = xsane.param.pixels_per_line/(float)xsane.resolution; /* width in inch */
            float imageheight = xsane.param.lines/(float)xsane.resolution; /* height in inch */

             xsane_save_ps(outfile, infile,
                           xsane.xsane_color /* gray, color */,
                           xsane.param.depth /* bits */,
                           xsane.param.pixels_per_line, xsane.param.lines,
                           preferences.printer[preferences.printernr]->leftoffset + preferences.printer[preferences.printernr]->width/2 - imagewidth*36,
                           preferences.printer[preferences.printernr]->bottomoffset + preferences.printer[preferences.printernr]->height/2 - imageheight*36,
                           imagewidth, imageheight);
             fclose(outfile);
           }
           else
           {
            char buf[256];

             snprintf(buf, sizeof(buf), "Failed to open `%s': %s", xsane.fax_filename, strerror(errno));
             gsg_error(buf);
           }
         }
         fclose(infile);
         remove(copyfilename);
       }
       else
       {
        char buf[256];
         snprintf(buf, sizeof(buf), "Failed to open imagefile(%s) for conversion: %s", xsane.fax_filename, strerror(errno));
         gsg_error(buf);
       }
       xsane_progress_free(xsane.progress);
       xsane.progress = 0;

       while (gtk_events_pending())
       {
         gtk_main_iteration();
       }
    }
  }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
  {
    int remaining;

    /* GIMP mode */
    if (xsane.y > xsane.param.lines)
    {
      xsane.y = xsane.param.lines;
    }

    remaining = xsane.y % gimp_tile_height();
    if (remaining)
    {
      gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - remaining, xsane.param.pixels_per_line, remaining);
    }
    gimp_drawable_flush(xsane.drawable);
    gimp_display_new(xsane.image_ID);
    gimp_drawable_detach(xsane.drawable);
    free(xsane.tile);
    xsane.tile = 0;
  }
#endif /* HAVE_LIBGIMP_GIMP_H */

  xsane.header_size = 0;
  sane_cancel(gsg_dialog_get_device(dialog));
  
  if ( (preferences.increase_filename_counter) && (xsane.xsane_mode == XSANE_SCAN) && (xsane.mode == STANDALONE) )
  {
    xsane_increase_counter_in_filename(preferences.filename, preferences.skip_existing_numbers);
    gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), (char *) preferences.filename);
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

    xsane_increase_counter_in_filename(xsane.fax_filename, preferences.skip_existing_numbers);
    xsane_fax_project_save();
    free(page);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel(void)
{
  sane_cancel(gsg_dialog_get_device(dialog));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_start_scan(void)
{
  SANE_Status status;
  SANE_Handle dev = gsg_dialog_get_device(dialog);
  const char *frame_type = 0;
  char buf[256];
  int fd;

  gsg_set_sensitivity(dialog, FALSE); /* turn off buttons and sliders ... */
  gtk_widget_set_sensitive(xsane.shell, FALSE);
  gtk_widget_set_sensitive(xsane.histogram_dialog, FALSE);

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == GIMP_EXTENSION && xsane.tile)
    {
      int height, remaining;

      /* write the last tile of the frame to the GIMP region: */

      if (xsane.y > xsane.param.lines)	/* sanity check */
      {
	xsane.y = xsane.param.lines;
      }

      remaining = xsane.y % gimp_tile_height();
      if (remaining)
      {
	gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - remaining, xsane.param.pixels_per_line, remaining);
      }

      /* initialize the tile with the first tile of the GIMP region: */

      height = gimp_tile_height();
      if (height >= xsane.param.lines)
      {
	height = xsane.param.lines;
      }
      gimp_pixel_rgn_get_rect(&xsane.region, xsane.tile, 0, 0, xsane.param.pixels_per_line, height);
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  xsane.x = xsane.y = 0;

  status = sane_start(dev);
  if (status != SANE_STATUS_GOOD)
  {
    gsg_set_sensitivity(dialog, TRUE);
    gtk_widget_set_sensitive(xsane.shell, TRUE);
    gtk_widget_set_sensitive(xsane.histogram_dialog, TRUE);

    snprintf(buf, sizeof(buf), "Failed to start scanner: %s", sane_strstatus(status));
    gsg_error(buf);
    return;
  }

  status = sane_get_parameters(dev, &xsane.param);
  if (status != SANE_STATUS_GOOD)
  {
    gsg_set_sensitivity(dialog, TRUE);
    gtk_widget_set_sensitive(xsane.shell, TRUE);
    gtk_widget_set_sensitive(xsane.histogram_dialog, TRUE);

    snprintf(buf, sizeof(buf), "Failed to get parameters: %s", sane_strstatus(status));
    gsg_error(buf);
    return;
  }

  xsane.num_bytes = xsane.param.lines * xsane.param.bytes_per_line;
  xsane.bytes_read = 0;

  switch (xsane.param.format)
  {
    case SANE_FRAME_RGB:	frame_type = "RGB"; break;
    case SANE_FRAME_RED:	frame_type = "red"; break;
    case SANE_FRAME_GREEN:	frame_type = "green"; break;
    case SANE_FRAME_BLUE:	frame_type = "blue"; break;
    case SANE_FRAME_GRAY:	frame_type = "gray"; break;
    default:			frame_type = "unknown"; break;
  }

  if (xsane.mode == STANDALONE)
  {				/* We are running in standalone mode */
      if (!xsane.header_size)
      {
	  switch (xsane.param.format)
          {
	    case SANE_FRAME_RGB:
	    case SANE_FRAME_RED:
	    case SANE_FRAME_GREEN:
	    case SANE_FRAME_BLUE:
              switch (xsane.param.depth)
	      {
                case 8: /* color 8 bit mode, write ppm header */
                  fprintf(xsane.out, "P6\n# SANE data follows\n%d %d\n255\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

		default: /* color, but not 8 bit mode, write as raw data because this is not defined in pnm */ 
                  fprintf(xsane.out, "SANE_RGB_RAW\n%d %d\n65535\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
              }
             break;

	    case SANE_FRAME_GRAY:
              switch (xsane.param.depth)
	      {
                case 1: /* 1 bit lineart mode, write pbm header */
                  fprintf(xsane.out, "P4\n# SANE data follows\n%d %d\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

                case 8: /* 8 bit grayscale mode, write pgm header */
                  fprintf(xsane.out, "P5\n# SANE data follows\n%d %d\n255\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

                default: /* grayscale mode but not 1 or 8 bit, write as raw data because this is not defined in pnm */
                  fprintf(xsane.out, "SANE_GRAYSCALE_RAW\n%d %d\n65535\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
              }
	     break;
	    default:
            /* unknown file format, do not write header */
          }
	  xsane.header_size = ftell(xsane.out);
      }

      if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
      {
	fseek(xsane.out, xsane.header_size + xsane.param.format - SANE_FRAME_RED, SEEK_SET);
      }

      if (xsane.xsane_mode == XSANE_SCAN)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for `%s'...", frame_type, preferences.filename);
      }
      else if (xsane.xsane_mode == XSANE_COPY)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for photocopy ...", frame_type);
      }
      else if (xsane.xsane_mode == XSANE_FAX)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for fax ...", frame_type);
      }
  }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
    {
      size_t tile_size;

      /* We are running under the GIMP */

      xsane.tile_offset = 0;
      tile_size = xsane.param.pixels_per_line * gimp_tile_height();
      if (xsane.param.format != SANE_FRAME_GRAY)
      {
	tile_size *= 3;	/* 24 bits/pixel */
      }
      if (xsane.tile)
      {
	xsane.first_frame = 0;
      }
      else
	{
	  GImageType image_type = RGB;
	  GDrawableType drawable_type = RGB_IMAGE;
	  gint32 layer_ID;

	  if (xsane.param.format == SANE_FRAME_GRAY)
	    {
	      image_type = GRAY;
	      drawable_type = GRAY_IMAGE;
	    }

	  xsane.image_ID = gimp_image_new(xsane.param.pixels_per_line,
					     xsane.param.lines, image_type);
	  layer_ID = gimp_layer_new(xsane.image_ID, "Background",
				     xsane.param.pixels_per_line,
				     xsane.param.lines,
				     drawable_type, 100, NORMAL_MODE);
	  gimp_image_add_layer(xsane.image_ID, layer_ID, 0);

	  xsane.drawable = gimp_drawable_get(layer_ID);
	  gimp_pixel_rgn_init(&xsane.region, xsane.drawable, 0, 0,
			       xsane.drawable->width,
			       xsane.drawable->height, TRUE, FALSE);
	  xsane.tile = g_new(guchar, tile_size);
	  xsane.first_frame = 1;
	}

      if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
      {
	xsane.tile_offset = xsane.param.format - SANE_FRAME_RED;
      }

      snprintf(buf, sizeof(buf), "Receiving %s data for GIMP...", frame_type);
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  dialog->pixelcolor = 0;

  if (xsane.progress)
  {
    xsane_progress_free(xsane.progress);
  }
  xsane.progress = xsane_progress_new("Scanning", buf, (GtkSignalFunc) xsane_cancel, 0);

  xsane.input_tag = -1;

  if (sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD && sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD)
  {
    xsane.input_tag = gdk_input_add(fd, GDK_INPUT_READ, xsane_read_image_data, 0);
  }
  else
  {
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the scan button is pressed */
void xsane_scan_dialog(GtkWidget * widget, gpointer call_data)
{
 char buf[256];

 /* xxxxxxxxxxxxxxxx set a loop for Document feeder here */

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  if (xsane.mode == STANDALONE)  				/* We are running in standalone mode */
  {
   char *extension;

    if ( (xsane.xsane_mode == XSANE_SCAN) && (preferences.overwrite_warning) ) /* test if filename already used */
    {
     FILE *testfile;

      testfile = fopen(preferences.filename, "r");
      if (testfile) /* filename used: skip */
      {
       char buf[256];

        fclose(testfile);
        snprintf(buf, sizeof(buf), "File %s already exists\n", preferences.filename);
        if (gsg_decision("Warning:", buf, "Overwrite", "Cancel", 1 /* wait */) == FALSE)
        {
          return;
        }
      }
    }


    extension = strrchr(preferences.filename, '.');
    if (extension)
    {
      extension++;
    }

    xsane.xsane_output_format = XSANE_UNKNOWN;

    if (xsane.param.depth <= 8)
    {
      if (extension)
      {
        if ( (!strcasecmp(extension, "pnm")) || (!strcasecmp(extension, "ppm")) ||
             (!strcasecmp(extension, "pgm")) || (!strcasecmp(extension, "pbm")) )
        {
          xsane.xsane_output_format = XSANE_PNM;
        }
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        else if (!strcasecmp(extension, "png"))
        {
          xsane.xsane_output_format = XSANE_PNG;
        }
#endif
#endif
#ifdef HAVE_LIBJPEG
        else if ( (!strcasecmp(extension, "jpg")) || (!strcasecmp(extension, "jpeg")) )
        {
          xsane.xsane_output_format = XSANE_JPEG;
        }
#endif
        else if (!strcasecmp(extension, "ps"))
        {
          xsane.xsane_output_format = XSANE_PS;
        }
#ifdef HAVE_LIBTIFF
        else if ( (!strcasecmp(extension, "tif")) || (!strcasecmp(extension, "tiff")) )
        {
          xsane.xsane_output_format = XSANE_TIFF;
        }
#endif
      }
    }
    else /* depth >8 bpp */
    {
      if (extension)
      {
        if (!strcasecmp(extension, "raw"))
        {
          xsane.xsane_output_format = XSANE_RAW16;
        }
        else if ( (!strcasecmp(extension, "pnm")) || (!strcasecmp(extension, "ppm")) ||
             (!strcasecmp(extension, "pgm")) || (!strcasecmp(extension, "pbm")) )
        {
          xsane.xsane_output_format = XSANE_PNM16;
        }
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        else if (!strcasecmp(extension, "png"))
        {
          xsane.xsane_output_format = XSANE_PNG;
        }
#endif
#endif
      }
    }

    if ( (xsane.xsane_output_format == XSANE_UNKNOWN) && (xsane.xsane_mode == XSANE_SCAN) )
    {
      if (extension)
      {
        snprintf(buf, sizeof(buf), "Unsupported %d-bit output format: %s", xsane.param.depth, extension);
      }
      else
      {
        snprintf(buf, sizeof(buf), "No output format given !!!");
      }

      gsg_error(buf);
      return;
    }
      
    if ( (xsane.xsane_mode == XSANE_COPY) || (xsane.xsane_mode == XSANE_FAX) || /* we have to do a conversion */
         ((xsane.xsane_mode == XSANE_SCAN)  && (xsane.xsane_output_format != XSANE_PNM)
                                            && (xsane.xsane_output_format != XSANE_RAW16) ) )
    {
     char filename[PATH_MAX];

      gsg_make_path(sizeof(filename), filename, "xsane", 0, "conversion", dialog->dev_name, ".tmp");
      xsane.out = fopen(filename, "w");
    }
    else /* no conversion following, save directly to the selected filename */
    {
      xsane.out = fopen(preferences.filename, "w");
    }

    if (!xsane.out)
    {
      snprintf(buf, sizeof(buf), "Failed to open `%s': %s", preferences.filename, strerror(errno));
      gsg_error(buf);
      return;
    }

    if (xsane.xsane_mode == XSANE_FAX)
    {
      mkdir(preferences.fax_project, 7*64 + 0*8 + 0);
    }
  }
	   
  if (xsane.param.depth > 1) /* if depth > 1 use gamma correction */
  {
   int size;
   int gamma_gray_size, gamma_red_size, gamma_green_size, gamma_blue_size;
   int gamma_gray_max, gamma_red_max, gamma_green_max, gamma_blue_max;
   const SANE_Option_Descriptor *opt;

    size = (int) pow(2, xsane.param.depth);
    gamma_gray_size  = size;
    gamma_red_size   = size;
    gamma_green_size = size;
    gamma_blue_size  = size;

    size--; 
    gamma_gray_max   = size;
    gamma_red_max    = size;
    gamma_green_max  = size;
    gamma_blue_max   = size;

    if (xsane.scanner_gamma_gray) /* gamma table for gray available */
    {
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);
      gamma_gray_size = opt->size / sizeof(opt->type);
      gamma_gray_max  = opt->constraint.range->max;
    }

    if (xsane.scanner_gamma_color) /* gamma table for red, green and blue available */
    {
     double gamma_red, gamma_green, gamma_blue;

      /* ok, scanner color gamma function is supported, so we do all conversions about that */
      /* we do not need any gamma tables while scanning, so we can free them after sending */
      /* the data to the scanner */

      /* if also gray gamma function is supported, set this to 1.0 to get the right colors */
      if (xsane.scanner_gamma_gray)
      {
        xsane.gamma_data = malloc(gamma_gray_size  * sizeof(SANE_Int));
        xsane_create_gamma_curve(xsane.gamma_data, 0, 1.0, 0.0, 0.0, gamma_gray_size, gamma_gray_max);
        gsg_update_vector(dialog, dialog->well_known.gamma_vector, xsane.gamma_data);
        free(xsane.gamma_data);
        xsane.gamma_data = 0;
      }

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);
      gamma_red_size = opt->size / sizeof(opt->type);
      gamma_red_max  = opt->constraint.range->max;

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_g);
      gamma_green_size = opt->size / sizeof(opt->type);
      gamma_green_max  = opt->constraint.range->max;

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_b);
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

      gsg_update_vector(dialog, dialog->well_known.gamma_vector_r, xsane.gamma_data_red);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_g, xsane.gamma_data_green);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_b, xsane.gamma_data_blue);

      free(xsane.gamma_data_red);
      free(xsane.gamma_data_green);
      free(xsane.gamma_data_blue);

      xsane.gamma_data_red   = 0;
      xsane.gamma_data_green = 0;
      xsane.gamma_data_blue  = 0;
    }
    else if (xsane.scanner_gamma_gray) /* only scanner gray gamma function available */
    {
     double gamma;
      /* ok, the scanner only supports gray gamma function */
      /* if we are doing a grayscale scan everyting is ok, */
      /* for a color scan the software has to do the gamma correction set by the component slider */

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

      gsg_update_vector(dialog, dialog->well_known.gamma_vector, xsane.gamma_data);
      free(xsane.gamma_data);
      xsane.gamma_data = 0;

      if (xsane.xsane_color) /* ok, we are doing a colorscan */
      {
        /* we have to create color gamma table for software conversion */
	/* but we only have to use color slider values, because gray slider value */
	/* is used by scanner gray gamma */

       double gamma_red, gamma_green, gamma_blue;

        xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
        xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
        xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

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
      
        xsane_create_gamma_curve(xsane.gamma_data_red, 0,
                                 gamma_red, xsane.brightness_red, xsane.contrast_red,
                                 gamma_red_size, gamma_red_max);

        xsane_create_gamma_curve(xsane.gamma_data_green, 0,
                                 gamma_green, xsane.brightness_green, xsane.contrast_green,
                                 gamma_green_size, gamma_green_max);

        xsane_create_gamma_curve(xsane.gamma_data_blue, 0,
                                 gamma_blue, xsane.brightness_blue, xsane.contrast_blue,
                                 gamma_blue_size, gamma_blue_max);

        /* gamma tables are freed after scan */
      }

    }
    else /* scanner does not support any gamma correction */
    {
      /* ok, we have to do it on our own */

      if (xsane.xsane_color == 0) /* no color scan */
      {
       double gamma;

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

        /* gamma table is freed after scan */
      }
      else /* color scan */
      {
       double gamma_red, gamma_green, gamma_blue;
        /* ok, we have to combin gray and color slider values */

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

        /* gamma tables are freed after scan */
      }

    }
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_start_scan();
}

/* ---------------------------------------------------------------------------------------------------------------------- */
