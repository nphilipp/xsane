/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preferences.c

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

/* --------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-rc-io.h"

#ifdef HAVE_LIBTIFF
# include <tiffio.h>
#else
# define COMPRESSION_PACKBITS	32773
# define COMPRESSION_JPEG	7
# define COMPRESSION_CCITTFAX3	2
#endif

/* --------------------------------------------------------------------- */

#define POFFSET(field)	((char *) &((Preferences *) 0)->field - (char *) 0)
#define PFIELD(p,offset,type)	(*((type *)(((char *)(p)) + (offset))))

#define PRTOFFSET(field)	((char *) &((Preferences_printer_t *) 0)->field - (char *) 0)
#define PAREAOFFSET(field)	((char *) &((Preferences_preset_area_t *) 0)->field - (char *) 0)
#define PMEDIUMOFFSET(field)	((char *) &((Preferences_medium_t *) 0)->field - (char *) 0)

/* --------------------------------------------------------------------- */

/* *** ATTENTION *** : DO NOT ENTER STRINGS HERE !!! */

Preferences preferences =
  {
       0,		/* xsane-version string */
       XSANE_VIEWER,    /* default xsane_mode viewer */
       0,		/* default path to temporary directory (not defined here) */
       0,		/* default working_directory */
       0,		/* no default filename */
       0,		/* no default filetype */
       0,		/* default cms_function: embed scanner profile  */
       0,		/* default cms_intent: perceptual  */
       0,		/* default cms_bpc (black point compensation) off  */
       0137,		/* image umask (permission mask for -rw-r------) */
       0027,		/* directory umask (permission mask for -rwxr-x----) */
       0,		/* no fax project */
       0,		/* no default faxcommand */
       0,		/* no default fax receiver option */
       0,		/* no default fax postscript option */
       0,		/* no default fax normal option */
       0,		/* no default fax fine option */
       0,		/* no fax viewer */
     215.70,            /* fax_width:  width of fax paper in mm */
     296.98,		/* fax_height: height of fax paper in mm */
       0.0,             /* fax_leftoffset */
       0.0,             /* fax_bottomoffset */
       1,               /* fax_fine_mode */
       1,               /* fax_ps_flatedecoded */
#ifdef XSANE_ACTIVATE_EMAIL
       0,               /* no default from email address */
       0,               /* no default reply to email address */
       0,               /* no default smtp server */
       25,              /* default smtp port */
       0,               /* no email authentication */
       0,               /* no default email authorization user */
       0,               /* no default email authorization passsword */
       0,               /* no default pop3 server */
       110,             /* default pop3 port */
       0,		/* no email project */
       0,		/* no email filetype */
#endif
       0,		/* no multipage project */
       0,		/* no multipage filetype */
       0,		/* no default ocrcommand */
       0,		/* no default ocr input file option */
       0,		/* no default ocr output file option */
       0,		/* do not use progress pipe */
       0,		/* no option for progress pipe filedeskriptor */
       0,		/* no progress keyword */
       0,		/* no doc viewer */
      80.0,		/* jpeg_quality */
       7.0,		/* png_compression */
       6.0,		/* tiff_zip_compression */
 COMPRESSION_PACKBITS,	/* tiff_compression16_nr */
 COMPRESSION_JPEG,	/* tiff_compression8_nr */
 COMPRESSION_CCITTFAX3,	/* tiff_compression1_nr */
       1,		/* save_devprefs_at_exit */
       1,		/* overwrite_warning */
       1,		/* skip_existing_numbers */
       1,               /* save_ps_flatedecoded */
       1,               /* save_pdf_flatedecoded */
       0,		/* save_pnm16_as_ascii */
       0,		/* reduce_16bit_to_8bit */
       1,		/* filename_counter_step */
       4,		/* filename_counter_len */
       1,		/* adf_pages_max */
       6,		/* show_range_mode */
       1,		/* tooltips enabled */
       1,		/* show histogram */
       1,		/* show gamma */
       0,		/* don't show batch scan */
       1,		/* show standard options */
       0,		/* don`t show advanced options */
       0,		/* don`t show resolution list */
      10.0,		/* length unit */
       1,		/* main window fixed (1) or scrolled (0) */
       0,		/* preview_own_cmap */
       1.5,		/* preview_oversampling */
       1.0,		/* preview_gamma */
       1.0,		/* preview_gamma_red */
       1.0,		/* preview_gamma_green */
       1.0,		/* preview_gamma_blue */
       1,		/* disable_gimp_preview_gamma */
       12,		/* preview_gamma_input_bits */
       3,		/* preview_pipette_range */
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
       1,		/* rgb default */
       0,		/* negative */
       1,		/* autoenhance_gamma: change gamma value with autoenhance button */
       1,		/* preselect_scan_area after preview scan */
       1,		/* auto_correct_colors after preview scan */
 GTK_UPDATE_DISCONTINUOUS, /* update policy for gtk frontend sliders */
       0,		/* medium_nr */
       0,		/* paper_orientation */
       0,		/* preset_area_definitions */
       0,		/* printernr */
       0		/* printerdefinitions */
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
    {"xsane-version",			xsane_rc_pref_string,	POFFSET(xsane_version_str)},
    {"xsane-mode",			xsane_rc_pref_int,	POFFSET(xsane_mode)},
    {"tmp-path",			xsane_rc_pref_string,	POFFSET(tmp_path)},
    {"working-directory",		xsane_rc_pref_string,	POFFSET(working_directory)},
    {"filename",			xsane_rc_pref_string,	POFFSET(filename)},
    {"filetype",			xsane_rc_pref_string,	POFFSET(filetype)},
    {"cms-function",			xsane_rc_pref_int,	POFFSET(cms_function)},
    {"cms-intent",			xsane_rc_pref_int,	POFFSET(cms_intent)},
    {"cms-bpc",				xsane_rc_pref_int,	POFFSET(cms_bpc)},
    {"image-umask",			xsane_rc_pref_int,	POFFSET(image_umask)},
    {"directory-umask",			xsane_rc_pref_int,	POFFSET(directory_umask)},
    {"fax-project",			xsane_rc_pref_string,	POFFSET(fax_project)},
    {"fax-command",			xsane_rc_pref_string,	POFFSET(fax_command)},
    {"fax-receiver-option",		xsane_rc_pref_string,	POFFSET(fax_receiver_option)},
    {"fax-postscript-option",		xsane_rc_pref_string,	POFFSET(fax_postscript_option)},
    {"fax-normal-option",		xsane_rc_pref_string,	POFFSET(fax_normal_option)},
    {"fax-fine-option",			xsane_rc_pref_string,	POFFSET(fax_fine_option)},
    {"fax-viewer",			xsane_rc_pref_string,	POFFSET(fax_viewer)},
    {"fax-width",			xsane_rc_pref_double,	POFFSET(fax_width)},
    {"fax-height",			xsane_rc_pref_double,	POFFSET(fax_height)},
    {"fax-left-offset",			xsane_rc_pref_double,	POFFSET(fax_leftoffset)},
    {"fax-bottom-offset",		xsane_rc_pref_double,	POFFSET(fax_bottomoffset)},
    {"fax-fine-mode",			xsane_rc_pref_int,	POFFSET(fax_fine_mode)},
    {"fax-ps-flatedecoded",		xsane_rc_pref_int,	POFFSET(fax_ps_flatedecoded)},
#ifdef XSANE_ACTIVATE_EMAIL
    {"e-mail-from",			xsane_rc_pref_string,	POFFSET(email_from)},
    {"e-mail-reply-to",			xsane_rc_pref_string,	POFFSET(email_reply_to)},
    {"e-mail-smtp-server",		xsane_rc_pref_string,	POFFSET(email_smtp_server)},
    {"e-mail-smtp-port",		xsane_rc_pref_int,	POFFSET(email_smtp_port)},
    {"e-mail-authentication",		xsane_rc_pref_int,	POFFSET(email_authentication)},
    {"e-mail-auth-user",		xsane_rc_pref_string,	POFFSET(email_auth_user)},
    {"e-mail-auth-pass",		xsane_rc_pref_string,	POFFSET(email_auth_pass)},
    {"e-mail-pop3-server",		xsane_rc_pref_string,	POFFSET(email_pop3_server)},
    {"e-mail-pop3-port",		xsane_rc_pref_int,	POFFSET(email_pop3_port)},
    {"e-mail-project",			xsane_rc_pref_string,	POFFSET(email_project)},
    {"e-mail-filetype",			xsane_rc_pref_string,	POFFSET(email_filetype)},
#endif
    {"multipage-project",		xsane_rc_pref_string,	POFFSET(multipage_project)},
    {"multipage-filetype",		xsane_rc_pref_string,	POFFSET(multipage_filetype)},
    {"ocr-command",			xsane_rc_pref_string,	POFFSET(ocr_command)},
    {"ocr-inputfile-option",		xsane_rc_pref_string,	POFFSET(ocr_inputfile_option)},
    {"ocr-outputfile-options",		xsane_rc_pref_string,	POFFSET(ocr_outputfile_option)},
    {"ocr-use-gui-pipe",		xsane_rc_pref_int,	POFFSET(ocr_use_gui_pipe)},
    {"ocr-gui-outfd-option",		xsane_rc_pref_string,	POFFSET(ocr_gui_outfd_option)},
    {"ocr-progress-keyword",		xsane_rc_pref_string,	POFFSET(ocr_progress_keyword)},
    {"browser",				xsane_rc_pref_string,	POFFSET(browser)},
    {"jpeg-quality",			xsane_rc_pref_double,	POFFSET(jpeg_quality)},
    {"png-compression",			xsane_rc_pref_double, 	POFFSET(png_compression)},
    {"tiff-zip-compression",		xsane_rc_pref_double,	POFFSET(tiff_zip_compression)},
    {"tiff-compression16_nr",		xsane_rc_pref_int, 	POFFSET(tiff_compression16_nr)},
    {"tiff-compression8_nr",		xsane_rc_pref_int, 	POFFSET(tiff_compression8_nr)},
    {"tiff-compression1_nr",		xsane_rc_pref_int, 	POFFSET(tiff_compression1_nr)},
    {"save-devprefs-at-exit",		xsane_rc_pref_int,	POFFSET(save_devprefs_at_exit)},
    {"overwrite-warning",		xsane_rc_pref_int,	POFFSET(overwrite_warning)},
    {"skip-existing-numbers",		xsane_rc_pref_int,	POFFSET(skip_existing_numbers)},
    {"save-ps-flatedecoded",		xsane_rc_pref_int,	POFFSET(save_ps_flatedecoded)},
    {"save-pdf-flatedecoded",		xsane_rc_pref_int,	POFFSET(save_pdf_flatedecoded)},
    {"save-pnm16-as-ascii",		xsane_rc_pref_int,	POFFSET( save_pnm16_as_ascii)},
    {"reduce-16bit-to8bit",		xsane_rc_pref_int,	POFFSET(reduce_16bit_to_8bit)},
    {"filename-counter-step",		xsane_rc_pref_int,	POFFSET(filename_counter_step)},
    {"filename-counter-len",		xsane_rc_pref_int,	POFFSET(filename_counter_len)},
    {"adf-pages-max",			xsane_rc_pref_int,	POFFSET(adf_pages_max)},
    {"show-range-mode",			xsane_rc_pref_int,	POFFSET(show_range_mode)},
    {"tool-tips",			xsane_rc_pref_int,	POFFSET(tooltips_enabled)},
    {"show-histogram",			xsane_rc_pref_int,	POFFSET(show_histogram)},
    {"show-gamma",			xsane_rc_pref_int,	POFFSET(show_gamma)},
    {"show-batch-scan",			xsane_rc_pref_int,	POFFSET(show_batch_scan)},
    {"show-standard-options",		xsane_rc_pref_int,	POFFSET(show_standard_options)},
    {"show-advanced-options",		xsane_rc_pref_int,	POFFSET(show_advanced_options)},
    {"show-resolution-list",		xsane_rc_pref_int,	POFFSET(show_resolution_list)},
    {"length-unit",			xsane_rc_pref_double,	POFFSET(length_unit)},
    {"main-window-fixed",		xsane_rc_pref_int,	POFFSET(main_window_fixed)},
    {"display-icm-profile",		xsane_rc_pref_string,	POFFSET(display_icm_profile)},
    {"custom-proofing-icm-profile",	xsane_rc_pref_string,	POFFSET(custom_proofing_icm_profile)},
    {"working-color-space-icm-profile",	xsane_rc_pref_string,	POFFSET(working_color_space_icm_profile)},
    {"preview-own-cmap",		xsane_rc_pref_int,	POFFSET(preview_own_cmap)},
    {"preview-oversampling",		xsane_rc_pref_double,	POFFSET(preview_oversampling)},
    {"preview-gamma",			xsane_rc_pref_double,	POFFSET(preview_gamma)},
    {"preview-gamma-red",		xsane_rc_pref_double,	POFFSET(preview_gamma_red)},
    {"preview-gamma-green",		xsane_rc_pref_double,	POFFSET(preview_gamma_green)},
    {"preview-gamma-blue",		xsane_rc_pref_double,	POFFSET(preview_gamma_blue)},
    {"disable-gimp-preview-gamma",	xsane_rc_pref_int,	POFFSET(disable_gimp_preview_gamma)},
    {"preview-gamma-input-bits",	xsane_rc_pref_int,	POFFSET(preview_gamma_input_bits)},
    {"preview-pipette-range",		xsane_rc_pref_int,	POFFSET(preview_pipette_range)},
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
    {"auto-enhance-gamma",		xsane_rc_pref_int,	POFFSET(auto_enhance_gamma)},
    {"preselect-scan-area",		xsane_rc_pref_int,	POFFSET(preselect_scan_area)},
    {"auto-correct-colors",		xsane_rc_pref_int,	POFFSET(auto_correct_colors)},
    {"gtk-update-policy",		xsane_rc_pref_int,	POFFSET(gtk_update_policy)},
    {"medium-nr",			xsane_rc_pref_int,	POFFSET(medium_nr)},
    {"paper-orientation",		xsane_rc_pref_int,	POFFSET(paper_orientation)},
    {"preset-area-definitions",		xsane_rc_pref_int,	POFFSET(preset_area_definitions)},
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
    {"printer-lineart-resolution",	xsane_rc_pref_int,	PRTOFFSET(lineart_resolution)},
    {"printer-grayscale-resolution",	xsane_rc_pref_int,	PRTOFFSET(grayscale_resolution)},
    {"printer-color-resolution",	xsane_rc_pref_int,	PRTOFFSET(color_resolution)},
    {"printer-width",			xsane_rc_pref_double,	PRTOFFSET(width)},
    {"printer-height",			xsane_rc_pref_double,	PRTOFFSET(height)},
    {"printer-left-offset",		xsane_rc_pref_double,	PRTOFFSET(leftoffset)},
    {"printer-bottom-offset",		xsane_rc_pref_double,	PRTOFFSET(bottomoffset)},
    {"printer-gamma",			xsane_rc_pref_double,	PRTOFFSET(gamma)},
    {"printer-gamma-red",		xsane_rc_pref_double,	PRTOFFSET(gamma_red)},
    {"printer-gamma-green",		xsane_rc_pref_double,	PRTOFFSET(gamma_green)},
    {"printer-gamma-blue",		xsane_rc_pref_double,	PRTOFFSET(gamma_blue)},
    {"printer-icm-profile",		xsane_rc_pref_string,	PRTOFFSET(icm_profile)},
    {"printer-ps-flatedecoded",		xsane_rc_pref_int,	PRTOFFSET(ps_flatedecoded)},
    {"printer-embed-csa",		xsane_rc_pref_int,	PRTOFFSET(embed_csa)},
    {"printer-embed-crd",		xsane_rc_pref_int,	PRTOFFSET(embed_crd)},
    {"printer-cms-bpc",			xsane_rc_pref_int,	PRTOFFSET(cms_bpc)}
  };

