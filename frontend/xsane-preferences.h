#ifndef xsanepreferences_h
#define xsanepreferences_h

#include <sane/sane.h>

typedef struct
  {
    const char *device;		/* name of preferred device (or NULL) */
    char *filename;		/* default filename */
    char *printer_command;	/* default printercommand */
    int printer_resolution;	/* printer resolution for copy mode  */
    int printer_width;		/* printer width of printable area in 1/72inch  */
    int printer_height;		/* printer height of printable area in 1/72inch  */
    int printer_leftoffset;	/* printer left offset in 1/72inch  */
    int printer_bottomoffset;	/* printer bottom offset in 1/72inch  */
    double jpeg_quality;	/* quality when saving image as jpeg */
    double png_compression;	/* compression when saving image as pnm */
    int overwrite_warning;	/* warn if file exists */
    int increase_filename_counter;	/* automatically increase counter */
    int skip_existing_numbers;	/* automatically increase counter */
    int tooltips_enabled;	/* should tooltips be disabled? */
    int show_histogram;		/* show histogram ? */
    int show_standard_options;	/* show standard options ? */
    int show_advanced_options;	/* show advanced options ? */
    double length_unit;		/* 1.0==mm, 10.0==cm, 25.4==inches, etc. */
    int preserve_preview;	/* save/restore preview image(s)? */
    int preview_own_cmap;	/* install colormap for preview */
    double preview_gamma;	/* gamma value for previews */
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
  }
Preferences;

extern Preferences preferences;

extern void preferences_save (int fd);
extern void preferences_restore (int fd);

#endif /* preferences_h */
