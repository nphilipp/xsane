/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane.h

   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998-2000 Oliver Rauch
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

#ifndef XSANE_H
#define XSANE_H

/* ---------------------------------------------------------------------------------------------------------------------- */

/* #define XSANE_TEST */
/* #define SUPPORT_RGBA */

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_VERSION		"0.60"
#define XSANE_AUTHOR		"Oliver Rauch"
#define XSANE_COPYRIGHT		"Oliver Rauch"
#define XSANE_DATE		"1998-2000"
#define XSANE_EMAIL		"Oliver.Rauch@Wolfsburg.DE"
#define XSANE_HOMEPAGE		"http://www.wolfsburg.de/~rauch/sane/sane-xsane.html"
#define XSANE_COPYRIGHT_TXT	XSANE_DATE " " XSANE_COPYRIGHT

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_DEFAULT_UMASK 0007
#define XSANE_HOLD_TIME 200
#define XSANE_DEFAULT_DEVICE "SANE_DEFAULT_DEVICE"

/* ---------------------------------------------------------------------------------------------------------------------- */

/* needed for most of the xsane sources: */

#ifdef _AIX
# include <lalloca.h>
#endif

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <pwd.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <locale.h>

#include <sane/sane.h>
#include <sane/saneopts.h>

#include "sane/config.h"
#include "sane/sanei_signal.h"

#include "xsane-text.h"
#include "xsane-fixedtext.h"
#include "xsane-icons.h"

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

/* ----------------------------- */

/* needed for xsane.h */
#include "xsane-back-gtk.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
# include <libgimp/gimp.h>

# ifdef HAVE_LIBGIMP_GIMPFEATURES_H
#  include <libgimp/gimpfeatures.h>
# else
#  define GIMP_CHECK_VERSION(major, minor, micro) 0
# endif /* HAVE_LIBGIMP_GIMPFEATURES_H */

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

