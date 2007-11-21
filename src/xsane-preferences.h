/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preferences.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2007 Oliver Rauch
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

#ifndef xsane_preferences_h
#define xsane_preferences_h

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <sane/sane.h>
#include <gtk/gtk.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    char   *name;			/* user defined printer name */
    char   *command;			/* printercommand */
    char   *copy_number_option;		/* option to define number of copies */
    int    lineart_resolution;		/* printer resolution for lineart mode  */
    int    grayscale_resolution;	/* printer resolution for grayscale mode  */
    int    color_resolution;		/* printer resolution for color mode  */
    double width;			/* printer width of printable area in mm */
    double height;			/* printer height of printable area in mm */
    double leftoffset;			/* printer left offset in mm */
    double bottomoffset;		/* printer bottom offset in mm */
    double gamma;			/* printer gamma */
    double gamma_red;			/* printer gamma red */
    double gamma_green;			/* printer gamma green */
    double gamma_blue;			/* printer gamma blue */
    char   *icm_profile;		/* printer ICM profile */
    int ps_flatedecoded;		/* flatedecode (zlib compression), ps level 3 */
    int embed_csa;			/* CSA = scanner ICM profile for postscript files */
    int embed_crd;			/* CRD = printer ICM profile for postscript files */
    int cms_bpc;			/* bpc */
  }
Preferences_printer_t;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    char *name;
    double xoffset;
    double yoffset;
    double width;
    double height;
  } Preferences_preset_area_t;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
{
  gchar *name;
  double shadow_gray;
  double shadow_red;
  double shadow_green;
  double shadow_blue;
  double highlight_gray;
  double highlight_red;
  double highlight_green;
  double highlight_blue;
  double gamma_gray;
  double gamma_red;
  double gamma_green;
  double gamma_blue;
  int negative;
} Preferences_medium_t;                                                         

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    char   *xsane_version_str;		/* xsane-version string */
    int    xsane_mode;			/* xsane_mode */
    char   *tmp_path;			/* path to temporary directory */
    char   *working_directory;		/* directory where xsane saves images etc */
    char   *filename;			/* default filename */
    char   *filetype;			/* default filetype */
    int    cms_function;		/* cms function (embed/srgb/working cs) */
    int    cms_intent;			/* cms rendering intent */
    int    cms_bpc;			/* cms black point compensation */
    int    image_umask;			/* image umask (permisson mask) */
    int    directory_umask;		/* directory umask (permisson mask) */

    char   *fax_project;		/* fax project */
    char   *fax_command;		/* faxcommand */
    char   *fax_receiver_option;	/* fax receiver option */
    char   *fax_postscript_option;	/* fax postscript option */
    char   *fax_normal_option;		/* fax normal mode option */
    char   *fax_fine_option;		/* fax fine mode option */
    char   *fax_viewer;			/* fax viewer */
    double fax_width;			/* width of fax paper in mm */
    double fax_height;			/* height of fax paper in mm */
    double fax_leftoffset;		/* left offset of fax paper in mm */
    double fax_bottomoffset;		/* bottom offset of fax paper in mm */
    int    fax_fine_mode;		/* use fine or normal mode */
    int    fax_ps_flatedecoded;		/* use postscript level 3 zlib compression */

#ifdef XSANE_ACTIVATE_EMAIL
    char   *email_from;			/* email address of sender */
    char   *email_reply_to;		/* email address for replied emails */
    char   *email_smtp_server;		/* ip address or domain name of smtp server */
    int    email_smtp_port;		/* port to connect to smtp sever */
    int    email_authentication;	/* type for email authentication */
    char   *email_auth_user;		/* user name for email authorization */
    char   *email_auth_pass;		/* password for email authorization */
    char   *email_pop3_server;		/* ip address or domain name of pop3 server */
    int    email_pop3_port;		/* port to connect to pop3 server */
    char   *email_project;		/* mail project */
    char   *email_filetype;		/* mail filetype */
