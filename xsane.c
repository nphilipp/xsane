/* xsane -- a graphical scanner-oriented SANE frontend

   Authors:
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Andreas Beck <becka@sunserver1.rz.uni-duesseldorf.de>
   Tristan Tarrant <ttarrant@etnoteam.it>
   David Mosberger-Tang <davidm@azstarnet.com>

   Copyright (C) 1998 Oliver Rauch, Andreas Beck, Tristan Tarrant, and David Mosberger

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

#ifdef _AIX
# include <lalloca.h>	/* MUST come first for AIX! */
#endif

#include <sane/config.h>
#include <lalloca.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <xsanegtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>

#include <progress.h>
#include <xsanepreferences.h>
#include <xsanepreview.h>
#include <xsane-icons.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#define OUTFILENAME     "out.pnm"
#define PRINTERCOMMAND  "lpr -"
#define XSANE_VERSION	"0.06 alpha"

/* ---------------------------------------------------------------------------------------------------------------------- */

int gsg_message_dialog_active = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H

#include <libgimp/gimp.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

static void query(void);
static void run(char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals);

GPlugInInfo PLUG_IN_INFO =
{
  NULL,				/* init_proc */
  NULL,				/* quit_proc */
  query,			/* query_proc */
  run,				/* run_proc */
};

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

enum
  {
    STANDALONE, GIMP_EXTENSION
  };

/* ---------------------------------------------------------------------------------------------------------------------- */

struct XsanePixmap
{
  GtkWidget *frame;
  GdkPixmap *pixmap;
  GtkWidget *pixmapwid;
  gint width;
  gint height;
};

/* ---------------------------------------------------------------------------------------------------------------------- */

static struct
  {
    GtkWidget *shell;
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *info_label;
    Preview *preview;
    gint32 mode;

    /* various scanning related state: */
    size_t num_bytes;
    size_t bytes_read;
    Progress_t *progress;
    int input_tag;
    SANE_Parameters param;
    int x, y;

    /* for standalone mode: */
    GtkWidget *filename_entry;
    FILE *out;
    int xsane_mode;
    long header_size;

    GtkWidget *histogram_dialog;
    struct XsanePixmap histogram_raw;
    struct XsanePixmap histogram_enh;

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
    double pipette_white;
    double pipette_black;
    double auto_brightness;
    double auto_contrast;

    int histogram_red;
    int histogram_green;
    int histogram_blue;
    int histogram_int;
    int histogram_lines;
    int histogram_log;

    GtkWidget *enable_enhancement_widget;
    GtkWidget *show_histogram_widget;
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
    GtkObject *pipette_white_widget;
    GtkObject *pipette_black_widget;

    SANE_Bool xsane_color;
    SANE_Bool enhancement_rgb_default;
    SANE_Bool scanner_gamma_selected;
    SANE_Bool software_gamma_selected;

    GtkWidget *outputfilename_entry;

    SANE_Int *gamma_data, *gamma_data_red, *gamma_data_green, *gamma_data_blue;
    SANE_Int *preview_gamma_data, *preview_gamma_data_red, *preview_gamma_data_green, *preview_gamma_data_blue;

#ifdef HAVE_LIBGIMP_GIMP_H
    /* for GIMP mode: */
    gint32 image_ID;
    GDrawable *drawable;
    guchar *tile;
    unsigned tile_offset;
    GPixelRgn region;
    int first_frame;		/* used for RED/GREEN/BLUE frames */
#endif
  }
xsane;

/* ---------------------------------------------------------------------------------------------------------------------- */

enum { XSANE_SCAN, XSANE_COPY, XSANE_FAX };
static int xsane_scanmode_number[] = { XSANE_SCAN, XSANE_COPY, XSANE_FAX };

/* ---------------------------------------------------------------------------------------------------------------------- */

static const char *prog_name;
static GtkWidget *choose_device_dialog;
static GSGDialog *dialog;
static const SANE_Device **devlist;
static gint seldev = -1;	/* The selected device */
static gint ndevs;		/* The number of available devices */
static struct option long_options[] =
  {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {0, }
  };

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

