/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-preferences.h

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
  }
Preferences_printer_t;

typedef struct
  {
    char   *xsane_version_str;		/* xsane-version string */
    char   *tmp_path;			/* path to temporary directory */
    char   *filename;			/* default filename */
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

    char   *doc_viewer;			/* doc viewer for helpfiles */

    double jpeg_quality;		/* quality when saving image as jpeg */
    double png_compression;		/* compression when saving image as pnm */
    int    tiff_compression16_nr;	/* compression type nr when saving 16i bit image as tiff */
    int    tiff_compression8_nr;	/* compression type nr when saving 8 bit image as tiff */
    int    tiff_compression1_nr;	/* compression type nr when saving 1 bit image as tiff */
    int    overwrite_warning;		/* warn if file exists */
    int    skip_existing_numbers;	/* skip used filenames when automatically increase counter */
    int    filename_counter_step;	/* filename_counter += filename_counter_step; */
    int    filename_counter_len;	/* minimum length of filename_counter */
    double psfile_width;		/* width of psfile in mm */
    double psfile_height;		/* height of psfile in mm */
    double psfile_leftoffset;		/* left offset of psfile in mm */
    double psfile_bottomoffset;		/* bottom offset of psfile in mm */

    int    tooltips_enabled;		/* should tooltips be disabled? */
    int    show_histogram;		/* show histogram ? */
    int    show_gamma;			/* show gamma curve ? */
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
    int    auto_enhance_gamma;
    int    gtk_update_policy;

    int    psrotate;			/* rotate by 90 degree in postscript mode - landscape */
    int    printernr;			/* number of printers */
    int    printerdefinitions;
    Preferences_printer_t *printer[10];
  }
Preferences;

extern Preferences preferences;

extern void preferences_save (int fd);
extern void preferences_restore (int fd);

#endif /* preferences_h */