/* --------------------------------------------------------------------- */

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc_preset_area[] =
  {
    {"preset-area-name",		xsane_rc_pref_string,	PAREAOFFSET(name)},
    {"preset-area-xoffset",		xsane_rc_pref_double,	PAREAOFFSET(xoffset)},
    {"preset-area-yoffset",		xsane_rc_pref_double,	PAREAOFFSET(yoffset)},
    {"preset-area-width",		xsane_rc_pref_double,	PAREAOFFSET(width)},
    {"preset-area-height",		xsane_rc_pref_double,	PAREAOFFSET(height)}
  };

/* --------------------------------------------------------------------- */

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, void *p, long offset);
    long offset;
  }
desc_medium[] =
  {
    {"medium-name",			xsane_rc_pref_string,	PMEDIUMOFFSET(name)},
    {"medium-shadow-gray",		xsane_rc_pref_double,	PMEDIUMOFFSET(shadow_gray)},
    {"medium-shadow-red",		xsane_rc_pref_double,	PMEDIUMOFFSET(shadow_red)},
    {"medium-shadow-green",		xsane_rc_pref_double,	PMEDIUMOFFSET(shadow_green)},
    {"medium-shadow-blue",		xsane_rc_pref_double,	PMEDIUMOFFSET(shadow_blue)},
    {"medium-highlight-gray",		xsane_rc_pref_double,	PMEDIUMOFFSET(highlight_gray)},
    {"medium-highlight-red",		xsane_rc_pref_double,	PMEDIUMOFFSET(highlight_red)},
    {"medium-highlight-green",		xsane_rc_pref_double,	PMEDIUMOFFSET(highlight_green)},
    {"medium-highlight-blue",		xsane_rc_pref_double,	PMEDIUMOFFSET(highlight_blue)},
    {"medium-gamma-gray",		xsane_rc_pref_double,	PMEDIUMOFFSET(gamma_gray)},
    {"medium-gamma-red",		xsane_rc_pref_double,	PMEDIUMOFFSET(gamma_red)},
    {"medium-gamma-green",		xsane_rc_pref_double,	PMEDIUMOFFSET(gamma_green)},
    {"medium-gamma-blue",		xsane_rc_pref_double,	PMEDIUMOFFSET(gamma_blue)},
    {"medium-negative",			xsane_rc_pref_int,	PMEDIUMOFFSET(negative)}
  };

