/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   Author:
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>

   Copyright (C) 1998,1999 Oliver Rauch

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

#ifndef XSANE_H
#define XSANE_H

#include <xsane-gtk.h>
#include <xsane-preferences.h>
#include <xsane-preview.h>


#ifdef HAVE_LIBGIMP_GIMP_H
# include <libgimp/gimp.h>
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define OUTFILENAME     	"out.pnm"
#define PRINTERCOMMAND  	"lpr -"
#define HIST_WIDTH		256
#define HIST_HEIGHT		100
#define XSANE_DIALOG_WIDTH	296
#define XSANE_DIALOG_HEIGHT	416
#define XSANE_DIALOG_POS_X	10
#define XSANE_DIALOG_POS_X2	316
#define XSANE_DIALOG_POS_Y	50
#define XSANE_DIALOG_POS_Y2	492

#define XSANE_SLIDER_ACTIVE	-1
#define XSANE_SLIDER_INACTIVE	-2
#define XSANE_SLIDER_WIDTH	260
#define XSANE_SLIDER_HEIGHT	12
#define XSANE_SLIDER_OFFSET	2
#define XSANE_SLIDER_EVENTS	GDK_EXPOSURE_MASK | GDK_ENTER_NOTIFY_MASK | \
				GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | \
				GDK_BUTTON1_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
 XSANE_UNKNOWN, XSANE_PNM, XSANE_JPEG, XSANE_PNG, XSANE_PS, XSANE_TIFF,
 XSANE_RAW16, XSANE_PNM16
};
 
/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
 STANDALONE, GIMP_EXTENSION
};

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsaneSlider
{
  int position[3];
  double value[3];
  double min, max;
  int active;
  GtkWidget *preview;
  int r, g, b;
} XsaneSlider;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsanePixmap
{
  GtkWidget *frame;
  GdkPixmap *pixmap;
  GtkWidget *pixmapwid;
} XsanePixmap;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsaneProgress_t
{
    GtkSignalFunc callback;
    gpointer callback_data;
    GtkWidget *shell;
    GtkWidget *pbar;
} XsaneProgress_t;

/* ---------------------------------------------------------------------------------------------------------------------- */

struct
{
    SANE_Int sane_backend_versioncode;

    GtkWidget *shell;
    GtkWidget *standard_options_shell;
    GtkWidget *advanced_options_shell;
    GtkWidget *main_dialog_scrolled;
    GtkWidget *histogram_dialog;

    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *info_label;
    Preview *preview;
    gint32 mode;

    /* various scanning related state: */
    size_t num_bytes;
    size_t bytes_read;
    XsaneProgress_t *progress;
    int input_tag;
    SANE_Parameters param;
    int x, y;

    /* for standalone mode: */
    GtkWidget *filename_entry;
    FILE *out;
    int xsane_mode;
    int xsane_output_format;
    long header_size;

    /* histogram window */
    struct XsanePixmap histogram_raw;
    struct XsanePixmap histogram_enh;

    struct XsaneSlider slider_gray;
    struct XsaneSlider slider_red;
    struct XsaneSlider slider_green;
    struct XsaneSlider slider_blue;

    GdkGC *gc_red;
    GdkGC *gc_green;
    GdkGC *gc_blue;
    GdkGC *gc_white;
    GdkGC *gc_black;
    GdkGC *gc_trans;
    GdkGC *gc_backg;
    GdkColor *bg_trans;

    double zoom;
    double resolution;

    double gamma;
    double gamma_red;
    double gamma_green;
    double gamma_blue;
    double brightness;
    double brightness_red;
    double brightness_green;
    double brightness_blue;
    double contrast;
    double contrast_red;
    double contrast_green;
    double contrast_blue;
    double auto_white;
    double auto_gray;
    double auto_black;

    int histogram_red;
    int histogram_green;
    int histogram_blue;
    int histogram_int;
    int histogram_lines;
    int histogram_log;

    GtkWidget *show_histogram_widget;
    GtkWidget *show_standard_options_widget;
    GtkWidget *show_advanced_options_widget;
    GtkObject *zoom_widget;
    GtkObject *resolution_widget;
    GtkObject *gamma_widget;
    GtkObject *gamma_red_widget;
    GtkObject *gamma_green_widget;
    GtkObject *gamma_blue_widget;
    GtkObject *brightness_widget;
    GtkObject *brightness_red_widget;
    GtkObject *brightness_green_widget;
    GtkObject *brightness_blue_widget;
    GtkObject *contrast_widget;
    GtkObject *contrast_red_widget;
    GtkObject *contrast_green_widget;
    GtkObject *contrast_blue_widget;

    SANE_Bool xsane_color;
    SANE_Bool scanner_gamma_color;
    SANE_Bool scanner_gamma_gray;
    SANE_Bool enhancement_rgb_default;

    GtkWidget *outputfilename_entry;

    SANE_Int *gamma_data, *gamma_data_red, *gamma_data_green, *gamma_data_blue;
    SANE_Int *preview_gamma_data_red, *preview_gamma_data_green, *preview_gamma_data_blue;

#ifdef HAVE_LIBGIMP_GIMP_H
    /* for GIMP mode: */
    gint32 image_ID;
    GDrawable *drawable;
    guchar *tile;
    unsigned tile_offset;
    GPixelRgn region;
    int first_frame;		/* used for RED/GREEN/BLUE frames */
#endif
} xsane;

/* ---------------------------------------------------------------------------------------------------------------------- */

struct
{
  GtkWidget *printer_command_entry;
  GtkWidget *printer_resolution_entry;
  GtkWidget *printer_leftoffset_entry;
  GtkWidget *printer_bottomoffset_entry;
  GtkWidget *printer_width_entry;
  GtkWidget *printer_height_entry;
  GtkWidget *jpeg_image_quality_scale;
  GtkWidget *pnm_image_compression_scale;
  GtkWidget *overwrite_warning_button;
  GtkWidget *increase_filename_counter_button;
  GtkWidget *skip_existing_numbers_button;
  GtkWidget *preview_gamma_entry;
  GtkWidget *preview_preserve_button;
  GtkWidget *preview_own_cmap_button;
} xsane_setup;

/* ---------------------------------------------------------------------------------------------------------------------- */

enum { XSANE_SCAN, XSANE_COPY, XSANE_FAX };

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_progress_update(XsaneProgress_t *p, gfloat newval);
extern void xsane_update_histogram();
extern void xsane_create_gamma_curve(SANE_Int *gammadata, double gamma,
                                     double brightness, double contrast, int numbers, int maxout);

/* ---------------------------------------------------------------------------------------------------------------------- */
#endif