int main(int argc, char ** argv);
static void interface(int argc, char **argv);
static void scan_start(void);
static void scan_done(void);
GtkWidget *xsane_xsane_new_callback(void);
static void update_bool_callback(GtkWidget *widget, gpointer data);
static void xsane_histogram_check_button_callback(GtkWidget *widget, gpointer data);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_ps_gray_out(FILE *outfile, FILE *imagefile,
                        int bits,
                        int pixel_width, int pixel_height,
                        int left, int bottom,
                        float width, float height)
{
 int x, y, count;

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: XSane Version %s (%s %s)\n", XSANE_VERSION,  PACKAGE, VERSION);
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
          left, bottom,(int) (left + width * 72.0),(int) (bottom + height * 72.0));
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");
  fprintf(outfile, "/pix %d string def\n", pixel_width);
  fprintf(outfile, "%d %d translate\n", left, bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d %d\n", pixel_width, pixel_height, bits);
  fprintf(outfile, "[%d %d %d %d %d %d]\n",pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");
  fprintf(outfile, "image\n");
  fprintf(outfile, "\n");
 
  count = 0;
  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fprintf(outfile, "%02x", fgetc(imagefile));
      if (++count >=40)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;
  }
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");

}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_ps_color_out(FILE *outfile, FILE *imagefile,
                        int bits,
                        int pixel_width, int pixel_height,
                        int left, int bottom,
                        float width, float height)
{
 int x, y, count;

  fprintf(outfile, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(outfile, "%%%%Creator: XSane Version %s (%s %s)\n", XSANE_VERSION,  PACKAGE, VERSION);
  fprintf(outfile, "%%%%BoundingBox: %d %d %d %d\n",
          left, bottom,(int) (left + width * 72.0),(int) (bottom + height * 72.0));
  fprintf(outfile, "%%\n");
  fprintf(outfile, "/origstate save def\n");
  fprintf(outfile, "20 dict begin\n");
  fprintf(outfile, "/pix %d string def\n", pixel_width * 3);
  fprintf(outfile, "%d %d translate\n", left, bottom);
  fprintf(outfile, "%f %f scale\n", width * 72.0, height * 72.0);
  fprintf(outfile, "%d %d %d\n", pixel_width, pixel_height, bits);
  fprintf(outfile, "[%d %d %d %d %d %d]\n",pixel_width, 0, 0, -pixel_height, 0 , pixel_height);
  fprintf(outfile, "{currentfile pix readhexstring pop}\n");
  fprintf(outfile, "false 3 colorimage\n");
  fprintf(outfile, "\n");
 
  count = 0;
  for (y=0; y<pixel_height; y++)
  {
    for (x=0; x<pixel_width; x++)
    {
      fprintf(outfile, "%02x", fgetc(imagefile));
      fprintf(outfile, "%02x", fgetc(imagefile));
      fprintf(outfile, "%02x", fgetc(imagefile));
      if (++count >=10)
      {
        fprintf(outfile, "\n");
	count = 0;
      }
    }
    fprintf(outfile, "\n");
    count = 0;
  }
  fprintf(outfile, "\n");
  fprintf(outfile, "showpage\n");
  fprintf(outfile, "end\n");
  fprintf(outfile, "origstate restore\n");
  fprintf(outfile, "\n");

}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_check_button_new(GtkWidget *parent, char *name, int *data, void *xsane_check_button_callback)
{
  GtkWidget *button;

  button = gtk_check_button_new_with_label((char *) name);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), *data);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_check_button_callback, (GtkObject *)data);
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
  gtk_widget_show(button);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_pixmap_new(GtkWidget *parent, char *title, int width, int height, struct XsanePixmap *hist)
{
  GdkBitmap *mask=NULL;

   hist->frame     = gtk_frame_new(title);
   hist->pixmap    = gdk_pixmap_new(xsane.shell->window, width, height, -1);
   hist->pixmapwid = gtk_pixmap_new(hist->pixmap, mask);
   gtk_container_add(GTK_CONTAINER(hist->frame), hist->pixmapwid);
   gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, width, height);

   gtk_box_pack_start(GTK_BOX(parent), hist->frame, FALSE, FALSE, 2);
   gtk_widget_show(hist->pixmapwid);
   gtk_widget_show(hist->frame);

   hist->width = width;
   hist->height = height;
 }

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_draw_histogram_with_points(struct XsanePixmap *hist, SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue, int show_red, int show_green, int show_blue, int show_inten, double scale)
{
 GdkRectangle rect;
 int i;
 int inten, red, green, blue;
#define XD 1
#define YD 2

  if(hist->pixmap)
  {
    rect.x=0;
    rect.y=0;
    rect.width=hist->width;
    rect.height=hist->height;

    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, hist->width, hist->height);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < hist->width; i++)
    {
      inten = show_inten * count[i]       * scale;

      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[i]   * scale;
        green = show_green * count_green[i] * scale;
        blue  = show_blue  * count_blue[i]  * scale;
      }

      if (inten > hist->height)
      inten = hist->height;

      if (red > hist->height)
      red = hist->height;

      if (green > hist->height)
      green = hist->height;

      if (blue > hist->height)
      blue = hist->height;


      gdk_draw_rectangle(hist->pixmap, xsane.gc_red,   TRUE, i, hist->height - red,   XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_green, TRUE, i, hist->height - green, XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_blue,  TRUE, i, hist->height - blue,  XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_black, TRUE, i, hist->height - inten, XD, YD);
    }

    gtk_widget_draw(hist->pixmapwid, &rect);
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_draw_histogram_with_lines(struct XsanePixmap *hist, SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue, int show_red, int show_green, int show_blue, int show_inten, double scale)
{
 GdkRectangle rect;
 int i, j, k;
 int inten, red, green, blue;
 int inten0=0, red0=0, green0=0, blue0=0;
 int val[4];
 int val2[4];
 int color[4];
 int val_swap;
 int color_swap;

  if (hist->pixmap)
  {
    rect.x=0;
    rect.y=0;
    rect.width=hist->width;
    rect.height=hist->height;

    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, hist->width, hist->height);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < hist->width; i++)
    {
      inten = show_inten * count[i]       * scale;
      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[i]   * scale;
        green = show_green * count_green[i] * scale;
        blue  = show_blue  * count_blue[i]  * scale;
      }

      if (inten > hist->height)
      inten = hist->height;

      if (red > hist->height)
      red = hist->height;

      if (green > hist->height)
      green = hist->height;

      if (blue > hist->height)
      blue = hist->height;

      val[0] = red;   color[0] = 0;
      val[1] = green; color[1] = 1;
      val[2] = blue;  color[2] = 2;
      val[3] = inten; color[3] = 3;

      for (j=0; j<3; j++)
      {
        for (k=j+1; k<4; k++)
        {
          if (val[j] < val[k])
          {
            val_swap   = val[j];
            color_swap = color[j];
            val[j]     = val[k];
            color[j]   = color[k];
            val[k]     = val_swap;
            color[k]   = color_swap;
	  }
	}
      }
      val2[0]=val[1]+1;
      val2[1]=val[2]+1;
      val2[2]=val[3]+1;
      val2[3]=0;

      for (j=0; j<4; j++)
      {
        switch(color[j])
	{
	  case 0: red0   = val2[j];
	  break;
	  case 1: green0 = val2[j];
	  break;
	  case 2: blue0  = val2[j];
	  break;
	  case 3: inten0 = val2[j];
	  break;
	}
      }


      gdk_draw_line(hist->pixmap, xsane.gc_red,   i, hist->height - red,   i, hist->height - red0);
      gdk_draw_line(hist->pixmap, xsane.gc_green, i, hist->height - green, i, hist->height - green0);
      gdk_draw_line(hist->pixmap, xsane.gc_blue,  i, hist->height - blue,  i, hist->height - blue0);
      gdk_draw_line(hist->pixmap, xsane.gc_black, i, hist->height - inten, i, hist->height - inten0);
    }

    gtk_widget_draw(hist->pixmapwid, &rect);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_destroy_histogram()
{
  if (xsane.histogram_dialog)
  {
    gtk_widget_destroy(xsane.histogram_dialog);

    if (xsane.histogram_raw.pixmap)
    {
      gdk_pixmap_unref(xsane.histogram_raw.pixmap);
      xsane.histogram_raw.pixmap    = 0;
      xsane.histogram_raw.pixmapwid = 0;
    }

    if (xsane.histogram_enh.pixmap)
    {
      gdk_pixmap_unref(xsane.histogram_enh.pixmap);
      xsane.histogram_enh.pixmap    = 0;
      xsane.histogram_enh.pixmapwid = 0;
    }

    xsane.histogram_dialog = 0;
  }
  preferences.show_histogram = FALSE;
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_histogram()
{
 SANE_Int *count_raw;
 SANE_Int *count_raw_red;
 SANE_Int *count_raw_green;
 SANE_Int *count_raw_blue;
 SANE_Int *count_enh;
 SANE_Int *count_enh_red;
 SANE_Int *count_enh_green;
 SANE_Int *count_enh_blue;
 int x_min, x_max, y_min, y_max;
 int i;
 int maxval_raw;
 int maxval_enh;
 int maxval;
 double scale;
 int min;
 int max;
 int val;
 int limit;
 GtkWidget *xsane_histogram_vbox;

  if (preferences.show_histogram == 0)
  {
    return;
  }

  if (xsane.histogram_dialog == 0)
  {
   char buf[256];

    xsane.histogram_dialog = gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(xsane.histogram_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_destroy_histogram), 0);
    sprintf(buf, "%s histogram", prog_name);
    gtk_window_set_title(GTK_WINDOW(xsane.histogram_dialog), buf);

    xsane_histogram_vbox = GTK_DIALOG(xsane.histogram_dialog)->vbox;

    xsane_check_button_new(xsane_histogram_vbox, "intensity",
                           &xsane.histogram_int,   xsane_histogram_check_button_callback);
    xsane_check_button_new(xsane_histogram_vbox, "red",
                           &xsane.histogram_red,   xsane_histogram_check_button_callback);
    xsane_check_button_new(xsane_histogram_vbox, "green",
                           &xsane.histogram_green, xsane_histogram_check_button_callback);
    xsane_check_button_new(xsane_histogram_vbox, "blue",
                           &xsane.histogram_blue,  xsane_histogram_check_button_callback);
    xsane_check_button_new(xsane_histogram_vbox, "pixels/lines",
                           &xsane.histogram_lines, xsane_histogram_check_button_callback);
    xsane_check_button_new(xsane_histogram_vbox, "linear/logarithm",
                           &xsane.histogram_log, xsane_histogram_check_button_callback);
  }

  xsane_histogram_vbox = GTK_DIALOG(xsane.histogram_dialog)->vbox;

  if (xsane.preview)
  {
    if (xsane.histogram_raw.pixmap == 0)
    {
      xsane_pixmap_new(xsane_histogram_vbox, "Histogram (raw image)", 256, 100, &(xsane.histogram_raw));
    }
  }
  else if (xsane.histogram_raw.pixmap)
  {
    gtk_widget_destroy(xsane.histogram_raw.frame);
    gdk_pixmap_unref(xsane.histogram_raw.pixmap);
    xsane.histogram_raw.frame     = 0;
    xsane.histogram_raw.pixmap    = 0;
    xsane.histogram_raw.pixmapwid = 0;
  }

  if ( (xsane.preview) && (preferences.gamma_selected) )
  {
    if (xsane.histogram_enh.pixmap == 0)
    {
      xsane_pixmap_new(xsane_histogram_vbox, "Histogram (enhanced image)", 256, 100, &(xsane.histogram_enh));
    }
  }
  else if (xsane.histogram_enh.pixmap)
  {
    gtk_widget_destroy(xsane.histogram_enh.frame);
    gdk_pixmap_unref(xsane.histogram_enh.pixmap);
    xsane.histogram_enh.frame     = 0;
    xsane.histogram_enh.pixmap    = 0;
    xsane.histogram_enh.pixmapwid = 0;
  }

  if ((xsane.preview) && (preferences.show_histogram))
  {
    count_raw       = calloc(256, sizeof(SANE_Int));
    count_raw_red   = calloc(256, sizeof(SANE_Int));
    count_raw_green = calloc(256, sizeof(SANE_Int));
    count_raw_blue  = calloc(256, sizeof(SANE_Int));
    count_enh       = calloc(256, sizeof(SANE_Int));
    count_enh_red   = calloc(256, sizeof(SANE_Int));
    count_enh_green = calloc(256, sizeof(SANE_Int));
    count_enh_blue  = calloc(256, sizeof(SANE_Int));

    x_min = ((double)xsane.preview->selection.coord[0]);
    x_max = ((double)xsane.preview->selection.coord[2]);
    y_min = ((double)xsane.preview->selection.coord[1]);
    y_max = ((double)xsane.preview->selection.coord[3]);

    preview_calculate_histogram(xsane.preview, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                                count_enh, count_enh_red, count_enh_green, count_enh_blue, x_min, y_min, x_max, y_max,
                                xsane.histogram_log);

    maxval_raw = 0;
    maxval_enh = 0;

    /* first and last 10 values are not used for calculating maximum value */
    for (i = 10 ; i < xsane.histogram_raw.width - 10; i++)
    {
      if (count_raw[i]       > maxval_raw) { maxval_raw = count_raw[i]; }
      if (count_raw_red[i]   > maxval_raw) { maxval_raw = count_raw_red[i]; }
      if (count_raw_green[i] > maxval_raw) { maxval_raw = count_raw_green[i]; }
      if (count_raw_blue[i]  > maxval_raw) { maxval_raw = count_raw_blue[i]; }
      if (count_enh[i]       > maxval_enh) { maxval_enh = count_enh[i]; }
      if (count_enh_red[i]   > maxval_enh) { maxval_enh = count_enh_red[i]; }
      if (count_enh_green[i] > maxval_enh) { maxval_enh = count_enh_green[i]; }
      if (count_enh_blue[i]  > maxval_enh) { maxval_enh = count_enh_blue[i]; }
    }
    maxval = ((maxval_enh > maxval_raw) ? maxval_enh : maxval_raw);
    scale = 100.0/maxval;

    min = -1;
    max = xsane.histogram_raw.width;

    limit = (x_max - x_min) * (y_max - y_min) / 500;
    if (limit < 3)
    {
      limit = 3;
    }

    val = 0;
    while (val < limit)
    {
      min++;
      val = count_raw[min] + count_raw_red[min] + count_raw_green[min] + count_raw_blue[min];
    }

    val = 0;
    while (val < limit)
    {
      max--;
      val = count_raw[max] + count_raw_red[max] + count_raw_green[max] + count_raw_blue[max];
    }

    xsane.auto_brightness = xsane.histogram_raw.width/2 - (min + max) / 2.0;
    xsane.auto_contrast   = ( ((double) xsane.histogram_raw.width) / (max - min) - 1.0) * 50.0;
     

    if (xsane.histogram_lines)
    {
      if (xsane.histogram_raw.pixmap)
      {
        xsane_draw_histogram_with_lines(&xsane.histogram_raw, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                          xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      }
      if (xsane.histogram_enh.pixmap)
      {
        xsane_draw_histogram_with_lines(&xsane.histogram_enh, count_enh, count_enh_red, count_enh_green, count_enh_blue,
                          xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      }
    }
    else
    {
      if (xsane.histogram_raw.pixmap)
      {
        xsane_draw_histogram_with_points(&xsane.histogram_raw, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                          xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      }
      if (xsane.histogram_enh.pixmap)
      {
        xsane_draw_histogram_with_points(&xsane.histogram_enh, count_enh, count_enh_red, count_enh_green, count_enh_blue,
                          xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      }
    }

    free(count_enh_blue);
    free(count_enh_green);
    free(count_enh_red);
    free(count_enh);
    free(count_raw_blue);
    free(count_raw_green);
    free(count_raw_red);
    free(count_raw);
  }
  gtk_widget_show(xsane.histogram_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_histogram_check_button_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;

  *valuep = (GTK_TOGGLE_BUTTON(widget)->active != 0);
  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_gamma_curve(SANE_Int *gammadata, double gamma, double brightness, double contrast, int maxin, int maxout)
{
 int i;
 double midout;
 double val;
 double m;
 double b;

  midout = (int)(maxout  / 2);

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midout;

  for (i=0; i<=maxout; i++)
  {
    val = ((double) i) - midout;
    val = val * m + b;
    if (val < 0)
    { val = 0; }
    else if (val > maxout)
    { val = maxout; }

    gammadata[i] = 0.5 + maxout * pow( val/maxout, (1.0/gamma) );
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_gamma()
{
  if (xsane.preview)
  {
    if (!xsane.preview_gamma_data_red)
    {
      xsane.preview_gamma_data_red   = malloc(256 * sizeof(SANE_Int));
      xsane.preview_gamma_data_green = malloc(256 * sizeof(SANE_Int));
      xsane.preview_gamma_data_blue  = malloc(256 * sizeof(SANE_Int));
    }

    xsane_create_gamma_curve(xsane.preview_gamma_data_red,
		 	     xsane.gamma * xsane.gamma_red,
			     xsane.brightness + xsane.brightness_red,
			     xsane.contrast + xsane.contrast_red, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_green,
			     xsane.gamma * xsane.gamma_green,
			     xsane.brightness + xsane.brightness_green,
			     xsane.contrast + xsane.contrast_green, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_blue,
			     xsane.gamma * xsane.gamma_blue,
			     xsane.brightness + xsane.brightness_blue,
			     xsane.contrast + xsane.contrast_blue , 256, 255);

    preview_gamma_correction(xsane.preview,
                             xsane.preview_gamma_data_red, xsane.preview_gamma_data_green, xsane.preview_gamma_data_blue);

  }
  xsane_update_histogram(&(xsane.histogram_raw), (int) (xsane.gamma*100));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_refresh_dialog(void *nothing)
{   
/*
  if (xsane.histogram_enh.pixmap)
  {
    gdk_pixmap_unref(xsane.histogram_enh.pixmap);
    xsane.histogram_enh.pixmap    = 0;
    xsane.histogram_enh.pixmapwid = 0;
  }

  if (xsane.histogram_raw.pixmap)
  {
    gdk_pixmap_unref(xsane.histogram_raw.pixmap);
    xsane.histogram_raw.pixmap    = 0;
    xsane.histogram_raw.pixmapwid = 0;
  }
*/
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_update()
{
  if (preferences.gamma_selected)
  {
    GTK_ADJUSTMENT(xsane.gamma_widget)->value      = xsane.gamma;
    GTK_ADJUSTMENT(xsane.brightness_widget)->value = xsane.brightness;
    GTK_ADJUSTMENT(xsane.contrast_widget)->value   = xsane.contrast;


    if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
    {
      GTK_ADJUSTMENT(xsane.gamma_red_widget)->value    = xsane.gamma_red;
      GTK_ADJUSTMENT(xsane.gamma_green_widget)->value  = xsane.gamma_green;
      GTK_ADJUSTMENT(xsane.gamma_blue_widget)->value   = xsane.gamma_blue;

      GTK_ADJUSTMENT(xsane.brightness_red_widget)->value   = xsane.brightness_red;
      GTK_ADJUSTMENT(xsane.brightness_green_widget)->value = xsane.brightness_green;
      GTK_ADJUSTMENT(xsane.brightness_blue_widget)->value  = xsane.brightness_blue;

      GTK_ADJUSTMENT(xsane.contrast_red_widget)->value   = xsane.contrast_red;
      GTK_ADJUSTMENT(xsane.contrast_green_widget)->value = xsane.contrast_green;
      GTK_ADJUSTMENT(xsane.contrast_blue_widget)->value  = xsane.contrast_blue;
    }
    gtk_signal_emit_by_name(xsane.gamma_widget, "value_changed");
    xsane_refresh_dialog(dialog);
  }
  else
  {
    xsane_update_gamma();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_restore_default()
{
  xsane.gamma            = 1.0;
  xsane.gamma_red        = 1.0;
  xsane.gamma_green      = 1.0;
  xsane.gamma_blue       = 1.0;
  xsane.brightness       = 0.0;
  xsane.brightness_red   = 0.0;
  xsane.brightness_green = 0.0;
  xsane.brightness_blue  = 0.0;
  xsane.contrast         = 0.0;
  xsane.contrast_red     = 0.0;
  xsane.contrast_green   = 0.0;
  xsane.contrast_blue    = 0.0;
  xsane_enhancement_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_restore_saved()
{
  xsane.gamma            = preferences.xsane_gamma;
  xsane.gamma_red        = preferences.xsane_gamma_red;
  xsane.gamma_green      = preferences.xsane_gamma_green;
  xsane.gamma_blue       = preferences.xsane_gamma_blue;
  xsane.brightness       = preferences.xsane_brightness;
  xsane.brightness_red   = preferences.xsane_brightness_red;
  xsane.brightness_green = preferences.xsane_brightness_green;
  xsane.brightness_blue  = preferences.xsane_brightness_blue;
  xsane.contrast         = preferences.xsane_contrast;
  xsane.contrast_red     = preferences.xsane_contrast_red;
  xsane.contrast_green   = preferences.xsane_contrast_green;
  xsane.contrast_blue    = preferences.xsane_contrast_blue;
  xsane_enhancement_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_save()
{
  preferences.xsane_gamma            = xsane.gamma;
  preferences.xsane_gamma_red        = xsane.gamma_red;
  preferences.xsane_gamma_green      = xsane.gamma_green;
  preferences.xsane_gamma_blue       = xsane.gamma_blue;
  preferences.xsane_brightness       = xsane.brightness;
  preferences.xsane_brightness_red   = xsane.brightness_red;
  preferences.xsane_brightness_green = xsane.brightness_green;
  preferences.xsane_brightness_blue  = xsane.brightness_blue;
  preferences.xsane_contrast         = xsane.contrast;
  preferences.xsane_contrast_red     = xsane.contrast_red;
  preferences.xsane_contrast_green   = xsane.contrast_green;
  preferences.xsane_contrast_blue    = xsane.contrast_blue;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Update the info line with the latest size information.  */
static void update_param(GSGDialog *dialog, void *arg)
{
 SANE_Parameters params;
 gchar buf[200];

  if (!xsane.info_label)
    return;

  if (sane_get_parameters(gsg_dialog_get_device(dialog), &params) == SANE_STATUS_GOOD)
    {
      u_long size = 10 * params.bytes_per_line * params.lines;
      const char *unit = "B";

      if (params.format >= SANE_FRAME_RED && params.format <= SANE_FRAME_BLUE)
      {
	size *= 3;
      }

      if (size >= 10 * 1024 * 1024)
	{
	  size /= 1024 * 1024;
	  unit = "MB";
	}
      else if (size >= 10 * 1024)
	{
	  size /= 1024;
	  unit = "KB";
	}
      sprintf (buf, "%dx%d: %lu.%01lu %s", params.pixels_per_line, params.lines, size / 10, size % 10, unit);

      if (params.format == SANE_FRAME_GRAY)
      {
        xsane.xsane_color = FALSE;
      }
      else
      {
        xsane.xsane_color = TRUE;
      }
    }
  else
    sprintf(buf, "Invalid parameters.");
  gtk_label_set(GTK_LABEL(xsane.info_label), buf);


  if (xsane.preview) preview_update(xsane.preview);

  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_zoom_update(GtkAdjustment *adj_data, double *val)
{
 const SANE_Option_Descriptor *opt;
 int status;
 SANE_Word dpi;

  *val=adj_data->value;

  xsane.resolution = xsane.zoom * preferences.printerresolution;
  opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
  switch (opt->type)
  {
    case SANE_TYPE_INT:
      dpi = xsane.resolution;
    break;

    case SANE_TYPE_FIXED:
      dpi = SANE_FIX(xsane.resolution);
    break;

    default:
     fprintf(stderr, "zoom_scale_update: unknown type %d\n", opt->type);
    return;
  }

  status = sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_SET_VALUE, &dpi, 0);
  update_param(dialog, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_resolution_update(GtkAdjustment *adj_data, double *val)
{
 const SANE_Option_Descriptor *opt;
 int status;
 SANE_Word dpi;

  *val=adj_data->value;

  opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
  switch (opt->type)
  {
    case SANE_TYPE_INT:
      dpi = xsane.resolution;
    break;

    case SANE_TYPE_FIXED:
      dpi = SANE_FIX(xsane.resolution);
    break;

    default:
     fprintf(stderr, "resolution_scale_update: unknown type %d\n", opt->type);
    return;
  }
  status = sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_SET_VALUE, &dpi, 0);
  update_param(dialog, 0);
  xsane.zoom = xsane.resolution / preferences.printerresolution;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pipette_changed(GtkAdjustment *adj_data, double *val)
{
  *val=adj_data->value;
/* xxxxxxxxxxxxxxxxx */
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_gamma_changed(GtkAdjustment *adj_data, double *val)
{
  *val=adj_data->value;
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_option_menu_lookup(GSGMenuItem menu_items[], const char *string)
{
  int i;

  for (i = 0; strcmp(menu_items[i].label, string) != 0; ++i);
  return i;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_callback(GtkWidget * widget, gpointer data)
{
  GSGMenuItem *menu_item = data;
  GSGDialogElement *elem = menu_item->elem;
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  int opt_num;
  double dval;
  SANE_Word val;
  void *valp = &val;

  opt_num = elem - dialog->element;
  opt = sane_get_option_descriptor(dialog->dev, opt_num);
  switch (opt->type)
    {
    case SANE_TYPE_INT:
      sscanf(menu_item->label, "%d", &val);
      break;

    case SANE_TYPE_FIXED:
      sscanf(menu_item->label, "%lg", &dval);
      val = SANE_FIX(dval);
      break;

    case SANE_TYPE_STRING:
      valp = menu_item->label;
      break;

    default:
      fprintf(stderr, "xsane_option_menu_callback: unexpected type %d\n", opt->type);
      break;
    }
  gsg_set_option(dialog, opt_num, valp, SANE_ACTION_SET_VALUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number)
{
 GtkWidget *option_menu, *menu, *item;
 GSGMenuItem *menu_items;
 GSGDialogElement *elem;
 int i, num_items;

  elem = dialog->element + option_number;

  for (num_items = 0; str_list[num_items]; ++num_items);
  menu_items = malloc(num_items * sizeof(menu_items[0]));

  menu = gtk_menu_new();
  for (i = 0; i < num_items; ++i)
    {
      item = gtk_menu_item_new_with_label(str_list[i]);
      gtk_container_add(GTK_CONTAINER(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_option_menu_callback, menu_items + i);

      gtk_widget_show(item);

      menu_items[i].label = str_list[i];
      menu_items[i].elem  = elem;
      menu_items[i].index = i;
    }

  option_menu = gtk_option_menu_new();
  gtk_box_pack_end(GTK_BOX(parent), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_option_menu_lookup(menu_items, val));

  gtk_widget_show(option_menu);

  elem->widget = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scale_new(GtkBox *parent, char *labeltext,
                     float min, float max, float quant, float step, float xxx,
                     int digits, double *val, GtkObject **data, void *xsane_scale_callback)
{
 GtkWidget *hbox;
 GtkWidget *label;
 GtkWidget *scale;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  label = gtk_label_new(labeltext);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  *data = gtk_adjustment_new(*val, min, max, quant, step, xxx);
  scale = gtk_hscale_new(GTK_ADJUSTMENT(*data));
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pixels */
  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
  /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
  gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);
  gtk_scale_set_digits(GTK_SCALE(scale), digits);
  gtk_box_pack_end(GTK_BOX(hbox), scale, TRUE, TRUE, 5); /* make scale sizable */

  gtk_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_scale_callback, val);

  gtk_widget_show(label);
  gtk_widget_show(scale);
  gtk_widget_show(hbox);

}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scale_new_with_pixmap(GtkBox *parent, const char *xpm_d[],
                     float min, float max, float quant, float step, float xxx,
                     int digits, double *val, GtkObject **data, int option, void *xsane_scale_callback)
{
 GtkWidget *hbox;
 GtkWidget *scale;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);

  *data = gtk_adjustment_new(*val, min, max, quant, step, xxx);
  scale = gtk_hscale_new(GTK_ADJUSTMENT(*data));
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pxiels */
  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
  /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
  gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);
  gtk_scale_set_digits(GTK_SCALE(scale), digits);
  gtk_box_pack_end(GTK_BOX(hbox), scale, TRUE, TRUE, 5); /* make scale sizeable */

  gtk_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_scale_callback, val);

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(scale);
  gtk_widget_show(hbox);

  gdk_pixmap_unref(pixmap);

  if ( (dialog) && (option) )
  {
   GSGDialogElement *elem;

    elem=dialog->element + option;
    elem->data   = *data;
    elem->widget = scale;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_modus_callback(GtkWidget *xsane_parent, int *num)
{
  xsane.xsane_mode = *num;
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_separator_new(GtkWidget *xsane_parent)
{
 GtkWidget *xsane_separator;

  xsane_separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(xsane_parent), xsane_separator, FALSE, FALSE, 0);
  gtk_widget_show(xsane_separator);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printercommand_changed_callback(GtkWidget *widget, gpointer data)
{
  if (preferences.printercommand)
  {
    free((void *) preferences.printercommand);
  }

  preferences.printercommand = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data)
{
  if (preferences.filename)
    free((void *) preferences.filename);
  preferences.filename = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void browse_filename_callback(GtkWidget *widget, gpointer data)
{
  char filename[1024];

  if (preferences.filename)
    {
      strncpy(filename, preferences.filename, sizeof(filename));
      filename[sizeof(filename) - 1] = '\0';
    }
  else
    strcpy(filename, OUTFILENAME);
  gsg_get_filename("Output Filename", filename, sizeof(filename), filename);
  gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), filename);

  if (preferences.filename)
    free((void *) preferences.filename);
  preferences.filename = strdup(filename);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_outputfilename_new(GtkWidget *vbox)
{
 GtkWidget *hbox;
 GtkWidget *text;
 GtkWidget *button;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) file_xpm);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_pixmap_unref(pixmap);

  text = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.filename);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 2);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_outputfilename_changed_callback, 0);

  xsane.outputfilename_entry = text;

  button = gtk_button_new_with_label("Browse");
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) browse_filename_callback, 0);

  gtk_widget_show(button);
  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
static int
encode_devname(const char *devname, int n, char *buf)
{
  static const char hexdigit[] = "0123456789abcdef";
  char *dst, *limit;
  const char *src;
  char ch;

  limit = buf + n;
  for (src = devname, dst = buf; *src; ++src)
    {
      if (dst >= limit)
	return -1;

      ch = *src;
      /* don't use the ctype.h macros here since we don't want to
	 allow anything non-ASCII here... */
      if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
	*dst++ = ch;
      else
	{
	  /* encode */

	  if (dst + 4 >= limit)
	    return -1;

	  *dst++ = '-';
	  if (ch == '-')
	    *dst++ = '-';
	  else
	    {
	      *dst++ = hexdigit[(ch >> 4) & 0x0f];
	      *dst++ = hexdigit[(ch >> 0) & 0x0f];
	      *dst++ = '-';
	    }
	}
    }
  if (dst >= limit)
    return -1;
  *dst = '\0';
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_rgb_default_callback(GtkWidget * widget)
{
  xsane.enhancement_rgb_default = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  if (xsane.enhancement_rgb_default)
  {
    xsane.gamma_red        = 1.0;
    xsane.gamma_green      = 1.0;
    xsane.gamma_blue       = 1.0;
    xsane.brightness_red   = 0.0;
    xsane.brightness_green = 0.0;
    xsane.brightness_blue  = 0.0;
    xsane.contrast_red     = 0.0;
    xsane.contrast_green   = 0.0;
    xsane.contrast_blue    = 0.0;
  }

  xsane_update_gamma();
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_auto_enhancement_callback(GtkWidget * widget)
{
  xsane.brightness       = xsane.auto_brightness;
  xsane.brightness_red   = 0.0;
  xsane.brightness_green = 0.0;
  xsane.brightness_blue  = 0.0;
  xsane.contrast         = xsane.auto_contrast;
  xsane.contrast_red     = 0.0;
  xsane.contrast_green   = 0.0;
  xsane.contrast_blue    = 0.0;

  xsane_enhancement_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_show_histogram_callback(GtkWidget * widget)
{
  preferences.show_histogram = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_histogram)
  {
    xsane_update_histogram();
  }
  else
  {
    xsane_destroy_histogram();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_gamma_callback(GtkWidget * widget)
{
 const SANE_Option_Descriptor *opt;

  preferences.gamma_selected = (GTK_CHECK_MENU_ITEM(widget)->active != 0);

  opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.custom_gamma);
  if ((opt) ? SANE_OPTION_IS_ACTIVE(opt->cap) : FALSE)
  {
   int status;

    xsane.scanner_gamma_selected = preferences.gamma_selected;
    status = sane_control_option(dialog->dev, dialog->well_known.custom_gamma,
                                  SANE_ACTION_SET_VALUE, &xsane.scanner_gamma_selected, 0);
    if (status != SANE_STATUS_GOOD)
    {
      xsane.scanner_gamma_selected = FALSE;
    }
    xsane.software_gamma_selected = FALSE;
  }
  else
  {
    xsane.software_gamma_selected = preferences.gamma_selected;
    xsane.scanner_gamma_selected  = FALSE;
  }
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_xsane_new_callback()
{
  /* creates the XSane option window */

  GtkWidget *xsane_vbox, *xsane_hbox;
  GtkWidget *xsane_modus_menu;
  GtkWidget *xsane_modus_item;
  GtkWidget *xsane_modus_option_menu;
  GtkWidget *xsane_vbox_xsane_modus;
  GtkWidget *xsane_hbox_xsane_modus;
  GtkWidget *xsane_label;
  GtkWidget *xsane_hbox_xsane_enhancement;
  GtkWidget *xsane_frame;
  GtkWidget *xsane_button;
  GSGDialogElement *elem;


  /* xsane main options */

  xsane_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(xsane_hbox);
  xsane_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(xsane_vbox);
  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, FALSE, FALSE, 0);

  /* Notebook XSane Frame Modus */

  xsane_frame = gtk_frame_new("XSane options");
  gtk_container_border_width(GTK_CONTAINER(xsane_frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(xsane_frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_frame, FALSE, FALSE, 0);
  gtk_widget_show(xsane_frame);

  xsane_vbox_xsane_modus = gtk_vbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(xsane_frame), xsane_vbox_xsane_modus);
  gtk_widget_show(xsane_vbox_xsane_modus);

/* scan copy fax selection */

  if (xsane.mode == STANDALONE)
  {
    xsane_hbox_xsane_modus = gtk_hbox_new(FALSE,2);
    gtk_container_border_width(GTK_CONTAINER(xsane_hbox_xsane_modus), 0);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_modus, FALSE, FALSE, 0);

    xsane_label = gtk_label_new("XSane mode");
    gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_modus), xsane_label, FALSE, FALSE, 2);
    gtk_widget_show(xsane_label);

    xsane_modus_menu = gtk_menu_new();

    xsane_modus_item = gtk_menu_item_new_with_label("Scan");
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_SCAN]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label("Copy");
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_COPY]);
    gtk_widget_show(xsane_modus_item);

#if 0
    xsane_modus_item = gtk_menu_item_new_with_label("Fax");
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_FAX]);
    gtk_widget_show(xsane_modus_item);
#endif

    xsane_modus_option_menu = gtk_option_menu_new();
    gtk_box_pack_end(GTK_BOX(xsane_hbox_xsane_modus), xsane_modus_option_menu, FALSE, FALSE, 2);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_modus_option_menu), xsane_modus_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_modus_option_menu), xsane.xsane_mode);
    gtk_widget_show(xsane_modus_option_menu);
    gtk_widget_show(xsane_hbox_xsane_modus);

    if (dialog)
    {
      dialog->xsanemode_widget = xsane_modus_option_menu;
    }
  }
  else
  {
    xsane.xsane_mode = XSANE_SCAN;
  }

  {
   GtkWidget *pixmapwidget;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   GtkWidget *hbox;
   const SANE_Option_Descriptor *opt;


    /* colormode */
    if (dialog)
    {
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.scanmode);
      if (opt)
      {
        if (SANE_OPTION_IS_ACTIVE(opt->cap))
        {
          hbox = gtk_hbox_new(FALSE, 2);
          gtk_container_border_width(GTK_CONTAINER(hbox), 2);
          gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

          pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) colormode_xpm);
          pixmapwidget = gtk_pixmap_new(pixmap, mask);
          gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
          gdk_pixmap_unref(pixmap);
          gtk_widget_show(pixmapwidget);

          switch (opt->constraint_type)
          {
            case SANE_CONSTRAINT_STRING_LIST:
  	    {
	     char *set;
             SANE_Status status;

              /* use a "list-selection" widget */
              set = malloc(opt->size);
              status = sane_control_option(dialog->dev, dialog->well_known.scanmode, SANE_ACTION_GET_VALUE, set, 0);

              xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scanmode);
	    }
             break;

	    default:
              fprintf(stderr, "scanmode_selection: unknown type %d\n", opt->type);
	  }
          gtk_widget_show(hbox);
        }
      }
    }


    /* input selection */
    if (dialog)
    {
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.scansource);
      if (opt)
      {
        if (SANE_OPTION_IS_ACTIVE(opt->cap))
        {
          hbox = gtk_hbox_new(FALSE, 2);
         gtk_container_border_width(GTK_CONTAINER(hbox), 2);
         gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

         pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) scanner_xpm);
         pixmapwidget = gtk_pixmap_new(pixmap, mask);
         gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
         gdk_pixmap_unref(pixmap);
         gtk_widget_show(pixmapwidget);

          switch (opt->constraint_type)
          {
            case SANE_CONSTRAINT_STRING_LIST:
  	    {
	     char *set;
             SANE_Status status;

              /* use a "list-selection" widget */
              set = malloc(opt->size);
              status = sane_control_option(dialog->dev, dialog->well_known.scansource, SANE_ACTION_GET_VALUE, set, 0);

              xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scansource);
	    }
             break;

	    default:
              fprintf(stderr, "scansource_selection: unknown type %d\n", opt->type);
	  }
          gtk_widget_show(hbox);
        }
      }
    }

  }

  if (xsane.xsane_mode == XSANE_SCAN)
  {
   const SANE_Option_Descriptor *opt;

    if (xsane.mode == STANDALONE)
    {
      xsane_outputfilename_new(xsane_vbox_xsane_modus);
    }

    if (dialog)
    {
      /* resolution selection */
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
      if (opt)
      {
        if (SANE_OPTION_IS_ACTIVE(opt->cap))
        {
	 SANE_Word quant=0;
	 SANE_Word min=0;
	 SANE_Word max=0;

          switch (opt->type)
          {
           case SANE_TYPE_INT:
             min   = opt->constraint.range->min;
             max   = opt->constraint.range->max;
             quant = opt->constraint.range->quant;
           break;

           case SANE_TYPE_FIXED:
             min   = SANE_UNFIX(opt->constraint.range->min);
             max   = SANE_UNFIX(opt->constraint.range->max);
             quant = SANE_UNFIX(opt->constraint.range->quant);
           break;

           default:
            fprintf(stderr, "zoom_scale_update: unknown type %d\n", opt->type);
	  }

          if (quant == 0)
          {
            quant = 1;
          }

          xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), resolution_xpm, min, max, quant, quant, 0.0, 0,
                                      &xsane.resolution, &xsane.resolution_widget, dialog->well_known.dpi, xsane_resolution_update);

          elem = dialog->element + dialog->well_known.dpi;
	  elem->data = xsane.resolution_widget;
        }
      }
    }
  }
  else if (xsane.xsane_mode == XSANE_COPY)
  {
   const SANE_Option_Descriptor *opt;
   GtkWidget *pixmapwidget;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   GtkWidget *hbox;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

    pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) printer_xpm);
    pixmapwidget = gtk_pixmap_new(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
    gdk_pixmap_unref(pixmap);
    gtk_widget_show(pixmapwidget);
    gtk_widget_show(hbox);


    if (dialog)
    {
      /* zoom selection */
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
      if (opt)
      {
        if (SANE_OPTION_IS_ACTIVE(opt->cap))
        {
	 double quant=0.0;
	 double min=0.0;
	 double max=0.0;

          switch (opt->type)
          {
           case SANE_TYPE_INT:
             min   = ((double) opt->constraint.range->min) / preferences.printerresolution;
             max   = ((double) opt->constraint.range->max) / preferences.printerresolution;
             quant = ((double) opt->constraint.range->quant) / preferences.printerresolution;
           break;

           case SANE_TYPE_FIXED:
             min   = SANE_UNFIX(opt->constraint.range->min) / preferences.printerresolution;
             max   = SANE_UNFIX(opt->constraint.range->max) / preferences.printerresolution;
             quant = SANE_UNFIX(opt->constraint.range->quant) / preferences.printerresolution;
           break;

           default:
            fprintf(stderr, "zoom_scale_update: unknown type %d\n", opt->type);
	  }

          if (quant == 0)
          {
            quant = 0.01;
          }

          xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), zoom_xpm, min, max,
				      quant, quant, 0.0, 2, &xsane.zoom, &xsane.zoom_widget, dialog->well_known.dpi, xsane_zoom_update);
        }
      }
    }
  }
  else /* XSANE_FAX */
  {
  }

  xsane.scanner_gamma_selected = FALSE;
  if (dialog)
  {
     /* test if scanner gamma table is selected */
     if (dialog->well_known.custom_gamma >0)
     {
      int status;
       status = sane_control_option(dialog->dev, dialog->well_known.custom_gamma,
                                    SANE_ACTION_GET_VALUE, &xsane.scanner_gamma_selected, 0);
       if (status == SANE_STATUS_GOOD)
       {
         xsane.software_gamma_selected = FALSE;
         preferences.gamma_selected = xsane.scanner_gamma_selected;
       }
     }
  }

  if (preferences.gamma_selected)
  {
    /* Notebook XSane Frame Enhancement */


    if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
    {
      xsane_separator_new(xsane_vbox_xsane_modus);
    }

    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_xpm, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma, &xsane.gamma_widget, 0, xsane_gamma_changed);
    if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
    {
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_red_xpm, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                  &xsane.gamma_red  , &xsane.gamma_red_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_green_xpm, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                  &xsane.gamma_green, &xsane.gamma_green_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_blue_xpm, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                  &xsane.gamma_blue , &xsane.gamma_blue_widget, 0, xsane_gamma_changed);

      xsane_separator_new(xsane_vbox_xsane_modus);
    }

    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness, &xsane.brightness_widget, 0, xsane_gamma_changed);
    if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
    {
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_red_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.brightness_red  , &xsane.brightness_red_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_green_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.brightness_green, &xsane.brightness_green_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_blue_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.brightness_blue,  &xsane.brightness_blue_widget, 0, xsane_gamma_changed);

      xsane_separator_new(xsane_vbox_xsane_modus);
    }

    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast, &xsane.contrast_widget, 0, xsane_gamma_changed);
    if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
    {
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_red_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.contrast_red  , &xsane.contrast_red_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_green_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.contrast_green, &xsane.contrast_green_widget, 0, xsane_gamma_changed);
      xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_blue_xpm, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                  &xsane.contrast_blue,  &xsane.contrast_blue_widget, 0, xsane_gamma_changed);

      xsane_separator_new(xsane_vbox_xsane_modus);
    }
