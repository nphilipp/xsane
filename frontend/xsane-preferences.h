#ifndef xsanepreferences_h
#define xsanepreferences_h

#include <sane/sane.h>


typedef struct
  {
    char *name;		/* user defined printer name */
    char *command;	/* printercommand */
    char *copy_number_option;	/* option to define number of copies */
    int resolution;	/* printer resolution for copy mode  */
    int width;		/* printer width of printable area in 1/72inch  */
    int height;		/* printer height of printable area in 1/72inch  */
    int leftoffset;	/* printer left offset in 1/72inch  */
    int bottomoffset;	/* printer bottom offset in 1/72inch  */
    double gamma;	/* printer gamma */
    double gamma_red;	/* printer gamma red */
    double gamma_green;	/* printer gamma green */
    double gamma_blue;	/* printer gamma blue */
  }
Preferences_printer_t;

typedef struct
  {
    const char *device;		/* name of preferred device (or NULL) */
    char *filename;		/* default filename */
    char *fax_project;		/* fax project */
    char *fax_command;		/* faxcommand */
    char *fax_receiver_option;	/* fax receiver option */
    char *fax_postscript_option; /* fax postscript option */
    char *fax_normal_option;	/* fax normal mode option */
    char *fax_fine_option;	/* fax fine mode option */
    char *fax_viewer;		/* fax viewer */
    char *doc_viewer;		/* doc viewer for helpfiles */
    double jpeg_quality;	/* quality when saving image as jpeg */
    double png_compression;	/* compression when saving image as pnm */
    int overwrite_warning;	/* warn if file exists */
    int increase_filename_counter;	/* automatically increase counter */
    int skip_existing_numbers;	/* automatically increase counter */
    int tooltips_enabled;	/* should tooltips be disabled? */
    int show_histogram;		/* show histogram ? */
    int show_standard_options;	/* show standard options ? */
    int show_advanced_options;	/* show advanced options ? */
    int show_resolution_list;	/* show resolution list instead of slider ? */
    double length_unit;		/* 1.0==mm, 10.0==cm, 25.4==inches, etc. */
    int main_window_fixed;	/* fixed (1) or scrolled (0) main window */
    int preserve_preview;	/* save/restore preview image(s)? */
    int preview_own_cmap;	/* install colormap for preview */
    double preview_gamma;	/* gamma value for previews */
    double preview_gamma_red;	/* red gamma value for previews */
    double preview_gamma_green;	/* green gamma value for previews */
    double preview_gamma_blue;	/* blue gamma value for previews */
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
    int xsane_rgb_default;
    int xsane_negative;
    int printernr;
    int printerdefinitions;
    Preferences_printer_t *printer[10];
  }
Preferences;

extern Preferences preferences;

extern void preferences_save (int fd);
extern void preferences_restore (int fd);

#endif /* preferences_h */