#endif
    char   *multipage_project;		/* multipage project */
    char   *multipage_filetype;		/* multipage filetype */

    char   *ocr_command;		/* ocrcommand */
    char   *ocr_inputfile_option;	/* option for input file */
    char   *ocr_outputfile_option;	/* option for output file */
    int    ocr_use_gui_pipe;		/* use progress pipe */
    char   *ocr_gui_outfd_option;	/* option for progress pipe filedeskriptor */
    char   *ocr_progress_keyword;	/* keyword for progress value in gui pipe */

    char   *browser;			/* doc viewer for helpfiles */

    double jpeg_quality;		/* quality when saving image as jpeg */
    double png_compression;		/* compression when saving image as pnm */
    double tiff_zip_compression;	/* compression rate for tiff zip (deflate) */
    int    tiff_compression16_nr;	/* compression type nr when saving 16i bit image as tiff */
    int    tiff_compression8_nr;	/* compression type nr when saving 8 bit image as tiff */
    int    tiff_compression1_nr;	/* compression type nr when saving 1 bit image as tiff */
    int    save_devprefs_at_exit;	/* save device preferences at exit */
    int    overwrite_warning;		/* warn if file exists */
    int    skip_existing_numbers;	/* skip used filenames when automatically increase counter */
    int    save_ps_flatedecoded;	/* use zlib to for postscript compression (flatedecode) */
    int    save_pdf_flatedecoded;	/* use zlib to for pdf compression (flatedecode) */
    int    save_pnm16_as_ascii;		/* selection if pnm 16 bit is saved as ascii or binary file */
    int    reduce_16bit_to_8bit;	/* reduce images with 16 bits/color to 8 bits/color */
    int    filename_counter_step;	/* filename_counter += filename_counter_step; */
    int    filename_counter_len;	/* minimum length of filename_counter */
    int    adf_pages_max;		/* maximum pages to scan in adf mode */

    int    show_range_mode;		/* how to show a range */
    int    tooltips_enabled;		/* should tooltips be disabled? */
    int    show_histogram;		/* show histogram ? */
    int    show_gamma;			/* show gamma curve ? */
    int    show_batch_scan;		/* show batch scan dialog ? */
    int    show_standard_options;	/* show standard options ? */
    int    show_advanced_options;	/* show advanced options ? */
    int    show_resolution_list;	/* show resolution list instead of slider ? */
    double length_unit;			/* 1.0==mm, 10.0==cm, 25.4==inches, etc. */
    int    main_window_fixed;		/* fixed (1) or scrolled (0) main window */
    int    preview_own_cmap;		/* install colormap for preview */
    double preview_oversampling;	/* resolution faktor for previews */
    double preview_gamma;		/* gamma value for previews */
    double preview_gamma_red;		/* red gamma value for previews */
    double preview_gamma_green;		/* green gamma value for previews */
    double preview_gamma_blue;		/* blue gamma value for previews */
    int    disable_gimp_preview_gamma;	/* fixed (1) or scrolled (0) main window */
    int    preview_gamma_input_bits;	/* handle preview gamma and histogram with # bits */
    int    preview_pipette_range;	/* dimension of a square that is used to pick pipette color */
    double xsane_gamma;
    double xsane_gamma_red;
    double xsane_gamma_green;
    double xsane_gamma_blue;
    double xsane_brightness;
    double xsane_brightness_red;
    double xsane_brightness_green;
    double xsane_brightness_blue;
    double xsane_contrast;
    double xsane_contrast_red;
    double xsane_contrast_green;
    double xsane_contrast_blue;

    int    xsane_rgb_default;
    int    xsane_negative;
    int    auto_enhance_gamma;		/* change gamma value with automatic color correction */
    int    preselect_scan_area;		/* automatic selection of scan area after preview scan */
    int    auto_correct_colors;		/* automatic color correction after preview scan */
    int    gtk_update_policy;
    int    medium_nr;

    char   *display_icm_profile;
    char   *custom_proofing_icm_profile;
    char   *working_color_space_icm_profile;

    int    paper_orientation;		/* image position on printer and page orientation */
    int    printernr;			/* number of printers */
    int    printerdefinitions;
    Preferences_printer_t **printer;
    int    preset_area_definitions;
    Preferences_preset_area_t **preset_area;
    int    medium_definitions;
    Preferences_medium_t **medium;
  }
Preferences;

extern Preferences preferences;

extern void preferences_save (int fd);
extern void preferences_restore (int fd);

extern void preferences_save_media (int fd);
extern void preferences_restore_media (int fd);

#endif /* preferences_h */