/*
   xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), pipette_white_xpm, 0.0, 100.0, 0.1, 0.1, 0.0, 1,
                               &xsane.pipette_white, &xsane.pipette_white_widget, 0, xsane_pipette_changed);

   xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), pipette_black_xpm, 0.0, 100.0, 0.1, 0.1, 0.0, 1,
                               &xsane.pipette_black, &xsane.pipette_black_widget, 0, xsane_pipette_changed);
*/

   if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
   {
     xsane_separator_new(xsane_vbox_xsane_modus);
   }

   if (xsane.xsane_color)
   {
     xsane_button = gtk_check_button_new_with_label("RGB default");
     gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(xsane_button), xsane.enhancement_rgb_default);
     gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_button, FALSE, FALSE, 2);
     gtk_widget_show(xsane_button);
     gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked",
                        (GtkSignalFunc) xsane_enhancement_rgb_default_callback, 0);
   }

   xsane_hbox_xsane_enhancement = gtk_hbox_new(FALSE,2);
   gtk_container_border_width(GTK_CONTAINER(xsane_hbox_xsane_enhancement), 0);
   gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_enhancement, FALSE, FALSE, 0);
   gtk_widget_show(xsane_hbox_xsane_enhancement);

   xsane_label = gtk_label_new("Set enhancement:");
   gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_label, FALSE, FALSE, 2);
   gtk_widget_show(xsane_label);

   xsane_button = gtk_button_new_with_label("Auto");
   gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
   gtk_widget_show(xsane_button);
   gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_auto_enhancement_callback, 0);

   xsane_button = gtk_button_new_with_label("Default");
   gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
   gtk_widget_show(xsane_button);
   gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_restore_default, 0);

   xsane_button = gtk_button_new_with_label("Restore");
   gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
   gtk_widget_show(xsane_button);
   gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_restore_saved, 0);

   xsane_button = gtk_button_new_with_label("Save");
   gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
   gtk_widget_show(xsane_button);
   gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_save, 0);

 }
 else
 {
   xsane_enhancement_restore_default();
 }
 xsane_update_histogram();

 return(xsane_hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int decode_devname(const char *encoded_devname, int n, char *buf)
{
  char *dst, *limit;
  const char *src;
  char ch, val;

  limit = buf + n;
  for (src = encoded_devname, dst = buf; *src; ++dst)
    {
      if (dst >= limit)
	return -1;

      ch = *src++;
      /* don't use the ctype.h macros here since we don't want to
	 allow anything non-ASCII here... */
      if (ch != '-')
	*dst = ch;
      else
	{
	  /* decode */

	  ch = *src++;
	  if (ch == '-')
	    *dst = ch;
	  else
	    {
	      if (ch >= 'a' && ch <= 'f')
		val = (ch - 'a') + 10;
	      else
		val = (ch - '0');
	      val <<= 4;

	      ch = *src++;
	      if (ch >= 'a' && ch <= 'f')
		val |= (ch - 'a') + 10;
	      else
		val |= (ch - '0');

	      *dst = val;

	      ++src;	/* simply skip terminating '-' for now... */
	    }
	}
    }
  if (dst >= limit)
    return -1;
  *dst = '\0';
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void query(void)
{
  static GParamDef args[] =
    {
      {PARAM_INT32, "run_mode", "Interactive, non-interactive"},
    };
  static GParamDef *return_vals = NULL;
  static int nargs = sizeof(args) / sizeof(args[0]);
  static int nreturn_vals = 0;
  char mpath[1024];
  char name[1024];
  size_t len;
  int i, j;

  gimp_install_procedure(
      "xsane",
      "Front-end to the SANE interface",
      "This function provides access to scanners and other image acquisition "
      "devices through the SANE (Scanner Access Now Easy) interface.",
       "Oliver Rauch, Andy Beck, Tristan Tarrant, and David Mosberger",
       "Oliver Rauch, Andy Beck, Tristan Tarrant, and David Mosberger",
      "1998",
      "<Toolbox>/Xtns/Acquire Image/Device dialog...",
      "RGB, GRAY",
      PROC_EXTENSION,
      nargs, nreturn_vals,
      args, return_vals);

  sane_init(0, 0);
  sane_get_devices(&devlist, SANE_FALSE);

  for (i = 0; devlist[i]; ++i)
    {
      strcpy(name, "xsane-");
      if (encode_devname(devlist[i]->name, sizeof(name) - 11, name + 11) < 0)
	continue;	/* name too long... */

      strncpy(mpath, "<Toolbox>/Xtns/Acquire Image/", sizeof(mpath));
      len = strlen(mpath);
      for (j = 0; devlist[i]->name[j]; ++j)
	{
	  if (devlist[i]->name[j] == '/')
	    mpath[len++] = '\'';
	  else
	    mpath[len++] = devlist[i]->name[j];
	}
      mpath[len++] = '\0';

      gimp_install_procedure
	(name, "Front-end to the SANE interface",
	 "This function provides access to scanners and other image "
	 "acquisition devices through the SANE (Scanner Access Now Easy) "
	 "interface.",
         "Oliver Rauch, Andy Beck, Tristan Tarrant, and David Mosberger",
         "Oliver Rauch, Andy Beck, Tristan Tarrant, and David Mosberger",
	 "1998", mpath, "RGB, GRAY", PROC_EXTENSION,
	 nargs, nreturn_vals, args, return_vals);
    }
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void run (char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals)
{
  static GParam values[2];
  GRunModeType run_mode;
  char devname[1024];
  char *args[2];
  int nargs;

  run_mode = param[0].data.d_int32;
  xsane.mode = GIMP_EXTENSION;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = STATUS_CALLING_ERROR;

  nargs = 0;
  args[nargs++] = "xssane";

  seldev = -1;
  if (strncmp(name, "xsane-", 6) == 0)
    {
      if (decode_devname(name + 6, sizeof(devname), devname) < 0)
	return;			/* name too long */
      args[nargs++] = devname;
    }

  switch (run_mode)
    {
    case RUN_INTERACTIVE:
      interface(nargs, args);
      values[0].data.d_status = STATUS_SUCCESS;
      break;

    case RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      break;

    case RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      break;

    default:
      break;
    }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void null_print_func(gchar *msg)
{
}

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void update_preview(GSGDialog *dialog, void *arg)
{
  if (xsane.preview)
  {
    preview_update(xsane.preview);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_xsane_save(void)
{
  char filename[PATH_MAX];
  int fd;

  /* first save xsane-specific preferences: */
  gsg_make_path(sizeof(filename), filename, "xsane", "xsane", 0, ".rc");
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      char buf[256];

      snprintf(buf, sizeof(buf), "Failed to create file: %s.", strerror(errno));
      gsg_error(buf);
      return;
    }
  preferences_save(fd);
  close(fd);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_xsane_restore(void)
{
  char filename[PATH_MAX];
  int fd;

  gsg_make_path(sizeof(filename), filename, "xsane", "xsane", 0, ".rc");
  fd = open(filename, O_RDONLY);
  if (fd >= 0)
    {
      preferences_restore(fd);
      close(fd);
    }
  if (!preferences.filename)
    preferences.filename = strdup(OUTFILENAME);

  if (!preferences.printercommand)
    preferences.printercommand = strdup(PRINTERCOMMAND);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void quit_xsane(void)
{
  if (xsane.preview)
    {
      Preview *preview = xsane.preview;
      xsane.preview = 0;
      preview_destroy(preview);
    }
  while (gsg_message_dialog_active)
    {
      if (!gtk_events_pending ())
	usleep(100000);
      gtk_main_iteration();
    }
  pref_xsane_save();
  if (dialog && gsg_dialog_get_device(dialog))
    sane_close(gsg_dialog_get_device(dialog));
  sane_exit();
  gtk_main_quit();

  if (xsane.preview_gamma_data_red)
  {
    free(xsane.preview_gamma_data_red);
    free(xsane.preview_gamma_data_green);
    free(xsane.preview_gamma_data_blue);
    xsane.preview_gamma_data_red   = 0;
    xsane.preview_gamma_data_green = 0;
    xsane.preview_gamma_data_blue  = 0;
  }

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == GIMP_EXTENSION)
    gimp_quit();
#endif
  exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when window manager's "delete" (or "close") function is
   invoked.  */
static gint scan_win_delete(GtkWidget *w, gpointer data)
{
  quit_xsane();
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void preview_window_destroyed(GtkWidget *widget, gpointer call_data)
{
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(call_data), FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the preview button is pressed. */

static void scan_preview(GtkWidget * widget, gpointer call_data)
{
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    if (!xsane.preview)
    {
      xsane.preview = preview_new(dialog);
      if (xsane.preview && xsane.preview->top)
      {
        gtk_signal_connect(GTK_OBJECT(xsane.preview->top), "destroy", GTK_SIGNAL_FUNC(preview_window_destroyed), widget);
      }
      else
      {
        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget), FALSE);
      }
    }
  }
  else if (xsane.preview)
  {
    preview_destroy(xsane.preview);
    xsane.preview = 0;
  }
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H
static void advance(void)
{
  if (++xsane.x >= xsane.param.pixels_per_line)
    {
      int tile_height = gimp_tile_height();

      xsane.x = 0;
      ++xsane.y;
      if (xsane.y % tile_height == 0)
	{
	  gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile,
				  0, xsane.y - tile_height,
				  xsane.param.pixels_per_line, tile_height);
	  if (xsane.param.format >= SANE_FRAME_RED
	      && xsane.param.format <= SANE_FRAME_BLUE)
	    {
	      int height;

	      xsane.tile_offset %= 3;
	      if (!xsane.first_frame)
		{
		  /* get the data for the existing tile: */
		  height = tile_height;
		  if (xsane.y + height >= xsane.param.lines)
		    height = xsane.param.lines - xsane.y;
		  gimp_pixel_rgn_get_rect(&xsane.region, xsane.tile, 0, xsane.y, xsane.param.pixels_per_line, height);
		}
	    }
	  else
	    xsane.tile_offset = 0;
	}
    }
}

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void input_available(gpointer data, gint source, GdkInputCondition cond)
{
  SANE_Handle dev = gsg_dialog_get_device (dialog);
  static unsigned char buf[32768];
  SANE_Status status;
  SANE_Int len;
  int i;

  while (1)
    {
      status = sane_read(dev, buf, sizeof(buf), &len);
      if (status != SANE_STATUS_GOOD)
	{
	  if (status == SANE_STATUS_EOF)
	    {
	      if (!xsane.param.last_frame)
		{
		  scan_start();
		  break;
		}
	    }
	  else
	    {
	      snprintf(buf, sizeof(buf), "Error during read: %s.", sane_strstatus (status));
	      gsg_error(buf);
	    }
	  scan_done();
	  return;
	}
      if (!len)
	break;			/* out of data for now */

      xsane.bytes_read += len;
      progress_update(xsane.progress, xsane.bytes_read / (gfloat) xsane.num_bytes);

      if (xsane.input_tag < 0)
	while (gtk_events_pending())
	  gtk_main_iteration();

      switch (xsane.param.format)
	{
	case SANE_FRAME_GRAY:
	  if (xsane.mode == STANDALONE)
	  {
	   int i;
	   char val;

            if (xsane.software_gamma_selected)
	    {
	      for (i=0; i < len; ++i)
	      {
	        val = xsane.preview_gamma_data_red[(int) buf[i]];
	        fwrite(&val, 1, 1, xsane.out);
	      }
	    }
	    else
	    {
	      fwrite(buf, 1, len, xsane.out);
	    }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
	  else
	    {
	      switch (xsane.param.depth)
		{
		case 1:
		  for (i = 0; i < len; ++i)
		    {
		      u_char mask;
		      int j;

		      mask = buf[i];
		      for (j = 7; j >= 0; --j)
			{
			  u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
			  xsane.tile[xsane.tile_offset++] = gl;
			  advance();
			  if (xsane.x == 0)
			    break;
			}
		    }
		  break;

		case 8:
                  if (xsane.software_gamma_selected)
		  {
		    for (i = 0; i < len; ++i)
		    {
		      xsane.tile[xsane.tile_offset++] = xsane.preview_gamma_data_red[(int) buf[i]];
		      advance();
		    }
		  }
		  else
		  {
		    for (i = 0; i < len; ++i)
		    {
		      xsane.tile[xsane.tile_offset++] = buf[i];
		      advance();
		    }
		  }
		  break;

		default:
		  goto bad_depth;
		}
	    }
#endif /* HAVE_LIBGIMP_GIMP_H */
	  break;

	case SANE_FRAME_RGB:
	  if (xsane.mode == STANDALONE)
	  {
	   int i;
	   char val;
	   const SANE_Option_Descriptor *opt;

	    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.custom_gamma);

            if (xsane.software_gamma_selected)
	    {
	      for (i=0; i < len; ++i)
	      {
	        if (dialog->pixelcolor == 0)
	        {
	          val = xsane.preview_gamma_data_red[(int) buf[i]];
		  dialog->pixelcolor++;
	        }
                else if (dialog->pixelcolor == 1)
	        {
	          val = xsane.preview_gamma_data_green[(int) buf[i]];
		  dialog->pixelcolor++;
	        }
                else
	        {
	          val = xsane.preview_gamma_data_blue[(int) buf[i]];
		  dialog->pixelcolor = 0;
	        }
	        fwrite(&val, 1, 1, xsane.out);
	      }
	    }
            else
	    {
	      fwrite(buf, 1, len, xsane.out);
	    }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
	  else
	    {
	      switch (xsane.param.depth)
		{
		case 1:
		  if (xsane.param.format == SANE_FRAME_RGB)
		    goto bad_depth;
		  for (i = 0; i < len; ++i)
		    {
		      u_char mask;
		      int j;

		      mask = buf[i];
		      for (j = 0; j < 8; ++j)
			{
			  u_char gl = (mask & 1) ? 0xff : 0x00;
			  mask >>= 1;
			  xsane.tile[xsane.tile_offset++] = gl;
			  advance();
			  if (xsane.x == 0)
			    break;
			}
		    }
		  break;

		case 8:
                  if (xsane.software_gamma_selected)
	          {
		    for (i = 0; i < len; ++i)
		    {

	              if (xsane.tile_offset % 3 == 0)
	              {
		        xsane.tile[xsane.tile_offset++] = xsane.preview_gamma_data_red[(int) buf[i]];
	              }
                      else if (xsane.tile_offset % 3 == 1)
	              {
		        xsane.tile[xsane.tile_offset++] = xsane.preview_gamma_data_green[(int) buf[i]];
	              }
                      else
                      {
		        xsane.tile[xsane.tile_offset++] = xsane.preview_gamma_data_blue[(int) buf[i]];
		      }

		      if (xsane.tile_offset % 3 == 0)
			advance();
		    }
	          }
		  else
		  {
		    for (i = 0; i < len; ++i)
		    {
		      xsane.tile[xsane.tile_offset++] = buf[i];
		      if (xsane.tile_offset % 3 == 0)
			advance();
		    }
		  }
		  break;

		default:
		  goto bad_depth;
		}
	    }
#endif /* HAVE_LIBGIMP_GIMP_H */
	  break;

	case SANE_FRAME_RED:
	case SANE_FRAME_GREEN:
	case SANE_FRAME_BLUE:
	  if (xsane.mode == STANDALONE)
	    for (i = 0; i < len; ++i)
	      {
		fwrite(buf + i, 1, 1, xsane.out);
		fseek(xsane.out, 2, SEEK_CUR);
	      }
#ifdef HAVE_LIBGIMP_GIMP_H
	  else
	    {
	      switch (xsane.param.depth)
		{
		case 1:
		  for (i = 0; i < len; ++i)
		    {
		      u_char mask;
		      int j;

		      mask = buf[i];
		      for (j = 0; j < 8; ++j)
			{
			  u_char gl = (mask & 1) ? 0xff : 0x00;
			  mask >>= 1;
			  xsane.tile[xsane.tile_offset] = gl;
			  xsane.tile_offset += 3;
			  advance();
			  if (xsane.x == 0)
			    break;
			}
		    }
		  break;

		case 8:
		  for (i = 0; i < len; ++i)
		    {
		      xsane.tile[xsane.tile_offset] = buf[i];
		      xsane.tile_offset += 3;
		      advance();
		    }
		  break;
		}
	    }
#endif /* HAVE_LIBGIMP_GIMP_H */
	  break;

	default:
	  fprintf (stderr, "%s.input_available: bad frame format %d\n",
		   prog_name, xsane.param.format);
	  scan_done();
	  return;
	}
    }
  return;

#ifdef HAVE_LIBGIMP_GIMP_H
bad_depth:
  snprintf(buf, sizeof(buf), "Cannot handle depth %d.",
	    xsane.param.depth);
  gsg_error(buf);
  scan_done();
  return;
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void scan_done(void)
{
  gsg_set_sensitivity(dialog, TRUE);

  if (xsane.input_tag >= 0)
    {
      gdk_input_remove(xsane.input_tag);
      xsane.input_tag = -1;
    }

  if (!xsane.progress)
    return;

  progress_free(xsane.progress);
  xsane.progress = 0;

  if (xsane.mode == STANDALONE)
    {
      fclose(xsane.out);
      xsane.out = 0;
      if (xsane.xsane_mode == XSANE_COPY)
      {
       FILE *outfile;
       FILE *infile;
       char copyfilename[PATH_MAX];

         gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", "photocopy", dialog->dev_name, ".tmp");
         infile = fopen(copyfilename, "r");
         outfile = popen(preferences.printercommand, "w");
	 if ((outfile != 0) && (infile != 0))
	 {
	   fseek(infile, xsane.header_size, SEEK_SET);
	   if (xsane.xsane_color)
	   {
             xsane_ps_color_out(outfile, infile,
                        xsane.param.depth /* bits */,
			xsane.param.pixels_per_line, xsane.param.lines,
                        0 /* left */, 0 /* bottom */,
			xsane.param.pixels_per_line/(float)preferences.printerresolution /* width in inch */,
			xsane.param.lines/(float)preferences.printerresolution /* height in inch */);
           }
	   else
	   {
             xsane_ps_gray_out(outfile, infile,
                        xsane.param.depth /* bits */,
			xsane.param.pixels_per_line, xsane.param.lines,
                        0 /* left */, 0 /* bottom */,
			xsane.param.pixels_per_line/(float)preferences.printerresolution /* width in inch */,
			xsane.param.lines/(float)preferences.printerresolution /* height in inch */);
	   }
           fclose(outfile);
	 }
	 fclose(infile);
	 remove(copyfilename);
      }
    }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
    {
      int remaining;

      /* GIMP mode */
      if (xsane.y > xsane.param.lines)
	xsane.y = xsane.param.lines;

      remaining = xsane.y % gimp_tile_height();
      if (remaining)
	gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile,
				 0, xsane.y - remaining,
				 xsane.param.pixels_per_line, remaining);
      gimp_drawable_flush(xsane.drawable);
      gimp_display_new(xsane.image_ID);
      gimp_drawable_detach(xsane.drawable);
      free(xsane.tile);
      xsane.tile = 0;
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  xsane.header_size = 0;
  sane_cancel(gsg_dialog_get_device(dialog));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void
progress_cancel(void)
{
  sane_cancel(gsg_dialog_get_device(dialog));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void scan_start(void)
{
  SANE_Status status;
  SANE_Handle dev = gsg_dialog_get_device(dialog);
  const char *frame_type = 0;
  char buf[256];
  int fd;

  gsg_set_sensitivity(dialog, FALSE);

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == GIMP_EXTENSION && xsane.tile)
    {
      int height, remaining;

      /* write the last tile of the frame to the GIMP region: */

      if (xsane.y > xsane.param.lines)	/* sanity check */
	xsane.y = xsane.param.lines;

      remaining = xsane.y % gimp_tile_height();
      if (remaining)
	gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile,
				 0, xsane.y - remaining,
				 xsane.param.pixels_per_line, remaining);

      /* initialize the tile with the first tile of the GIMP region: */

      height = gimp_tile_height();
      if (height >= xsane.param.lines)
	height = xsane.param.lines;
      gimp_pixel_rgn_get_rect(&xsane.region, xsane.tile, 0, 0, xsane.param.pixels_per_line, height);
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  xsane.x = xsane.y = 0;

  status = sane_start(dev);
  if (status != SANE_STATUS_GOOD)
    {
      gsg_set_sensitivity(dialog, TRUE);

      snprintf(buf, sizeof(buf), "Failed to start scanner: %s", sane_strstatus(status));
      gsg_error(buf);
      return;
    }

  status = sane_get_parameters(dev, &xsane.param);
  if (status != SANE_STATUS_GOOD)
    {
      gsg_set_sensitivity(dialog, TRUE);

      snprintf(buf, sizeof(buf), "Failed to get parameters: %s", sane_strstatus(status));
      gsg_error(buf);
      return;
    }

  xsane.num_bytes = xsane.param.lines * xsane.param.bytes_per_line;
  xsane.bytes_read = 0;

  switch (xsane.param.format)
    {
    case SANE_FRAME_RGB:	frame_type = "RGB"; break;
    case SANE_FRAME_RED:	frame_type = "red"; break;
    case SANE_FRAME_GREEN:	frame_type = "green"; break;
    case SANE_FRAME_BLUE:	frame_type = "blue"; break;
    case SANE_FRAME_GRAY:	frame_type = "gray"; break;
    }

  if (xsane.mode == STANDALONE)
    {				/* We are running in standalone mode */
      if (!xsane.header_size)
	{
	  switch (xsane.param.format)
	    {
	    case SANE_FRAME_RGB:
	    case SANE_FRAME_RED:
	    case SANE_FRAME_GREEN:
	    case SANE_FRAME_BLUE:
	      fprintf(xsane.out, "P6\n# SANE data follows\n%d %d\n255\n",
		       xsane.param.pixels_per_line, xsane.param.lines);
	      break;

	    case SANE_FRAME_GRAY:
	      if (xsane.param.depth == 1)
		fprintf(xsane.out, "P4\n# SANE data follows\n%d %d\n",
			 xsane.param.pixels_per_line, xsane.param.lines);
	      else
		fprintf(xsane.out, "P5\n# SANE data follows\n%d %d\n255\n",
			 xsane.param.pixels_per_line, xsane.param.lines);
	      break;
	    }
	  xsane.header_size = ftell(xsane.out);
	}
      if (xsane.param.format >= SANE_FRAME_RED
	  && xsane.param.format <= SANE_FRAME_BLUE)
	fseek(xsane.out, xsane.header_size + xsane.param.format - SANE_FRAME_RED, SEEK_SET);
      if (xsane.xsane_mode == XSANE_SCAN)
      snprintf(buf, sizeof(buf), "Receiving %s data for `%s'...", frame_type, preferences.filename);
      else if (xsane.xsane_mode == XSANE_COPY)
      snprintf(buf, sizeof(buf), "Receiving %s data for photocopy ...", frame_type);
      else if (xsane.xsane_mode == XSANE_FAX)
      snprintf(buf, sizeof(buf), "Receiving %s data for fax ...", frame_type);
    }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
    {
      size_t tile_size;

      /* We are running under the GIMP */

      xsane.tile_offset = 0;
      tile_size = xsane.param.pixels_per_line * gimp_tile_height();
      if (xsane.param.format != SANE_FRAME_GRAY)
	tile_size *= 3;	/* 24 bits/pixel */
      if (xsane.tile)
	xsane.first_frame = 0;
      else
	{
	  GImageType image_type = RGB;
	  GDrawableType drawable_type = RGB_IMAGE;
	  gint32 layer_ID;

	  if (xsane.param.format == SANE_FRAME_GRAY)
	    {
	      image_type = GRAY;
	      drawable_type = GRAY_IMAGE;
	    }

	  xsane.image_ID = gimp_image_new(xsane.param.pixels_per_line,
					     xsane.param.lines, image_type);
	  layer_ID = gimp_layer_new(xsane.image_ID, "Background",
				     xsane.param.pixels_per_line,
				     xsane.param.lines,
				     drawable_type, 100, NORMAL_MODE);
	  gimp_image_add_layer(xsane.image_ID, layer_ID, 0);

	  xsane.drawable = gimp_drawable_get(layer_ID);
	  gimp_pixel_rgn_init(&xsane.region, xsane.drawable, 0, 0,
			       xsane.drawable->width,
			       xsane.drawable->height, TRUE, FALSE);
	  xsane.tile = g_new(guchar, tile_size);
	  xsane.first_frame = 1;
	}
      if (xsane.param.format >= SANE_FRAME_RED
	  && xsane.param.format <= SANE_FRAME_BLUE)
	xsane.tile_offset = xsane.param.format - SANE_FRAME_RED;
      snprintf(buf, sizeof(buf), "Receiving %s data for GIMP...",
		frame_type);
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  dialog->pixelcolor = 0;

  if (xsane.progress)
    progress_free(xsane.progress);
  xsane.progress = progress_new("Scanning", buf, (GtkSignalFunc) progress_cancel, 0);

  xsane.input_tag = -1;
  if (sane_set_io_mode (dev, SANE_TRUE) == SANE_STATUS_GOOD && sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD)
    xsane.input_tag = gdk_input_add(fd, GDK_INPUT_READ, input_available, 0);
  else
    input_available(0, -1, GDK_INPUT_READ);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the scan button is pressed */
static void scan_dialog(GtkWidget * widget, gpointer call_data)
{
  char buf[256];

  if (xsane.mode == STANDALONE)  				/* We are running in standalone mode */
  {
      if (xsane.xsane_mode == XSANE_COPY)
      {
       char filename[PATH_MAX];

        gsg_make_path(sizeof(filename), filename, "xsane", "photocopy", dialog->dev_name, ".tmp");
        xsane.out = fopen(filename, "w");
      }
      else
      {
        xsane.out = fopen(preferences.filename, "w");
      }

      if (!xsane.out)
      {
        snprintf(buf, sizeof(buf), "Failed to open `%s': %s", preferences.filename, strerror(errno));
        gsg_error(buf);
        return;
      }
  }
	   

  if (xsane.scanner_gamma_selected)
  {
   int gamma_gray_size  = 256;
   int gamma_red_size   = 256;
   int gamma_green_size = 256;
   int gamma_blue_size  = 256;
   int gamma_gray_max   = 256;
   int gamma_red_max    = 256;
   int gamma_green_max  = 256;
   int gamma_blue_max   = 256;

    if (dialog)
    {
     const SANE_Option_Descriptor *opt;

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);
      if (opt)
      {
        gamma_gray_size = opt->size;
        gamma_gray_max  = opt->constraint.range->max;
      }

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);
      if (opt)
      {
        gamma_red_size = opt->size;
        gamma_red_max  = opt->constraint.range->max;
      }

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_g);
      if (opt)
      {
        gamma_green_size = opt->size;
        gamma_green_max  = opt->constraint.range->max;
      }

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_b);
      if (opt)
      {
        gamma_blue_size = opt->size;
        gamma_blue_max  = opt->constraint.range->max;
      }
    }

    if (xsane.xsane_color)
    {
      /* set gray gamma to 1.0, this prevents unnecessary color reduction */

      xsane.gamma_data       = malloc(gamma_gray_size  * sizeof(SANE_Int));
      xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
      xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
      xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

      xsane_create_gamma_curve(xsane.gamma_data, 1.0, 0.0, 0.0, gamma_gray_size, gamma_gray_max);

      xsane_create_gamma_curve(xsane.gamma_data_red,
		 	       xsane.gamma * xsane.gamma_red,
			       xsane.brightness + xsane.brightness_red,
			       xsane.contrast + xsane.contrast_red, gamma_red_size, gamma_red_max);

      xsane_create_gamma_curve(xsane.gamma_data_green,
			       xsane.gamma * xsane.gamma_green,
			       xsane.brightness + xsane.brightness_green,
			       xsane.contrast + xsane.contrast_green, gamma_green_size, gamma_green_max);

      xsane_create_gamma_curve(xsane.gamma_data_blue,
			       xsane.gamma * xsane.gamma_blue,
			       xsane.brightness + xsane.brightness_blue,
			       xsane.contrast + xsane.contrast_blue , gamma_blue_size, gamma_blue_max);

      gsg_update_vector(dialog, dialog->well_known.gamma_vector  , xsane.gamma_data);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_r, xsane.gamma_data_red);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_g, xsane.gamma_data_green);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_b, xsane.gamma_data_blue);

      free(xsane.gamma_data);
      free(xsane.gamma_data_red);
      free(xsane.gamma_data_green);
      free(xsane.gamma_data_blue);
      xsane.gamma_data       = 0;
      xsane.gamma_data_red   = 0;
      xsane.gamma_data_green = 0;
      xsane.gamma_data_blue  = 0;
    }
    else
    {
      xsane.gamma_data = malloc(gamma_gray_size  * sizeof(SANE_Int));
      xsane_create_gamma_curve(xsane.gamma_data,
		 	       xsane.gamma,
			       xsane.brightness,
			       xsane.contrast, gamma_gray_size, gamma_gray_max);

      gsg_update_vector(dialog, dialog->well_known.gamma_vector  , xsane.gamma_data);
      free(xsane.gamma_data);
      xsane.gamma_data       = 0;
    }
  }

  scan_start();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0

static void zoom_in_preview(GtkWidget * widget, gpointer data)
{
  if (Selection.x1 >= Selection.x2
      || Selection.y1 >= Selection.y2
      || !Selection.active)
    return;

  Selection.active = FALSE;
  draw_selection(TRUE);

  gtk_ruler_set_range(GTK_RULER (xsane.hruler), Selection.x1, Selection.x2, 0, 20);
  gtk_ruler_set_range(GTK_RULER (xsane.vruler), Selection.y1, Selection.y2, 0, 20);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void zoom_out_preview(GtkWidget * widget, gpointer data)
{
  gtk_ruler_set_range(GTK_RULER(xsane.hruler), 0, Preview.PhysWidth, 0, 20);
  gtk_ruler_set_range(GTK_RULER(xsane.vruler), 0, Preview.PhysHeight, 0, 20);
}

#endif /* 0 */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void files_exit_callback(GtkWidget *widget, gpointer data)
{
  quit_xsane();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *files_build_menu(void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new();

  item = gtk_menu_item_new();
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label("Exit");
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) files_exit_callback, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_set_unit_callback(GtkWidget *widget, gpointer data)
{
  const char *unit = data;
  double unit_conversion_factor = 1.0;

  if (strcmp (unit, "cm") == 0)
  {
    unit_conversion_factor = 10.0;
  }
  else if (strcmp (unit, "in") == 0)
  {
    unit_conversion_factor = 25.4;
  }

  preferences.length_unit = unit_conversion_factor;

  xsane_refresh_dialog(dialog);
  if (xsane.preview)
  {
    preview_update(xsane.preview);
  }

  pref_xsane_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void update_int_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;
  char *start, *end;
  int v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = (int) strtol(start, &end, 10);
  if (end > start && v > 0)
    *valuep = v;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void update_bool_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;

  *valuep = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void update_double_callback(GtkWidget *widget, gpointer data)
{
  double *valuep = data;
  char *start, *end;
  double v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = strtod(start, &end);
  if (end > start && v > 0.0)
    *valuep = v;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data)
{
  char buf[1024];

  gtk_widget_destroy((GtkWidget *)data);
  pref_xsane_save();

  snprintf(buf, sizeof(buf), "It may be necessary to restart %s for the changes to take effect.", prog_name);
  gsg_warning(buf);
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void close_callback(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = data;

  gtk_widget_destroy(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_dialog(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog, *vbox, *hbox, *button, *label, *text;
  char buf[64];

  dialog = gtk_dialog_new();
  sprintf(buf, "%s setup", prog_name);
  gtk_window_set_title(GTK_WINDOW(dialog), buf);

  vbox = GTK_DIALOG(dialog)->vbox;


  /* printercommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

  label = gtk_label_new("Printercommand:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printercommand);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 2);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_printercommand_changed_callback, 0);

  gtk_widget_show(text);
  gtk_widget_show(hbox);

  /* printerresolution : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

  label = gtk_label_new("Printerresolution (dpi):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  sprintf(buf, "%d", preferences.printerresolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 2);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) update_int_callback, &preferences.printerresolution);

  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* preserve preview image: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
  button = gtk_check_button_new_with_label("Preserve preview image");
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) update_bool_callback, &preferences.preserve_preview);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preserve_preview);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);

  gtk_widget_show(button);
  gtk_widget_show(hbox);

  /* private colormap: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
  button = gtk_check_button_new_with_label("Use private colormap");
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) update_bool_callback, &preferences.preview_own_cmap);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preview_own_cmap);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);

  gtk_widget_show(button);
  gtk_widget_show(hbox);

  /* preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  sprintf(buf, "%g", preferences.preview_gamma);
  text = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 2);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) update_double_callback, &preferences.preview_gamma);
  gtk_widget_show(text);

  /* fill in action area: */
  hbox = GTK_DIALOG(dialog)->action_area;

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_options_ok_callback, dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) close_callback, dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_device_save(GtkWidget *widget, gpointer data)
{
  char filename[PATH_MAX];
  int fd;

  gsg_make_path(sizeof(filename), filename, "xsane", 0, dialog->dev_name, ".rc");
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      char buf[256];

      snprintf(buf, sizeof(buf), "Failed to create file: %s.", strerror(errno));
      gsg_error(buf);
      return;
    }
  sanei_save_values(fd, dialog->dev);
  close(fd);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_device_restore(void)
{
  char filename[PATH_MAX];
  int fd;

  gsg_make_path(sizeof(filename), filename, "xsane", 0, dialog->dev_name, ".rc");
  fd = open(filename, O_RDONLY);
  if (fd < 0)
    return;
  sanei_load_values(fd, dialog->dev);
  close(fd);

  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void pref_toggle_tooltips(GtkWidget *widget, gpointer data)
{
  preferences.tooltips_enabled = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  gsg_set_tooltips(dialog, preferences.tooltips_enabled);
  pref_xsane_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget * pref_build_menu(void)
{
  GtkWidget *menu, *item, *submenu, *subitem;

  menu = gtk_menu_new();


  /* XSane setup dialog */

  item = gtk_menu_item_new_with_label("XSane setup");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_setup_dialog, 0);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);


  /* tooltips submenu: */

  item = gtk_check_menu_item_new_with_label("Show tooltips");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(item), preferences.tooltips_enabled);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", (GtkSignalFunc) pref_toggle_tooltips, 0);


  /* enable enhancement submenu: */

  xsane.enable_enhancement_widget = gtk_check_menu_item_new_with_label("Enable enhancement");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.enable_enhancement_widget), preferences.gamma_selected);
  gtk_menu_append(GTK_MENU(menu), xsane.enable_enhancement_widget);
  gtk_widget_show(xsane.enable_enhancement_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.enable_enhancement_widget), "toggled", (GtkSignalFunc) xsane_gamma_callback, 0);


  /* enable enhancement submenu: */

  xsane.show_histogram_widget = gtk_check_menu_item_new_with_label("Show histogram");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  gtk_menu_append(GTK_MENU(menu), xsane.show_histogram_widget);
  gtk_widget_show(xsane.show_histogram_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_histogram_widget), "toggled", (GtkSignalFunc) xsane_show_histogram_callback, 0);


  /* length unit submenu: */

  item = gtk_menu_item_new_with_label("Length unit");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  submenu = gtk_menu_new();

  subitem = gtk_menu_item_new_with_label("millimeters");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) pref_set_unit_callback, "mm");
  gtk_widget_show(subitem);

  subitem = gtk_menu_item_new_with_label("centimeters");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) pref_set_unit_callback, "cm");
  gtk_widget_show(subitem);

  subitem = gtk_menu_item_new_with_label("inches");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) pref_set_unit_callback, "in");
  gtk_widget_show(subitem);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label("Save device settings");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) pref_device_save, 0);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label("Restore device settings");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) pref_device_restore, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Create the main dialog box.  */

