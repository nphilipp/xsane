/* xsane
   Copyright (C) 1999 Oliver Rauch
   Copyright (C) 1997 David Mosberger-Tang
   This file is part of the XSANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* --------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-rc-io.h"

/* --------------------------------------------------------------------- */

#define POFFSET(field)	((char *) &((Preferences *) 0)->field - (char *) 0)
#define PFIELD(p,offset,type)	(*((type *)(((char *)(p)) + (offset))))

#define PRTOFFSET(field)	((char *) &((Preferences_printer_t *) 0)->field - (char *) 0)

/* --------------------------------------------------------------------- */

Preferences preferences =
  {
       0,		/* no default filename */
       0137,		/* image umask (permission mask for -rw-r------) */
       0027,		/* image umask (permission mask for -rwxr-x----) */
       0,		/* no fax project */
       0,		/* no default faxcommand */
       0,		/* no default fax receiver option */
       0,		/* no default fax postscript option */
       0,		/* no default fax normal option */
       0,		/* no default fax fine option */
       0,		/* no fax viewer */
       0,		/* no doc viewer */
      80.0,		/* jpeg_quality */
      7.0,		/* png_compression */
       5,		/* tiff_compression_nr */
       5,		/* tiff_compression_1_nr */
       1,		/* overwrite_warning */
       1,		/* increase_filename_counter */
       1,		/* skip_existing_numbers */
       1,		/* tooltips enabled */
       0,		/* (dont) show histogram */
       0,		/* (dont) show standard options */
       0,		/* (dont) show advanced options */
       0,		/* (dont) show resolution list */
    10.0,		/* length unit */
       1,		/* main window fixed (1) or scrolled (0) */
       1,		/* preserve_preview */
       0,		/* preview_own_cmap */
     1.0,		/* preview_gamma */
     1.0,		/* preview_gamma_red */
     1.0,		/* preview_gamma_green */
     1.0,		/* preview_gamma_blue */
     1.6,		/* gamma */
     1.0,		/* gamma red */
     1.0,		/* gamma green */
     1.0,		/* gamma blue */
     0.0,		/* brightness */
     0.0,		/* brightness red */
     0.0,		/* brightness green */
     0.0,		/* brightness blue */
     0.0,		/* contrast */
     0.0,		/* contrast red */
     0.0,		/* contrast green */
     0.0,		/* contrast blue */
     1,			/* rgb default */
     0,			/* negative */
     GTK_UPDATE_DISCONTINUOUS, /* update policy for gtk frontend sliders */
     0,			/* printernr */
     0			/* printerdefinitions */
  };

/* --------------------------------------------------------------------- */

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc[] =
  {
    {"filename",			xsane_rc_pref_string,	POFFSET(filename)},
    {"image-umask",			xsane_rc_pref_int,	POFFSET(image_umask)},
    {"directory-umask",			xsane_rc_pref_int,	POFFSET(directory_umask)},
    {"fax-project",			xsane_rc_pref_string,	POFFSET(fax_project)},
    {"fax-command",			xsane_rc_pref_string,	POFFSET(fax_command)},
    {"fax-receiver-option",		xsane_rc_pref_string,	POFFSET(fax_receiver_option)},
    {"fax-postscript-option",		xsane_rc_pref_string,	POFFSET(fax_postscript_option)},
    {"fax-normal-option",		xsane_rc_pref_string,	POFFSET(fax_normal_option)},
    {"fax-fine-option",			xsane_rc_pref_string,	POFFSET(fax_fine_option)},
    {"fax-viewer",			xsane_rc_pref_string,	POFFSET(fax_viewer)},
    {"doc-viewer",			xsane_rc_pref_string,	POFFSET(doc_viewer)},
    {"overwrite-warning",		xsane_rc_pref_int,	POFFSET(overwrite_warning)},
    {"increase-filename-counter",	xsane_rc_pref_int,	POFFSET(increase_filename_counter)},
    {"skip-existing-numbers",		xsane_rc_pref_int,	POFFSET(skip_existing_numbers)},
    {"jpeg-quality",			xsane_rc_pref_double,	POFFSET(jpeg_quality)},
    {"png-compression",			xsane_rc_pref_double, 	POFFSET(png_compression)},
    {"tiff-compression_nr",		xsane_rc_pref_int, 	POFFSET(tiff_compression_nr)},
    {"tiff-compression_1_nr",		xsane_rc_pref_int, 	POFFSET(tiff_compression_1_nr)},
    {"tool-tips",			xsane_rc_pref_int,	POFFSET(tooltips_enabled)},
    {"show-histogram",			xsane_rc_pref_int,	POFFSET(show_histogram)},
    {"show-standard-options",		xsane_rc_pref_int,	POFFSET(show_standard_options)},
    {"show-advanced-options",		xsane_rc_pref_int,	POFFSET(show_advanced_options)},
    {"show-resolution-list",		xsane_rc_pref_int,	POFFSET(show_resolution_list)},
    {"length-unit",			xsane_rc_pref_double,	POFFSET(length_unit)},
    {"main-window-fixed",		xsane_rc_pref_int,	POFFSET(main_window_fixed)},
    {"preserve-preview",		xsane_rc_pref_int,	POFFSET(preserve_preview)},
    {"preview-own-cmap",		xsane_rc_pref_int,	POFFSET(preview_own_cmap)},
    {"preview-gamma",			xsane_rc_pref_double,	POFFSET(preview_gamma)},
    {"preview-gamma-red",		xsane_rc_pref_double,	POFFSET(preview_gamma_red)},
    {"preview-gamma-green",		xsane_rc_pref_double,	POFFSET(preview_gamma_green)},
    {"preview-gamma-blue",		xsane_rc_pref_double,	POFFSET(preview_gamma_blue)},
    {"gamma",				xsane_rc_pref_double,	POFFSET(xsane_gamma)},
    {"gamma-red",			xsane_rc_pref_double,	POFFSET(xsane_gamma_red)},
    {"gamma-green",			xsane_rc_pref_double,	POFFSET(xsane_gamma_green)},
    {"gamma-blue",			xsane_rc_pref_double,	POFFSET(xsane_gamma_blue)},
    {"brightness",			xsane_rc_pref_double,	POFFSET(xsane_brightness)},
    {"brightness-red",			xsane_rc_pref_double,	POFFSET(xsane_brightness_red)},
    {"brightness-green",		xsane_rc_pref_double,	POFFSET(xsane_brightness_green)},
    {"brightness-blue",			xsane_rc_pref_double,	POFFSET(xsane_brightness_blue)},
    {"contrast",			xsane_rc_pref_double,	POFFSET(xsane_contrast)},
    {"contrast-red",			xsane_rc_pref_double,	POFFSET(xsane_contrast_red)},
    {"contrast-green",			xsane_rc_pref_double,	POFFSET(xsane_contrast_green)},
    {"contrast-blue",			xsane_rc_pref_double,	POFFSET(xsane_contrast_blue)},
    {"rgb-default",			xsane_rc_pref_int,	POFFSET(xsane_rgb_default)},
    {"negative",			xsane_rc_pref_int,	POFFSET(xsane_negative)},
    {"gtk-update-policy",		xsane_rc_pref_int,	POFFSET(gtk_update_policy)},
    {"printernr",			xsane_rc_pref_int,	POFFSET(printernr)},
    {"printerdefinitions",		xsane_rc_pref_int,	POFFSET(printerdefinitions)}
  };

