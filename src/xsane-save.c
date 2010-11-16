/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-save.c

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
#include "xsane-save.h"
#include <time.h>
#include <sys/wait.h> 

/* the following test is always false */
#ifdef _native_WIN32
# include <winsock.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
#endif

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef HAVE_OS2_H
#include <process.h>
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_ANY_GIMP
 
#include <libgimp/gimp.h>
 
static void xsane_gimp_query(void);
#ifdef HAVE_GIMP_2
static void xsane_gimp_run(const gchar *name, gint nparams, const GimpParam *param, gint *nreturn_vals, GimpParam **return_vals);
#else
static void xsane_gimp_run(char *name, int nparams, GimpParam *param, int *nreturn_vals, GimpParam **return_vals);
#endif
 
GimpPlugInInfo PLUG_IN_INFO =
{
#if 1
  NULL,                         /* init_proc */
#else
  xsane_gimp_query,             /* init_proc that queries xsane each time gimp is started */
#endif
  NULL,                         /* quit_proc */
  xsane_gimp_query,             /* query_proc */
  xsane_gimp_run,               /* run_proc */
};


static int xsane_decode_devname(const char *encoded_devname, int n,
char *buf);
static int xsane_encode_devname(const char *devname, int n, char *buf);
void null_print_func(gchar *msg);
 
#endif /* HAVE_ANY_GIMP */

/* ---------------------------------------------------------------------------------------------------------------------- */
/* why this routine ? 
 Problem: link attack
          Bad user wants to overwrite a file (mywork.txt) of good user.
          File permissions of mywork.txt is 700 so that bad user can not
          change or overwrite the file. Directory permissions allow bad user
          to write into directory. Bad user sets symlink from a file that good
          user will write soon (image.pnm) to mywork.txt.
          ==> Good user overwrites his own file, he is allowed to do so.

 Solution: remove file.
           Create outputfile and make sure that it does not exist while creation.

           The file is created with the requested image-file permissions.

 Note: This case is a bit curious because it is only a small part of a larger problem:
 When other users have write access to the directory they simply can move
 mywork.txt to image.pnm. If they do it in the right moment the file is
 overwritten without any notice of good user. If they do it long before xsane
 wants to write image.pnm then xsane will possibly ask if image.pnm shall be
 overwritten. So the real solution is to make the direcoty permissions safe!!!
 But some users asked for this and so I added this.


 This routine shall not be called for temporary files because temp files shall not
 be removed after they have been created safe. (Although a temporary file should
 not be a symlink so there should be no problem with this)
*/