static void device_dialog(void)
{
  GtkWidget *vbox, *hbox, *button, *frame;
  GtkWidget *main_dialog_window, *standard_dialog_window, *advanced_dialog_window;
  GtkWidget *menubar, *menubar_item;
  const gchar *devname;
  char windowname[255];
  GtkWidget *xsane_window;
  GtkWidget *xsane_hbox;
  GtkWidget *xsane_vbox;
  GtkWidget *xsane_notebook;
  GtkWidget *xsane_notebook_label_main;
  GtkWidget *xsane_notebook_label_standard;
  GtkWidget *xsane_notebook_label_advanced;
  GtkWidget *xsane_window_main;
  GtkWidget *xsane_window_standard;
  GtkWidget *xsane_window_advanced;
  GtkWidget *xsane_vbox_main;
  GtkWidget *xsane_vbox_standard;
  GtkWidget *xsane_vbox_advanced;
  GdkColor color_red;
  GdkColor color_green;
  GdkColor color_blue;
  GdkColor color_backg;
  GdkGCValues vals;
  GtkStyle *style;
  GdkColormap *colormap;


  devname = devlist[seldev]->name;
  sprintf(windowname, "XSane %s [%s]", XSANE_VERSION, devname);

  /* create the dialog box */
  xsane.shell = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(xsane.shell), (char *) windowname);
  gtk_window_set_policy(GTK_WINDOW(xsane.shell), FALSE, TRUE, TRUE);
  gtk_signal_connect(GTK_OBJECT(xsane.shell), "delete_event", GTK_SIGNAL_FUNC(scan_win_delete), NULL);

  /* create the main vbox */
  vbox = GTK_DIALOG(xsane.shell)->vbox;

  /* restore preferences */
  pref_xsane_restore();

  /* create the menubar */

  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

  /* "Files" submenu: */
  menubar_item = gtk_menu_item_new_with_label("File");
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), files_build_menu());
  gtk_widget_show(menubar_item);

  /* "Preferences" submenu: */
  menubar_item = gtk_menu_item_new_with_label("Preferences");
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), pref_build_menu());
  gtk_widget_show(menubar_item);

  gtk_widget_show(menubar);

  xsane_window = vbox;

  xsane_hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(xsane_window), xsane_hbox, TRUE, TRUE, 0);
  gtk_widget_show(xsane_hbox);

  xsane_vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, TRUE, TRUE, 0);
  gtk_widget_show(xsane_vbox);

  xsane_notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(xsane_notebook), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_notebook, TRUE, TRUE, 0);
  gtk_widget_show(xsane_notebook);



  /* Notebook scrolled window main */

  xsane_window_main = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane_window_main), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_set_usize(xsane_window_main, 300, 420);

  xsane_notebook_label_main = gtk_label_new("XSane options");
  gtk_notebook_append_page(GTK_NOTEBOOK(xsane_notebook), xsane_window_main, xsane_notebook_label_main);
  gtk_widget_show(xsane_window_main);

  xsane_vbox_main = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
  gtk_container_add(GTK_CONTAINER(xsane_window_main), xsane_vbox_main);
  gtk_widget_show(xsane_vbox_main);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  main_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_main), main_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(main_dialog_window);



  /* Notebook scrolled window standard */

  xsane_window_standard = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane_window_standard), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  xsane_notebook_label_standard = gtk_label_new("Options");
  gtk_notebook_append_page(GTK_NOTEBOOK(xsane_notebook), xsane_window_standard, xsane_notebook_label_standard);
  gtk_widget_show(xsane_window_standard);

  xsane_vbox_standard = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_standard), 5);
  gtk_container_add(GTK_CONTAINER(xsane_window_standard), xsane_vbox_standard);
  gtk_widget_show(xsane_vbox_standard);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  standard_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_standard), standard_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(standard_dialog_window);



  /* Notebook scrolled window advanced */

  xsane_window_advanced = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane_window_advanced), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  xsane_notebook_label_advanced = gtk_label_new("Advanced options");
  gtk_notebook_append_page(GTK_NOTEBOOK(xsane_notebook), xsane_window_advanced, xsane_notebook_label_advanced);
  gtk_widget_show(xsane_window_advanced);

  xsane_vbox_advanced = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_advanced), 5);
  gtk_container_add(GTK_CONTAINER(xsane_window_advanced), xsane_vbox_advanced);
  gtk_widget_show(xsane_vbox_advanced);

  /* create  a subwindow so the advanced dialog keeps its position on rebuilds: */
  advanced_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_advanced), advanced_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(advanced_dialog_window);

  gtk_widget_show(xsane_window);


  /* create window and set colors */

  gtk_widget_show(xsane.shell);
  gtk_widget_hide(xsane.shell);

  style = gtk_widget_get_style(xsane.shell);

  xsane.gc_black = style->black_gc;
  xsane.gc_white = style->white_gc;
  xsane.gc_trans = style->bg_gc[GTK_STATE_NORMAL];
  xsane.bg_trans = &style->bg[GTK_STATE_NORMAL];

  colormap = gdk_window_get_colormap(xsane.shell->window);

  gdk_gc_get_values(xsane.gc_black, &vals);
  xsane.gc_red   = gdk_gc_new_with_values(xsane.shell->window, &vals, 0);
  color_red.red   = 40000;
  color_red.green = 10000;
  color_red.blue  = 10000;
  gdk_color_alloc(colormap, &color_red);
  gdk_gc_set_foreground(xsane.gc_red, &color_red);

  xsane.gc_green = gdk_gc_new_with_values(xsane.shell->window, &vals, 0);
  color_green.red   = 10000;
  color_green.green = 40000;
  color_green.blue  = 10000;
  gdk_color_alloc(colormap, &color_green);
  gdk_gc_set_foreground(xsane.gc_green, &color_green);

  xsane.gc_blue  = gdk_gc_new_with_values(xsane.shell->window, &vals, 0);
  color_blue.red   = 10000;
  color_blue.green = 10000;
  color_blue.blue  = 40000;
  gdk_color_alloc(colormap, &color_blue);
  gdk_gc_set_foreground(xsane.gc_blue, &color_blue);

  xsane.gc_backg  = gdk_gc_new_with_values(xsane.shell->window, &vals, 0);
  color_backg.red   = 50000;
  color_backg.green = 50000;
  color_backg.blue  = 50000;
  gdk_color_alloc(colormap, &color_backg);
  gdk_gc_set_foreground(xsane.gc_backg, &color_backg);


  dialog = gsg_create_dialog(main_dialog_window,
                             standard_dialog_window,
                             advanced_dialog_window,
			     devname,
			     update_preview, 0,
			     update_param, 0,
		   	     xsane_xsane_new_callback, 0,
			     xsane.mode);

  if (!dialog)
  {
    printf("Could not create dialog\n");
    return;
  }

  /* The info row */
  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(hbox), 3);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show(frame);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(hbox), 2);
  gtk_container_add(GTK_CONTAINER(frame), hbox);
  gtk_widget_show(hbox);

  xsane.info_label = gtk_label_new("0x0: 0KB");
  gtk_box_pack_start(GTK_BOX(hbox), xsane.info_label, FALSE, FALSE, 0);
  gtk_widget_show(xsane.info_label);

  /* The bottom row of buttons */
  hbox = GTK_DIALOG(xsane.shell)->action_area;

  /* The Scan button */
  button = gtk_button_new_with_label("Start");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) scan_dialog, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  /* The Preview button */
  button = gtk_toggle_button_new_with_label("Preview Window");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) scan_preview, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