/* --------------------------------------------------------------------- */

void preferences_save(int fd)
{
 Wire w;
 int i, n;

  DBG(DBG_proc, "preferences_save\n");

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

  for (i = 0; i < NELEMS(desc); ++i)
  {
    DBG(DBG_info2, "saving preferences value for %s\n", desc[i].name);
    xsane_rc_io_w_string(&w, &desc[i].name);
    (*desc[i].codec) (&w, &preferences, desc[i].offset);
  }

  /* save printers */

  n=0;

  while (n < preferences.printerdefinitions)
  {
    DBG(DBG_info2, "saving preferences printer definition %s\n", preferences.printer[n]->name);
    for (i = 0; i < NELEMS(desc_printer); ++i)
    {
      xsane_rc_io_w_string(&w, &desc_printer[i].name);
      (*desc_printer[i].codec) (&w, preferences.printer[n], desc_printer[i].offset);
    }
    n++;
  }

  /* save preset areas */

  n=1; /* start with number 1, number 0 (full size) is not saved */

  while (n < preferences.preset_area_definitions)
  {
    DBG(DBG_info2, "saving preferences preset area definition %s\n", preferences.preset_area[n]->name);
    for (i = 0; i < NELEMS(desc_preset_area); ++i)
    {
      xsane_rc_io_w_string(&w, &desc_preset_area[i].name);
      (*desc_preset_area[i].codec) (&w, preferences.preset_area[n], desc_preset_area[i].offset);
    }
    n++;
  }

  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);	/* flush it out */
  xsane_rc_io_w_exit(&w);
}

