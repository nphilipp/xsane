#ifndef gtkglue_h
#define gtkglue_h

#include <sys/types.h>

#include <gtk/gtk.h>

#include <sane/config.h>
#include <sane/sane.h>

struct GSGDialog;

typedef void (*GSGCallback) (struct GSGDialog *dialog, void *arg);
typedef GtkWidget *(*XSANECallback) (void);

typedef enum
  {
    GSG_TL_X,	/* top-left x */
    GSG_TL_Y,	/* top-left y */
    GSG_BR_X,	/* bottom-right x */
    GSG_BR_Y	/* bottom-right y */
  }
GSGCornerCoordinates;

typedef struct
  {
    /* The option number of the well-known options.  Each of these may
       be -1 in case the backend doesn't define the respective option.  */
    int scanmode;
    int scansource;
    int preview;
    int dpi;
    int coord[4];
    int gamma_vector;
    int gamma_vector_r;
    int gamma_vector_g;
    int gamma_vector_b;
    int bit_depth;
  }
GSGWellKnownOptions;

typedef struct
  {
    gchar *label;
    struct GSGDialogElement *elem;
    gint index;
  }
GSGMenuItem;

typedef struct GSGDialogElement
  {
    struct GSGDialog *dialog;	/* wasteful, but is there a better solution? */
    GtkWidget *automatic;	/* auto button for options that support this */
    GtkWidget *widget;
    GtkObject *data;
    int menu_size;		/* # of items in menu (if any) */
    GSGMenuItem *menu;
  }
GSGDialogElement;

typedef struct GSGDialog
  {
    GtkWidget *xsane_window;
    GtkWidget *standard_window;
    GtkWidget *advanced_window;
    GtkWidget *xsane_hbox;
    GtkWidget *standard_hbox;
    GtkWidget *advanced_hbox;
    GtkWidget *xsanemode_widget;
    GtkTooltips *tooltips;
    GdkColor tooltips_fg;
    GdkColor tooltips_bg;
    SANE_Handle *dev;
    const char *dev_name;
    GSGWellKnownOptions well_known;
    int num_elements;
    GSGDialogElement *element;
    gint idle_id;
    u_int rebuild : 1;
    /* This callback gets invoked whenever the backend notifies us
       that the option descriptors have changed.  */
    GSGCallback option_reload_callback;
    void *option_reload_arg;
    /* This callback gets invoked whenever the backend notifies us
       that the parameters have changed.  */
    GSGCallback param_change_callback;
    void *param_change_arg;
    XSANECallback update_xsane_callback;
    void *update_xsane_arg;
    int pixelcolor;
  }
GSGDialog;

extern int gsg_message_dialog_active;

/* Construct the path and return it in filename_ret (this buffer must
   be at least max_len bytes long).  The path is constructed as
   follows:

      ~/.sane/${PROG_NAME}/${PREFIX}${DEV_NAME}${POSTFIX}

   If PROG_NAME is NULL, an empty string is used and the leading slash
   is removed.  On success, 0 is returned, on error a negative number and
   ERRNO is set to the appropriate value.  */
extern int gsg_make_path (size_t max_len, char *filename_ret,
			  const char *prog_name,
			  const char *dir_name,
			  const char *prefix, const char *dev_name,
			  const char *postfix);
extern gint gsg_decision(gchar *title, gchar *message, gchar *oktext, gchar *rejecttext, gint wait);
extern void gsg_message (gchar *title, gchar * message);
extern void gsg_error (gchar * error_message);
extern void gsg_warning (gchar * warning_message);
extern int gsg_get_filename (const char *label, const char *default_name,
			     size_t max_len, char *filename);

extern void gsg_sync (GSGDialog *dialog);
extern void gsg_update_vector(GSGDialog *dialog, int opt_num, SANE_Int *vector);
extern void gsg_refresh_dialog (GSGDialog *dialog);
extern void gsg_update_scan_window (GSGDialog *dialog);
extern void gsg_set_advanced (GSGDialog *dialog, int advanced);
extern void gsg_set_tooltips (GSGDialog *dialog, int enable);
extern void gsg_set_tooltip (GtkTooltips *tooltips, GtkWidget *widget, const char *desc);
extern void gsg_set_sensitivity (GSGDialog *dialog, int sensitive);
extern void gsg_destroy_dialog (GSGDialog * dialog);
extern void gsg_set_option (GSGDialog * dialog, int opt_num, void *val, SANE_Action action);
extern GtkWidget * gsg_group_new (GtkWidget *parent, const char * title);
extern void gsg_button_new (GtkWidget * parent, const char *name, SANE_Word val,
            GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc);
extern void gsg_scale_new (GtkWidget * parent, const char *name, gfloat val,
           gfloat min, gfloat max, gfloat quant, int automatic,
	   GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc);
extern void gsg_option_menu_new (GtkWidget *parent, const char *name, char *str_list[],
           const char *val, GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc);
extern void gsg_text_entry_new (GtkWidget * parent, const char *name, const char *val,
                GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc);
extern void gsg_push_button_callback (GtkWidget * widget, gpointer data);
extern const char * gsg_unit_string (SANE_Unit unit);

#define gsg_dialog_get_device(dialog)	((dialog)->dev)

#endif /* gtkglue_h */