/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2002 Oliver Rauch
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

#if 0
#define DEF_GTK_ACCEL_LOCKED 0
#else
#define DEF_GTK_ACCEL_LOCKED GTK_ACCEL_LOCKED
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* #define XSANE_TEST */
/* #define SUPPORT_RGBA */

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_VERSION		"0.90"
#define XSANE_AUTHOR		"Oliver Rauch"
#define XSANE_COPYRIGHT		"Oliver Rauch"
#define XSANE_DATE		"1998-2002"
#define XSANE_EMAIL		"Oliver.Rauch@xsane.org"
#define XSANE_HOMEPAGE		"http://www.xsane.org"
#define XSANE_COPYRIGHT_TXT	XSANE_DATE " " XSANE_COPYRIGHT

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_DEBUG_ENVIRONMENT "XSANE_DEBUG"

#define XSANE_DEFAULT_UMASK 0007
#define XSANE_HOLD_TIME 200
#define XSANE_CONTINUOUS_HOLD_TIME 10
#define XSANE_DEFAULT_DEVICE "SANE_DEFAULT_DEVICE"
#define XSANE_3PASS_BUFFER_RGB_SIZE 1024

#ifndef SLASH
# define SLASH '/'
#endif 

#ifndef XSANE_FIXED_HOME_PATH
# define XSANE_FIXED_HOME_PATH /tmp
#endif

#ifndef ENVIRONMENT_HOME_DIR_NAME
# define ENVIRONMENT_HOME_DIR_NAME HOME
#endif

#ifndef ENVIRONMENT_TEMP_DIR_NAME
# define ENVIRONMENT_TEMP_DIR_NAME TMP
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* needed for most of the xsane sources: */

#ifdef _AIX
# include <lalloca.h>
#endif

#if defined(__hpux) || defined(__sgi)
# include <alloca.h>
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

/* OS/2 want sys/types before sys/stat */
#include <sys/types.h>
#include <sys/stat.h>

#include <locale.h>

#include <sane/sane.h>
#include <sane/saneopts.h>
#include "xsaneopts.h"

#include "../include/config.h"
#include "../include/sanei_signal.h"

#include "xsane-text.h"
#include "xsane-fixedtext.h"
#include "xsane-icons.h"
#include "xsane-viewer.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

#if GTK_MAJOR_VERSION == 2
# define HAVE_GTK2
#endif

#ifdef HAVE_GTK2
# define HAVE_GTK_TEXT_VIEW_H
# define DEF_GTK_MENU_ACCEL_VISIBLE GTK_ACCEL_VISIBLE
#else /* we don't have gtk+-2.0 */
# include "xsane-gtk-1_x-compat.h"
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

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

#ifdef HAVE_OS2_H
# define strcasecmp stricmp
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
# define XSANE_ACTIVATE_MAIL
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
  XSANE_PATH_LOCAL_SANE = 0,
  XSANE_PATH_SYSTEM,
  XSANE_PATH_TMP
};

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
{
  char *name;
  float xoffset;
  float yoffset;
  float width;
  float height;
} pref_default_preset_area_t;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    /* The option number of the well-known options.  Each of these may
       be -1 in case the backend doesn't define the respective option.  */
    int scanmode;
    int scansource;
    int preview;
    int dpi;
    int dpi_x;
    int dpi_y;
    int coord[4];
    int gamma_vector;
    int gamma_vector_r;
    int gamma_vector_g;
    int gamma_vector_b;
    int bit_depth;
    int threshold;
    int highlight;
    int highlight_r;
    int highlight_g;
    int highlight_b;
    int shadow;
    int shadow_r;
    int shadow_g;
    int shadow_b;
    int batch_scan_start;
    int batch_scan_loop;
    int batch_scan_end;
    int batch_scan_next_tl_y;
  }
GSGWellKnownOptions;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
  {
    gchar *label;
    struct GSGDialogElement *elem;
    gint index;
  }