enum { XSANE_SCAN, XSANE_COPY, XSANE_FAX };
enum { XSANE_LINEART_STANDARD, XSANE_LINEART_XSANE, XSANE_LINEART_GRAYSCALE };

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_pref_save(void);
extern void xsane_interface(int argc, char **argv);
extern void xsane_fax_project_save(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

extern const char *prog_name;
extern const char *device_text;
extern GtkWidget *choose_device_dialog;
extern GSGDialog *dialog;
extern const SANE_Device **devlist;
extern int selected_dev;        /* The selected device */

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
#define XSANE_SHELL_WIDTH	296
#define XSANE_SHELL_HEIGHT	451
#define XSANE_SHELL_POS_X	1
#define XSANE_SHELL_POS_Y	50
#define XSANE_HISTOGRAM_POS_X	280
#define XSANE_HISTOGRAM_POS_Y	50
#define XSANE_STD_OPTIONS_POS_X	1
#define XSANE_STD_OPTIONS_POS_Y	400
#define XSANE_ADV_OPTIONS_POS_X	280
#define XSANE_ADV_OPTIONS_POS_Y	420
#define XSANE_PREVIEW_POS_X	560
#define XSANE_PREVIEW_POS_Y	50
#define XSANE_PREVIEW_WIDTH	100
#define XSANE_PREVIEW_HEIGHT	100

#define XSANE_SLIDER_ACTIVE	0
#define XSANE_SLIDER_INACTIVE	4
#define XSANE_SLIDER_WIDTH	260
#define XSANE_SLIDER_HEIGHT	10
#define XSANE_SLIDER_OFFSET	2
#define XSANE_SLIDER_EVENTS	GDK_EXPOSURE_MASK | GDK_ENTER_NOTIFY_MASK | \
				GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | \
				GDK_BUTTON1_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK
#define INF			5.0e9
#define MM_PER_INCH		25.4

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef SANE_NAME_DOCUMENT_FEEDER
#define SANE_NAME_DOCUMENT_FEEDER "Automatic Document Feeder"
#endif 

/* ---------------------------------------------------------------------------------------------------------------------- */

#define STRINGIFY1(x)   #x
#define STRINGIFY(x)    STRINGIFY1(x)

#define NELEMS(a)       ((int)(sizeof (a) / sizeof (a[0])))

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
    char *filetype;
    char *output_filename;
    char *dummy_filename;

    SANE_Int sensitivity;

    /* dialogs */
    GtkWidget *shell;
    GtkWidget *menubar;
    GtkWidget *standard_options_shell;
    GtkWidget *advanced_options_shell;
    GtkWidget *main_dialog_scrolled;
    GtkWidget *histogram_dialog;
    GtkWidget *fax_dialog;
    GtkWidget *fax_list;

    GtkWidget *fax_project_box;
    GtkWidget *fax_project_exists;
    GtkWidget *fax_project_not_exists;

    GdkPixmap *window_icon_pixmap;
    GdkBitmap *window_icon_mask;        

    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *info_label;
    GtkObject *start_button;
    GtkObject *cancel_button;
    GtkSignalFunc cancel_callback;
    Preview   *preview;
    int mode;

    int main_window_fixed;
    int mode_selection;

    /* various scanning related state: */
    size_t num_bytes;
    size_t bytes_read;
    GtkWidget *progress_bar;
    int input_tag;
    SANE_Parameters param;
    int x, y;

    /* for standalone mode: */
    GtkWidget *filename_entry;
    GtkWidget *fax_project_entry;
    GtkWidget *fax_receiver_entry;
    GtkWidget *filetype_option_menu;
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
    guint slider_timer; /* has to be guint */

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
    double zoom_x;
    double zoom_y;
    double resolution;
    double resolution_x;
    double resolution_y;

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
    GtkObject *threshold_widget;

    SANE_Int xsane_color;
    SANE_Bool scanner_gamma_color;
    SANE_Bool scanner_gamma_gray;

    int fax_fine_mode;

    GtkWidget *outputfilename_entry;
    GtkWidget *copy_number_entry;

    SANE_Int *gamma_data, *gamma_data_red, *gamma_data_green, *gamma_data_blue;
    SANE_Int *preview_gamma_data_red, *preview_gamma_data_green, *preview_gamma_data_blue;
    SANE_Int *histogram_gamma_data_red, *histogram_gamma_data_green, *histogram_gamma_data_blue;

    char *fax_filename;
    char *fax_receiver;

    int block_update_param;

    int broken_pipe; /* for printercommand pipe */

/* -------------------------------------------------- */

/* device preferences: */

/* we have to use double and int here, gint or SANE_Word
   is not allowed because we need a defined size for
   rc_io-routintes that are based on double, int, ... */

    /* window position and geometry */
    int shell_posx;
    int shell_posy;
    int shell_height;
    int shell_width;
    int standard_options_shell_posx;
    int standard_options_shell_posy;
    int advanced_options_shell_posx;
    int advanced_options_shell_posy;
    int histogram_dialog_posx;
    int histogram_dialog_posy;
    int preview_dialog_posx;
    int preview_dialog_posy;
    int preview_dialog_width;
    int preview_dialog_height;

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

    int lineart_mode;
    double threshold;
    double threshold_min;
    double threshold_max;
    double threshold_mul;
    double threshold_off;
    SANE_String grayscale_scanmode;

    int enhancement_rgb_default;
    int negative;
    int show_preview;

/* -------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
    /* for GIMP mode: */
    gint32 image_ID; /* has to be gint32 */
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
  GtkWidget *printer_lineart_resolution_entry;
  GtkWidget *printer_grayscale_resolution_entry;
  GtkWidget *printer_color_resolution_entry;
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
  GtkWidget *preview_lineart_mode_entry;
  GtkWidget *preview_grayscale_scanmode_entry;
  GtkWidget *preview_threshold_min_entry;
  GtkWidget *preview_threshold_max_entry;
  GtkWidget *preview_threshold_mul_entry;
  GtkWidget *preview_threshold_off_entry;
  GtkWidget *auto_enhance_gamma_button;
  GtkWidget *disable_gimp_preview_gamma_button;
  GtkWidget *preview_oversampling_entry;
  GtkWidget *preview_own_cmap_button;
  GtkWidget *doc_viewer_entry;

  GtkWidget *fax_command_entry;
  GtkWidget *fax_receiver_option_entry;
  GtkWidget *fax_postscript_option_entry;
  GtkWidget *fax_normal_option_entry;
  GtkWidget *fax_fine_option_entry;
  GtkWidget *fax_viewer_entry;
  GtkWidget *fax_width_entry;
  GtkWidget *fax_leftoffset_entry;
  GtkWidget *fax_bottomoffset_entry;
  GtkWidget *fax_height_entry;
  GtkWidget *psfile_width_entry;
  GtkWidget *psfile_leftoffset_entry;
  GtkWidget *psfile_bottomoffset_entry;
  GtkWidget *psfile_height_entry;
  GtkWidget *tmp_path_entry;

  int tiff_compression_nr;
  int tiff_compression_1_nr;

  int lineart_mode;

  int image_permissions;
  int directory_permissions;

} XsaneSetup;

extern struct XsaneSetup xsane_setup;

/* ---------------------------------------------------------------------------------------------------------------------- */

#endif
