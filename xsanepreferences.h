#ifndef xsanepreferences_h
#define xsanepreferences_h

#include <sane/sane.h>

typedef struct
  {
    const char *device;		/* name of preferred device (or NULL) */
    const char *filename;	/* default filename */
    const char *printercommand;	/* default printercommand */
    int printerresolution;	/* printer resolution for copy mode  */
    int tooltips_enabled;	/* should tooltips be disabled? */
    int gamma_selected;		/* enhancement enabled ? */
    int show_histogram;		/* show histogram ? */
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