/* --------------------------------------------------------------------- */

void preferences_restore(int fd)
{
 SANE_String name;
 Wire w;
 int i, n;
 int item_identified = 1;

  DBG(DBG_proc, "preferences_restore\n");

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
      xsane_rc_io_w_exit(&w);
      preferences.printerdefinitions = 0;
      preferences.preset_area_definitions = 0;
     return;
    }

    xsane_rc_io_w_string(&w, &name);
    if (w.status == EINVAL) /* not a string */
    {
      w.status = 0;
      xsane_rc_io_w_skip_newline(&w); /* skip this line */
    }
    else if (w.status || !name) /* other error */
    {
      xsane_rc_io_w_exit(&w);
     return;
    }
    else /* identifier read */
    {
      for (i = 0; i < NELEMS(desc); ++i)
      {
        if (strcmp(name, desc[i].name) == 0)
        {
          DBG(DBG_info2, "reading preferences value for %s\n", desc[i].name);
          (*desc[i].codec) (&w, &preferences, desc[i].offset);
          break;
        }
      }

      if ( (!strcmp(name, "printer-name")) || (!strcmp(name, "preset-area-name")) )
      {
        break;
      }
      else if (i == NELEMS(desc))
      {
        DBG(DBG_info2, "preferences identifier %s unknown\n", name);
        xsane_rc_io_w_skip_newline(&w); /* skip this line */
      }
    }
  }

  preferences.printer = NULL;
  preferences.printer = calloc(1, sizeof(void *)); 
  n=-1;
  item_identified = 1;
  while (item_identified)
  {
    if (strcmp(name, desc_printer[0].name) == 0)
    {
      n++;
      preferences.printer = realloc(preferences.printer, (n+1)*sizeof(void *)); 
      preferences.printer[n] = calloc(sizeof(Preferences_printer_t), 1);
      (*desc_printer[0].codec) (&w, preferences.printer[n], desc_printer[0].offset);
      DBG(DBG_info2, "reading preferences printer definition %s\n", preferences.printer[n]->name);
      item_identified = 1;
    }
    else
    {
      item_identified = 0;
      for (i = 1; i < NELEMS(desc_printer); ++i)
      {
        if (strcmp(name, desc_printer[i].name) == 0)
        {
          (*desc_printer[i].codec) (&w, preferences.printer[n], desc_printer[i].offset);
          item_identified = 1;
        }
      }
    }

    if (item_identified)
    {
      xsane_rc_io_w_space(&w, 3);
      if (w.status)
      {
        xsane_rc_io_w_exit(&w);
       return;
      }

      xsane_rc_io_w_string(&w, &name);
      if (w.status || !name)
      {
        xsane_rc_io_w_exit(&w);
       return;
      }
    }
  }
  preferences.printerdefinitions = n+1;
  /* finished reading printer definitions */



  n=-1;
  if (strcmp(name, desc_preset_area[0].name) == 0) /* at least one preset area definition ? */
  {
    /* then we define "Full size" as definition 0 */
    n++;
    preferences.preset_area = calloc(1, sizeof(void *)); 

    preferences.preset_area[0] = calloc(sizeof(Preferences_preset_area_t), 1);
    preferences.preset_area[0]->name    = strdup(_(MENU_ITEM_SURFACE_FULL_SIZE));
    preferences.preset_area[0]->xoffset = 0.0;
    preferences.preset_area[0]->yoffset = 0.0;
    preferences.preset_area[0]->width   = INF;
    preferences.preset_area[0]->height  = INF;
  }

  item_identified = 1;
  while (item_identified)
  {
    if (strcmp(name, desc_preset_area[0].name) == 0)
    {
      n++;
      preferences.preset_area = realloc(preferences.preset_area, (n+1) * sizeof(void *)); 
      preferences.preset_area[n] = calloc(sizeof(Preferences_preset_area_t), 1);
      (*desc_preset_area[0].codec) (&w, preferences.preset_area[n], desc_preset_area[0].offset);
      DBG(DBG_info2, "reading preset area definition %s\n", preferences.preset_area[n]->name);
      item_identified = 1;
    }
    else
    {
      item_identified = 0;

      for (i = 1; i < NELEMS(desc_preset_area); ++i)
      {
        if (strcmp(name, desc_preset_area[i].name) == 0)
        {
          (*desc_preset_area[i].codec) (&w, preferences.preset_area[n], desc_preset_area[i].offset);
          item_identified = 1;
        }
      }
    }

    if (item_identified)
    {
      xsane_rc_io_w_space(&w, 3);
      if (w.status)
      {
        xsane_rc_io_w_exit(&w);
       return;
      }

      xsane_rc_io_w_string(&w, &name);
      if (w.status || !name)
      {
        xsane_rc_io_w_exit(&w);
       return;
      }
    }
  }

  preferences.preset_area_definitions = n+1;

  xsane_rc_io_w_exit(&w);
}

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

