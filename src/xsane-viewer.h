#ifndef XSANE_VIEWER_H
#define XSANE_VIEWER_H

#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct Viewer
{
  struct Viewer *next_viewer;

  char *filename;
  char *output_filename;

  int reduce_to_lineart;
  float zoom;
  int image_saved;
  int cancel_save;

  GtkWidget *top;
  GtkWidget *button_box;

  GtkWidget *viewport;
  GtkWidget *window;

  GtkWidget *save;
  GtkWidget *clone;

  GtkWidget *despeckle;
  GtkWidget *blur;

  GtkWidget *rotate90;
  GtkWidget *rotate180;
  GtkWidget *rotate270;

  GtkWidget *mirror_x;
  GtkWidget *mirror_y;

  GtkProgressBar *progress_bar;
}
Viewer;

extern Viewer *xsane_viewer_new(char *filename, int reduce_to_lineart, char *output_filename);

#endif
