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

/* ---------------------------------------------------------------------------------------------------------------------- */

/* #define XSANE_TEST */
/* #define SUPPORT_RGBA */

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_VERSION	"0.38"
#define XSANE_AUTHOR	"Oliver Rauch"
#define XSANE_COPYRIGHT	"Oliver Rauch"
#define XSANE_DATE	"1998/1999"
#define XSANE_EMAIL	"Oliver.Rauch@Wolfsburg.DE"

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <getopt.h>
#include <xsane-back-gtk.h>
#include <xsane-preferences.h>
#include <xsane-preview.h>

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
#include <libgimp/gimp.h>

#ifdef HAVE_LIBGIMP_GIMPFEATURES_H
#include <libgimp/gimpfeatures.h>
#else
#define GIMP_CHECK_VERSION(major,m minor, micro) 0
#endif /* HAVE_LIBGIMP_GIMPFEATURES_H */

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

enum { XSANE_SCAN, XSANE_COPY, XSANE_FAX };

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_pref_save(void);
extern void xsane_interface(int argc, char **argv);
extern void xsane_fax_project_save(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

extern const char *prog_name;
extern GtkWidget *choose_device_dialog;
extern GSGDialog *dialog;
extern const SANE_Device **devlist;
extern gint seldev;        /* The selected device */
extern gint ndevs;              /* The number of available devices */

/* ---------------------------------------------------------------------------------------------------------------------- */

extern int xsane_scanmode_number[];

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define OUTFILENAME     	"out.pnm"
#define FAXPROJECT 	    	"faxproject"
#define FAXFILENAME     	"page-001.fax"
#define PRINTERNAME	  	"new printer"
#define PRINTERCOMMAND  	"lpr -"
#define PRINTERCOPYNUMBEROPTION "-#"
#define FAXCOMMAND 	 	"sendfax"
#define FAXRECEIVEROPT		"-d"
#define FAXPOSTSCRIPTOPT	""
#define FAXNORMALOPT		"-l"
#define FAXFINEOPT		"-m"
#define FAXVIEWER 	 	"xv"
#define DOCVIEWERNETSCAPEREMOTE	"netscape-remote"
#define DOCVIEWER 	 	DOCVIEWERNETSCAPEREMOTE	

#define XSANE_BRIGHTNESS_MIN	-400.0
#define XSANE_BRIGHTNESS_MAX	400.0
#define XSANE_CONTRAST_GRAY_MIN	-100.0
#define XSANE_CONTRAST_MIN	-400.0
#define XSANE_CONTRAST_MAX	400.0
#define XSANE_GAMMA_MIN		0.3
#define XSANE_GAMMA_MAX		3.0

#define HIST_WIDTH		256
#define HIST_HEIGHT		100
#define XSANE_DIALOG_WIDTH	296
#define XSANE_DIALOG_HEIGHT	451
#define XSANE_DIALOG_POS_X	10
#define XSANE_DIALOG_POS_X2	316
#define XSANE_DIALOG_POS_Y	50
#define XSANE_DIALOG_POS_Y2	528

#define XSANE_SLIDER_ACTIVE	0
#define XSANE_SLIDER_INACTIVE	4
#define XSANE_SLIDER_WIDTH	260
#define XSANE_SLIDER_HEIGHT	10
#define XSANE_SLIDER_OFFSET	2
#define XSANE_SLIDER_EVENTS	GDK_EXPOSURE_MASK | GDK_ENTER_NOTIFY_MASK | \
				GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | \
				GDK_BUTTON1_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK
#define INF			5.0e9

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef SANE_NAME_DOCUMENT_FEEDER
#define SANE_NAME_DOCUMENT_FEEDER "Automatic Document Feeder"
#endif 

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
 XSANE_UNKNOWN, XSANE_PNM, XSANE_JPEG, XSANE_PNG, XSANE_PS, XSANE_TIFF, XSANE_RGBA,
 XSANE_RAW16, XSANE_PNM16
};
 
/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
 XSANE_STANDALONE, XSANE_GIMP_EXTENSION
};

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsaneProgress_t
{
    GtkSignalFunc callback;
    gpointer callback_data;
    GtkWidget *shell;
    GtkWidget *pbar;
} XsaneProgress_t;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsanePixmap
{
  GtkWidget *frame;
  GdkPixmap *pixmap;
  GtkWidget *pixmapwid;
} XsanePixmap;

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