/* --------------------------------------------------------------------- */

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc_printer[] =
  {
    {"printer-name",			xsane_rc_pref_string,	PRTOFFSET(name)},
    {"printer-command",			xsane_rc_pref_string,	PRTOFFSET(command)},
    {"printer-copy-number-option",	xsane_rc_pref_string,	PRTOFFSET(copy_number_option)},
    {"printer-resolution",		xsane_rc_pref_int,	PRTOFFSET(resolution)},
    {"printer-width",			xsane_rc_pref_int,	PRTOFFSET(width)},
    {"printer-height",			xsane_rc_pref_int,	PRTOFFSET(height)},
    {"printer-left-offset",		xsane_rc_pref_int,	PRTOFFSET(leftoffset)},
    {"printer-bottom-offset",		xsane_rc_pref_int,	PRTOFFSET(bottomoffset)},
    {"printer-gamma",			xsane_rc_pref_double,	PRTOFFSET(gamma)},
    {"printer-gamma-red",		xsane_rc_pref_double,	PRTOFFSET(gamma_red)},
    {"printer-gamma-green",		xsane_rc_pref_double,	PRTOFFSET(gamma_green)},
    {"printer-gamma-blue",		xsane_rc_pref_double,	PRTOFFSET(gamma_blue)}
  };

/* --------------------------------------------------------------------- */

void preferences_save(int fd)
{
 Wire w;
 int i, n;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

  for (i = 0; i < NELEMS(desc); ++i)
  {
    xsane_rc_io_w_string(&w, &desc[i].name);
    (*desc[i].codec) (&w, &preferences, desc[i].offset);
  }

  n=0;

  while (n < preferences.printerdefinitions)
  {
    for (i = 0; i < NELEMS(desc_printer); ++i)
    {
      xsane_rc_io_w_string(&w, &desc_printer[i].name);
      (*desc_printer[i].codec) (&w, preferences.printer[n], desc_printer[i].offset);
    }
    n++;
  }

  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);	/* flush it out */
}

/* --------------------------------------------------------------------- */

void preferences_restore(int fd)
{
 SANE_String name;
 Wire w;
 int i, n;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);


  while (1)
  {
    xsane_rc_io_w_space(&w, 3);
    if (w.status)
    {
      return;
    }

    xsane_rc_io_w_string(&w, &name);
    if (w.status || !name)
    {
      return;
    }

    for (i = 0; i < NELEMS (desc); ++i)
    {
      if (strcmp(name, desc[i].name) == 0)
      {
        (*desc[i].codec) (&w, &preferences, desc[i].offset);
        break;
      }
    }
    if (!strcmp(name, "printerdefinitions"))
    {
      break;
    }
  }


  n=0;
  while (n < preferences.printerdefinitions)
  {
    preferences.printer[n] = calloc(sizeof(Preferences_printer_t), 1);
    for (i = 0; i < NELEMS(desc_printer); ++i)
    {
      xsane_rc_io_w_space (&w, 3);
      if (w.status)
      {
        return;
      }

      xsane_rc_io_w_string(&w, &name);
      if (w.status || !name)
      {
        return;
      }

      if (strcmp(name, desc_printer[i].name) == 0)
      {
        (*desc_printer[i].codec) (&w, preferences.printer[n], desc_printer[i].offset);
      }
      else
      {
        break;
      }
    }
    n++;
  }

}