void preferences_save_media(int fd)
{
 Wire w;
 int i, n;
 char *medium_defs="MEDIUM_DEFINITIONS";

  DBG(DBG_proc, "preferences_save_media\n");

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

  xsane_rc_io_w_string(&w, &medium_defs);
  xsane_rc_io_w_word(&w, &preferences.medium_definitions);

  /* save media */

  n=0;

  DBG(DBG_info, "saving %d medium definitions\n", preferences.medium_definitions);

  while (n < preferences.medium_definitions)
  {
    DBG(DBG_info2, "=> saving medium definition %s\n", preferences.medium[n]->name);
    for (i = 0; i < NELEMS(desc_medium); ++i)
    {
      xsane_rc_io_w_string(&w, &desc_medium[i].name);
      (*desc_medium[i].codec) (&w, preferences.medium[n], desc_medium[i].offset);
    }
    n++;
  }

  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);	/* flush it out */
  xsane_rc_io_w_exit(&w);
}

/* --------------------------------------------------------------------- */

void preferences_restore_media(int fd)
{
 SANE_String name;
 Wire w;
 int i, n;

  DBG(DBG_proc, "preferences_restore_media\n");

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);

  preferences.medium_definitions = 0;

  xsane_rc_io_w_space(&w, 3);
  if (w.status)
  {
    xsane_rc_io_w_exit(&w);
   return;
  }

  xsane_rc_io_w_string(&w, &name);
  if (w.status || !name)
  {
    xsane_rc_io_w_exit(&w);
   return;
  }

  if (strcmp(name, "MEDIUM_DEFINITIONS")) /* falsche Datei */
  {
    DBG(DBG_info, "no medium definitions in file\n");
    xsane_rc_io_w_exit(&w);
   return;
  }

  xsane_rc_io_w_word(&w, &preferences.medium_definitions);

  if (preferences.medium_definitions)
  {
    DBG(DBG_info, "reading %d medium definition\n", preferences.medium_definitions);
    preferences.medium = calloc(preferences.medium_definitions, sizeof(void *)); 

    n=0;
    while (n < preferences.medium_definitions)
    {
      preferences.medium[n] = calloc(sizeof(Preferences_medium_t), 1);
      for (i = 0; i < NELEMS(desc_medium); ++i)
      {
        xsane_rc_io_w_space(&w, 3);
        if (w.status)
        {
          xsane_rc_io_w_exit(&w);
         return;
        }

        xsane_rc_io_w_string(&w, &name);
        if (w.status || !name)
        {
          xsane_rc_io_w_exit(&w);
         return;
        }

        if (strcmp(name, desc_medium[i].name) == 0)
        {
          (*desc_medium[i].codec) (&w, preferences.medium[n], desc_medium[i].offset);
        }
        else
        {
          break;
        }
      }
      DBG(DBG_info2, "=> medium definition %s read\n", preferences.medium[n]->name);
      n++;
    }
  }

  xsane_rc_io_w_exit(&w);
}

/* --------------------------------------------------------------------- */
