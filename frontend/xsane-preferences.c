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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sane/sane.h>

#include "xsane-preferences.h"
#include "sane/sanei.h"
#include "sane/sanei_wire.h"
#include "sane/sanei_codec_ascii.h"

#define POFFSET(field)	((char *) &((Preferences *) 0)->field - (char *) 0)
#define PFIELD(p,offset,type)	(*((type *)(((char *)(p)) + (offset))))

#define PRTOFFSET(field)	((char *) &((Preferences_printer_t *) 0)->field - (char *) 0)

Preferences preferences =
  {
       0,		/* no preferred device (must be 0 or malloced!) */
       0,		/* no default filename */
       0,		/* no default faxcommand */
      80.0,		/* jpeg_quality */
      7.0,		/* png_compression */
       1,		/* overwrite_warning */
       1,		/* increase_filename_counter */
       1,		/* skip_existing_numbers */
       1,		/* tooltips enabled */
       0,		/* (dont) show histogram */
       0,		/* (dont) show standard options */
       0,		/* (dont) show advanced options */
    10.0,		/* length unit */
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
     0,			/* printernr */
     0			/* printerdefinitions */
  };


static void w_string (Wire *w, void *p, long offset);
static void w_double (Wire *w, void *p, long offset);
static void w_int (Wire *w, void *p, long offset);

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc[] =
  {
    {"device",				w_string,	POFFSET(device)},
    {"filename",			w_string,	POFFSET(filename)},
    {"fax-cmd",				w_string,	POFFSET(fax_command)},
    {"overwrite-warning",		w_int,		POFFSET(overwrite_warning)},
    {"increase-filename-counter",	w_int,		POFFSET(increase_filename_counter)},
    {"skip-existing-numbers",		w_int,		POFFSET(skip_existing_numbers)},
    {"jpeg-quality",			w_double,	POFFSET(jpeg_quality)},
    {"png-compression",			w_double, 	POFFSET(png_compression)},
    {"tool-tips",			w_int,		POFFSET(tooltips_enabled)},
    {"show-histogram",			w_int,		POFFSET(show_histogram)},
    {"show-standard-options",		w_int,		POFFSET(show_standard_options)},
    {"show-advanced-options",		w_int,		POFFSET(show_advanced_options)},
    {"length-unit",			w_double,	POFFSET(length_unit)},
    {"preserve-preview",		w_int,		POFFSET(preserve_preview)},
    {"preview-own-cmap",		w_int,		POFFSET(preview_own_cmap)},
    {"preview-gamma",			w_double,	POFFSET(preview_gamma)},
    {"preview-gamma-red",		w_double,	POFFSET(preview_gamma_red)},
    {"preview-gamma-green",		w_double,	POFFSET(preview_gamma_green)},
    {"preview-gamma-blue",		w_double,	POFFSET(preview_gamma_blue)},
    {"gamma",				w_double,	POFFSET(xsane_gamma)},
    {"gamma-red",			w_double,	POFFSET(xsane_gamma_red)},
    {"gamma-green",			w_double,	POFFSET(xsane_gamma_green)},
    {"gamma-blue",			w_double,	POFFSET(xsane_gamma_blue)},
    {"brightness",			w_double,	POFFSET(xsane_brightness)},
    {"brightness-red",			w_double,	POFFSET(xsane_brightness_red)},
    {"brightness-green",		w_double,	POFFSET(xsane_brightness_green)},
    {"brightness-blue",			w_double,	POFFSET(xsane_brightness_blue)},
    {"contrast",			w_double,	POFFSET(xsane_contrast)},
    {"contrast-red",			w_double,	POFFSET(xsane_contrast_red)},
    {"contrast-green",			w_double,	POFFSET(xsane_contrast_green)},
    {"contrast-blue",			w_double,	POFFSET(xsane_contrast_blue)},
    {"printernr",			w_int,		POFFSET(printernr)},
    {"printerdefinitions",		w_int,		POFFSET(printerdefinitions)}
  };

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc_printer[] =
  {
    {"printer-name",		w_string,	PRTOFFSET(name)},
    {"printer-command",		w_string,	PRTOFFSET(command)},
    {"printer-resolution",	w_int,		PRTOFFSET(resolution)},
    {"printer-width",		w_int,		PRTOFFSET(width)},
    {"printer-height",		w_int,		PRTOFFSET(height)},
    {"printer-left-offset",	w_int,		PRTOFFSET(leftoffset)},
    {"printer-bottom-offset",	w_int,		PRTOFFSET(bottomoffset)},
    {"printer-gamma",		w_double,	PRTOFFSET(gamma)},
    {"printer-gamma-red",	w_double,	PRTOFFSET(gamma_red)},
    {"printer-gamma-green",	w_double,	PRTOFFSET(gamma_green)},
    {"printer-gamma-blue",	w_double,	PRTOFFSET(gamma_blue)}
  };

/* --------------------------------------------------------------------- */

static void w_string(Wire *w, void *p, long offset)
{
  SANE_String string;

  if (w->direction == WIRE_ENCODE)
    string = PFIELD (p, offset, char *);

  sanei_w_string (w, &string);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
     const char **field;

      field = &PFIELD (p, offset, const char *);
      if (*field)
      {
        free ((char *) *field);
      }
      *field = string ? strdup (string) : 0;
    }
    sanei_w_free (w, (WireCodecFunc) sanei_w_string, &string);
  }
}

/* --------------------------------------------------------------------- */

static void w_double(Wire *w, void *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
  {
    word = SANE_FIX (PFIELD (p, offset, double));
  }

  sanei_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
      PFIELD (p, offset, double) = SANE_UNFIX (word);
    }
    sanei_w_free (w, (WireCodecFunc) sanei_w_word, &word);
  }
}

/* --------------------------------------------------------------------- */

static void w_int(Wire *w, void *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
  {
    word = PFIELD (p, offset, int);
  }

  sanei_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
      PFIELD (p, offset, int) = word;
    }
    sanei_w_free (w, (WireCodecFunc) sanei_w_word, &word);
  }
}

/* --------------------------------------------------------------------- */

void preferences_save(int fd)
{
 Wire w;
 int i, n;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  sanei_w_init (&w, sanei_codec_ascii_init);
  sanei_w_set_dir (&w, WIRE_ENCODE);


  for (i = 0; i < NELEMS(desc); ++i)
  {
    sanei_w_string (&w, &desc[i].name);
    (*desc[i].codec) (&w, &preferences, desc[i].offset);
  }

  n=0;

  while (n < preferences.printerdefinitions)
  {
    for (i = 0; i < NELEMS(desc_printer); ++i)
    {
      sanei_w_string (&w, &desc_printer[i].name);
      (*desc_printer[i].codec) (&w, preferences.printer[n], desc_printer[i].offset);
    }
    n++;
  }

  sanei_w_set_dir (&w, WIRE_DECODE);	/* flush it out */
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
  sanei_w_init (&w, sanei_codec_ascii_init);
  sanei_w_set_dir (&w, WIRE_DECODE);


  while (1)
  {
    sanei_w_space (&w, 3);
    if (w.status)
    {
      return;
    }

    sanei_w_string (&w, &name);
    if (w.status || !name)
    {
      return;
    }

    for (i = 0; i < NELEMS (desc); ++i)
    {
      if (strcmp (name, desc[i].name) == 0)
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
      sanei_w_space (&w, 3);
      if (w.status)
      {
        return;
      }

      sanei_w_string (&w, &name);
      if (w.status || !name)
      {
        return;
      }

      if (strcmp (name, desc_printer[i].name) == 0)
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