int xsane_create_secure_file(const char *filename)
/* returns 0 on success, -1 on error */
{
 int fd;

  DBG(DBG_proc, "xsane_create_secure_file\n");

  remove(filename); /* we need to remove the file because open(..., O_EXCL) will fail otherwise */
  umask((mode_t) preferences.image_umask); /* define image file permissions */   
  fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   

  if (fd > 0)
  {
    DBG(DBG_info, "file %s is created and secure\n", filename);
    close(fd);
    fd = 0;
  }
  else
  {
    DBG(DBG_info, "could not create secure file %s\n", filename);
  }

 return fd; /* -1 means file is not safe !!! otherwise 0 */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_cancel_save(int *cancel_save)
{
  DBG(DBG_proc, "xsane_cancel_save\n");
  *cancel_save = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_convert_text_to_filename(char **text)
{
  DBG(DBG_proc, "xsane_convert_text_to_filename\n");

  if (text)
  {
   char *filename = *text;
   char buf[TEXTBUFSIZE];
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

int xsane_get_filesize(char *filename)
{
 FILE *infile;
 int pos;
 int size;

  infile = fopen(filename, "rb"); /* read binary (b for win32) */
  if (infile == NULL)
  {
   return 0;
  }

  pos = ftell(infile);
  fseek(infile, 0, SEEK_END); /* get size */
  size = ftell(infile);
  fseek(infile, pos, SEEK_SET); /* go to previous position */

  fclose(infile);

 return size;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_ensure_counter_in_filename(char **filename, int counter_len)
{
 char *position_point = NULL;
 char *position;
 int counter = 1;

  DBG(DBG_proc, "xsane_ensure_counter_in_filename\n");

  if (!counter_len)
  {
   counter_len = 1;
  }

  position_point = strrchr(*filename, '.');

  if (!position_point) /* nothing usable ? */
  {
    position_point = *filename + strlen(*filename); /* position_point - 1 is last character */
  }

  if (position_point)
  {
    position = position_point-1;
    if ( (position < *filename) || (*position < '0') || (*position >'9') ) /* we have no counter */
    {
     char buf[PATH_MAX];
     int len;

      len = position_point - (*filename); /* length until "." or end of string */
      strncpy(buf, *filename, len);
      snprintf(buf+len, sizeof(buf)-len, "-%0*d%s", counter_len,  counter, position_point);
      *filename = strdup(buf);
    }
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

  if ( (!step) && (!min_counter_len) )
  {
    return; /* do not touch counter */
  }

  while (1) /* loop because we may have to skip existing files */
  {
    position_point = strrchr(*filename, '.');

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
        counter = counter + step; /* update counter */

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
          if (preferences.filetype) /* add filetype to filename */
          {
            snprintf(buf, sizeof(buf), "%s%s", *filename, preferences.filetype);
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

void xsane_read_pnm_header(FILE *file, Image_info *image_info)
{
 int max_val, filetype_nr;
 char buf[TEXTBUFSIZE];
 int items_done;

  fgets(buf, sizeof(buf)-1, file);
  DBG(DBG_info, "filetype header :%s", buf);
 
  if (buf[0] == 'P')
  {
    filetype_nr = atoi(buf+1); /* get filetype number */

    image_info->resolution_x = 72.0;
    image_info->resolution_y = 72.0;
    image_info->reduce_to_lineart = FALSE;
    image_info->enable_color_management = FALSE;

    while (strcmp(buf, "# XSANE data follows\n"))
    {
      fgets(buf, sizeof(buf)-1, file);

      if (!strncmp(buf, "#  resolution_x    =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->resolution_x);
      }
      else if (!strncmp(buf, "#  resolution_y    =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->resolution_y);
      }
      else if (!strncmp(buf, "#  threshold       =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->threshold);
      }
      else if (!strncmp(buf, "#  gamma           =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->gamma);
      }
      else if (!strncmp(buf, "#  gamma      IRGB =", 20))
      {
        sscanf(buf+20, "%lf %lf %lf %lf",
                        &image_info->gamma,
                             &image_info->gamma_red, 
                                &image_info->gamma_green, 
                                    &image_info->gamma_blue);
      }
      else if (!strncmp(buf, "#  brightness      =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->brightness);
      }
      else if (!strncmp(buf, "#  brightness IRGB =", 20))
      {
        sscanf(buf+20, "%lf %lf %lf %lf",
                        &image_info->brightness,
                             &image_info->brightness_red, 
                                &image_info->brightness_green, 
                                    &image_info->brightness_blue);
      }
      else if (!strncmp(buf, "#  contrast        =", 20))
      {
        sscanf(buf+20, "%lf", &image_info->contrast);
      }
      else if (!strncmp(buf, "#  contrast   IRGB =", 20))
      {
        sscanf(buf+20, "%lf %lf %lf %lf",
                        &image_info->contrast,
                             &image_info->contrast_red, 
                                &image_info->contrast_green, 
                                    &image_info->contrast_blue);
      }
      else if (!strncmp(buf, "#  color-management=", 20))
      {
        sscanf(buf+20, "%d", &image_info->enable_color_management);
      }
      else if (!strncmp(buf, "#  cms-function    =", 20))
      {
        sscanf(buf+20, "%d", &image_info->cms_function);
      }
      else if (!strncmp(buf, "#  cms-intent      =", 20))
      {
        sscanf(buf+20, "%d", &image_info->cms_intent);
      }
      else if (!strncmp(buf, "#  cms-bpc         =", 20))
      {
        sscanf(buf+20, "%d", &image_info->cms_bpc);
      }
      else if (!strncmp(buf, "#  icm-profile     =", 20))
      {
        sscanf(buf+20, "%s", image_info->icm_profile);
      }
      else if (!strncmp(buf, "#  reduce to lineart", 20))
      {
        image_info->reduce_to_lineart = TRUE;
      }
    }

    items_done = fscanf(file, "%d %d", &image_info->image_width, &image_info->image_height);

    image_info->depth = 1;

    if (filetype_nr != 4) /* P4 = lineart */
    {
      items_done = fscanf(file, "%d", &max_val);

      if (max_val == 255)
      {
        image_info->depth = 8;
      }
      else if (max_val == 65535)
      {
        image_info->depth = 16;
      }
    }

    fgetc(file); /* read exactly one newline character */
    

    image_info->channels = 1;
 
    if (filetype_nr == 6) /* ppm RGB */
    {
      image_info->channels = 3;
    } 
  }
#ifdef SUPPORT_RGBA
  else if (buf[0] == 'S') /* RGBA format */
  {
    items_done = fscanf(file, "%d %d\n%d", &image_info->image_width, &image_info->image_height, &max_val);
    fgetc(file); /* read exactly one newline character */

    image_info->depth = 1;

    if (max_val == 255)
    {
      image_info->depth = 8;
    }
    else if (max_val == 65535)
    {
      image_info->depth = 16;
    }

    image_info->channels = 4;
  }
#endif

  DBG(DBG_info, "xsane_read_pnm_header: width=%d, height=%d, depth=%d, colors=%d, resolution_x=%f, resolution_y=%f\n",
      image_info->image_width, image_info->image_height, image_info->depth, image_info->channels,
      image_info->resolution_x, image_info->resolution_y);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_write_pnm_header(FILE *file, Image_info *image_info, int save_pnm16_as_ascii)
{
 int maxval;
 int magic;

  fflush(file);
  rewind(file);

  if (image_info->depth > 8)
  {
    maxval = 65535;

    if (save_pnm16_as_ascii)
    {
      magic = 2; /* thats the magic number for grayscale ascii, 3 = color ascii */
    }
    else /* save pnm as binary */
    {
      magic = 5; /* that is the magic number for grayscake binary, 6 = color binary */
    }
  }
  else
  {
    maxval = 255;
    magic = 5; /* 8 bit images are always saved in binary mode */
  }


  if (image_info->channels == 1)
  {
    if (image_info->depth == 1)
    {
      /* do not touch the texts and length here, the reading routine needs to know the exact texts */
      fprintf(file, "P4\n"
                       "# XSane settings:\n"
                       "#  resolution_x    = %6.1f\n"
                       "#  resolution_y    = %6.1f\n"
                       "#  threshold       = %4.1f\n"
                       "# XSANE data follows\n"
                       "%05d %05d\n",
                       image_info->resolution_x,
                       image_info->resolution_y,
                       image_info->threshold,
                       image_info->image_width, image_info->image_height);
    }
    else if (image_info->reduce_to_lineart)
    {
      /* do not touch the texts and length here, the reading routine needs to know the exact texts */
      fprintf(file, "P%d\n"
                       "# XSane settings:\n"
                       "#  resolution_x    = %6.1f\n"
                       "#  resolution_y    = %6.1f\n"
                       "#  threshold       = %4.1f\n"
                       "#  reduce to lineart\n"
                       "# XSANE data follows\n"
                       "%05d %05d\n"
                       "%d\n",
                       magic, /* P5 for binary, P2 for ascii */
                       image_info->resolution_x,
                       image_info->resolution_y,
                       image_info->threshold,
                       image_info->image_width, image_info->image_height,
                       maxval);
    }
    else
    {
      fprintf(file, "P%d\n"
                       "# XSane settings:\n"
                       "#  resolution_x    = %6.1f\n"
                       "#  resolution_y    = %6.1f\n"
                       "#  gamma           = %3.2f\n"
                       "#  brightness      = %4.1f\n"
                       "#  contrast        = %4.1f\n"
                       "#  color-management= %d\n"
                       "#  cms-function    = %d\n"
                       "#  cms-intent      = %d\n"
                       "#  cms-bpc         = %d\n"
                       "#  icm-profile     = %s\n"
                       "# XSANE data follows\n"
                       "%05d %05d\n"
                       "%d\n",
                       magic, /* P5 for binary, P2 for ascii */
                       image_info->resolution_x,
                       image_info->resolution_y,
                       image_info->gamma,
                       image_info->brightness,
                       image_info->contrast,
		       image_info->enable_color_management,
		       image_info->cms_function,
		       image_info->cms_intent,
		       image_info->cms_bpc,
		       image_info->icm_profile,
                       image_info->image_width, image_info->image_height,
                       maxval);
    }
  }
  else if (image_info->channels == 3)
  {
    fprintf(file, "P%d\n"
                     "# XSane settings:\n"
                     "#  resolution_x    = %6.1f\n"
                     "#  resolution_y    = %6.1f\n"
                     "#  gamma      IRGB = %3.2f %3.2f %3.2f %3.2f\n"
                     "#  brightness IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                     "#  contrast   IRGB = %4.1f %4.1f %4.1f %4.1f\n"
                     "#  color-management= %d\n"
                     "#  cms-function    = %d\n"
                     "#  cms-intent      = %d\n"
                     "#  cms-bpc         = %d\n"
                     "#  icm-profile     = %s\n"
                     "# XSANE data follows\n"
                     "%05d %05d\n" \
                     "%d\n",
                     magic+1, /* P6 for binary, P3 for ascii */
                     image_info->resolution_x,
                     image_info->resolution_y,
                     image_info->gamma,      image_info->gamma_red,      image_info->gamma_green,      image_info->gamma_blue,
                     image_info->brightness, image_info->brightness_red, image_info->brightness_green, image_info->brightness_blue,
                     image_info->contrast,   image_info->contrast_red,   image_info->contrast_green,   image_info->contrast_blue,
		     image_info->enable_color_management,
		     image_info->cms_function,
		     image_info->cms_intent,
		     image_info->cms_bpc,
		     image_info->icm_profile,
                     image_info->image_width, image_info->image_height,
                     maxval);
  }
#ifdef SUPPORT_RGBA
  else if (image_info->channels == 4)
  {
        fprintf(file, "SANE_RGBA\n" \
                         "%d %d\n" \
                         "%d\n",
                         image_info->image_width, image_info->image_height, maxval);
  }
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_copy_file(FILE *outfile, FILE *infile, GtkProgressBar *progress_bar, int *cancel_save)
{
 long size;
 long bytes_sum = 0;
 size_t bytes;
 unsigned char buf[65536];

  DBG(DBG_proc, "copying file\n");

  fseek(infile, 0, SEEK_END);
  size = ftell(infile);
  fseek(infile, 0, SEEK_SET);

  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

  while (!feof(infile))
  {
    bytes = fread(buf, 1, sizeof(buf), infile);
    if (bytes > 0)
    {
      fwrite(buf, 1, bytes, outfile);
      bytes_sum += bytes;
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) bytes_sum / size); /* update progress bar */

    if (ferror(infile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_READ, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

  fflush(outfile);

  if (size != bytes_sum)
  {
    DBG(DBG_info, "copy errro, not complete, %ld bytes of %ld bytes copied\n", bytes_sum, size);
    *cancel_save = 1;
   return (*cancel_save);
  }

  DBG(DBG_info, "copy complete, %ld bytes copied\n", bytes_sum);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_copy_file_by_name(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save)
{
 FILE *infile;
 FILE *outfile;

  DBG(DBG_proc, "copying file %s to %s\n", input_filename, output_filename);

  outfile = fopen(output_filename, "wb"); /* b = binary mode for win32 */

  if (outfile == 0)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, output_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
   return -2;
  }

  infile = fopen(input_filename, "rb"); /* read binary (b for win32) */
  if (infile == 0)
  {
   char buf[TEXTBUFSIZE];
    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, input_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);     

    fclose(outfile);
    remove(output_filename); /* remove already created output file */
   return -1;
  }

  xsane_copy_file(outfile, infile, progress_bar, cancel_save);

  fclose(infile);
  fclose(outfile);

  gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "");
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
cmsHTRANSFORM xsane_create_cms_transform(Image_info *image_info, int cms_function, int cms_intent, int cms_bpc)
{
 cmsHPROFILE hInProfile = NULL;
 cmsHPROFILE hOutProfile = NULL;
 cmsHTRANSFORM hTransform = NULL;
 DWORD cms_input_format;
 DWORD cms_output_format;
 DWORD cms_flags = 0;

  if (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)
  {
    return NULL;
  }

  DBG(DBG_info, "Prepare CMS transform\n");

  cmsErrorAction(LCMS_ERROR_SHOW);

  if (cms_bpc)
  {
    cms_flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
  }

  if (image_info->channels == 1) /* == 1 (grayscale) */
  {
    if (image_info->depth == 8)
    {
      cms_input_format  = TYPE_GRAY_8;
      cms_output_format = TYPE_GRAY_8;
    }
    else
    {
      cms_input_format  = TYPE_GRAY_16;
      cms_output_format = TYPE_GRAY_16;
    }
  }
  else /* color */
  {
    if (image_info->depth == 8)
    {
      cms_input_format  = TYPE_RGB_8;
      cms_output_format = TYPE_RGB_8;
    }
    else
    {
      cms_input_format  = TYPE_RGB_16;
      cms_output_format = TYPE_RGB_16;
    }
  }

  hInProfile  = cmsOpenProfileFromFile(image_info->icm_profile, "r");
  if (!hInProfile)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s\n%s %s: %s\n", ERR_CMS_CONVERSION, ERR_CMS_OPEN_ICM_FILE, CMS_SCANNER_ICM, image_info->icm_profile);
    xsane_back_gtk_error(buf, TRUE);
  }
  if (cms_function == XSANE_CMS_FUNCTION_CONVERT_TO_SRGB)
  {
    if (image_info->channels == 1) /* == 1 (grayscale) */
    {
#if 1 /* xxx oli */
     LPGAMMATABLE Gamma = cmsBuildGamma(256, 2.2);

      hOutProfile = cmsCreateGrayProfile(cmsD50_xyY(), Gamma);
      cmsFreeGamma(Gamma);
#endif
    }
    else
    {
      hOutProfile = cmsCreate_sRGBProfile();
    }
  }
  else
  {
    hOutProfile = cmsOpenProfileFromFile(preferences.working_color_space_icm_profile, "r");
    if (!hOutProfile)
    {
     char buf[TEXTBUFSIZE];

      cmsCloseProfile(hInProfile);

      snprintf(buf, sizeof(buf), "%s\n%s %s: %s\n", ERR_CMS_CONVERSION, ERR_CMS_OPEN_ICM_FILE, CMS_DISPLAY_ICM, preferences.display_icm_profile);
      xsane_back_gtk_error(buf, TRUE);
    }
  }

  if (!hOutProfile)
  {
   char buf[TEXTBUFSIZE];

    cmsCloseProfile(hInProfile);

    snprintf(buf, sizeof(buf), "%s\n", ERR_CMS_CONVERSION);
    xsane_back_gtk_error(buf, TRUE);
  }

  hTransform = cmsCreateTransform(hInProfile, cms_input_format,
                                  hOutProfile, cms_output_format,
                                  cms_intent, cms_flags);

  cmsCloseProfile(hInProfile);
  cmsCloseProfile(hOutProfile);

  if (!hTransform)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s\n%s\n", ERR_CMS_CONVERSION, ERR_CMS_CREATE_TRANSFORM);
    xsane_back_gtk_error(buf, TRUE);
  }

 return hTransform;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_grayscale_image_as_lineart(FILE *outfile, FILE *imagefile, Image_info *image_info, GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y, bit;
 u_char bitval, packed;

  *cancel_save = 0;

  image_info->depth = 1;

  xsane_write_pnm_header(outfile, image_info, 0);

  for (y = 0; y < image_info->image_height; y++)
  {
    bit = 128;
    packed = 0;

    for (x = 0; x < image_info->image_width; x++)
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

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }


    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height); /* update progress bar */

    if (*cancel_save)
    {
      break;
    }
  }
 
 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_scaled_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float x_scale, float y_scale, GtkProgressBar *progress_bar, int *cancel_save)
{
 int original_image_width  = image_info->image_width;
 int original_image_height = image_info->image_height;
 int new_image_width  = image_info->image_width  * x_scale + 0.5;
 int new_image_height = image_info->image_height * y_scale + 0.5;
 unsigned char *original_line;
 guint16 *original_line16 = NULL;
 unsigned char *new_line;
 float *pixel_val;
 float *pixel_norm;
 int bytespp = 1;
 float x, y;
 int c;
 int oldy;
 int x_new, y_new;
 float x_go, y_go;
 float factor, x_factor, y_factor;
 guint16 color;
 int read_line;
 size_t bytes_read;

  DBG(DBG_proc, "xsane_save_scaled_image\n");

  *cancel_save = 0;

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  image_info->image_width  = new_image_width;
  image_info->image_height = new_image_height;
  image_info->resolution_x *= x_scale;
  image_info->resolution_y *= y_scale;

  original_line = malloc(original_image_width * image_info->channels * bytespp);
  if (!original_line)
  {
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }

  new_line = malloc(new_image_width * image_info->channels * bytespp);
  if (!new_line)
  {
    free(original_line);
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }

  pixel_val = malloc(new_image_width * image_info->channels * sizeof(float));
  if (!pixel_val)
  {
    free(original_line);
    free(new_line);
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }               

  pixel_norm = malloc(new_image_width * image_info->channels * sizeof(float));
  if (!pixel_norm)
  {
    free(original_line);
    free(new_line);
    free(pixel_val);
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }               

  xsane_write_pnm_header(outfile, image_info, 0);

  read_line = TRUE;

  memset(pixel_val,  0, new_image_width * image_info->channels * sizeof(float));
  memset(pixel_norm, 0, new_image_width * image_info->channels * sizeof(float));

  y_new = 0;
  y_go = 1.0 / y_scale;
  y_factor = 1.0;
  y = 0.0;

  while (y < original_image_height)
  {
    DBG(DBG_info2, "xsane_save_scaled_image: original line %d, new line %d\n", (int) y, y_new);

    xsane_progress_bar_set_fraction(progress_bar, (float) y / original_image_height);

    if (read_line)
    {
      DBG(DBG_info, "xsane_save_scaled_image: reading original line %d\n", (int) y);
      bytes_read = fread(original_line, original_image_width, image_info->channels * bytespp, imagefile); /* read one line */
      original_line16 = (guint16 *) original_line;
    }

    x_new = 0;
    x_go = 1.0 / x_scale;
    x = 0.0;
    x_factor = 1.0;

    while ( (x < original_image_width) && (x_new < new_image_width) ) /* add this line to anti aliasing buffer */
    {
      factor = x_factor * y_factor;

      for (c = 0; c < image_info->channels; c++)
      {
        if (bytespp == 1)
        {
          color = original_line[((int) x) * image_info->channels + c];
        }
        else /* bytespp == 2 */
        {
          color = original_line16[((int) x) * image_info->channels + c];
        }

        pixel_val [x_new * image_info->channels + c] += factor * color;
        pixel_norm[x_new * image_info->channels + c] += factor;
      }

      x_go -= x_factor;

      if (x_go <= 0.0) /* change of pixel in new image */
      {
        x_new++;
        x_go = 1.0 / x_scale;

        x_factor = x - (int) x; /* use pixel rest */
        if (x_factor > x_go)
        {
          x_factor = x_go;
        }
      }
      else
      {
        x_factor = x_go;
      }

      if (x_factor > 1.0)
      {
        x_factor = 1.0;
      }

      x += x_factor;
    }

    y_go -= y_factor;

    if (y_go <= 0.0) /* normalize one line and write to destination image file */
    {
      DBG(DBG_info2, "xsane_save_scaled_image: writing new line %d\n", y_new);

      if (bytespp == 1)
      {
        for (x_new = 0; x_new < new_image_width * image_info->channels; x_new++)
        {
          new_line[x_new] = (int) (pixel_val[x_new] / pixel_norm[x_new]);
        }
      }
      else /* bytespp == 2 */
      {
       guint16 *new_line16 = (guint16 *) new_line;

        for (x_new = 0; x_new < new_image_width * image_info->channels; x_new++)
        {
          new_line16[x_new] = (int) (pixel_val[x_new] / pixel_norm[x_new]);
        }
      }

      fwrite(new_line, new_image_width, image_info->channels * bytespp, outfile); /* write one line */

      if (ferror(outfile))
      {
       char buf[TEXTBUFSIZE];
 
        snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
        DBG(DBG_error, "%s\n", buf);
        xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
        *cancel_save = 1;
       break;
      }

      /* reset values and norm factors */
      memset(pixel_val,  0, new_image_width * image_info->channels * sizeof(float));
      memset(pixel_norm, 0, new_image_width * image_info->channels * sizeof(float));

      y_new++;
      y_go = 1.0 / y_scale;

      y_factor = y - (int) y;
      if (y_factor > y_go)
      {
        y_factor = y_go;
      }
    }
    else
    {
      y_factor = y_go;
    }

    if (y_factor > 1.0)
    {
      y_factor = 1.0;
    }

    oldy = (int) y;
    y += y_factor;
    read_line = (oldy != (int) y);
  }

  if (read_line) /* we have to write one more line */
  {
    fwrite(new_line, new_image_width, image_info->channels * bytespp, outfile); /* write one line */
  }

  free(original_line);
  free(new_line);
  free(pixel_val);
  free(pixel_norm);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
int xsane_save_scaled_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float x_scale, float y_scale, GtkProgressBar *progress_bar, int *cancel_save)
{
 float original_y;
 int old_original_y;
 int x, y, i;
 int original_image_width  = image_info->image_width;
 int new_image_width  = image_info->image_width * x_scale;
 int new_image_height = image_info->image_height * y_scale;
 unsigned char *original_line;
 unsigned char *new_line;
 int bytespp = 1;

  DBG(DBG_proc, "xsane_save_scaled_image\n");

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  image_info->image_width  = new_image_width;
  image_info->image_height = new_image_height;
  image_info->resolution_x *= x_scale;
  image_info->resolution_y *= y_scale;

  original_line = malloc(original_image_width * image_info->channels * bytespp);
  if (!original_line)
  {
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }

  new_line = malloc(new_image_width * image_info->channels * bytespp);
  if (!new_line)
  {
    free(original_line);
    DBG(DBG_error, "xsane_save_scaled_image: out of memory\n");
   return -1;
  }

  xsane_write_pnm_header(outfile, image_info, 0);

  original_y = 0.0;
  old_original_y = -1;

  for (y = 0; y < new_image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    for (; ((int) original_y) - old_original_y; old_original_y += 1)
    {
      bytes_read = fread(original_line, original_image_width, image_info->channels * bytespp, imagefile); /* read one line */
    }

    for (x = 0; x < new_image_width; x++)
    {
      for (i = 0; i < image_info->channels * bytespp; i++)
      {
        new_line[x * image_info->channels * bytespp + i] = original_line[((int) (x / x_scale)) * image_info->channels * bytespp + i];
      }
    }

    fwrite(new_line, new_image_width, image_info->channels * bytespp, outfile); /* write one line */

    original_y += 1/y_scale;

    if (*cancel_save)
    {
      break;
    }
  }

  free(original_line);
  free(new_line);

  fflush(outfile);

 return (*cancel_save);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_despeckle_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int radius, GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y, sx, sy, i;
 int xmin, xmax;
 int ymin, ymax;
 int count;
 unsigned char *line_cache;
 unsigned char *line_cache_ptr;
 guint16 *color_cache;
 guint16 *color_cache_ptr;
 int bytespp = 1;
 int color_radius;
 int color_width  = image_info->image_width * image_info->channels;
 size_t bytes_read;

  radius--; /* correct radius : 1 means nothing happens */

  if (radius < 1)
  {
    radius = 1;
  }

  color_radius = radius * image_info->channels;

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  xsane_write_pnm_header(outfile, image_info, 0);

  line_cache = malloc(color_width * bytespp * (2 * radius + 1));
  if (!line_cache)
  {
    DBG(DBG_error, "xsane_despeckle_image: out of memory\n");
   return -1;
  }

  bytes_read = fread(line_cache, color_width * bytespp, (2 * radius + 1), imagefile);

  color_cache = malloc((size_t) sizeof(guint16) * (2*radius+1)*(2*radius+1));

  if (!color_cache)
  {
    free(line_cache);
    DBG(DBG_error, "xsane_despeckle_image: out of memory\n");
   return -1;
  }

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float)  y / image_info->image_height);

    ymin = y - radius;
    ymax = y + radius;

    if (ymin < 0)
    {
      ymin = 0;
    }

    if (ymax > image_info->image_height)
    {
      ymax = image_info->image_height;
    }

    for (x = 0; x < color_width; x++)
    {
      xmin = x - color_radius;
      xmax = x + color_radius;

      if (xmin < 0)
      {
        xmin = x % image_info->channels;
      }

      if (xmax > color_width)
      {
        xmax = color_width;
      }

      count = 0;

      color_cache_ptr = color_cache;


      if (bytespp == 1)
      {
        for (sy = ymin; sy <= ymax; sy++) /* search area defined by radius  - y part */
        {
          line_cache_ptr = line_cache + (sy-ymin) * color_width + xmin;

          for (sx = xmin; sx <= xmax; sx+=image_info->channels) /* x part */
          {
            *color_cache_ptr = *line_cache_ptr;
            color_cache_ptr++;
            line_cache_ptr += image_info->channels;
          }
        }

        /* sort color_cache */

        count = color_cache_ptr - color_cache;

        if (count > 1)
        {
         int d, j, val;

          for (d = count / 2; d > 0; d = d / 2)
          {
            for (i = d; i < count; i++)
            {
              for (j = i - d, color_cache_ptr = color_cache + j; j >= 0 && color_cache_ptr[0] > color_cache_ptr[d]; j -= d, color_cache_ptr -= d)
              {
                val                = color_cache_ptr[0];
                color_cache_ptr[0] = color_cache_ptr[d];
                color_cache_ptr[d] = val;
              };
            }
          }
        }

        fputc((char) (color_cache[count/2]), outfile);
      }
      else /* 16 bit/color */
      {
       guint16 val16;
       guint16 *line_cache16 = (guint16 *) line_cache;
       guint16 *line_cache16_ptr;
       char *bytes16 = (char *) &val16;

        for (sy = ymin; sy <= ymax; sy++)
        {
          line_cache16_ptr = line_cache16 + (sy-ymin) * color_width + xmin;

          for (sx = xmin; sx <= xmax; sx+=image_info->channels)
          {
            *color_cache_ptr = *line_cache16_ptr;
            color_cache_ptr++;
            line_cache16_ptr += image_info->channels;
          }
        }

        /* sort color_cache */

        count = color_cache_ptr - color_cache;

        if (count > 1)
        {
         int d,j, val;

          for (d = count / 2; d > 0; d = d / 2)
          {
            for (i = d; i < count; i++)
            {
              for (j = i - d, color_cache_ptr = color_cache + j; j >= 0 && color_cache_ptr[0] > color_cache_ptr[d]; j -= d, color_cache_ptr -= d)
              {
                val                = color_cache_ptr[0];
                color_cache_ptr[0] = color_cache_ptr[d];
                color_cache_ptr[d] = val;
              };
            }
          }
        }

        val16 = color_cache[count/2];
        fputc(bytes16[0], outfile); /* write bytes in machine byte order */
        fputc(bytes16[1], outfile);
      }
    }

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if ((y > radius) && (y < image_info->image_height - radius))
    {
      memcpy(line_cache, line_cache + color_width * bytespp, 
             color_width * bytespp * 2 * radius);
      bytes_read = fread(line_cache + color_width * bytespp * 2 * radius,
            color_width * bytespp, 1, imagefile);
    }
  }

  fflush(outfile);

  free(line_cache);
  free(color_cache);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_blur_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float radius, GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y, sx, sy;
 int xmin, xmax;
 int ymin, ymax;
 double val, norm, outer_factor;
 unsigned char *line_cache;
 int bytespp = 1;
 int intradius;
 int xmin_flag;
 int xmax_flag;
 int ymin_flag;
 int ymax_flag;
 size_t bytes_read;

  *cancel_save = 0;

  intradius = (int) radius;

  outer_factor = radius - (int) radius;

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  xsane_write_pnm_header(outfile, image_info, 0);

  line_cache = malloc(image_info->image_width * image_info->channels * bytespp * (2 * intradius + 1));
  if (!line_cache)
  {
    DBG(DBG_error, "xsane_blur_image: out of memory\n");
   return -1;
  }

  bytes_read = fread(line_cache, image_info->image_width * image_info->channels * bytespp, (2 * intradius + 1), imagefile);

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float)  y / image_info->image_height);

    for (x = 0; x < image_info->image_width * image_info->channels; x++)
    {
      xmin_flag = xmax_flag = ymin_flag = ymax_flag = TRUE;

      xmin = x - intradius * image_info->channels;
      xmax = x + intradius * image_info->channels;

      if (xmin < 0)
      {
        xmin = x % image_info->channels;
        xmin_flag = FALSE;
      }

      if (xmax > image_info->image_width * image_info->channels)
      {
        xmax = image_info->image_width * image_info->channels;
        xmax_flag = FALSE;
      }

      ymin = y - intradius;
      ymax = y + intradius;

      if (ymin < 0)
      {
        ymin = 0;
        ymin_flag = FALSE;
      }

      if (ymax > image_info->image_height)
      {
        ymax = image_info->image_height;
        ymax_flag = FALSE;
      }

      val  = 0.0;
      norm = 0.0;

      if (bytespp == 1)
      {
        if (xmin_flag) /* integrate over left margin */
        {
          for (sy = ymin+1; sy <= ymax-1 ; sy++)
          {
            val += outer_factor * line_cache[(sy-ymin) * image_info->image_width * image_info->channels + xmin];
            norm += outer_factor;
          }
        }

        if (xmax_flag) /* integrate over right margin */
        {
          for (sy = ymin+1; sy <= ymax-1 ; sy++)
          {
            val += outer_factor * line_cache[(sy-ymin) * image_info->image_width * image_info->channels + xmax];
            norm += outer_factor;
          }
        }

        if (ymin_flag) /* integrate over top margin */
        {
          for (sx = xmin+image_info->channels; sx <= xmax-image_info->channels ; sx += image_info->channels)
          {
            val += outer_factor * line_cache[sx];
            norm += outer_factor;
          }
        }

        if (ymax_flag) /* integrate over bottom margin */
        {
          for (sx = xmin+image_info->channels; sx <= xmax-image_info->channels ; sx += image_info->channels)
          {
            val += outer_factor * line_cache[(ymax-ymin) * image_info->image_width * image_info->channels + sx];
            norm += outer_factor;
          }
        }

        for (sy = ymin+1; sy <= ymax-1; sy++) /* integrate internal square */
        {
          for (sx = xmin+image_info->channels; sx <= xmax-image_info->channels; sx+=image_info->channels)
          {
            val += line_cache[(sy-ymin) * image_info->image_width * image_info->channels + sx];
            norm += 1.0;
          }
        }
        fputc((char) ((int) (val/norm)), outfile);
      }
      else /* bytespp == 2 */
      {
       guint16 *line_cache16 = (guint16 *) line_cache;
       guint16 val16;
       char *bytes16 = (char *) &val16;

        if (xmin_flag) /* integrate over left margin */
        {
          for (sy = ymin+1; sy <= ymax-1 ; sy++)
          {
            val += outer_factor * line_cache16[(sy-ymin) * image_info->image_width * image_info->channels + xmin];
            norm += outer_factor;
          }
        }

        if (xmax_flag) /* integrate over right margin */
        {
          for (sy = ymin+1; sy <= ymax-1 ; sy++)
          {
            val += outer_factor * line_cache16[(sy-ymin) * image_info->image_width * image_info->channels + xmax];
            norm += outer_factor;
          }
        }

        if (ymin_flag) /* integrate over top margin */
        {
          for (sx = xmin+image_info->channels; sx <= xmax-image_info->channels ; sx += image_info->channels)
          {
            val += outer_factor * line_cache16[sx];
            norm += outer_factor;
          }
        }

        if (ymax_flag) /* integrate over bottom margin */
        {
          for (sx = xmin+image_info->channels; sx <= xmax-image_info->channels ; sx += image_info->channels)
          {
            val += outer_factor * line_cache16[(ymax-ymin) * image_info->image_width * image_info->channels + sx];
            norm += outer_factor;
          }
        }

        for (sy = ymin; sy <= ymax; sy++) /* integrate internal square */
        {
          for (sx = xmin; sx <= xmax; sx+=image_info->channels)
          {
            val += line_cache16[(sy-ymin) * image_info->image_width * image_info->channels + sx];
            norm += 1.0;
          }
        }

        val16 = val / norm;
        fputc(bytes16[0], outfile); /* write bytes in machine byte order */
        fputc(bytes16[1], outfile);
      }
    }

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

      /* reset values and norm factors */

    if ((y > intradius) && (y < image_info->image_height - intradius))
    {
      memcpy(line_cache, line_cache + image_info->image_width * image_info->channels * bytespp, 
             image_info->image_width * image_info->channels * bytespp * 2 * intradius);
      bytes_read = fread(line_cache + image_info->image_width * image_info->channels * bytespp * 2 * intradius,
            image_info->image_width * image_info->channels * bytespp, 1, imagefile);
    }
  }

  fflush(outfile);
  free(line_cache);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0
int xsane_save_blur_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int radius, GtkProgressBar *progress_bar)
{
 int x, y, sx, sy;
 int xmin, xmax;
 int ymin, ymax;
 int pos0;
 int val, count;
 unsigned char *line_cache;
 int bytespp = 1;

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  pos0 = ftell(imagefile); /* mark position to skip header */

  xsane_write_pnm_header(outfile, image_info, 0);

  line_cache = malloc(image_info->image_width * image_info->channels * bytespp * (2 * radius + 1));
  if (!line_cache)
  {
    DBG(DBG_error, "xsane_blur_image: out of memory\n");
   return -1;
  }

  bytes_read = fread(line_cache, image_info->image_width * image_info->channels * bytespp, (2 * radius + 1), imagefile);

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float)  y / image_info->image_height);

    for (x = 0; x < image_info->image_width * image_info->channels; x++)
    {
      xmin = x - radius * image_info->channels;
      xmax = x + radius * image_info->channels;

      if (xmin < 0)
      {
        xmin = x % image_info->channels;
      }

      if (xmax > image_info->image_width * image_info->channels)
      {
        xmax = image_info->image_width * image_info->channels;
      }

      ymin = y - radius;
      ymax = y + radius;

      if (ymin < 0)
      {
        ymin = 0;
      }

      if (ymax > image_info->image_height)
      {
        ymax = image_info->image_height;
      }

      val = 0;
      count = 0;

      if (bytespp == 1)
      {
        for (sy = ymin; sy <= ymax; sy++)
        {
          for (sx = xmin; sx <= xmax; sx+=image_info->channels)
          {
            val += line_cache[(sy-ymin) * image_info->image_width * image_info->channels + sx];
            count++;
          }
        }
        fputc((char) (val/count), outfile);
      }
      else
      {
       guint16 *line_cache16 = (guint16 *) line_cache;
       guint16 val16;
       char *bytes16 = (char *) &val16;

        for (sy = ymin; sy <= ymax; sy++)
        {
          for (sx = xmin; sx <= xmax; sx+=image_info->channels)
          {
            val += line_cache16[(sy-ymin) * image_info->image_width * image_info->channels + sx];
            count++;
          }
        }

        val16 = val / count;
        fputc(bytes16[0], outfile); /* write bytes in machine byte order */
        fputc(bytes16[1], outfile);
      }
    }

    if ((y > radius) && (y < image_info->image_height - radius))
    {
      memcpy(line_cache, line_cache + image_info->image_width * image_info->channels * bytespp, 
             image_info->image_width * image_info->channels * bytespp * 2 * radius);
      bytes_read = fread(line_cache + image_info->image_width * image_info->channels * bytespp * 2 * radius,
            image_info->image_width * image_info->channels * bytespp, 1, imagefile);
    }
  }

  fflush(outfile);
  free(line_cache);

 return 0;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_rotate_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int rotation, GtkProgressBar *progress_bar, int *cancel_save)
/* returns true if operation was cancelled */
{
 int x, y, pos0, bytespp, i;
 int pixel_width  = image_info->image_width;
 int pixel_height = image_info->image_height;
 float resolution_x = image_info->resolution_x;
 float resolution_y = image_info->resolution_y;

#ifdef HAVE_MMAP
 char *mmaped_imagefile = NULL;
#endif

  DBG(DBG_proc, "xsane_save_rotate_image\n");

  *cancel_save = 0;

  pos0 = ftell(imagefile); /* mark position to skip header */

  bytespp = image_info->channels;

  if (image_info->depth > 8)
  {
    bytespp *= 2;
  }

  if (image_info->depth < 8) /* lineart images are expanded to grayscale until transformation is done */
  {
    image_info->depth = 8; /* so we have at least 8 bits/pixel here */
  }

#ifdef HAVE_MMAP
  mmaped_imagefile = mmap(NULL, pixel_width * pixel_height * bytespp + pos0, PROT_READ, MAP_PRIVATE, fileno(imagefile), 0);
  if (mmaped_imagefile == (char *) -1) /* mmap failed */
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
    default:
     break;

    case 0: /* 0 degree */
      xsane_write_pnm_header(outfile, image_info, 0);

      for (y = 0; y < pixel_height; y++)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) y / pixel_height);

        for (x = 0; x < pixel_width; x++)
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
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }

        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }
     break;

    case 1: /* 90 degree */
      image_info->image_width  = pixel_height;
      image_info->image_height = pixel_width;

      image_info->resolution_x = resolution_y;
      image_info->resolution_y = resolution_x;

      xsane_write_pnm_header(outfile, image_info, 0);

      for (x=0; x<pixel_width; x++)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) x / pixel_width);

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


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }

     break;

    case 2: /* 180 degree */
      xsane_write_pnm_header(outfile, image_info, 0);

      for (y = pixel_height-1; y >= 0; y--)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) (pixel_height - y) / pixel_height);

        for (x = pixel_width-1; x >= 0; x--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i = 0; i < bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }
     break;

    case 3: /* 270 degree */
      image_info->image_width  = pixel_height;
      image_info->image_height = pixel_width;

      image_info->resolution_x = resolution_y;
      image_info->resolution_y = resolution_x;

      xsane_write_pnm_header(outfile, image_info, 0);

      for (x = pixel_width-1; x >= 0; x--)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) (pixel_width - x) / pixel_width);

        for (y = 0; y < pixel_height; y++)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i = 0; i < bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }
     break;

    case 4: /* 0 degree, x mirror */
      xsane_write_pnm_header(outfile, image_info, 0);

      for (y = 0; y < pixel_height; y++)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) y / pixel_height);

        for (x = pixel_width-1; x >= 0; x--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i = 0; i < bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }
     break;

    case 5: /* 90 degree, x mirror */
      image_info->image_width  = pixel_height;
      image_info->image_height = pixel_width;

      image_info->resolution_x = resolution_y;
      image_info->resolution_y = resolution_x;

      xsane_write_pnm_header(outfile, image_info, 0);

      for (x = 0; x < pixel_width; x++)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) x / pixel_width);

        for (y = 0; y < pixel_height; y++)
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
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }

     break;

    case 6: /* 180 degree, x mirror */
      xsane_write_pnm_header(outfile, image_info, 0);

      for (y = pixel_height-1; y >= 0; y--)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) (pixel_height - y) / pixel_height);

        for (x = 0; x < pixel_width; x++)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i = 0; i < bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
        {
          break;
        }
      }
     break;

    case 7: /* 270 degree, x mirror */
      image_info->image_width  = pixel_height;
      image_info->image_height = pixel_width;

      image_info->resolution_x = resolution_y;
      image_info->resolution_y = resolution_x;

      xsane_write_pnm_header(outfile, image_info, 0);

      for (x = pixel_width-1; x >= 0; x--)
      {
        xsane_progress_bar_set_fraction(progress_bar, (float) (pixel_width - x) / pixel_width);

        for (y = pixel_height-1; y >= 0; y--)
        {
#ifdef HAVE_MMAP
          if (mmaped_imagefile)
          {
           char *p = mmaped_imagefile + pos0 + bytespp * (x + y * pixel_width);  /* calculate correct position */

            for (i = 0; i < bytespp; i++)
            {
              fputc(*p++, outfile);
            }
          }
          else
#endif
          {
            fseek(imagefile, pos0 + bytespp * (x + y * pixel_width), SEEK_SET); /* go to the correct position */
            for (i = 0; i < bytespp; i++)
            {
              fputc(fgetc(imagefile), outfile);
            }
          }
        }


        if (ferror(outfile))
        {
         char buf[TEXTBUFSIZE];

          snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
          DBG(DBG_error, "%s\n", buf);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
          *cancel_save = 1;
         break;
        }

        if (*cancel_save)
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

  fflush(outfile);

  return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_ps_create_document_header(FILE *outfile, int pages,
                                          int paper_left_margin, int paper_bottom_margin,
                                          int paper_width, int paper_height,
                                          int paper_orientation, int flatedecode)
{
 int box_left, box_bottom, box_right, box_top;

  DBG(DBG_proc, "xsane_save_ps_create_document_header\n");

  if (paper_orientation >= 8) /* rotate with 90 degrees - landscape mode */
  {
    box_left        = paper_width - paper_left_margin - paper_height;
    box_bottom      = paper_bottom_margin;
    box_right       = box_left   + ceil(paper_height);
    box_top         = box_bottom + ceil(paper_width);
  }
  else /* do not rotate, portrait mode */
  {
    box_left        = paper_left_margin;
    box_bottom      = paper_bottom_margin;
    box_right       = box_left   + ceil(paper_width);
    box_top         = box_bottom + ceil(paper_height);
  }

  fprintf(outfile, "%%!PS-Adobe-3.0\n");
  fprintf(outfile, "%%%%Creator: XSane version %s (sane %d.%d) - by Oliver Rauch\n", VERSION,
                        SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                        SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "%%%%DocumentData: Clean7Bit\n");
  if (flatedecode)
  {
    fprintf(outfile, "%%%%LanguageLevel: 3\n");
  }
  else
  {
    fprintf(outfile, "%%%%LanguageLevel: 2\n");
  }

  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n", box_left, box_bottom, box_right, box_top);

  if (pages)
  {
    fprintf(outfile, "%%%%Pages: %d\n", pages);
  }
  else
  {
    fprintf(outfile, "%%%%Pages: (atend)\n");
  }
  
  fprintf(outfile, "%%%%EndComments\n");
  fprintf(outfile, "%%%%BeginDocument: xsane.ps\n");
  fprintf(outfile, "\n");
/*  fprintf(outfile, "/origstate save def\n"); */
  fprintf(outfile, "20 dict begin\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_ps_create_document_trailer(FILE *outfile, int pages)
{
  DBG(DBG_proc, "xsane_save_ps_create_document_trailer\n");

  fprintf(outfile, "end\n");
/*  fprintf(outfile, "origstate restore\n"); */

  if (pages)
  {
    fprintf(outfile, "%%%%Trailer\n");
    fprintf(outfile, "%%%%Pages: %d\n", pages);
  }

  fprintf(outfile, "%%%%EOF\n");
  fprintf(outfile, "%%%%EndDocument\n");
  fprintf(outfile, "\n");

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_create_image_header(FILE *outfile,
                                              Image_info *image_info,
                                              float width, float height,
                                              int degree, int position_left, int position_bottom,
                                              int box_left, int box_bottom, int box_right, int box_top,
                                              int flatedecode)
{
 int depth; 

  depth = image_info->depth;

  if (depth > 8)
  {
    depth = 12;
  }

  if (depth == 1)
  {
    fprintf(outfile, "/grays %d string def\n", image_info->image_width);
    fprintf(outfile, "/npixels 0 def\n");
    fprintf(outfile, "/rgbindx 0 def\n");
  }

  fprintf(outfile, "%d rotate\n", degree);
  fprintf(outfile, "%d %d translate\n", position_left, position_bottom);
  fprintf(outfile, "%f %f scale\n", width, height);

  fprintf(outfile, "<<\n");
  fprintf(outfile, " /ImageType 1\n");
  fprintf(outfile, " /Width %d\n", image_info->image_width);
  fprintf(outfile, " /Height %d\n", image_info->image_height);
  fprintf(outfile, " /BitsPerComponent %d\n", depth);
  if (image_info->channels == 3)
  {
    fprintf(outfile, " /Decode [0 1 0 1 0 1]\n");
  }
  else
  {
    fprintf(outfile, " /Decode [0 1]\n");
  }
  fprintf(outfile, " /ImageMatrix [%d %d %d %d %d %d]\n", image_info->image_width, 0, 0, -image_info->image_height, 0, image_info->image_height);
  fprintf(outfile, " /DataSource currentfile /ASCII85Decode filter");
#ifdef HAVE_LIBZ
  if (flatedecode)
  {
    fprintf(outfile, " /FlateDecode filter");
  }
#endif
  fprintf(outfile, "\n");
  fprintf(outfile, ">>\n");
  {
    fprintf(outfile, "image\n");
    fprintf(outfile, "\n");
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_ps_create_page_trailer(FILE *outfile)
{
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "%%%%PageTrailer\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBZ
/* Utility function for the PDF output */
static int xsane_write_flatedecode(FILE *outfile, unsigned char *line, int len, int finish)
{
  static unsigned char *cbuf = NULL;
  static int cbuflen = 0;
  static int linelen = 0;
  int outlen;
  static int init = 0;
  static z_stream s;
  int ret;
  int flush;
  static int count = 0;

  DBG(DBG_proc, "xsane_write_flatedecode\n");

  if (linelen != len)
  {
    linelen = len;
    if (cbuf != NULL)
    {
      free(cbuf);
    }
    /* buffer length = length + 0.1 * length + 12 (mandatory) */
    cbuflen = len + len / 10 + 12;
    cbuf = malloc(cbuflen);
  }

  if (cbuf == NULL)
  {
    DBG(DBG_error, "cbuf allocation failed\n");
   return 1;
  }

  if (!init)
  {
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;

    ret = deflateInit(&s, Z_DEFAULT_COMPRESSION);

    if (ret != Z_OK)
    {
      DBG(DBG_error, "deflateInit failed\n");
      free(cbuf);
     return 1;
    }

    init = 1;
  }
  
  s.avail_in = len;
  s.next_in = line;

  do
  {
    s.avail_out = cbuflen;
    s.next_out = cbuf;

    flush = (finish) ? Z_FINISH : Z_NO_FLUSH;

    ret = deflate(&s, flush);

    if (ret == Z_STREAM_ERROR)
    {
      DBG(DBG_error, "deflate failed\n");
      free(cbuf);
     return 1;
    }

    outlen = cbuflen - s.avail_out;

    fwrite(cbuf, outlen, 1, outfile);
  } while (s.avail_out == 0);

  if (finish)
  {
    DBG(DBG_info, "xsane_write_flatedecode finished\n");
    deflateEnd(&s);
    free(cbuf);
    cbuf = NULL;
    init = 0;
    cbuflen = 0;
    linelen = 0;
    count = 0;
  }

 return 0;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBZ
/* Utility function for the PostScript output */
static int xsane_write_compressed_a85_flatedecode(FILE *outfile, unsigned char *line, int len, int finish)
{
  static unsigned char *cbuf = NULL;
  static int cbuflen = 0;
  static int linelen = 0;
  int i, j;
  int outlen;
  static int init = 0;
  static z_stream s;
  int ret;
  int flush;
  static int a85count = 0;
  static guint32 a85tuple = 0;
  static unsigned char a85block[6] = {0, 0, 0, 0, 0, 0};
  static int count = 0;

  DBG(DBG_proc, "xsane_write_compressed_a85_flatedecode\n");

  if (linelen != len)
  {
    linelen = len;
    if (cbuf != NULL)
    {
      free(cbuf);
    }
    /* buffer length = length + 0.1 * length + 12 (mandatory) */
    cbuflen = len + len / 10 + 12;
    cbuf = malloc(cbuflen);
  }

  if (cbuf == NULL)
  {
    DBG(DBG_error, "cbuf allocation failed\n");
   return 1;
  }

  if (!init)
  {
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;

    ret = deflateInit(&s, Z_DEFAULT_COMPRESSION);

    if (ret != Z_OK)
    {
      DBG(DBG_error, "deflateInit failed\n");
      free(cbuf);
     return 1;
    }

    init = 1;
  }
  
  s.avail_in = len;
  s.next_in = line;

  do
  {
    s.avail_out = cbuflen;
    s.next_out = cbuf;

    flush = (finish) ? Z_FINISH : Z_NO_FLUSH;

    ret = deflate(&s, flush);

    if (ret == Z_STREAM_ERROR)
    {
      DBG(DBG_error, "deflate failed\n");
      free(cbuf);
     return 1;
    }

    outlen = cbuflen - s.avail_out;

    /* ASCII85 (base 85) encoding */
    for (i = 0; i < outlen; i++)
    {
      switch (a85count)
      {
        case 0:
          a85tuple |= (cbuf[i] << 24);
          a85count++;
         break;

        case 1:
          a85tuple |= (cbuf[i] << 16);
          a85count++;
         break;

        case 2:
          a85tuple |= (cbuf[i] << 8);
          a85count++;
         break;

        case 3:
          a85tuple |= (cbuf[i] << 0);

          if (count == 40)
          {
            fprintf(outfile, "\n");
            count = 0;
          }

          if (a85tuple == 0)
          {
            fprintf(outfile, "z");
            count++;
          }
          else
          {
            /* The ASCII chars must be written in reverse order, hence -> a85block[4-j] */
            for (j = 0; j < 5; j++)
            {
              a85block[4-j] = a85tuple % 85 + '!';
              a85tuple /= 85;
            }

            for (j = 0; j < 5; j++)
            {
              fprintf(outfile, "%c", a85block[j]);
              count++;
              if (count == 40)
              {
                fprintf(outfile, "\n");
                count = 0;
              }
            }
          }

          a85count = 0;
          a85tuple = 0;
         break;

        default:
         break;
      }
    }
  } while (s.avail_out == 0);

  if (finish)
  {
    DBG(DBG_info, "finish\n");
    if (a85count > 0)
    {
      a85count++;
      for (j = 0; j <= a85count; j++)
      {
        a85block[j] = a85tuple % 85 + '!';
        a85tuple /= 85;
      }
      /* Reverse order */
      for (j--; j > 0; j--)
      {
        if (count == 40)
        {
          fprintf(outfile, "\n");
          count = 0;
        }
        fprintf(outfile, "%c", a85block[j]);
        count++;
      }
    }

    /* ASCII85 EOD marker + newline*/
    if (count + 2 > 40)
    {
      fprintf(outfile, "\n");
    }
    fprintf(outfile, "~>\n");
    deflateEnd(&s);
    free(cbuf);
    cbuf = NULL;
    init = 0;
    a85tuple = 0;
    a85count = 0;
    cbuflen = 0;
    linelen = 0;
    count = 0;
  }

 return 0;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Utility function for the PostScript output */
static int xsane_write_compressed_a85(FILE *outfile, unsigned char *line, int len, int finish)
{
  static unsigned char *cbuf = NULL;
  static int cbuflen = 0;
  static int linelen = 0;
  int i, j;
  int outlen;
  static int a85count = 0;
  static guint32 a85tuple = 0;
  static unsigned char a85block[6] = {0, 0, 0, 0, 0, 0};
  static int count = 0;

  DBG(DBG_proc, "xsane_write_compressed_a85\n");

    cbuf = line;
    outlen = len;

    /* ASCII85 (base 85) encoding */
    for (i = 0; i < outlen; i++)
    {
      switch (a85count)
      {
        case 0:
          a85tuple |= (cbuf[i] << 24);
          a85count++;
         break;

        case 1:
          a85tuple |= (cbuf[i] << 16);
          a85count++;
         break;

        case 2:
          a85tuple |= (cbuf[i] << 8);
          a85count++;
         break;

        case 3:
          a85tuple |= (cbuf[i] << 0);

          if (count == 40)
          {
            fprintf(outfile, "\n");
            count = 0;
          }

          if (a85tuple == 0)
          {
            fprintf(outfile, "z");
            count++;
          }
          else
          {
            /* The ASCII chars must be written in reverse order, hence -> a85block[4-j] */
            for (j = 0; j < 5; j++)
            {
              a85block[4-j] = a85tuple % 85 + '!';
              a85tuple /= 85;
            }

            for (j = 0; j < 5; j++)
            {
              fprintf(outfile, "%c", a85block[j]);
              count++;
              if (count == 40)
              {
                fprintf(outfile, "\n");
                count = 0;
              }
            }
          }

          a85count = 0;
          a85tuple = 0;
         break;

        default:
         break;
      }
    }

  if (finish)
  {
    DBG(DBG_info, "finish\n");
    if (a85count > 0)
    {
      a85count++;
      for (j = 0; j <= a85count; j++)
      {
        a85block[j] = a85tuple % 85 + '!';
        a85tuple /= 85;
      }
      /* Reverse order */
      for (j--; j > 0; j--)
      {
        if (count == 40)
        {
          fprintf(outfile, "\n");
          count = 0;
        }
        fprintf(outfile, "%c", a85block[j]);
        count++;
      }
    }

    /* ASCII85 EOD marker + newline*/
    if (count + 2 > 40)
    {
      fprintf(outfile, "\n");
    }
    fprintf(outfile, "~>\n");
    a85tuple = 0;
    a85count = 0;
    cbuflen = 0;
    linelen = 0;
    count = 0;
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
static int xsane_write_CSA(FILE *outfile, char *input_profile, int intent)
{
 cmsHPROFILE hProfile;
 size_t n;
 char* buffer;

  hProfile = cmsOpenProfileFromFile(input_profile, "r");
  if (!hProfile)
  {
    return -1;
  }

  n = cmsGetPostScriptCSA(hProfile, intent, NULL, 0);
  if (n == 0)
  {
    return -2;
  }

  buffer = (char*) malloc(n + 1);
  if (!buffer)
  {
    return -3;
  }

  cmsGetPostScriptCSA(hProfile, intent, buffer, n);
  buffer[n] = 0;

  fprintf(outfile, "%s", buffer);
  fprintf(outfile, "setcolorspace\n");

  free(buffer);
  cmsCloseProfile(hProfile);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_write_CRD(FILE *outfile, char *output_profile, int intent, int cms_bpc)
{
 cmsHPROFILE hProfile;
 size_t n;
 char* buffer;
 DWORD flags = cmsFLAGS_NODEFAULTRESOURCEDEF;

  hProfile = cmsOpenProfileFromFile(output_profile, "r");
  if (!hProfile)
  {
    return -1;
  }

  if (cms_bpc)
  {
    flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
  }

  n = cmsGetPostScriptCRDEx(hProfile, intent, flags, NULL, 0);
  if (n == 0)
  {
    return -2;
  }

  buffer = (char*) malloc(n + 1);
  if (!buffer)
  {
    return -3;
  }

  cmsGetPostScriptCRDEx(hProfile, intent, flags, buffer, n);
  buffer[n] = 0;

  fprintf(outfile, "%s", buffer);
  fprintf(outfile, "setcolorrendering\n");

  free(buffer);
  cmsCloseProfile(hProfile);

 return 0;
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_ps_pdf_bw(FILE *outfile, FILE *imagefile, Image_info *image_info, int ascii85decode, int flatedecode, GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y;
 int bytes_per_line = (image_info->image_width+7)/8;
 int ret = 0;
 unsigned char *line;

  DBG(DBG_proc, "xsane_save_ps_pdf_bw\n");

  *cancel_save = 0;

  line = (unsigned char *) malloc(bytes_per_line);

  if (line == NULL)
  {
    char buf[TEXTBUFSIZE];
      
    snprintf(buf, sizeof(buf), "%s malloc failed", ERR_DURING_SAVE);
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
   return (*cancel_save);
  }

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    for (x = 0; x < bytes_per_line; x++)
    {
      line[x] = fgetc(imagefile) ^ 255;
    }

    if (ascii85decode)
    {
#ifdef HAVE_LIBZ
      if (flatedecode)
      {
        ret = xsane_write_compressed_a85_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
      else
#endif
      {
        ret = xsane_write_compressed_a85(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
    }
#ifdef HAVE_LIBZ
    else if (flatedecode)
    {
      ret = xsane_write_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
    }
#endif
    else
    {
      fwrite(line, bytes_per_line, 1, outfile);
      ret = 0;
    }

    if ((ret != 0) || (ferror(outfile)))
    {
     char buf[TEXTBUFSIZE];

      if (ret == 0)
      {
        snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      }
      else
      {
        snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_ZLIB);
      }

      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;

     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

  free(line);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_ps_pdf_gray(FILE *outfile, FILE *imagefile, Image_info *image_info, int ascii85decode, int flatedecode, cmsHTRANSFORM hTransform, int do_transform, GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y;
 int ret = 0;
 unsigned char *line = NULL, *linep = NULL, *line16 = NULL;
 int bytes_per_line;
 int bytes_per_line16 = 0;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *line_raw = NULL;
#endif


  DBG(DBG_proc, "xsane_save_ps_pdf_gray\n");

  *cancel_save = 0;

  if (image_info->depth > 8) /* reduce 16 bit images to 12 bit */
  {
    bytes_per_line16 = image_info->image_width * 2;

    bytes_per_line   = (image_info->image_width/2) * 3;

    if (image_info->image_width & 1)
    {
      bytes_per_line += 2;
    }

    DBG(DBG_info, "bytes_per_line16 = %d\n", bytes_per_line16);
    DBG(DBG_info, "bytes_per_line   = %d\n", bytes_per_line);
  
    line16 = (unsigned char *) malloc(bytes_per_line16);
  
    if (line16 == NULL)
    {
     char buf[TEXTBUFSIZE];
        
      snprintf(buf, sizeof(buf), "%s malloc for line16 failed", ERR_DURING_SAVE);
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     return (*cancel_save);
    }
    DBG(DBG_info, "line16 allocated\n");
  }
  else
  {
    bytes_per_line   = image_info->image_width;
    bytes_per_line16 = image_info->image_width;
    DBG(DBG_info, "bytes_per_line = %d\n", bytes_per_line);
  }

  line = (unsigned char *) malloc(bytes_per_line);

  if (line == NULL)
  {
   char buf[TEXTBUFSIZE];
      
    snprintf(buf, sizeof(buf), "%s malloc failed", ERR_DURING_SAVE);
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
   return (*cancel_save);
  }

  DBG(DBG_info, "line allocated\n");

#ifdef HAVE_LIBLCMS
  if (do_transform && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    line_raw = (unsigned char *) malloc(bytes_per_line16);

    if (line_raw == NULL)
    {
     char buf[TEXTBUFSIZE];
       
      snprintf(buf, sizeof(buf), "%s malloc for line_raw failed", ERR_DURING_SAVE);
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);

      free(line);

      if (line16)
      {
        free(line16);
      }

      *cancel_save = 1;
     return (*cancel_save);
    }

    DBG(DBG_info, "line_raw allocated\n");
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
    if (image_info->depth > 8) /* reduce 16 bit images */
    {
#if 0
     guint16 val;

      for (x = 0; x < image_info->image_width; x++)
      {
        bytes_read = fread(&val, 2, 1, imagefile);
        line[x] = val/256;
      }
#endif
#if 1
#ifdef HAVE_LIBLCMS
      if (do_transform && (hTransform != NULL))
      {
        bytes_read = fread(line_raw, 2, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, line_raw, line16, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(line16, 2, image_info->image_width, imagefile);
      }

      linep = line;

#if __BYTE_ORDER == __LITTLE_ENDIAN
      for (x = 0; x < image_info->image_width; x=x+2)
      {
        *linep++ =   line16[2*x+1]; /* pixel0 high+middle */

        if (x == image_info->image_width-1)
        {
          *linep++ = (line16[2*x+0] & 240); /* pixel0 low */
          break;
        }

        *linep++ =  (line16[2*x+0] & 240)      |  (line16[2*x+3] >> 4); /* pixel0 low | pixel1 high */
        *linep++ = ((line16[2*x+3] & 15) << 4) | ((line16[2*x+2] & 240) >> 4); /* pixel1 middle | pixel1 low */
      }
#else
      for (x = 0; x < image_info->image_width; x=x+2)
      {
        *linep++ =   line16[2*x+0]; /* pixel0 high+middle */

        if (x == image_info->image_width-1)
        {
          *linep++ = (line16[2*x+1] & 240); /* pixel0 low */
          break;
        }

        *linep++ =  (line16[2*x+1] & 240)      |  (line16[2*x+2] >> 4); /* pixel0 low | pixel1 high */
        *linep++ = ((line16[2*x+2] & 15) << 4) | ((line16[2*x+3] & 240) >> 4); /* pixel1 middle | pixel1 low */
      }
#endif
#endif
    }
    else /* 8 bits/sample */
    {
#if 0
      for (x = 0; x < image_info->image_width; x++)
      {
        line[x] = fgetc(imagefile);
      }
#endif
#ifdef HAVE_LIBLCMS
      if (do_transform && (hTransform != NULL))
      {
        bytes_read = fread(line_raw, 1, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, line_raw, line, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(line, 1, image_info->image_width, imagefile);
      }
    }

    if (ascii85decode)
    {
#ifdef HAVE_LIBZ
      if (flatedecode)
      {
        ret = xsane_write_compressed_a85_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
      else
#endif
      {
        ret = xsane_write_compressed_a85(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
    }
#ifdef HAVE_LIBZ
    else if (flatedecode)
    {
      ret = xsane_write_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
    }
#endif
    else
    {
      fwrite(line, bytes_per_line, 1, outfile);
      ret = 0;
    }

    if ((ret != 0) || (ferror(outfile)))
    {
     char buf[TEXTBUFSIZE];

      if (ret == 0)
      {
        snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      }
      else
      {
        snprintf(buf, sizeof(buf), "%s zlib error or memory allocation problem", ERR_DURING_SAVE);
      }

      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;

     break;
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (line_raw)
  {
    free(line_raw);
  }
#endif

  if (line16)
  {
    free(line16);
  }

  free(line);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_ps_pdf_color(FILE *outfile, FILE *imagefile, Image_info *image_info, int ascii85decode, int flatedecode,
                                   cmsHTRANSFORM hTransform, int do_transform,
                                   GtkProgressBar *progress_bar, int *cancel_save)
{
 int x, y;
 int ret = 0;
 unsigned char *line = NULL, *linep = NULL, *line16 = NULL;
 int bytes_per_line;
 int bytes_per_line16 = 0;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *line_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_ps_pdf_color\n");

  *cancel_save = 0;

  if (image_info->depth > 8) /* reduce 16 bit images to 12 bit */
  {
    bytes_per_line16 = image_info->image_width * 3 * 2;

    bytes_per_line   = (image_info->image_width/2) * 3 * 3;
    if (image_info->image_width & 1)
    {
      bytes_per_line += 5;
    }

    DBG(DBG_info, "bytes_per_line16 = %d\n", bytes_per_line16);
    DBG(DBG_info, "bytes_per_line   = %d\n", bytes_per_line);
  
    line16 = (unsigned char *) malloc(bytes_per_line16);
  
    if (line16 == NULL)
    {
     char buf[TEXTBUFSIZE];
        
      snprintf(buf, sizeof(buf), "%s malloc for line16 failed", ERR_DURING_SAVE);
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     return (*cancel_save);
    }
    DBG(DBG_info, "line16 allocated\n");
  }
  else
  {
    bytes_per_line   = image_info->image_width * 3;
    bytes_per_line16 = image_info->image_width * 3;
    DBG(DBG_info, "bytes_per_line = %d\n", bytes_per_line);
  }

  line = (unsigned char *) malloc(bytes_per_line);

  if (line == NULL)
  {
   char buf[TEXTBUFSIZE];
      
    snprintf(buf, sizeof(buf), "%s malloc for line failed", ERR_DURING_SAVE);
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);

    if (line16)
    {
      free(line16);
    }

    *cancel_save = 1;
   return (*cancel_save);
  }
  DBG(DBG_info, "line allocated\n");

#ifdef HAVE_LIBLCMS
  if (do_transform && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    line_raw = (unsigned char *) malloc(bytes_per_line16);

    if (line_raw == NULL)
    {
     char buf[TEXTBUFSIZE];
       
      snprintf(buf, sizeof(buf), "%s malloc for line_raw failed", ERR_DURING_SAVE);
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);

      free(line);

      if (line16)
      {
        free(line16);
      }

      *cancel_save = 1;
     return (*cancel_save);
    }

    DBG(DBG_info, "line_raw allocated\n");
  }
#endif
 
  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    linep = line;

    if (image_info->depth > 8) /* reduce 16 bit images to 12 bit */
    {
#ifdef HAVE_LIBLCMS
      if (do_transform && (hTransform != NULL))
      {
        bytes_read = fread(line_raw, 6, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, line_raw, line16, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(line16, 6, image_info->image_width, imagefile);
      }

#if __BYTE_ORDER == __LITTLE_ENDIAN
      for (x = 0; x < image_info->image_width; x=x+2)
      {
        *linep++ =   line16[6*x+1]; /* red high+middle */
        *linep++ =  (line16[6*x+0] & 240)      |  (line16[6*x+3] >> 4); /* red low | green high */
        *linep++ = ((line16[6*x+3] & 15) << 4) | ((line16[6*x+2] & 240) >> 4); /* green middle | green low */

        *linep++ =  line16[6*x+5]; /* blue high+middle */

        if (x == image_info->image_width-1)
        {
          *linep++ = (line16[6*x+4] & 240); /* blue low */
          break;
        }

        *linep++ =  (line16[6*x+4] & 240)      |  (line16[6*x+7] >> 4); /* blue low | red high */
        *linep++ = ((line16[6*x+7] & 15) << 4) | ((line16[6*x+6] & 240) >> 4); /* red middle | red low */

        *linep++ =   line16[6*x+9]; /* green high+middle */
        *linep++ =  (line16[6*x+8] & 240)       |  (line16[6*x+11] >> 4); /* green low | blue high */
        *linep++ = ((line16[6*x+11] & 15) << 4) | ((line16[6*x+10] & 240) >> 4); /* blue middle | blue low */
      }
#else
      for (x = 0; x < image_info->image_width; x=x+2)
      {
        *linep++ =   line16[6*x+0]; /* red high+middle */
        *linep++ =  (line16[6*x+1] & 240)      |  (line16[6*x+2] >> 4); /* red low | green high */
        *linep++ = ((line16[6*x+2] & 15) << 4) | ((line16[6*x+3] & 240) >> 4); /* green middle | green low */

        *linep++ =  line16[6*x+4]; /* blue high+middle */

        if (x == image_info->image_width-1)
        {
          *linep++ = (line16[6*x+5] & 240); /* blue low */
          break;
        }

        *linep++ =  (line16[6*x+5] & 240)      |  (line16[6*x+6] >> 4); /* blue low | red high */
        *linep++ = ((line16[6*x+6] & 15) << 4) | ((line16[6*x+7] & 240) >> 4); /* red middle | red low */

        *linep++ =   line16[6*x+8]; /* green high+middle */
        *linep++ =  (line16[6*x+9] & 240)       |  (line16[6*x+10] >> 4); /* green low | blue high */
        *linep++ = ((line16[6*x+10] & 15) << 4) | ((line16[6*x+11] & 240) >> 4); /* blue middle | blue low */
      }
#endif
    }
    else /* 8 bits/sample */
    {
#ifdef HAVE_LIBLCMS
      if (do_transform && (hTransform != NULL))
      {
        bytes_read = fread(line_raw, 3, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, line_raw, line, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(line, 3, image_info->image_width, imagefile);
      }
    }

    if (ascii85decode)
    {
#ifdef HAVE_LIBZ
      if (flatedecode)
      {
        ret = xsane_write_compressed_a85_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
      else
#endif
      {
        ret = xsane_write_compressed_a85(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
      }
    }
#ifdef HAVE_LIBZ
    else if (flatedecode)
    {
      ret = xsane_write_flatedecode(outfile, line, bytes_per_line, (y == image_info->image_height - 1));
    }
#endif
    else
    {
      fwrite(line, bytes_per_line, 1, outfile);
      ret = 0;
    }

    if ((ret != 0) || (ferror(outfile)))
    {
     char buf[TEXTBUFSIZE];

      if (ret == 0)
      {
        snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      }
      else
      {
        snprintf(buf, sizeof(buf), "%s zlib error or memory allocation problem", ERR_DURING_SAVE);
      }

      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;

     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (line_raw)
  {
    free(line_raw);
  }
#endif

  if (line16)
  {
    free(line16);
  }

  free(line);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_ps_page(FILE *outfile, int page,
                       FILE *imagefile, Image_info *image_info, float width, float height,
                       int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                       int flatedecode,
                       cmsHTRANSFORM hTransform, int apply_ICM_profile, int embed_CSA, char *CSA_profile, int intent,
                       GtkProgressBar *progress_bar, int *cancel_save)
{
 int degree, position_left, position_bottom, box_left, box_bottom, box_right, box_top;
 int left, bottom;

  DBG(DBG_proc, "xsane_save_ps_page\n");

  switch (paper_orientation)
  {
    default:
    case 0: /* top left portrait */
      left   = 0.0;
      bottom = paper_height - height;
     break;

    case 1: /* top right portrait */
      left   = paper_width  - width;
      bottom = paper_height - height;
     break;

    case 2: /* bottom right portrait */
      left   = paper_width - width;
      bottom = 0.0;
     break;

    case 3: /* bottom left portrait */
      left   = 0.0;
      bottom = 0.0;
     break;

    case 4: /* center portrait */
      left   = paper_width  / 2.0 - width  / 2.0;
      bottom = paper_height / 2.0 - height / 2.0;
     break;


    case 8: /* top left landscape */
      left   = 0.0;
      bottom = paper_width - height;
     break;

    case 9: /* top right landscape */
      left   = paper_height - width;
      bottom = paper_width  - height;
     break;

    case 10: /* bottom right landscape */
      left   = paper_height - width;
      bottom = 0.0;
     break;

    case 11: /* bottom left landscape */
      left   = 0.0;
      bottom = 0.0;
     break;

    case 12: /* center landscape */
      left   = paper_height / 2.0 - width  / 2.0;
      bottom = paper_width  / 2.0 - height / 2.0;
     break;
  }


  if (paper_orientation >= 8) /* rotate with 90 degrees - landscape mode */
  {
    degree = 90;
    position_left   = left   + paper_bottom_margin;
    position_bottom = bottom - paper_width - paper_left_margin;
    box_left        = paper_width - paper_left_margin - bottom - height;
    box_bottom      = left   + paper_bottom_margin;
    box_right       = box_left   + ceil(height);
    box_top         = box_bottom + ceil(width);
  }
  else /* do not rotate, portrait mode */
  {
    degree = 0;
    position_left   = left   + paper_left_margin;
    position_bottom = bottom + paper_bottom_margin;
    box_left        = left   + paper_left_margin;
    box_bottom      = bottom + paper_bottom_margin;
    box_right       = box_left   + ceil(width);
    box_top         = box_bottom + ceil(height);
  }

  fprintf(outfile, "\n");
  fprintf(outfile, "%%%%Page: %d %d\n", page, page);
  fprintf(outfile, "%%%%PageBoundingBox: %d %d %d %d\n", box_left, box_bottom, box_right, box_top);

#ifdef HAVE_LIBLCMS
  if ((apply_ICM_profile) && (embed_CSA))
  {
    xsane_write_CSA(outfile, CSA_profile, intent); /* write scanner profile to ps file */
  }
  else
#endif
  {
    if (image_info->channels == 1) /* lineart, halftone, grayscale */
    {
      fprintf(outfile, "/DeviceGray setcolorspace\n");
    }
    else
    {
      fprintf(outfile, "/DeviceRGB setcolorspace\n");
    }
  }

  xsane_save_ps_create_image_header(outfile, image_info, width, height,
                                    degree, position_left, position_bottom,
                                    box_left, box_bottom, box_right, box_top,
	                            flatedecode);

  if (image_info->channels == 1) /* lineart, halftone, grayscale */
  {
    if (image_info->depth == 1) /* lineart, halftone */
    {
      xsane_save_ps_pdf_bw(outfile, imagefile, image_info, TRUE, flatedecode, progress_bar, cancel_save);
    }
    else /* grayscale */
    {
      xsane_save_ps_pdf_gray(outfile, imagefile, image_info, TRUE, flatedecode, hTransform, apply_ICM_profile && (!embed_CSA), progress_bar, cancel_save);
    }
  }
  else /* color RGB */
  {
    xsane_save_ps_pdf_color(outfile, imagefile, image_info, TRUE, flatedecode, hTransform, apply_ICM_profile && (!embed_CSA), progress_bar, cancel_save);
  }

  xsane_save_ps_create_page_trailer(outfile);

  if (ferror(outfile))
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_ps(FILE *outfile, FILE *imagefile, Image_info *image_info, float width, float height,
                  int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                  int flatedecode,
		  cmsHTRANSFORM hTransform, int apply_ICM_profile, int embed_CSA, char *CSA_profile,
                  int embed_CRD, char *CRD_profile, int cms_bpc, int intent,
                  GtkProgressBar *progress_bar, int *cancel_save)
{
  DBG(DBG_proc, "xsane_save_ps\n");

  *cancel_save = 0;

  xsane_save_ps_create_document_header(outfile, 1 /* pages */, paper_left_margin, paper_bottom_margin, paper_width, paper_height, paper_orientation, flatedecode);

#ifdef HAVE_LIBLCMS
  if ((apply_ICM_profile) && (embed_CRD))
  {
      xsane_write_CRD(outfile, CRD_profile, intent, cms_bpc); /* write printer profile to ps file */
  }
#endif



  xsane_save_ps_page(outfile, 1 /* page */, 
                     imagefile, image_info, width, height,
                     paper_left_margin, paper_bottom_margin, paper_width, paper_height, paper_orientation,
                     flatedecode,
		     hTransform, apply_ICM_profile, embed_CSA, CSA_profile, intent,
                     progress_bar, cancel_save);

  xsane_save_ps_create_document_trailer(outfile, 0 /* we defined pages at beginning */);

  if (ferror(outfile))
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_embed_pdf_icm_profile(FILE *outfile, struct pdf_xref *xref, char *icm_filename, int flatedecode, int icc_object)
{
 FILE *icm_profile;
 size_t size, embed_len;
 unsigned char *embed_buffer;
 int ret;

  DBG(DBG_proc, "xsane_embed_pdf_icm_profile(%s)\n", icm_filename);

  icm_profile = fopen(icm_filename, "rb");
  if (icm_profile == NULL)
  {
    DBG(DBG_error, "Could not open ICM profile \"%s\" for reading\n", icm_filename);
   return -1;
  }

  fseek(icm_profile, 0, SEEK_END);
  size = ftell(icm_profile);
  fseek(icm_profile, 0, SEEK_SET);
  
  embed_buffer = malloc(size + 1);
  if (embed_buffer)
  {
    xref->obj[icc_object] = ftell(outfile);
    fprintf(outfile, "%d 0 obj\n", icc_object);
    fprintf(outfile, "   << /N 3\n"); /* 3 channels */
    fprintf(outfile, "      /Alternate /DeviceRGB\n");
#ifdef HAVE_LIBZ
    if (flatedecode)
    {  
      fprintf(outfile, "      /Filter /FlateDecode\n");
    }
#endif

    fprintf(outfile, "      /Length             >>\n");

    /* Position of the stream length, to be written later on */
    xref->slenp = ftell(outfile) - 15;

    fprintf(outfile, "stream\n");

    /* Start of the stream data */
    xref->slen = ftell(outfile);
  
    embed_len = fread(embed_buffer, 1, size, icm_profile);
    embed_buffer[embed_len] = 0;
    fclose(icm_profile);

#ifdef HAVE_LIBZ
    if (flatedecode)
    {
      ret = xsane_write_flatedecode(outfile, embed_buffer, size, TRUE); 
    }
    else
#endif
    {
      fwrite(embed_buffer, size, 1, outfile);
      ret = 0;
    }
  
    /* Go back and write the length of the stream */
    xref->slen = ftell(outfile) - xref->slen;
    fseek(outfile, xref->slenp, SEEK_SET);
    fprintf(outfile, "%lu", xref->slen);
    fseek(outfile, 0L, SEEK_END);

    fprintf(outfile, "endstream\n");
    fprintf(outfile, "endobj\n");
    fprintf(outfile, "\n");

    free(embed_buffer);
  }
  else
  {
    DBG(DBG_info, "Embedding ICM profile \"%s\" to PDF: no mem\n", icm_filename);
    fclose(icm_profile);
   return -2;
  }


  DBG(DBG_info, "Embedding ICM profile \"%s\" to PDF file retuned with status %d\n", icm_filename, ret);
 return ret;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pdf_create_document_header(FILE *outfile, struct pdf_xref *xref, int pages, int flatedecode)
{
 int i;

  DBG(DBG_proc, "xsane_save_pdf_create_document_header\n");

  fprintf(outfile, "%%PDF-1.4\n");
  fprintf(outfile, "\n");
  xref->obj[1] = ftell(outfile);
  fprintf(outfile, "1 0 obj\n");
  fprintf(outfile, "   << /Type /Catalog\n");
  fprintf(outfile, "      /Outlines 2 0 R\n");
  fprintf(outfile, "      /Pages 3 0 R\n");
  fprintf(outfile, "   >>\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");
  xref->obj[2] = ftell(outfile);
  fprintf(outfile, "2 0 obj\n");
  fprintf(outfile, "   << /Type /Outlines\n");
  fprintf(outfile, "      /Count 0\n");
  fprintf(outfile, "   >>\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");
  xref->obj[3] = ftell(outfile);
  fprintf(outfile, "3 0 obj\n");
  fprintf(outfile, "   << /Type /Pages\n");
  fprintf(outfile, "      /Kids [\n");
  for (i=0; i < pages; i++)
  {
    fprintf(outfile, "             %d 0 R\n", i * 2 + 6);
  }
  fprintf(outfile, "            ]\n");
  fprintf(outfile, "      /Count %d\n", pages);
  fprintf(outfile, "   >>\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");

  xref->obj[4] = 0;
  xref->obj[5] = 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* page = [1 .. pages] */
static void xsane_save_pdf_create_page_header(FILE *outfile, struct pdf_xref *xref, int page,
                                              Image_info *image_info,
                                              float width, float height,
                                              int paper_left_margin, int paper_bottom_margin,
                                              int paper_width, int paper_height,
                                              int paper_orientation,
                                              int flatedecode, int icc_object,
                                              GtkProgressBar *progress_bar)
{
 int position_left, position_bottom, box_left, box_bottom, box_right, box_top, depth;
 int left, bottom;
 float rad;

  DBG(DBG_proc, "xsane_save_pdf_create_page_header\n");

  switch (paper_orientation)
  {
    default:
    case 0: /* top left portrait */
      left   = 0.0;
      bottom = paper_height - height;
     break;

    case 1: /* top right portrait */
      left   = paper_width  - width;
      bottom = paper_height - height;
     break;

    case 2: /* bottom right portrait */
      left   = paper_width - width;
      bottom = 0.0;
     break;

    case 3: /* bottom left portrait */
      left   = 0.0;
      bottom = 0.0;
     break;

    case 4: /* center portrait */
      left   = paper_width  / 2.0 - width  / 2.0;
      bottom = paper_height / 2.0 - height / 2.0;
     break;


    case 8: /* top left landscape */
      left   = 0.0;
      bottom = paper_width - height;
     break;

    case 9: /* top right landscape */
      left   = paper_height - width;
      bottom = paper_width  - height;
     break;

    case 10: /* bottom right landscape */
      left   = paper_height - width;
      bottom = 0.0;
     break;

    case 11: /* bottom left landscape */
      left   = 0.0;
      bottom = 0.0;
     break;

    case 12: /* center landscape */
      left   = paper_height / 2.0 - width  / 2.0;
      bottom = paper_width  / 2.0 - height / 2.0;
     break;
  }


  if (paper_orientation >= 8) /* rotate with 90 degrees - landscape mode */
  {
    rad = -M_PI_2; /* pi / 2 */
    position_left   = left   + paper_bottom_margin;
    position_bottom = bottom - paper_width - paper_left_margin;
    box_left        = paper_width - paper_left_margin - bottom - height;
    box_bottom      = left   + paper_bottom_margin;
    box_right       = box_left   + ceil(height);
    box_top         = box_bottom + ceil(width);
  }
  else /* do not rotate, portrait mode */
  {
    rad = 0;
    position_left   = left   + paper_left_margin;
    position_bottom = bottom + paper_bottom_margin;
    box_left        = left   + paper_left_margin;
    box_bottom      = bottom + paper_bottom_margin;
    box_right       = box_left   + ceil(width);
    box_top         = box_bottom + ceil(height);
  }

  depth = image_info->depth;

  if (depth > 8) /* PDF does not support 16bits/sample in a standard image */
  {
    depth = 8;
  }

  xref->obj[page * 2 + 4] = ftell(outfile);
  fprintf(outfile, "%d 0 obj\n", page * 2 + 4);
  fprintf(outfile, "    << /Type /Page\n");
  fprintf(outfile, "       /Parent 3 0 R\n");
  fprintf(outfile, "       /MediaBox [%d %d %d %d]\n", box_left, box_bottom, box_right, box_top);
  fprintf(outfile, "       /Contents %d 0 R\n", page * 2 + 5);
  fprintf(outfile, "       /Resources << /ProcSet %d 0 R >>\n", page * 2 + 6);
  fprintf(outfile, "    >>\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");

  /* Offset of object 5, for xref */
  xref->obj[page * 2 + 5] = ftell(outfile);

  fprintf(outfile, "%d 0 obj\n", page * 2 + 5);
  fprintf(outfile, "    << /Length             >>\n");

  /* Position of the stream length, to be written later on */
  xref->slenp = ftell(outfile) - 15;

  fprintf(outfile, "stream\n");

  /* Start of the stream data */
  xref->slen = ftell(outfile);

  fprintf(outfile, "q\n");
  fprintf(outfile, "1 0 0 1 %d %d cm\n", position_left, position_bottom); /* translate */
  fprintf(outfile, "%f %f -%f %f 0 0 cm\n", cos(rad), sin(rad), sin(rad), cos(rad)); /* rotate */
  fprintf(outfile, "%f 0 0 %f 0 0 cm\n", width, height); /* scale */
  fprintf(outfile, "BI\n");
  fprintf(outfile, "  /W %d\n", image_info->image_width);
  fprintf(outfile, "  /H %d\n", image_info->image_height);

  if ((icc_object) && (image_info->depth != 1))
  {
    fprintf(outfile, "  /ColorSpace [/ICCBased %d 0 R]\n", icc_object);
  }

  if (image_info->channels == 3) /* what about RGBA here ? */
  {
    if (icc_object == 0)
    {
      fprintf(outfile, "  /CS /RGB\n");
    }
    fprintf(outfile, "  /BPC %d\n", depth);
  }
  else if (image_info->depth == 1) /* BW */
  {
    fprintf(outfile, "  /CS /G\n");
    fprintf(outfile, "  /BPC 1\n");
  }
  else /* gray */
  {
    if (icc_object == 0)
    {
      fprintf(outfile, "  /CS /G\n");
    }
    fprintf(outfile, "  /BPC %d\n", depth);
  }

#ifdef HAVE_LIBZ
  if (flatedecode)
  {  
    fprintf(outfile, "  /F /FlateDecode\n");
  }
#endif

  fprintf(outfile, "ID\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_save_pdf_create_document_trailer(FILE *outfile, struct pdf_xref *xref, int pages)
{
 struct tm *t;
 time_t tt;
 int i;

  /* PDF document trailer */

  /* Offset of object 6, for xref */
  xref->obj[pages * 2 + 6] = ftell(outfile);

  fprintf(outfile, "%d 0 obj\n", pages * 2 + 6);
  fprintf(outfile, "    [/PDF]\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");

  /* Offset of object 7, for xref */
  xref->obj[pages * 2 + 7] = ftell(outfile);

  fprintf(outfile, "%d 0 obj\n", pages * 2 + 7);
  fprintf(outfile, "   << /Title (XSane scanned image)\n");
  fprintf(outfile, "      /Creator (XSane version %s (sane %d.%d) - by Oliver Rauch)\n",
	  VERSION,
	  SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
	  SANE_VERSION_MINOR(xsane.sane_backend_versioncode));
  fprintf(outfile, "      /Producer (XSane %s)\n", VERSION);

  tt = time(NULL);
  t = gmtime(&tt);

  fprintf(outfile, "      /CreationDate (D:%04d%02d%02d%02d%02d%02d+00'00')\n",
	  1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
  fprintf(outfile, "   >>\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");

  /* Offset of xref, for startxref below */
  xref->xref = ftell(outfile);

  fprintf(outfile, "xref\n");
  fprintf(outfile, "0 %d\n", pages * 2 + 8);
  fprintf(outfile, "0000000000 65535 f \n");

  for (i=1; i <= pages * 2 + 7; i++)
  {
    if (xref->obj[i] > 0)
    {
      fprintf(outfile, "%010lu 00000 n \n", xref->obj[i]);
    }
    else
    {
      fprintf(outfile, "%010lu 00000 f \n", 0L);
    }
  }

  fprintf(outfile, "\n");
  fprintf(outfile, "trailer\n");
  fprintf(outfile, "    << /Size %d\n", pages * 2 + 8);
  fprintf(outfile, "       /Root 1 0 R\n");
  fprintf(outfile, "       /Info %d 0 R\n", pages * 2 + 7);
  fprintf(outfile, "    >>\n");
  fprintf(outfile, "startxref\n");
  fprintf(outfile, "%lu\n", xref->xref);
  fprintf(outfile, "%%%%EOF\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_save_pdf_create_page_trailer(FILE *outfile, struct pdf_xref *xref)
{
  /* PDF page trailer */
  fprintf(outfile, "EI\n");
  fprintf(outfile, "Q\n");

  /* Go back and write the length of the stream */
  xref->slen = ftell(outfile) - xref->slen; /* we had a "-1" at the end but I do not understand the reason for -1, without looks better */
  fseek(outfile, xref->slenp, SEEK_SET);
  fprintf(outfile, "%lu", xref->slen);
  fseek(outfile, 0L, SEEK_END);

  fprintf(outfile, "endstream\n");
  fprintf(outfile, "endobj\n");
  fprintf(outfile, "\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_pdf_page(FILE *outfile, struct pdf_xref *xref, int page,
                        FILE *imagefile, Image_info *image_info, float width, float height,
                        int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
			int flatedecode,
                        cmsHTRANSFORM hTransform, int do_transform, int icc_object,
                        GtkProgressBar *progress_bar, int *cancel_save)
{

  DBG(DBG_proc, "xsane_save_pdf_page\n");

  xsane_save_pdf_create_page_header(outfile, xref, page,
                                    image_info, width, height,
			            paper_left_margin, paper_bottom_margin, paper_width, paper_height, paper_orientation,
                                    flatedecode, icc_object,
			            progress_bar);

  if (image_info->channels == 1) /* lineart, halftone, grayscale */
  {
    if (image_info->depth == 1) /* lineart, halftone */
    {
      xsane_save_ps_pdf_bw(outfile, imagefile, image_info, FALSE, flatedecode, progress_bar, cancel_save);
    }
    else /* grayscale */
    {
      xsane_save_ps_pdf_gray(outfile, imagefile, image_info, FALSE, flatedecode, hTransform, do_transform, progress_bar, cancel_save);
    }
  }
  else /* color RGB */
  {
    xsane_save_ps_pdf_color(outfile, imagefile, image_info, FALSE, flatedecode, hTransform, do_transform, progress_bar, cancel_save);
  }

  xsane_save_pdf_create_page_trailer(outfile, xref);

  if (ferror(outfile))
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_pdf(FILE *outfile, FILE *imagefile, Image_info *image_info, float width, float height,
                   int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                   int flatedecode,
                   cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                   GtkProgressBar *progress_bar, int *cancel_save)
{
 struct pdf_xref xref;
 int icc_object = 0;

  DBG(DBG_proc, "xsane_save_pdf\n");

  *cancel_save = 0;

  xsane_save_pdf_create_document_header(outfile, &xref, 1, flatedecode);

  if (apply_ICM_profile && (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE))
  {
    icc_object = 4;
    xsane_embed_pdf_icm_profile(outfile, &xref, image_info->icm_profile, flatedecode, icc_object);
  }

  xsane_save_pdf_page(outfile, &xref, 1,
                   imagefile, image_info, width, height,
                   paper_left_margin, paper_bottom_margin, paper_width, paper_height, paper_orientation,
                   flatedecode,
                   hTransform, apply_ICM_profile && ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)) /* do_transform */, icc_object,
                   progress_bar, cancel_save);

  xsane_save_pdf_create_document_trailer(outfile, &xref, 1);

  if (ferror(outfile))
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
    DBG(DBG_error, "%s\n", buf);
    xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    *cancel_save = 1;
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBJPEG
typedef struct
{
  struct jpeg_error_mgr pub;/* "public" fields */
  int *cancel_save; 
} xsane_jpeg_error_mgr;

typedef xsane_jpeg_error_mgr *xsane_jpeg_error_mgr_ptr;

static void xsane_jpeg_error_exit(j_common_ptr cinfo)
{
 char buf[TEXTBUFSIZE];

  /* cinfo->err points to a xsane_jpeg_error_mgr struct */
  xsane_jpeg_error_mgr_ptr xsane_jpeg_error_mgr_data = (xsane_jpeg_error_mgr_ptr) cinfo->err;


  if (!*xsane_jpeg_error_mgr_data->cancel_save)
  {
    /* output original error message */
    (*cinfo->err->output_message) (cinfo);
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBJPEG);
    xsane_back_gtk_error(buf, TRUE);
  }

  *xsane_jpeg_error_mgr_data->cancel_save = 1;
}

/* ---------------------------------------------------------- */

#ifdef HAVE_LIBLCMS
static void xsane_jpeg_write_icm_profile(j_compress_ptr cinfo_ptr, const JOCTET *icm_data_ptr, unsigned int icm_data_len)
{
#define ICM_MARKER  (JPEG_APP0 + 2)     /* JPEG marker code for ICM */
#define ICM_OVERHEAD_LEN  14            /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533      /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICM_OVERHEAD_LEN)

  unsigned int num_markers;     /* total number of markers we'll write */
  int cur_marker = 1;           /* per spec, counting starts at 1 */
  unsigned int length;          /* number of bytes to write in this marker */

  /* Calculate the number of markers we'll need, rounding up of course */
  num_markers = icm_data_len / MAX_DATA_BYTES_IN_MARKER;
  if (num_markers * MAX_DATA_BYTES_IN_MARKER != icm_data_len)
  {
    num_markers++;
  }

  while (icm_data_len > 0)
  {
    length = icm_data_len; /* length of profile to put in this marker */
    if (length > MAX_DATA_BYTES_IN_MARKER)
    {
      length = MAX_DATA_BYTES_IN_MARKER;
    }
    icm_data_len -= length;

    /* Write the JPEG marker header (APP2 code and marker length) */
    jpeg_write_m_header(cinfo_ptr, ICM_MARKER, (unsigned int) (length + ICM_OVERHEAD_LEN));

    /* Write the marker identifying string "ICC_PROFILE" (null-terminated).
     * We code it in this less-than-transparent way so that the code works
     * even if the local character set is not ASCII.
     */
    jpeg_write_m_byte(cinfo_ptr, 0x49);
    jpeg_write_m_byte(cinfo_ptr, 0x43);
    jpeg_write_m_byte(cinfo_ptr, 0x43);
    jpeg_write_m_byte(cinfo_ptr, 0x5F);
    jpeg_write_m_byte(cinfo_ptr, 0x50);
    jpeg_write_m_byte(cinfo_ptr, 0x52);
    jpeg_write_m_byte(cinfo_ptr, 0x4F);
    jpeg_write_m_byte(cinfo_ptr, 0x46);
    jpeg_write_m_byte(cinfo_ptr, 0x49);
    jpeg_write_m_byte(cinfo_ptr, 0x4C);
    jpeg_write_m_byte(cinfo_ptr, 0x45);
    jpeg_write_m_byte(cinfo_ptr, 0x0);

    /* Add the sequencing info */
    jpeg_write_m_byte(cinfo_ptr, cur_marker);
    jpeg_write_m_byte(cinfo_ptr, (int) num_markers);

    /* Add the profile data */
    while (length--)
    {
      jpeg_write_m_byte(cinfo_ptr, *icm_data_ptr);
      icm_data_ptr++;
    }
    cur_marker++;
  }
}

/* ---------------------------------------------------------- */

static void xsane_jpeg_embed_scanner_icm_profile(j_compress_ptr cinfo_ptr, const char *icm_filename)
{
 FILE *icm_profile;
 size_t size, embed_len;
 LPBYTE embed_buffer;

  DBG(DBG_proc, "xsane_jpeg_embed_scanner_icm_profile(%s)\n", icm_filename);

  icm_profile = fopen(icm_filename, "rb");
  if (icm_profile == NULL)
  {
    return;
  }

  fseek(icm_profile, 0, SEEK_END);
  size = ftell(icm_profile);
  fseek(icm_profile, 0, SEEK_SET);

  embed_buffer = (LPBYTE) malloc(size + 1);
  if (embed_buffer)
  {
    embed_len = fread(embed_buffer, 1, size, icm_profile);
    fclose(icm_profile);
    embed_buffer[embed_len] = 0;

    xsane_jpeg_write_icm_profile(cinfo_ptr, embed_buffer, embed_len);
    free(embed_buffer);

    DBG(DBG_info, "ICM profile %s has been embedded to jpeg file\n", icm_filename);
  }
}
#endif

/* ---------------------------------------------------------- */

int xsane_save_jpeg(FILE *outfile, int quality, FILE *imagefile, Image_info *image_info,
                    cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                    GtkProgressBar *progress_bar, int *cancel_save)
{
 unsigned char *data;
 char buf[TEXTBUFSIZE];
 int components = 1;
 int x,y;
 int bytespp = 1;
 struct jpeg_compress_struct cinfo;
 xsane_jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_jpeg\n");

  *cancel_save = 0;

  if (image_info->channels == 3)
  {
    components = 3;
  }

  if (image_info->depth > 8)
  {
    bytespp = 2;
  }

  data = malloc(image_info->image_width * components * bytespp);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if (apply_ICM_profile && (cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * components * bytespp);

    if (!data_raw)
    {
      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = xsane_jpeg_error_exit;
  jerr.cancel_save = cancel_save; 

  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);
  cinfo.image_width      = image_info->image_width;
  cinfo.image_height     = image_info->image_height;
  cinfo.input_components = components;
  if (image_info->channels == 3)
  {
    cinfo.in_color_space   = JCS_RGB;
  }
  else
  {
    cinfo.in_color_space   = JCS_GRAYSCALE;
  }
  jpeg_set_defaults(&cinfo);

  jpeg_set_quality(&cinfo, quality, TRUE);

  cinfo.density_unit     = 1; /* dpi */
  cinfo.X_density        = image_info->resolution_x;
  cinfo.Y_density        = image_info->resolution_y;

#if 0
  cinfo.smoothing_factor = 0.0; /* 0 .. 100 */
  cinfo.dct_method = JDCT_FLOAT; /* JDCT_ISLOW, JDCT_IFAST, JDCT_FLOAT */
#endif

  jpeg_start_compress(&cinfo, TRUE);

#ifdef HAVE_LIBLCMS
  if (apply_ICM_profile)
  {
    if (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)
    {
      xsane_jpeg_embed_scanner_icm_profile(&cinfo, image_info->icm_profile);
    }
    else if (cms_function == XSANE_CMS_FUNCTION_CONVERT_TO_WORKING_CS)
    {
      xsane_jpeg_embed_scanner_icm_profile(&cinfo, preferences.working_color_space_icm_profile);
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (image_info->depth == 1)
    {
     int byte = 0;
     int mask = 128;

      for (x = 0; x < image_info->image_width; x++)
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
    else if (image_info->depth > 8) /* jpeg does not support 16 bits/sample, so we reduce it at first */
    {
      guint16 *data16 = (guint16 *) data;
#ifdef HAVE_LIBLCMS
      if (apply_ICM_profile && (cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
      {
        bytes_read = fread(data_raw, components * 2, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(data, components * 2, image_info->image_width, imagefile);
      }

      for (x = 0; x < image_info->image_width * components; x++)
      {
        data[x] = data16[x] / 256;
      }

    }
    else /* 8 bits/sample */
    {
#ifdef HAVE_LIBLCMS
      if (apply_ICM_profile && (cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
      {
        bytes_read = fread(data_raw, components, image_info->image_width, imagefile);
        cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
      }
      else
#endif
      {
        bytes_read = fread(data, components, image_info->image_width, imagefile);
      }
    }

    row_pointer[0] = data;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);

    if (*cancel_save)
    {
      cinfo.image_height = y; /* correct image height */
      break;
    }
  }

  jpeg_finish_compress(&cinfo);

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBTIFF
#ifdef HAVE_LIBLCMS
static void xsane_tiff_embed_scanner_icm_profile(TIFF *tiffile, const char *icm_filename)
{
 FILE *icm_profile;
 size_t size;
 char *icm_profile_buffer;

  DBG(DBG_proc, "xsane_tiff_embed_scanner_icm_profile(%s)\n", icm_filename);
  if((icm_profile = fopen(icm_filename, "rb")))
  {
    fseek(icm_profile, 0, SEEK_END);
    size = ftell(icm_profile);
    fseek(icm_profile, 0, SEEK_SET);

    icm_profile_buffer = (char *) malloc(size + 1);

    if (icm_profile_buffer)
    {
      if (fread(icm_profile_buffer, 1, size, icm_profile) == size)
      {
        icm_profile_buffer[size] = 0;

        TIFFSetField(tiffile, TIFFTAG_ICCPROFILE, size, icm_profile_buffer);
      }
      else
      {
        DBG(DBG_error, "Can not read ICM profile data\n");
      }

      free(icm_profile_buffer);
    }
    else
    {
      DBG(DBG_error, "Can not get enogh memory for ICM profile\n");
    }


    fclose(icm_profile);
  }
  else
  {
    DBG(DBG_error, "Can not embed ICM profile\n");
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* pages = 0 => single page tiff, page = 0 */
/* pages > 0 => page = [1 .. pages] */
int xsane_save_tiff_page(TIFF *tiffile, int page, int pages, int quality, FILE *imagefile, Image_info *image_info,
                         cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                         GtkProgressBar *progress_bar, int *cancel_save)
{
 char *data;
 char buf[TEXTBUFSIZE];
 int y, w;
 int components;
 int compression;
 int bytes;
 struct tm *ptm;
 time_t now;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 char *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_tiff_page(%d/%d\n", page, pages);

  *cancel_save = 0;

  if (image_info->depth == 1)
  {
    compression = preferences.tiff_compression1_nr;
  }
  else if (image_info->depth == 8)
  {
    compression = preferences.tiff_compression8_nr;
  }
  else
  {
    compression = preferences.tiff_compression16_nr;
  }


  if (image_info->channels == 3)
  {
    components = 3;
  }
  else
  {
    components = 1;
  }

  if (image_info->depth <= 8)
  {
    bytes = 1;
  }
  else
  {
    bytes = 2;
  }

  data = (char *)_TIFFmalloc(image_info->image_width * components * bytes);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = (char *) malloc(image_info->image_width * components * bytes);

    if (!data_raw)
    {
      _TIFFfree(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  
  TIFFSetField(tiffile, TIFFTAG_IMAGEWIDTH, image_info->image_width);
  TIFFSetField(tiffile, TIFFTAG_IMAGELENGTH, image_info->image_height);
  TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE, image_info->depth); 
  TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL, components);
  TIFFSetField(tiffile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tiffile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tiffile, TIFFTAG_COMPRESSION, compression);
  TIFFSetField(tiffile, TIFFTAG_SOFTWARE, "xsane");

  time(&now);
  ptm = localtime(&now);
  sprintf(buf, "%04d:%02d:%02d %02d:%02d:%02d", 1900+ptm->tm_year, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  TIFFSetField(tiffile, TIFFTAG_DATETIME, buf);

  if (image_info->resolution_x > 0.0)
  {
    TIFFSetField(tiffile, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tiffile, TIFFTAG_XRESOLUTION, image_info->resolution_x);
    TIFFSetField(tiffile, TIFFTAG_YRESOLUTION, image_info->resolution_y);   
  }

  if (compression == COMPRESSION_DEFLATE)
  {
    TIFFSetField(tiffile, TIFFTAG_ZIPQUALITY, (int) preferences.tiff_zip_compression);
  }
  else if (compression == COMPRESSION_JPEG)
  {
    TIFFSetField(tiffile, TIFFTAG_JPEGQUALITY, quality);
  }

  if (image_info->channels == 3)
  {
    if (compression == COMPRESSION_JPEG)
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
      TIFFSetField(tiffile, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);  /* convert from RGB (to YCBCR) */
    }
    else /* no jpeg compression */
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    }

#ifdef HAVE_LIBLCMS
    if (apply_ICM_profile)
    {
      if (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)
      {
        xsane_tiff_embed_scanner_icm_profile(tiffile, image_info->icm_profile);
      }
      else if (cms_function == XSANE_CMS_FUNCTION_CONVERT_TO_WORKING_CS)
      {
        xsane_tiff_embed_scanner_icm_profile(tiffile, preferences.working_color_space_icm_profile);
      }
    }
#endif
  }
  else
  {
    if (image_info->depth == 1) /* lineart */
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    }
    else /* grayscale */
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK); 
      /* we have to do nothing special for jpeg! */
    }
  }

  TIFFSetField(tiffile, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiffile, -1));

  if (pages)
  {
    TIFFSetField(tiffile, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tiffile, TIFFTAG_PAGENUMBER, page, pages);
  }

  w = TIFFScanlineSize(tiffile);

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);
    
#ifdef HAVE_LIBLCMS
    if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
    {
      bytes_read = fread(data_raw, 1, w, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 1, w, imagefile);
    }

    if (TIFFWriteScanline(tiffile, data, y, 0) != 1)
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s", ERR_DURING_SAVE);
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

  if (pages)
  {
    TIFFWriteDirectory(tiffile);
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif

  _TIFFfree(data);
 return (*cancel_save);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#if defined(PNG_iCCP_SUPPORTED)
#ifdef HAVE_LIBLCMS
static void xsane_png_embed_scanner_icm_profile(png_structp png_ptr, png_infop png_info_ptr, const char *icm_filename)
{
 FILE *icm_profile;
 gchar *profile_buffer;
 size_t size;

  DBG(DBG_proc, "xsane_png_embed_scanner_icm_profile(%s)\n", icm_filename);
  icm_profile = fopen(icm_filename, "rb");

  if (icm_profile)
  {
    fseek(icm_profile, 0, SEEK_END);
    size = ftell(icm_profile);
    fseek(icm_profile, 0, SEEK_SET);

    profile_buffer = malloc(size);

    if (profile_buffer)
    {
      if (fread(profile_buffer, 1, size, icm_profile) == size)
      {
        png_set_iCCP(png_ptr, png_info_ptr, "ICC profile", 0, profile_buffer, size);
      }
      else
      {
        DBG(DBG_error, "can not read ICC profile data\n");
      }

      free(profile_buffer);
    }
    else
    {
      DBG(DBG_error, "can not allocate profile_buffer\n");
    }

    fclose(icm_profile);
  }
  else
  {
    DBG(DBG_error, "can not open ICM-profile\n");
  }
}
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
int xsane_save_png(FILE *outfile, int compression, FILE *imagefile, Image_info *image_info,
                   cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                   GtkProgressBar *progress_bar, int *cancel_save)
{
 png_structp png_ptr;
 png_infop png_info_ptr;
 png_bytep row_ptr;
 png_color_8 sig_bit;
 unsigned char *data;
 char buf[TEXTBUFSIZE];
 int colortype, components, byte_width;
 int y;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_png\n");

  *cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    return -1; /* error */
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBTIFF);
    xsane_back_gtk_error(buf, TRUE);
    return -1; /* error */
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return -1; /* error */
  }

  byte_width = image_info->image_width;

  if (image_info->channels == 4) /* RGBA */
  {
    components = 4;
    colortype = PNG_COLOR_TYPE_RGB_ALPHA;
  }
  else if (image_info->channels == 3) /* RGB */
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
  png_set_IHDR(png_ptr, png_info_ptr, image_info->image_width, image_info->image_height, image_info->depth,
               colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  if (image_info->channels >=3)
  {
    sig_bit.red   = image_info->depth;
    sig_bit.green = image_info->depth;
    sig_bit.blue  = image_info->depth;

    if (image_info->channels == 4)
    {
      sig_bit.alpha = image_info->depth;
    }

  }
  else
  {
    sig_bit.gray  = image_info->depth;

    if (image_info->depth == 1)
    {
      byte_width = (image_info->image_width+7)/8;
      png_set_invert_mono(png_ptr);
    }
  }

  png_set_sBIT(png_ptr, png_info_ptr, &sig_bit);
#if defined(PNG_pHYs_SUPPORTED)
  png_set_pHYs(png_ptr, png_info_ptr,
               image_info->resolution_x * 100.0 / 2.54,
               image_info->resolution_y * 100.0 / 2.54, PNG_RESOLUTION_METER);
#endif

#if defined(PNG_iCCP_SUPPORTED)
#ifdef HAVE_LIBLCMS
  if (apply_ICM_profile)
  {
    if (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)
    {
      xsane_png_embed_scanner_icm_profile(png_ptr, png_info_ptr, image_info->icm_profile);
    }
    else if (cms_function == XSANE_CMS_FUNCTION_CONVERT_TO_WORKING_CS)
    {
      xsane_png_embed_scanner_icm_profile(png_ptr, png_info_ptr, preferences.working_color_space_icm_profile);
    }
  }
#endif
#endif

  png_write_info(png_ptr, png_info_ptr);
  png_set_shift(png_ptr, &sig_bit);

  data = malloc(image_info->image_width * components);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * components);

    if (!data_raw)
    {
      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
      png_destroy_write_struct(&png_ptr, (png_infopp) 0);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

#ifdef HAVE_LIBLCMS
    if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
    {
      bytes_read = fread(data_raw, components, byte_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, components, byte_width, imagefile);
    }

    row_ptr = data;
    png_write_rows(png_ptr, &row_ptr, 1); /* errors are caught by test sor setjmp(...) */

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);
  png_write_end(png_ptr, png_info_ptr);
  png_destroy_write_struct(&png_ptr, (png_infopp) 0);

 return (*cancel_save);
}
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
int xsane_save_png_16(FILE *outfile, int compression, FILE *imagefile, Image_info *image_info,
                      cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                      GtkProgressBar *progress_bar, int *cancel_save)
{
 png_structp png_ptr;
 png_infop png_info_ptr;
 png_bytep row_ptr;
 png_color_8 sig_bit; /* should be 16, but then I get a warning about wrong type */
 unsigned char *data;
 char buf[TEXTBUFSIZE];
 int colortype, components;
 int y;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_png16\n");

  *cancel_save = 0;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    return -1; /* error */
  }

  png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    return -1; /* error */
  }

  if (setjmp(png_ptr->jmpbuf))
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_LIBPNG);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return -1; /* error */
  }

  if (image_info->channels == 4) /* RGBA */
  {
    components = 4;
    colortype = PNG_COLOR_TYPE_RGB_ALPHA;
  }
  else if (image_info->channels == 3) /* RGB */
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
  png_set_IHDR(png_ptr, png_info_ptr, image_info->image_width, image_info->image_height, 16,
               colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  sig_bit.red   = image_info->depth;
  sig_bit.green = image_info->depth;
  sig_bit.blue  = image_info->depth;
  sig_bit.alpha = image_info->depth;
  sig_bit.gray  = image_info->depth;

  png_set_sBIT(png_ptr, png_info_ptr, &sig_bit);

#if defined(PNG_pHYs_SUPPORTED)
  png_set_pHYs(png_ptr, png_info_ptr,
               image_info->resolution_x * 100.0 / 2.54,
               image_info->resolution_y * 100.0 / 2.54, PNG_RESOLUTION_METER);
#endif

#if defined(PNG_iCCP_SUPPORTED)
#ifdef HAVE_LIBLCMS
  if (apply_ICM_profile)
  {
    if (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)
    {
      xsane_png_embed_scanner_icm_profile(png_ptr, png_info_ptr, image_info->icm_profile);
    }
    else if (cms_function == XSANE_CMS_FUNCTION_CONVERT_TO_WORKING_CS)
    {
      xsane_png_embed_scanner_icm_profile(png_ptr, png_info_ptr, preferences.working_color_space_icm_profile);
    }
  }
#endif
#endif

  png_write_info(png_ptr, png_info_ptr);
  png_set_shift(png_ptr, &sig_bit);
  png_set_packing(png_ptr);

  data = malloc(image_info->image_width * components * 2);

  if (!data)
  {
    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
    png_destroy_write_struct(&png_ptr, (png_infopp) 0);
    return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * components * 2);

    if (!data_raw)
    {
      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
      png_destroy_write_struct(&png_ptr, (png_infopp) 0);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

#ifdef HAVE_LIBLCMS
    if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && (hTransform != NULL))
    {
      bytes_read = fread(data_raw, components * 2, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, components * 2, image_info->image_width, imagefile);
    }

#if __BYTE_ORDER == __LITTLE_ENDIAN
    /* we have to write data in network order (MSB first), so when we run on a low endian machine then we have to swap bytes */
    {
     int x;

      for (x = 0; x < image_info->image_width * components; x++)
      {
        unsigned char help;

        help = data[x*2+0];
        data[x*2+0] = data[x*2+1];
        data[x*2+1] = help;
      }
    }
#endif /* LITTLE_ENDIAN */

    row_ptr = data;
    png_write_rows(png_ptr, &row_ptr, 1);
    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);
  png_write_end(png_ptr, png_info_ptr);
  png_destroy_write_struct(&png_ptr, (png_infopp) 0);

 return (*cancel_save);
}
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_16_ascii_gray(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                        cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                        GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint16 *data;
 int count = 0;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint16 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_16_ascii_gray\n");

  *cancel_save = 0;

  data = malloc(image_info->image_width * 2);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((apply_ICM_profile) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * 2);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if ((apply_ICM_profile) && (hTransform != NULL))
    {
      bytes_read = fread(data_raw, 2, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 2, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fprintf(outfile, "%d ", data[x]);

      if (++count >= 10)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }

    fprintf(outfile, "\n");

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    count = 0;

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_16_ascii_color(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                         cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                         GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint16 *data;
 int count = 0;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint16 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_16_ascii_color\n");

  *cancel_save = 0;


  data = malloc(image_info->image_width * 6);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((apply_ICM_profile) && (hTransform != NULL))
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * 6);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if ((apply_ICM_profile) && (hTransform != NULL))
    {
      bytes_read = fread(data_raw, 6, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 6, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fprintf(outfile, "%d ", data[3*x+0]);
      fprintf(outfile, "%d ", data[3*x+1]);
      fprintf(outfile, "%d ", data[3*x+2]);

      if (++count >= 3)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    count = 0;

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_16_binary_gray(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                         cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                         GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint16 *data;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint16 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_16_binary_gray\n");

  *cancel_save = 0;

  data = malloc(image_info->image_width * 2);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * 2);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if (hTransform != NULL)
    {
      bytes_read = fread(data_raw, 2, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 2, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fputc(data[3*x+0] / 256, outfile);
      fputc(data[3*x+0] & 255, outfile);
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_16_binary_color(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                          cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                          GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint16 *data;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint16 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_16_binary_color\n");

  *cancel_save = 0;

  data = malloc(image_info->image_width * 6);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * 6);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if (hTransform != NULL)
    {
      bytes_read = fread(data_raw, 6, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 6, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fputc(data[3*x+0] / 256, outfile);
      fputc(data[3*x+0] & 255, outfile);
      fputc(data[3*x+1] / 256, outfile);
      fputc(data[3*x+1] & 255, outfile);
      fputc(data[3*x+2] / 256, outfile);
      fputc(data[3*x+2] & 255, outfile);
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_8_gray(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                 cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                 GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint8 *data;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint8 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_8_gray\n");

  *cancel_save = 0;

  data = malloc(image_info->image_width);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if (hTransform != NULL)
    {
      bytes_read = fread(data_raw, 1, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 1, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fputc(data[x], outfile);
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_8_color(FILE *outfile, FILE *imagefile, Image_info *image_info,
                                  cmsHTRANSFORM hTransform, int apply_ICM_profile,
                                  GtkProgressBar *progress_bar, int *cancel_save)
{
 int x,y;
 guint8 *data;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 guint8 *data_raw = NULL;
#endif

  DBG(DBG_proc, "xsane_save_pnm_8_color\n");

  *cancel_save = 0;

  data = malloc(image_info->image_width * 3);

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info->image_width * 3);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  for (y = 0; y < image_info->image_height; y++)
  {
#ifdef HAVE_LIBLCMS
    if (hTransform != NULL)
    {
      bytes_read = fread(data_raw, 3, image_info->image_width, imagefile);
      cmsDoTransform(hTransform, data_raw, data, image_info->image_width);
    }
    else
#endif
    {
      bytes_read = fread(data, 3, image_info->image_width, imagefile);
    }

    for (x = 0; x < image_info->image_width; x++)
    {
      fputc(data[3*x+0], outfile);
      fputc(data[3*x+1], outfile);
      fputc(data[3*x+2], outfile);
    }

    xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info->image_height);

    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      *cancel_save = 1;
     break;
    }

    if (*cancel_save)
    {
      break;
    }
  }

#ifdef HAVE_LIBLCMS
  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_save_pnm_8(FILE *outfile, FILE *imagefile, Image_info *image_info,
                            cmsHTRANSFORM hTransform, int apply_ICM_profile,
                            GtkProgressBar *progress_bar, int *cancel_save)
{
  DBG(DBG_proc, "xsane_save_pnm_8\n");

  *cancel_save = 0;

  xsane_write_pnm_header(outfile, image_info, preferences.save_pnm16_as_ascii);

  if (image_info->channels > 1)
  {
    xsane_save_pnm_8_color(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
  }
  else
  {
    xsane_save_pnm_8_gray(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_save_pnm_16(FILE *outfile, FILE *imagefile, Image_info *image_info,
                      cmsHTRANSFORM hTransform, int apply_ICM_profile,
                      GtkProgressBar *progress_bar, int *cancel_save)
{
  DBG(DBG_proc, "xsane_save_pnm_16\n");

  *cancel_save = 0;

  xsane_write_pnm_header(outfile, image_info, preferences.save_pnm16_as_ascii);

  if (image_info->channels > 1)
  {
    if (preferences.save_pnm16_as_ascii)
    {
      xsane_save_pnm_16_ascii_color(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
    }
    else
    {
      xsane_save_pnm_16_binary_color(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
    }
  }
  else
  {
    if (preferences.save_pnm16_as_ascii)
    {
      xsane_save_pnm_16_ascii_gray(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
    }
    else
    {
      xsane_save_pnm_16_binary_gray(outfile, imagefile, image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
    }
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* 0=ok, <0=error, 1=canceled */
int xsane_save_image_as_lineart(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save)
{
 FILE *outfile;
 FILE *infile;
 char buf[TEXTBUFSIZE];
 Image_info image_info;

  *cancel_save = 0;

  outfile = fopen(output_filename, "wb"); /* b = binary mode for win32 */

  if (outfile == 0)
  {
    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, output_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
   return -2;
  }

  infile = fopen(input_filename, "rb"); /* read binary (b for win32) */
  if (infile == 0)
  {
   char buf[TEXTBUFSIZE];
    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, input_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);     

    fclose(outfile);
    remove(output_filename); /* remove already created output file */
   return -1;
  }

  xsane_read_pnm_header(infile, &image_info);

  xsane_save_grayscale_image_as_lineart(outfile, infile, &image_info, progress_bar, cancel_save);

  fclose(infile);
  fclose(outfile);

  if (*cancel_save) /* remove output file if saving has been canceled */
  {
    remove(output_filename);
  }

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
 
int xsane_save_image_as_text(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save)
{
 char *arg[1000];
 char buf[TEXTBUFSIZE];
 int argnr;
 pid_t pid;
 int i;
 int pipefd[2]; /* for progress communication with gocr */
 FILE *ocr_progress = NULL;
 
  DBG(DBG_proc, "xsane_save_image_as_text\n");
 
  argnr = xsane_parse_options(preferences.ocr_command, arg);
 
  arg[argnr++] = strdup(preferences.ocr_inputfile_option);
  arg[argnr++] = strdup(input_filename);
 
  arg[argnr++] = strdup(preferences.ocr_outputfile_option);
  arg[argnr++] = strdup(output_filename);
 
  if (preferences.ocr_use_gui_pipe)
  {
    if (!pipe(pipefd)) /* success */
    {
      DBG(DBG_info, "xsane_save_image_as_text: created pipe for progress communication\n");
 
      arg[argnr++] = strdup(preferences.ocr_gui_outfd_option);
 
      snprintf(buf, sizeof(buf),"%d", pipefd[1]);
      arg[argnr++] = strdup(buf);
    }
    else
    {
      DBG(DBG_info, "xsane_save_image_as_text: could not create pipe for progress communication\n");
      pipefd[0] = 0;
      pipefd[1] = 0;
    }
  }
  else
  {
    DBG(DBG_info, "xsane_save_image_as_text: no pipe for progress communication requested\n");
    pipefd[0] = 0;
    pipefd[1] = 0;
  }
 
  arg[argnr] = 0;

#ifndef HAVE_OS2_H 
  pid = fork();
 
  if (pid == 0) /* new process */
  {
   FILE *ipc_file = NULL;
 
    if (xsane.ipc_pipefd[0]) /* did we create the progress pipe? */
    {
      close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
      ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
    }
 
    if (pipefd[0]) /* did we create the progress pipe? */
    {
      close(pipefd[0]); /* close reading end of pipe */
    }
 
    DBG(DBG_info, "trying to change user id for new subprocess:\n");
    DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
    setuid(getuid());
    DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());
 
 
    execvp(arg[0], arg); /* does not return if successfully */
    DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_OCR_CMD, preferences.ocr_command);
 
    /* send error message via IPC pipe to parent process */
    if (ipc_file)
    {
      fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_OCR_CMD, preferences.ocr_command, strerror(errno));
      fflush(ipc_file); /* make sure message is displayed */
      fclose(ipc_file);
    }
 
    _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
  }

#else
  pid = spawnvp(P_NOWAIT, arg[0], arg);
  if (pid == -1)
  {
   DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_OCR_CMD, preferences.ocr_command);
  }
#endif
 
  if (pipefd[1])
  {
    close(pipefd[1]); /* close writing end of pipe */
    ocr_progress = fdopen(pipefd[0], "r"); /* open reading end of pipe as file */
  }
 
  for (i=0; i<argnr; i++)
  {
    free(arg[i]);
  }
 
  if (ocr_progress) /* pipe available */
  {
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
 
    while (!feof(ocr_progress))
    {
     int progress, subprogress;
     float fprogress;
 
      fgets(buf, sizeof(buf), ocr_progress);
 
      if (!strncmp(preferences.ocr_progress_keyword, buf, strlen(preferences.ocr_progress_keyword)))
      {
        sscanf(buf + strlen(preferences.ocr_progress_keyword), "%d %d", &progress, &subprogress);
 
        snprintf(buf, sizeof(buf), "%s (%d:%d)", PROGRESS_OCR, progress, subprogress);
        gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), buf);
 
        fprogress = progress / 100.0; 
 
        if (fprogress < 0.0)
        {
          fprogress = 0.0;
        }
 
        if (fprogress > 11.0)
        {
          fprogress = 1.0;
        }
 
        xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fprogress);
      }
 
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }
 
    gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "");
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
  }
  else /* no pipe available */
  {
    while (pid)
    {
     int status = 0;
     pid_t pid_status = waitpid(pid, &status, WNOHANG);
 
      if (pid == pid_status)
      {
        pid = 0; /* ok, child process has terminated */
      }
 
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }
  }
 
  if (pipefd[0])
  {
    fclose(ocr_progress); /* close reading end of pipe */
  }

 return (*cancel_save);
}                                                                                                                                                      
/* ---------------------------------------------------------------------------------------------------------------------- */

/* save image in destination file format. lineart images that are stored as grayscale image are reduced to lineart! */
int xsane_save_image_as(char *output_filename, char *input_filename, int output_format,
                        int apply_ICM_profile, int cms_function, int cms_intent, int cms_bpc,
                        GtkProgressBar *progress_bar, int *cancel_save)
{
 FILE *outfile;
 FILE *infile;
 char buf[TEXTBUFSIZE];
 Image_info image_info;
 char temporary_filename[PATH_MAX];
 int remove_input_file = FALSE;
 cmsHTRANSFORM hTransform = NULL;
  
  DBG(DBG_proc, "xsane_save_image_as(output_file=%s, input_file=%s, type=%d)\n", output_filename, input_filename, output_format);

  *cancel_save = 0;

  infile = fopen(input_filename, "rb"); /* read binary (b for win32) */
  if (infile == 0)
  {
   char buf[TEXTBUFSIZE];
    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, input_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);     

   return -1;
  }

  xsane_read_pnm_header(infile, &image_info);

  if ((image_info.reduce_to_lineart) && (output_format != XSANE_PNM))
  {
    DBG(DBG_info, "original image is a lineart => reduce to lineart\n");
    fclose(infile);
    xsane_back_gtk_make_path(sizeof(temporary_filename), temporary_filename, 0, 0, "xsane-conversion-", xsane.dev_name, ".pbm", XSANE_PATH_TMP);

    snprintf(buf, sizeof(buf), "%s: %s", PROGRESS_PACKING_DATA, output_filename);

    gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), buf);
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

    xsane_save_image_as_lineart(temporary_filename, input_filename, progress_bar, cancel_save);

    input_filename = temporary_filename;
    remove_input_file = TRUE;

    infile = fopen(input_filename, "rb"); /* read binary (b for win32) */
    if (infile == 0)
    {
     char buf[TEXTBUFSIZE];
      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, input_filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);     

     return -1;
    }

    xsane_read_pnm_header(infile, &image_info);
  }

#ifdef HAVE_LIBLCMS
  if (apply_ICM_profile && ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) || ((output_format == XSANE_PNM) || (output_format == XSANE_PNM16))))
  {
    hTransform = xsane_create_cms_transform(&image_info, cms_function, cms_intent, cms_bpc);
  }
#endif

  if (1)
  {
    snprintf(buf, sizeof(buf), "%s: %s", PROGRESS_SAVING_DATA, output_filename);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s", PROGRESS_SAVING_DATA);
  }

  gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(progress_bar), PANGO_ELLIPSIZE_START); /* this is new API, can be removed for old GTK versions */
  gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), buf);
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);


#ifdef HAVE_LIBTIFF
  if (output_format == XSANE_TIFF)		/* routines that want to have filename  for saving */
  {
   TIFF *tiffile;

    if (xsane_create_secure_file(output_filename)) /* remove possibly existing symbolic links for security */
    {
      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, output_filename);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }

    tiffile = TIFFOpen(output_filename, "w");
    if (!tiffile)
    {
      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_OPEN_FAILED, output_filename);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }

    xsane_save_tiff_page(tiffile, 0, 0, preferences.jpeg_quality, infile, &image_info, hTransform, apply_ICM_profile, cms_function, progress_bar, cancel_save);

    TIFFClose(tiffile);
  }
  else							/* routines that want to have filedescriptor for saving */
#endif /* HAVE_LIBTIFF */
  {
    if (xsane_create_secure_file(output_filename)) /* remove possibly existing symbolic links for security */
    {
      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, output_filename);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }

    outfile = fopen(output_filename, "wb"); /* b = binary mode for win32 */

    if (outfile != 0)
    {
      switch(output_format)
      {
        case XSANE_PNM:
          if (image_info.reduce_to_lineart)
          {
            xsane_save_grayscale_image_as_lineart(outfile, infile, &image_info, progress_bar, cancel_save);
          }
          else
          {
            xsane_save_pnm_8(outfile, infile, &image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
          }
         break;

#ifdef HAVE_LIBJPEG
        case XSANE_JPEG:
          xsane_save_jpeg(outfile, preferences.jpeg_quality, infile, &image_info, hTransform, apply_ICM_profile, cms_function, progress_bar, cancel_save);
         break; /* switch format == XSANE_JPEG */
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        case XSANE_PNG:
          if (image_info.depth <= 8)
          {
            xsane_save_png(outfile, preferences.png_compression, infile, &image_info, hTransform, apply_ICM_profile, cms_function, progress_bar, cancel_save);
          }
          else
          {
            xsane_save_png_16(outfile, preferences.png_compression, infile, &image_info, hTransform, apply_ICM_profile, cms_function, progress_bar, cancel_save);
          }
         break; /* switch format == XSANE_PNG */
#endif
#endif

        case XSANE_PNM16:
          xsane_save_pnm_16(outfile, infile, &image_info, hTransform, apply_ICM_profile, progress_bar, cancel_save);
         break; /* switch fomat == XSANE_PNM16 */

        case XSANE_PS: /* save postscript, use original size */
        { 
         float imagewidth, imageheight;

           imagewidth  = 72.0 * image_info.image_width/image_info.resolution_x; /* width in 1/72 inch */
           imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

            xsane_save_ps(outfile, infile,
                          &image_info,
                          imagewidth, imageheight,
                          0, /* paper_left_margin */
                          0, /* paper_bottom_margin */
                          (int) imagewidth, /* paper_width */
                          (int) imageheight, /* paper_height */
                          0 /* portrait top left */,
                          preferences.save_ps_flatedecoded,
                          hTransform, apply_ICM_profile,
                          (cms_function == XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE), image_info.icm_profile,
		          0, NULL, 0, /* no CRD */
			  0 /* intent */,
                          progress_bar,
                          cancel_save);
        }
        break; /* switch format == XSANE_PS */

        case XSANE_PDF: /* save PDF, use original size */
        { 
         float imagewidth, imageheight;

           imagewidth  = 72.0 * image_info.image_width/image_info.resolution_x; /* width in 1/72 inch */
           imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

            xsane_save_pdf(outfile, infile,
                          &image_info,
                          imagewidth, imageheight,
                          0, /* paper_left_margin */
                          0, /* paper_bottom_margin */
                          (int) imagewidth, /* paper_width */
                          (int) imageheight, /* paper_height */
                          0 /* portrait top left */,
                          preferences.save_pdf_flatedecoded,
                          hTransform, apply_ICM_profile, cms_function,
                          progress_bar,
                          cancel_save);
        }
        break; /* switch format == XSANE_PDF */

        case XSANE_TEXT: /* save as text using ocr program like gocr/jocr */
        {
          xsane_save_image_as_text(output_filename, input_filename, progress_bar, cancel_save);
        }
        break; /* switch format == XSANE_TEXT */

        default:
          snprintf(buf, sizeof(buf),"%s", ERR_UNKNOWN_SAVING_FORMAT);
          xsane_back_gtk_error(buf, TRUE);

          fclose(outfile);
          fclose(infile);

          remove(output_filename); /* no usable output: remove output file  */

          if (remove_input_file)
          {
            remove(input_filename); /* remove lineart pbm file  */
          }

          gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "");
          xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

         return -2;
         break; /* switch format == default */
      }
      fclose(outfile);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, output_filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);

      fclose(infile);

      if (remove_input_file)
      {
        remove(input_filename); /* remove lineart pbm file  */
      }

      gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "");
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

     return -2;
    }
  }

  fclose (infile);

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    cmsDeleteTransform(hTransform);
  }
#endif

  if (remove_input_file)
  {
    remove(input_filename); /* remove lineart pbm file  */
  }

  if (*cancel_save) /* remove output file if saving has been canceled */
  {
    remove(output_filename);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "");
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

 return (*cancel_save);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_ANY_GIMP
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

#ifdef HAVE_GIMP_2
static void xsane_gimp_run(const gchar *name, gint nparams, const GimpParam *param, gint *nreturn_vals, GimpParam **return_vals)
{
 GimpRunMode run_mode;
#else /* GIMP-1.x */
static void xsane_gimp_run(char *name, int nparams, GimpParam *param, int *nreturn_vals, GimpParam **return_vals)
{
 GimpRunModeType run_mode;
#endif

 static GimpParam values[2];
 char devname[1024];
 char *args[2];
 int nargs;

  DBG(DBG_proc, "xsane_gimp_run\n");

  run_mode = param[0].data.d_int32;
  xsane.mode = XSANE_GIMP_EXTENSION;
  xsane.xsane_mode = XSANE_SAVE;

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
#ifdef HAVE_GIMP_2
      gimp_extension_ack();
#endif
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
/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_transfer_to_gimp(char *input_filename, int apply_ICM_profile, int cms_function, GtkProgressBar *progress_bar, int *cancel_save)
{
 int remaining;
 size_t tile_size;
 GimpImageType image_type    = GIMP_GRAY;
 GimpImageType drawable_type = GIMP_GRAY_IMAGE;
 gint32 layer_ID;
 gint32 image_ID;
 GimpDrawable *drawable;
 guchar *tile;
 GimpPixelRgn region;
 unsigned tile_offset;
 int i, x, y;
 Image_info image_info;
 FILE *imagefile;
 int bytes;
 unsigned char *data = NULL;
 guint16 *data16 = NULL;
 size_t bytes_read;
#ifdef HAVE_LIBLCMS
 unsigned char *data_raw = NULL;
 cmsHTRANSFORM hTransform = NULL;
#endif

  DBG(DBG_info, "xsane_transer_to_gimp\n");

  *cancel_save = 0;

  imagefile = fopen(input_filename, "rb"); /* read binary (b for win32) */
  if (imagefile == 0)
  {
   char buf[TEXTBUFSIZE];
    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, input_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);     

   return -1;
  }

  xsane_read_pnm_header(imagefile, &image_info);

  if (image_info.depth == 16)
  {
    bytes = 2;
  }
  else
  {
    bytes = 1;
  }

  data = malloc(image_info.image_width * 3 * bytes);
  data16 = (guint16 *) data;

  if (!data)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
    xsane_back_gtk_error(buf, TRUE);
   return -1; /* error */
  }

#ifdef HAVE_LIBLCMS
  if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)  && apply_ICM_profile && (image_info.depth != 1))
  {
    hTransform = xsane_create_cms_transform(&image_info, cms_function, preferences.cms_intent, preferences.cms_bpc);
  }

  if (hTransform != NULL)
  {
    DBG(DBG_info, "Doing CMS color conversion\n");

    data_raw = malloc(image_info.image_width * 3 * bytes);

    if (!data_raw)
    {
     char buf[TEXTBUFSIZE];

      free(data);

      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, ERR_NO_MEM);
      xsane_back_gtk_error(buf, TRUE);
     return -1; /* error */
    }
  }
#endif

  x = 0;
  y = 0; 
  tile_offset = 0;
  tile_size = image_info.image_width * gimp_tile_height();

  if (image_info.channels == 3) /* RGB */
  {
    tile_size *= 3;  /* 24 bits/pixel RGB */
    image_type    = GIMP_RGB;
    drawable_type = GIMP_RGB_IMAGE;
  }
  else if (image_info.channels == 4) /* RGBA */
  {
    tile_size *= 4;  /* 32 bits/pixel RGBA */
    image_type    = GIMP_RGB;
    drawable_type = GIMP_RGBA_IMAGE; /* interpret infrared as alpha */
  }
  /* colors == 0/1 is predefined */
 
  image_ID = gimp_image_new(image_info.image_width, image_info.image_height, image_type);

#ifdef HAVE_LIBLCMS
  if ((cms_function != XSANE_CMS_FUNCTION_CONVERT_TO_SRGB) && apply_ICM_profile) /* embed profile */
  {
   GimpParasite *parasite;
   FILE *icm_profile;
   guchar *profile_buffer;
   gint32 size;

    DBG(DBG_error, "Opening ICM profile %s\n", image_info.icm_profile);
    icm_profile = fopen(image_info.icm_profile, "rb");

    if (icm_profile)
    {
      fseek(icm_profile, 0, SEEK_END);
      size = ftell(icm_profile);
      fseek(icm_profile, 0, SEEK_SET);

      profile_buffer = malloc(size);

      if (profile_buffer)
      {
        if (fread(profile_buffer, 1, size, icm_profile) == size)
        {
          parasite = gimp_parasite_new("icc-profile", 0, size, profile_buffer);
          gimp_image_parasite_attach(image_ID, parasite);
          gimp_parasite_free(parasite);
        }
        else
        {
          DBG(DBG_error, "can not read profile data\n");
        }

        free(profile_buffer);
      }
      else
      {
        DBG(DBG_error, "can not allocate profile_buffer\n");
      }

      fclose(icm_profile);
    }
    else
    {
      DBG(DBG_error, "can not open ICM-profile\n");
    }
  }
#endif

 
/* the following is supported since gimp-1.1.? */
#ifdef GIMP_HAVE_RESOLUTION_INFO
  if (image_info.resolution_x > 0)
  {
    gimp_image_set_resolution(image_ID, image_info.resolution_x, image_info.resolution_y);
  }
/*          gimp_image_set_unit(image_ID, unit?); */
#endif
 
  layer_ID = gimp_layer_new(image_ID, "Background", image_info.image_width, image_info.image_height, drawable_type, 100.0, GIMP_NORMAL_MODE);
  gimp_image_add_layer(image_ID, layer_ID, 0);
  drawable = gimp_drawable_get(layer_ID);
  gimp_pixel_rgn_init(&region, drawable, 0, 0, drawable->width, drawable->height, TRUE, FALSE);
  tile = g_new(guchar, tile_size);
 

  if (image_info.channels == 1) /* gray */
  {
    switch (image_info.depth)
    {
      case 1: /* 1 bit gray => conversion to 8 bit gray */
        for (i = 0; i < ( (image_info.image_width + 7) / 8) * image_info.image_height; ++i)
        {
         u_char mask;
         int j;

          mask = fgetc(imagefile);
          for (j = 7; j >= 0; --j)
          {
            u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
            tile[tile_offset++] = gl;

            x++;

            if (x >= image_info.image_width)
            {
             int tile_height = gimp_tile_height();

              x = 0;
              y++;

              if (y % tile_height == 0)
              {
                gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
                tile_offset = 0;
              }

              xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */

              break; /* leave for j loop */
            }
          }

          if (*cancel_save)
          {
            break;
          }
        }
       break; /* leave switch depth 1 */

      case 8: /* 8 bit gray */
        for (y = 1; y <= image_info.image_height; y++)
        {
         int tile_height = gimp_tile_height();

#ifdef HAVE_LIBLCMS
          if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && apply_ICM_profile && (hTransform != NULL))
          {
            bytes_read = fread(data_raw, 1, image_info.image_width, imagefile);
            cmsDoTransform(hTransform, data_raw, data, image_info.image_width);
          }
          else
#endif
          {
            bytes_read = fread(data, 1, image_info.image_width, imagefile);
          }

          for (x = 0; x < image_info.image_width; x++)
	  {
            tile[tile_offset++] = data[x];
	  }

          if (y % tile_height == 0)
          {
            gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
            tile_offset = 0;
          }

          xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */

          if (*cancel_save)
          {
            break;
          }
        }
       break; /* case 8 */


      case 16: /* 16 bit gray has to be reduced to 8 bit */
        for (y = 1; y <= image_info.image_height; y++)
        {
         int tile_height = gimp_tile_height();

#ifdef HAVE_LIBLCMS
          if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && apply_ICM_profile && (hTransform != NULL))
          {
            bytes_read = fread(data_raw, 2, image_info.image_width, imagefile);
            cmsDoTransform(hTransform, data_raw, data, image_info.image_width);
          }
          else
#endif
          {
            bytes_read = fread(data, 2, image_info.image_width, imagefile);
          }

          for (x = 0; x < image_info.image_width; x++)
	  {
            tile[tile_offset++] = data16[x]/256;
	  }

          if (y % tile_height == 0)
          {
            gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
            tile_offset = 0;
          }

          xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */

          if (*cancel_save)
          {
            break;
          }
        }
       break; /* case 16 */

      default: /* bad depth */
       break; /* default */
    }
  }
  else if (image_info.channels == 3) /* RGB */
  {
    switch (image_info.depth)
    {
      case 8: /* 8 bit RGB */

        for (y = 1; y <= image_info.image_height; y++)
        {
         int tile_height = gimp_tile_height();

#ifdef HAVE_LIBLCMS
          if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && apply_ICM_profile && (hTransform != NULL))
          {
            bytes_read = fread(data_raw, 3, image_info.image_width, imagefile);
            cmsDoTransform(hTransform, data_raw, data, image_info.image_width);
          }
          else
#endif
          {
            bytes_read = fread(data, 3, image_info.image_width, imagefile);
          }

          for (x = 0; x < image_info.image_width; x++)
	  {
            tile[tile_offset++] = data[3*x+0];
            tile[tile_offset++] = data[3*x+1];
            tile[tile_offset++] = data[3*x+2];
	  }

          if (y % tile_height == 0)
          {
            gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
            tile_offset = 0;
          }

          xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */

          if (*cancel_save)
          {
            break;
          }
        }
       break; /* case 8 */


      case 16: /* 16 bit RGB has to be reduced to 8 bit */

        for (y = 1; y <= image_info.image_height; y++)
        {
         int tile_height = gimp_tile_height();

#ifdef HAVE_LIBLCMS
          if ((cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE) && apply_ICM_profile && (hTransform != NULL))
          {
            bytes_read = fread(data_raw, 6, image_info.image_width, imagefile);
            cmsDoTransform(hTransform, data_raw, data, image_info.image_width);
          }
          else
#endif
          {
            bytes_read = fread(data, 6, image_info.image_width, imagefile);
          }

          for (x = 0; x < image_info.image_width; x++)
	  {
            tile[tile_offset++] = data16[3*x+0]/256;
            tile[tile_offset++] = data16[3*x+1]/256;
            tile[tile_offset++] = data16[3*x+2]/256;
	  }

          if (y % tile_height == 0)
          {
            gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
            tile_offset = 0;
          }

          xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */

          if (*cancel_save)
          {
            break;
          }
        }
       break; /* case 16 */

      default: /* bad depth */
       break; /* default */
    }
  }
#ifdef SUPPORT_RGBA
  else if (image_info.channels == 4) /* RGBA */
  {
   int i;

    switch (image_info.depth)
    {
      case 8: /* 8 bit RGBA */
      case 16: /* 16 bit RGBA already has been reduced to 8 bit */
        for (i = 0; i < image_info.image_width * image_info.image_height * 4; ++i)
        {
          tile[tile_offset++] = fgetc(imagefile);
          if (tile_offset % 4 == 0)
          {
            x++;

            if (x >= image_info.image_width)
            {
             int tile_height = gimp_tile_height();

              x = 0;
              y++;

              if (y % tile_height == 0)
              {
                gimp_pixel_rgn_set_rect(&region, tile, 0, y - tile_height, image_info.image_width, tile_height);
                tile_offset = 0;
              }

              xsane_progress_bar_set_fraction(progress_bar, (float) y / image_info.image_height); /* update progress bar */
            }
          }

          if (*cancel_save)
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
  if (y > image_info.image_height)
  {
    y = image_info.image_height;
  }
 
  remaining = y % gimp_tile_height();

  if (remaining)
  {
    gimp_pixel_rgn_set_rect(&region, tile, 0, y - remaining, image_info.image_width, remaining);
  }

  gimp_drawable_flush(drawable);
  gimp_display_new(image_ID);
  gimp_drawable_detach(drawable);
  g_free(tile);
  tile = 0;

  fclose(imagefile);

#ifdef HAVE_LIBLCMS
  if (hTransform != NULL)
  {
    cmsDeleteTransform(hTransform);
  }

  if (data_raw)
  {
    free(data_raw);
  }
#endif
  free(data);

 return 0;
}
#endif /* HAVE_ANY_GIMP */ 

/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_ACTIVATE_EMAIL

/* character base of base64 coding */
static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ---------------------------------------------------------------------------------------------------------------------- */

static void write_3chars_as_base64(unsigned char c1, unsigned char c2, unsigned char c3, int pads, int fd_socket)
{
 char buf[4];
 ssize_t bytes_written;

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

  bytes_written = write(fd_socket, buf, 4);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_string_base64(int fd_socket, char *string, int len)
{
 int i;
 int pad;
 unsigned char c1, c2, c3;
 ssize_t bytes_written;

  for (i = 0; i < len; i+=3)
  {
    c1 = (unsigned char) string[i];
    c2 = (unsigned char) string[i+1];
    c3 = (unsigned char) string[i+2];

    pad = i - len + 3;

    if (pad < 0)
    {
      pad = 0;
    }
    else if (pad)
    {
      c3 = 0;

      if (pad == 2)
      {
        c2 = 0;
      }
    }

    write_3chars_as_base64(c1, c2, c3, pad, fd_socket);
  }
  bytes_written = write(fd_socket, "\r\n", 2);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_base64(int fd_socket, FILE *infile) 
{
 int c1, c2, c3;
 int pos = 0;
 ssize_t bytes_written;

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
      bytes_written = write(fd_socket, "\r\n", 2);
      
      pos = 0;
    }

    xsane.email_progress_bytes += 3;
    if ((int)  ((xsane.email_progress_bytes * 100) / xsane.email_progress_size) != (int) (xsane.email_progress_val * 100))
    {
      xsane.email_progress_val = (float) xsane.email_progress_bytes / xsane.email_progress_size;
      xsane_front_gtk_email_project_update_lockfile_status();
    }
  }

  if (pos)
  {
    bytes_written = write(fd_socket, "\r\n", 2);
  }

  xsane.email_progress_val = 1.0;
  xsane_front_gtk_email_project_update_lockfile_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_header(int fd_socket, char *from, char *reply_to, char *to, char *subject, char *boundary, int related)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "From: %s\r\n", from);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Reply-To: %s\r\n", reply_to);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "To: %s\r\n", to);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Subject: %s\r\n", subject);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "MIME-Version: 1.0\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  if (related) /* related means that we need a special link in the html part to display the image */
  {
    snprintf(buf, sizeof(buf), "Content-Type: multipart/related;\r\n");
    bytes_written = write(fd_socket, buf, strlen(buf));
  }
  else
  {
    snprintf(buf, sizeof(buf), "Content-Type: multipart/mixed;\r\n");
    bytes_written = write(fd_socket, buf, strlen(buf));
  }

  snprintf(buf, sizeof(buf), " boundary=\"%s\"\r\n\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_footer(int fd_socket, char *boundary)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_mime_ascii(int fd_socket, char *boundary)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "--%s\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: text/plain;\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        charset=\"iso-8859-1\"\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: 8bit\r\n\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_mime_html(int fd_socket, char *boundary)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "--%s\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: text/html;\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        charset=\"us-ascii\"\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: 7bit\r\n\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "<html>\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_attach_image(int fd_socket, char *boundary, char *content_id, char *content_type, FILE *infile, char *filename)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "--%s\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", content_type);
  bytes_written = write(fd_socket, buf, strlen(buf));

  if (content_id)
  {
    snprintf(buf, sizeof(buf), "Content-ID: <%s>\r\n", content_id);
    bytes_written = write(fd_socket, buf, strlen(buf));
  }

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: base64\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Disposition: inline;\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        filename=\"%s\"\r\n", filename);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  write_base64(fd_socket, infile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void write_email_attach_file(int fd_socket, char *boundary, FILE *infile, char *filename)
{
 char buf[1024];
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "--%s\r\n", boundary);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Type: application/octet-stream\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        name=\"%s\"\r\n", filename);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Transfer-Encoding: base64\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "Content-Disposition: attachment;\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "        filename=\"%s\"\r\n", filename);
  bytes_written = write(fd_socket, buf, strlen(buf));

  snprintf(buf, sizeof(buf), "\r\n");
  bytes_written = write(fd_socket, buf, strlen(buf));

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
 ssize_t bytes_written;

  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "USER %s\r\n", user);
  DBG(DBG_info2, "> USER xxx\n");
  bytes_written = write(fd_socket, buf, strlen(buf));
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
  bytes_written = write(fd_socket, buf, strlen(buf));
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
  bytes_written = write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int asmtp_authentication(int fd_socket, int auth_type, char *user, char *passwd)
{
 int len;
 char buf[1024];
 ssize_t bytes_written;

  DBG(DBG_proc, "asmtp_authentication\n");

  switch (auth_type)
  {
    case EMAIL_AUTH_ASMTP_PLAIN:
      snprintf(buf, sizeof(buf), "AUTH PLAIN ");
      DBG(DBG_info2, "> %s\\0(USER)\\0(PASSWORD)\n", buf);
      bytes_written = write(fd_socket, buf, strlen(buf));
      snprintf(buf, sizeof(buf), "%c%s%c%s", 0, user, 0, passwd);
      write_string_base64(fd_socket, buf, strlen(user)+strlen(passwd)+2);
      len = read(fd_socket, buf, sizeof(buf));
      if (len >= 0)
      {
        buf[len] = 0;
      }
      DBG(DBG_info2, "< %s", buf);
     break;

    case EMAIL_AUTH_ASMTP_LOGIN:
      snprintf(buf, sizeof(buf), "AUTH LOGIN\r\n");
      DBG(DBG_info2, "> %s", buf);
      bytes_written = write(fd_socket, buf, strlen(buf));
      len = read(fd_socket, buf, sizeof(buf));
      if (len >= 0)
      {
        buf[len] = 0;
      }
      DBG(DBG_info2, "< %s", buf);
      if (buf[0] != '3')
      {
        DBG(DBG_info, "=> error\n");
       return (-1);
      }

      DBG(DBG_info2, "> (USERNAME)\n");
      write_string_base64(fd_socket, user, strlen(user));

      len = read(fd_socket, buf, sizeof(buf));
      if (len >= 0)
      {
        buf[len] = 0;
      }
      DBG(DBG_info2, "< %s", buf);
      if (buf[0] != '3')
      {
        DBG(DBG_info, "=> error\n");
       return (-1);
      }

      DBG(DBG_info2, "> (PASSWORD)\n");
      write_string_base64(fd_socket, passwd, strlen(passwd));

      len = read(fd_socket, buf, sizeof(buf));
      if (len >= 0)
      {
        buf[len] = 0;
      }
      DBG(DBG_info2, "< %s", buf);
      if (buf[0] != '2')
      {
        DBG(DBG_info, "=> error\n");
       return (-1);
      }
     break;

#if 0
    case EMAIL_AUTH_ASMTP_CRAM_MD5:
      snprintf(buf, sizeof(buf), "AUTH CRAM-MD5\r\n");
      DBG(DBG_info2, "> %s", buf);
      bytes_written = write(fd_socket, buf, strlen(buf));
      len = read(fd_socket, buf, sizeof(buf));
      if (len >= 0)
      {
        buf[len] = 0;
      }
      DBG(DBG_info2, "< %s", buf);
     break;
#endif

    default:
       DBG(DBG_proc, "no valid asmtp authentication type\n");
     break;
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* not only a write routine, also reads data */
/* returns -1 on error, 0 when ok */
int write_smtp_header(int fd_socket, char *from, char *to, int auth_type, char *user, char *passwd)
{
 char buf[1024];
 int len;
 char to_line[1024];
 char *to_pos = NULL;
 char *pos = NULL;
 ssize_t bytes_written;

  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  if (auth_type < EMAIL_AUTH_ASMTP_PLAIN)
  {
    snprintf(buf, sizeof(buf), "HELO localhost\r\n");
  }
  else
  {
    snprintf(buf, sizeof(buf), "EHLO localhost\r\n");
  }
  DBG(DBG_info2, "> %s", buf);
  bytes_written = write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  if (buf[0] != '2')
  {
    DBG(DBG_info, "=> error\n");

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_EMAIL_STATUS_SMTP_CONNECTION_FAILED);
    xsane_front_gtk_email_project_update_lockfile_status();
   return -1;
  }

  if (asmtp_authentication(fd_socket, auth_type, user, passwd))
  {
    xsane.email_status = strdup(TEXT_EMAIL_STATUS_ASMTP_AUTH_FAILED);
    xsane_front_gtk_email_project_update_lockfile_status();
   return -1;
  }

  while (from[0] == ' ')
  {
    from = from + 1;
  }
  snprintf(buf, sizeof(buf), "MAIL FROM: <%s>\r\n", from);
  DBG(DBG_info2, "> %s", buf);
  bytes_written = write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  if (buf[0] != '2')
  {
    DBG(DBG_info, "=> error\n");

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_EMAIL_STATUS_SMTP_ERR_FROM);
    xsane_front_gtk_email_project_update_lockfile_status();
   return -1;
  }
 

  strncpy(to_line, to, sizeof(to_line)); /* it is not allowed to modify the "to" string, so we make a copy */
  to_pos = to_line;
  while (to_pos != NULL)
  {
    while (*to_pos == ' ')
    {
      to_pos = to_pos + 1;
    }
    pos = strchr(to_pos, ',');

    if (pos)
    {
      *pos = 0; /* end of string marker */
    }

    snprintf(buf, sizeof(buf), "RCPT TO: <%s>\r\n", to_pos);

    DBG(DBG_info2, "> %s", buf);
    bytes_written = write(fd_socket, buf, strlen(buf));
    len = read(fd_socket, buf, sizeof(buf));
    if (len >= 0)
    {
      buf[len] = 0;
    }
    DBG(DBG_info2, "< %s\n", buf);
  
    if (buf[0] != '2')
    {
      DBG(DBG_info, "=> error\n");

      if (xsane.email_status)
      {
        free(xsane.email_status);
      }
      xsane.email_status = strdup(TEXT_EMAIL_STATUS_SMTP_ERR_RCPT);
      xsane_front_gtk_email_project_update_lockfile_status();
     return -1;
    }

    if (pos)
    {
      to_pos = pos+1;
    }
    else
    {
      to_pos = NULL;
    }
  }

  snprintf(buf, sizeof(buf), "DATA\r\n");
  DBG(DBG_info2, "> %s", buf);
  bytes_written = write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  if ((buf[0] != '2') && (buf[0] != '3'))
  {
    DBG(DBG_info, "=> error\n");

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_EMAIL_STATUS_SMTP_ERR_DATA);
    xsane_front_gtk_email_project_update_lockfile_status();
   return -1;
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* not only a write routine, also reads data */
int write_smtp_footer(int fd_socket)
{
 char buf[1024];
 int len;
 ssize_t bytes_written;

  snprintf(buf, sizeof(buf), "\r\n.\r\n");
  DBG(DBG_info2, "> %s", buf);
  bytes_written = write(fd_socket, buf, strlen(buf));
  len = read(fd_socket, buf, sizeof(buf));
  if (len >= 0)
  {
    buf[len] = 0;
  }
  DBG(DBG_info2, "< %s\n", buf);

  snprintf(buf, sizeof(buf), "QUIT\r\n");
  DBG(DBG_info2, "> %s", buf);
  bytes_written = write(fd_socket, buf, strlen(buf));
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