#if 0
  /* The Zoom in button */
  button = gtk_button_new_with_label("Zoom");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) zoom_in_preview, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  /* The Zoom out button */
  button = gtk_button_new_with_label("Zoom out");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) zoom_out_preview, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
#endif

  pref_device_restore();	/* restore device-settings */

  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.enable_enhancement_widget), preferences.gamma_selected);
  xsane_gamma_callback(xsane.enable_enhancement_widget);
  update_param(dialog, 0);
  gtk_widget_show(xsane.shell);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void ok_choose_dialog_callback(void)
{
  gtk_widget_destroy(choose_device_dialog);
  device_dialog();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void select_device_callback(GtkWidget * widget, GdkEventButton *event, gpointer data)
{
  seldev = (long) data;
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    ok_choose_dialog_callback();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint32 choose_device(void)
{
  GtkWidget *main_vbox, *vbox, *hbox, *button;
  GSList *owner;
  const SANE_Device *adev;
  gint i;

  choose_device_dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(choose_device_dialog), "Select device");

  main_vbox = GTK_DIALOG(choose_device_dialog)->vbox;

  /* The list of drivers */
  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(vbox), 3);
  gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  /* The radio buttons */
  owner = NULL;
  for (i = 0; i < ndevs; i++)
    {
      adev = devlist[i];

      button = gtk_radio_button_new_with_label(owner, (char *) adev->name);
      gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
			 (GtkSignalFunc) select_device_callback,
			 (void *) (long) i);
      gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      owner = gtk_radio_button_group(GTK_RADIO_BUTTON(button));;
    }

  /* The bottom row of buttons */
  hbox = GTK_DIALOG(choose_device_dialog)->action_area;

  /* The OK button */
  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) ok_choose_dialog_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  /* The Cancel button */
  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) files_exit_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(choose_device_dialog);
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void usage(void)
{
    printf("Usage: %s [OPTION]... [DEVICE]\n\
\n\
Start up graphical user interface to access SANE (Scanner Access Now\n\
Easy) devices.\n\
\n\
-h, --help                 display this help message and exit\n\
-V, --version              print version information\n", prog_name);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void
init(int argc, char **argv)
{
  char filename[PATH_MAX];
  struct stat st;

  gtk_init(&argc, &argv);
#ifdef HAVE_LIBGIMP_GIMP_H
  gtk_rc_parse(gimp_gtkrc());

  gdk_set_use_xshm(gimp_use_xshm());
#endif

  gsg_make_path(sizeof(filename), filename, 0, "sane-style", 0, ".rc");
  if (stat(filename, &st) >= 0)
    gtk_rc_parse(filename);
  else
    {
      strncpy(filename, STRINGIFY(PATH_SANE_DATA_DIR) "/sane-style.rc", sizeof(filename));
      filename[sizeof(filename) - 1] = '\0';
      if (stat(filename, &st) >= 0)
	gtk_rc_parse(filename);
    }

  sane_init(0, 0);

  if (argc > 1)
    {
      static SANE_Device dev;
      static const SANE_Device *device_list[] = { &dev, 0 };
      int ch;

      while((ch = getopt_long(argc, argv, "ghV", long_options, 0)) != EOF)
	{
	  switch(ch)
	    {
	    case 'g':
	      printf("%s: GIMP support missing.\n", argv[0]);
	      exit(0);

	    case 'V':
	      printf("xsane %s (%s %s)\n", XSANE_VERSION, PACKAGE, VERSION);
	      exit(0);

	    case 'h':
	    default:
	      usage();
	      exit(0);
	    }
	}

      if (optind < argc)
	{
	  memset(&dev, 0, sizeof(dev));
	  dev.name   = argv[argc - 1];
	  dev.vendor = "Unknown";
	  dev.type   = "unknown";
	  dev.model  = "unknown";

	  devlist = device_list;
	  seldev = 0;
	}
    }

  if (seldev < 0)
    sane_get_devices(&devlist, SANE_FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void interface(int argc, char **argv)
{
  xsane.info_label = NULL;

  init(argc, argv);

  for (ndevs = 0; devlist[ndevs]; ++ndevs);

  if (seldev >= 0)
    {
      if (seldev >= ndevs)
	{
	  fprintf(stderr, "%s: device %d is unavailable.\n",
		   prog_name, seldev);
	  quit_xsane();
	}
      device_dialog();
      if (!dialog)
	quit_xsane();
    }
  else
    {
      if (ndevs > 0)
	{
	  seldev = 0;
	  if (ndevs == 1)
	    {
	      device_dialog();
	      if (!dialog)
		quit_xsane();
	    }
	  else
	    choose_device();
	}
      else
	{
	  fprintf(stderr, "%s: no devices available.\n", prog_name);
	  quit_xsane();
	}
    }
  gtk_main();
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  memset(&xsane, 0, sizeof(xsane));
  xsane.mode = STANDALONE;
  xsane.xsane_mode = XSANE_SCAN;

  xsane.histogram_lines = 1;

  xsane.zoom             = 1.0;
  xsane.resolution       = 100.0;

  xsane.gamma            = 1.0;
  xsane.gamma_red        = 1.0;
  xsane.gamma_green      = 1.0;
  xsane.gamma_blue       = 1.0;
  xsane.brightness       = 0.0;
  xsane.brightness_red   = 0.0;
  xsane.brightness_green = 0.0;
  xsane.brightness_blue  = 0.0;
  xsane.contrast         = 0.0;
  xsane.contrast_red     = 0.0;
  xsane.contrast_green   = 0.0;
  xsane.contrast_blue    = 0.0;
  xsane.pipette_white    = 100.0;
  xsane.pipette_black    = 0.0;
  xsane.auto_brightness  = 0.0;
  xsane.auto_contrast    = 0.0;

  xsane.histogram_red    = 1;
  xsane.histogram_green  = 1;
  xsane.histogram_blue   = 1;
  xsane.histogram_int    = 1;
  xsane.histogram_log    = 1;

  xsane.xsane_color                = TRUE;
  xsane.enhancement_rgb_default    = TRUE;
  xsane.scanner_gamma_selected     = FALSE;
  xsane.software_gamma_selected    = FALSE;

  prog_name = strrchr(argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

#ifdef HAVE_LIBGIMP_GIMP_H
  {
    GPrintFunc old_print_func;
    int result;

    /* Temporarily install a print function that discards all output.
       This is to avoid annoying "you must run this program under
       gimp" messages when xsane gets invoked in stand-alone
       mode.  */
    old_print_func = g_set_print_handler(null_print_func);
    /* gimp_main() returns 1 if xsane wasn't invoked by GIMP */
    result = gimp_main(argc, argv);
    g_set_message_handler(old_print_func);
    if (result)
      interface(argc, argv);
  }
#else
  interface(argc, argv);
#endif
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