GSGMenuItem;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct GSGDialogElement
  {
    GtkWidget *automatic;	/* auto button for options that support this */
    GtkWidget *widget;
    GtkObject *data;
    int menu_size;		/* # of items in menu (if any) */
    GSGMenuItem *menu;
  }
GSGDialogElement;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct Image_info
  {
    int image_width;
    int image_height;

    int depth;
    int colors;

    double resolution_x;
    double resolution_y;

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

    double threshold;
  }
Image_info;

/* ---------------------------------------------------------------------------------------------------------------------- */
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

# ifdef GIMP_CHECK_VERSION
#  if GIMP_CHECK_VERSION(1,1,25)
/* ok, we have the new gimp interface */
#  else
/* we have the old gimp interface and need the compatibility header file */
#   include "xsane-gimp-1_0-compat.h" 
#  endif
# else
/* we have the old gimp interface and need the compatibility header file */
#  include "xsane-gimp-1_0-compat.h" 
# endif

  extern GimpPlugInInfo PLUG_IN_INFO; /* needed for win32 */

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

enum { XSANE_VIEWER, XSANE_SAVE, XSANE_COPY, XSANE_FAX, XSANE_MAIL };
enum { XSANE_LINEART_STANDARD, XSANE_LINEART_XSANE, XSANE_LINEART_GRAYSCALE };

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_pref_save(void);
extern void xsane_interface(int argc, char **argv);
extern void xsane_fax_project_save(void);
extern void xsane_mail_project_save(void);
extern void xsane_batch_scan_add(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */
#ifndef TEMP_PATH
# define TEMP_PATH		/tmp/
#endif

#define OUT_FILENAME     	"out.pnm"
#define PRINTERNAME	  	"new printer"
#define PRINTERCOMMAND  	"lpr"
#define PRINTERCOPYNUMBEROPTION "-#"
#define FAXPROJECT 	    	"faxproject"
#define FAXCOMMAND 	 	"sendfax"
#define FAXRECEIVEROPT		"-d"
#define FAXPOSTSCRIPTOPT	""
#define FAXNORMALOPT		"-l"
#define FAXFINEOPT		"-m"
#define FAXVIEWER 	 	"gv"
#define MAILPROJECT 	    	"mailproject"
#define MAILCOMMAND 	 	"sendmail"
#define MAILVIEWER 	 	"xv"
#define OCRCOMMAND 	 	"gocr"
#define OCRINPUTFILEOPT	 	"-i"
#define OCROUTPUTFILEOPT	"-o"
#define OCROUTFDOPT		"-x"
#define OCRPROGRESSKEY		""
#define DOCVIEWER_NETSCAPE	"netscape"
#define DOCVIEWER 	 	DOCVIEWER_NETSCAPE

#define XSANE_MEDIUM_CALIB_BRIGHTNESS_MIN	-1000.0
#define XSANE_MEDIUM_CALIB_BRIGHTNESS_MAX	 1000.0
#define XSANE_MEDIUM_CALIB_CONTRAST_MIN	 	    0.0
#define XSANE_MEDIUM_CALIB_CONTRAST_MAX	 	 1000.0

#define XSANE_BRIGHTNESS_MIN	-100.0
#define XSANE_BRIGHTNESS_MAX	 100.0
#define XSANE_CONTRAST_GRAY_MIN	-100.0
#define XSANE_CONTRAST_MIN	-100.0
#define XSANE_CONTRAST_MAX	 100.0

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
#define XSANE_GAMMA_POS_X	280
#define XSANE_GAMMA_POS_Y	420
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

#define STRINGIFY1(x)   #x
#define STRINGIFY(x)    STRINGIFY1(x)

#define NELEMS(a)       ((int)(sizeof (a) / sizeof (a[0])))

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
{
 XSANE_UNKNOWN, XSANE_PNM, XSANE_JPEG, XSANE_PNG, XSANE_PS, XSANE_TIFF, XSANE_RGBA,
 XSANE_RAW16, XSANE_PNM16, XSANE_TEXT
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

typedef struct XsaneChildprocess
{
  pid_t pid;
  struct XsaneChildprocess *next;
} XsaneChildprocess;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct Xsane
{
    SANE_Int sane_backend_versioncode;
    char *backend;
    char *backend_translation;
    char *device_set_filename;
    char *output_filename;
    char *dummy_filename;

    SANE_Int sensitivity;

    GtkWidget *xsane_window;
    GtkWidget *standard_window;
    GtkWidget *advanced_window;
    GtkWidget *gamma_window;
    GtkWidget *xsane_hbox;
    GtkWidget *standard_hbox;
    GtkWidget *advanced_hbox;
    GtkWidget *xsanemode_widget;

    GtkAccelGroup *accelerator_group;
    GtkTooltips *tooltips;
    GdkColor tooltips_fg;
    GdkColor tooltips_bg;

    SANE_Handle *dev;
    const char *dev_name;
    GSGWellKnownOptions well_known;
    int num_elements;
    GSGDialogElement *element;
    u_int rebuild : 1;
    int pixelcolor;
    int scanning;
    int reading_data;
    int cancel_scan;
    int batch_loop; /* is set when batch scanning and not last scan */
    int batch_scan_use_stored_scanmode;
    int batch_scan_use_stored_resolution;
    int batch_scan_use_stored_bit_depth;
    SANE_Status status_of_last_scan;

/* free gamma curve widgets */
    GtkWidget *gamma_curve_gray;
    GtkWidget *gamma_curve_red;
    GtkWidget *gamma_curve_green;
    GtkWidget *gamma_curve_blue;

/* previous global stand alone varaibales */
    const char   *prog_name;      /* name of this program, normally "xsane" */
    const char   *device_text;      /* name of the selected device */
    GtkWidget    *choose_device_dialog;      /* the widget of the device selection dialog */
    const SANE_Device **devlist;      /* the list of available devices */
    int          selected_dev;        /* the selected device */
    int          num_of_devs;
    int          back_gtk_message_dialog_active;


    /* dialogs */
    GtkWidget *shell;
    GtkWidget *menubar;
    GtkWidget *standard_options_shell;
    GtkWidget *advanced_options_shell;
    GtkWidget *main_dialog_scrolled;
    GtkWidget *histogram_dialog;
    GtkWidget *gamma_dialog;

    GtkWidget *batch_scan_dialog;
    GtkWidget *batch_scan_button_box;
    GtkWidget *batch_scan_action_box;
    GtkWidget *batch_scan_list;
    GtkAdjustment *batch_scan_vadjustment;

    GtkWidget *fax_dialog;
    GtkWidget *fax_list;
    GtkWidget *fax_project_box;
    GtkWidget *fax_project_exists;
    GtkWidget *fax_project_not_exists;
    GtkWidget *fax_project_entry;
    GtkWidget *fax_receiver_entry;

    GtkWidget *mail_dialog;
    GtkWidget *mail_list;
    GtkWidget *mail_project_box;
    GtkWidget *mail_project_exists;
    GtkWidget *mail_project_not_exists;
    GtkWidget *mail_project_entry;
    GtkWidget *mail_receiver_entry;
    GtkWidget *mail_subject_entry;
    GtkWidget *mail_text_widget;
    GtkWidget *mail_html_mode_widget;
    GtkWidget *mail_status_label;

    GdkPixmap *window_icon_pixmap;
    GdkBitmap *window_icon_mask;        

    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *info_label;
    GtkObject *start_button;
    GtkObject *cancel_button;
    GtkSignalFunc cancel_callback;
    Viewer    *viewer_list;
    Preview   *preview;
    int preview_gamma_size;
    int mode;
    int medium_nr;

    int main_window_fixed;
    int mode_selection;

#ifndef HAVE_GTK2
    int get_deskrelative_origin;
#endif

    /* various scanning related state: */
    SANE_Int depth;
    size_t num_bytes;
    size_t bytes_read;
    int read_offset_16;
    char last_offset_16_byte;
    int  lineart_to_grayscale_x;
    GtkProgressBar *progress_bar;
    int input_tag;
    SANE_Parameters param;
    int adf_page_counter;
    int scan_rotation;

    /* for standalone mode: */
    GtkWidget *filename_entry;
    GtkWidget *filetype_option_menu;

    /* saving and transformation values: */
    FILE *out;
    int xsane_mode;
    int xsane_output_format;
    long header_size;
    int expand_lineart_to_grayscale;
    int reduce_16bit_to_8bit;

    /* histogram window */
    struct XsanePixmap histogram_raw;
    struct XsanePixmap histogram_enh;

    struct XsaneSlider slider_gray;
    struct XsaneSlider slider_red;
    struct XsaneSlider slider_green;
    struct XsaneSlider slider_blue;
    guint  batch_scan_gamma_timer; /* has to be guint */
    guint  slider_timer; /* has to be guint */
    int    slider_timer_restart;

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

    int no_preview_medium_gamma; /* disable preview medium gamma */
    int medium_calibration; /* enable calibration mode for medium */
    int brightness_min;
    int brightness_max;
    int contrast_gray_min;
    int contrast_min;
    int contrast_max;

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

    GtkWidget *medium_widget;
    GtkWidget *length_unit_mm;
    GtkWidget *length_unit_cm;
    GtkWidget *length_unit_in;
    GtkWidget *update_policy_continu;
    GtkWidget *update_policy_discont;
    GtkWidget *update_policy_delayed;
    GtkWidget *show_preview_widget;
    GtkWidget *show_histogram_widget;
    GtkWidget *show_gamma_widget;
    GtkWidget *show_batch_scan_widget;
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

    SANE_Int xsane_colors;
    SANE_Bool scanner_gamma_color;
    SANE_Bool scanner_gamma_gray;

    int fax_fine_mode;
    int mail_project_save;
    int mail_html_mode;

    GtkWidget *outputfilename_entry;
    GtkWidget *copy_number_entry;

    gfloat *free_gamma_data, *free_gamma_data_red, *free_gamma_data_green, *free_gamma_data_blue;
    SANE_Int *gamma_data, *gamma_data_red, *gamma_data_green, *gamma_data_blue;
    u_char *preview_gamma_data_red, *preview_gamma_data_green, *preview_gamma_data_blue;
    u_char *histogram_gamma_data_red, *histogram_gamma_data_green, *histogram_gamma_data_blue;
    u_char *histogram_medium_gamma_data_red, *histogram_medium_gamma_data_green, *histogram_medium_gamma_data_blue;

    char *fax_filename;
    char *fax_receiver;

    char *mail_status;
    char *mail_filename;
    char *mail_receiver;
    char *mail_subject;

    int block_update_param;
    int block_enhancement_update;

    int broken_pipe; /* for printercommand pipe */

    int cancel_save;

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
    int gamma_dialog_posx;
    int gamma_dialog_posy;
    int preview_dialog_posx;
    int preview_dialog_posy;
    int preview_dialog_width;
    int preview_dialog_height;

    double medium_gamma_gray;
    double medium_gamma_red;
    double medium_gamma_green;
    double medium_gamma_blue;

    double medium_shadow_gray;
    double medium_shadow_red;
    double medium_shadow_green;
    double medium_shadow_blue;

    double medium_highlight_gray;
    double medium_highlight_red;
    double medium_highlight_green;
    double medium_highlight_blue;

    int medium_negative;

    int medium_changed;

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
    char *grayscale_scanmode;

    int enhancement_rgb_default;
    int negative;
    int show_preview;
    int print_filenames;
    int force_filename;
    char *external_filename;
    char *adf_scansource;

/* -------------------------------------------------- */

    int ipc_pipefd[2]; /* for inter process communication error messages */
    XsaneChildprocess *childprocess_list;

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
  GtkWidget *save_devprefs_at_exit_button;
  GtkWidget *overwrite_warning_button;
  GtkWidget *increase_filename_counter_button;
  GtkWidget *skip_existing_numbers_button;
  GtkWidget *save_pnm16_as_ascii_button;
  GtkWidget *reduce_16bit_to_8bit_button;

  GtkWidget *main_window_fixed_button;

  GtkWidget *preview_gamma_entry;
  GtkWidget *preview_gamma_red_entry;
  GtkWidget *preview_gamma_green_entry;
  GtkWidget *preview_gamma_blue_entry;
  GtkWidget *preview_lineart_mode_entry;
  GtkWidget *preview_grayscale_scanmode_widget;
  GtkWidget *preview_threshold_min_entry;
  GtkWidget *preview_threshold_max_entry;
  GtkWidget *preview_threshold_mul_entry;
  GtkWidget *preview_threshold_off_entry;
  GtkWidget *auto_enhance_gamma_button;
  GtkWidget *preselect_scanarea_button;
  GtkWidget *auto_correct_colors_button;
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
  GtkWidget *tmp_path_entry;

  GtkWidget *mail_smtp_server_entry;
  GtkWidget *mail_smtp_port_entry;
  GtkWidget *mail_from_entry;
  GtkWidget *mail_reply_to_entry;
  GtkWidget *mail_pop3_authentification_entry;
  GtkWidget *mail_pop3_server_entry;
  GtkWidget *mail_pop3_port_entry;
  GtkWidget *mail_pop3_user_entry;
  GtkWidget *mail_pop3_pass_entry;
  GtkWidget *mail_viewer_entry;

  GtkWidget *ocr_command_entry;
  GtkWidget *ocr_inputfile_option_entry;
  GtkWidget *ocr_outputfile_option_entry;
  GtkWidget *ocr_use_gui_pipe_entry;
  GtkWidget *ocr_gui_outfd_option_entry;
  GtkWidget *ocr_progress_keyword_entry;

  int filename_counter_len;

  int tiff_compression16_nr;
  int tiff_compression8_nr;
  int tiff_compression1_nr;

  int show_range_mode;
  int lineart_mode;

  int image_permissions;
  int directory_permissions;

  int preview_pipette_range;

  char *grayscale_scanmode;
  char *adf_scansource;

} XsaneSetup;

extern struct XsaneSetup xsane_setup;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern int DBG_LEVEL;

#ifdef __GNUC__
# define DBG(level, msg, args...)                \
  {                                              \
    if (DBG_LEVEL >= (level))                    \
    {                                            \
      fprintf (stderr, "[xsane] " msg, ##args);  \
      fflush(stderr);                            \
    }                                            \
  }
#else
  extern void xsane_debug_message(int level, const char *fmt, ...);
# define DBG xsane_debug_message
#endif

# define DBG_init()                                          \
  {                                                          \
   char *dbg_level_string = getenv(XSANE_DEBUG_ENVIRONMENT); \
                                                             \
    if (dbg_level_string)                                    \
    {                                                        \
      DBG_LEVEL = atoi(dbg_level_string);                    \
      DBG(1, "Setting debug level to %d\n", DBG_LEVEL);      \
    }                                                        \
  }

#define DBG_error0    0
#define DBG_error     1
#define DBG_warning   2
#define DBG_info      3
#define DBG_info2     4
#define DBG_proc      5
#define DBG_proc2     50
#define DBG_optdesc   70	/* xsane_get_option_descriptor */
#define DBG_proc3     100	/* for routines that are called very very often */
#define DBG_wire      100	/* rc_io_w routines */

/* ---------------------------------------------------------------------------------------------------------------------- */

#endif