typedef struct Xsane
{
    SANE_Int sane_backend_versioncode;
    char *backend;
    char *device_set_filename;

    SANE_Int sensitivity;

    /* dialogs */
    GtkWidget *shell;
    GtkWidget *standard_options_shell;
    GtkWidget *advanced_options_shell;
    GtkWidget *main_dialog_scrolled;
    GtkWidget *histogram_dialog;
    GtkWidget *fax_dialog;
    GtkWidget *fax_list;

    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *info_label;
    GtkObject *start_button;
    Preview   *preview;
    gint32    mode;

    int main_window_fixed;
    int mode_selection;

    /* various scanning related state: */
    size_t num_bytes;
    size_t bytes_read;
    XsaneProgress_t *progress;
    int input_tag;
    SANE_Parameters param;
    int x, y;

    /* for standalone mode: */
    GtkWidget *filename_entry;
    GtkWidget *fax_project_entry;
    GtkWidget *fax_receiver_entry;
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

    int negative;
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
    double auto_white_red;
    double auto_gray_red;
    double auto_black_red;
    double auto_white_green;
    double auto_gray_green;
    double auto_black_green;
    double auto_white_blue;
    double auto_gray_blue;
    double auto_black_blue;

    int histogram_red;
    int histogram_green;
    int histogram_blue;
    int histogram_int;
    int histogram_lines;
    int histogram_log;

    /* colors */
    GdkGC *gc_red;
    GdkGC *gc_green;
    GdkGC *gc_blue;
    GdkGC *gc_black;
    GdkGC *gc_trans;
    GdkGC *gc_backg;
    GdkColor *bg_trans;

    int copy_number;
    double zoom;
    double resolution;

    GtkWidget *length_unit_widget;
    GtkWidget *length_unit_mm;
    GtkWidget *length_unit_cm;
    GtkWidget *length_unit_in;
    GtkWidget *update_policy_continu;
    GtkWidget *update_policy_discont;
    GtkWidget *update_policy_delayed;
    GtkWidget *show_preview_widget;
    GtkWidget *show_histogram_widget;
    GtkWidget *show_standard_options_widget;
    GtkWidget *show_advanced_options_widget;
    GtkWidget *show_resolution_list_widget;
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

    SANE_Int xsane_color;
    SANE_Bool show_preview;
    SANE_Bool scanner_gamma_color;
    SANE_Bool scanner_gamma_gray;
    SANE_Bool enhancement_rgb_default;

    SANE_Bool fax_fine_mode;

    GtkWidget *outputfilename_entry;
    GtkWidget *copy_number_entry;

    SANE_Int *gamma_data, *gamma_data_red, *gamma_data_green, *gamma_data_blue;
    SANE_Int *preview_gamma_data_red, *preview_gamma_data_green, *preview_gamma_data_blue;
    SANE_Int *histogram_gamma_data_red, *histogram_gamma_data_green, *histogram_gamma_data_blue;

    char *dummy_filename;

    char *fax_filename;
    char *fax_receiver;

    int block_update_param;

    int broken_pipe; /* for printercommand pipe */

#ifdef HAVE_LIBGIMP_GIMP_H
    /* for GIMP mode: */
    gint32 image_ID;
    GDrawable *drawable;
    guchar *tile;
    unsigned tile_offset;
    GPixelRgn region;
    int first_frame;		/* used for RED/GREEN/BLUE frames */
#endif
} Xsane;

extern struct Xsane xsane;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct XsaneSetup
{
  GtkWidget *printer_name_entry;
  GtkWidget *printer_command_entry;
  GtkWidget *printer_copy_number_option_entry;
  GtkWidget *printer_resolution_entry;
  GtkWidget *printer_leftoffset_entry;
  GtkWidget *printer_bottomoffset_entry;
  GtkWidget *printer_gamma_entry;
  GtkWidget *printer_gamma_red_entry;
  GtkWidget *printer_gamma_green_entry;
  GtkWidget *printer_gamma_blue_entry;
  GtkWidget *printer_width_entry;
  GtkWidget *printer_height_entry;

  GtkWidget *jpeg_image_quality_scale;
  GtkWidget *pnm_image_compression_scale;
  GtkWidget *overwrite_warning_button;
  GtkWidget *increase_filename_counter_button;
  GtkWidget *skip_existing_numbers_button;

  GtkWidget *main_window_fixed_button;

  GtkWidget *preview_gamma_entry;
  GtkWidget *preview_gamma_red_entry;
  GtkWidget *preview_gamma_green_entry;
  GtkWidget *preview_gamma_blue_entry;
  GtkWidget *preview_preserve_button;
  GtkWidget *preview_own_cmap_button;
  GtkWidget *doc_viewer_entry;

  GtkWidget *fax_command_entry;
  GtkWidget *fax_receiver_option_entry;
  GtkWidget *fax_postscript_option_entry;
  GtkWidget *fax_normal_option_entry;
  GtkWidget *fax_fine_option_entry;
  GtkWidget *fax_viewer_entry;

  int tiff_compression_nr;
  int tiff_compression_1_nr;
} XsaneSetup;

extern struct XsaneSetup xsane_setup;

/* ---------------------------------------------------------------------------------------------------------------------- */

#endif
