/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998,1999 Oliver Rauch
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

#define XSANE_VERSION "0.20\337"

/* ---------------------------------------------------------------------------------------------------------------------- */

/* #define XSANE_TEST */

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef _AIX
# include <lalloca.h>   /* MUST come first for AIX! */
#endif

#include "sane/config.h"

#include <lalloca.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/stat.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
#include "sane/sanei_signal.h"

#include "xsane-gtk.h"
#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-text.h"
#include "xsane-icons.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif


/* ---------------------------------------------------------------------------------------------------------------------- */

int gsg_message_dialog_active = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBGIMP_GIMP_H

#include <libgimp/gimp.h>

static void xsane_gimp_query(void);
static void xsane_gimp_run(char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals);

GPlugInInfo PLUG_IN_INFO =
{
  NULL,                         /* init_proc */
  NULL,                         /* quit_proc */
  xsane_gimp_query,             /* query_proc */
  xsane_gimp_run,               /* run_proc */
};

#endif /* HAVE_LIBGIMP_GIMP_H */

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

static int xsane_scanmode_number[] = { XSANE_SCAN, XSANE_COPY, XSANE_FAX };

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

int main(int argc, char ** argv);
static void xsane_interface(int argc, char **argv);
static void xsane_start_scan(void);
static void xsane_scan_done(void);
static void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data);
static void xsane_about_dialog(GtkWidget *widget, gpointer data);
static void xsane_info_dialog(GtkWidget *widget, gpointer data);
static void xsane_scale_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
                                        float min, float max, float quant, float step, float xxx,
                                        int digits, double *val, GtkObject **data, int option, void *xsane_scale_callback);
static void xsane_separator_new(GtkWidget *xsane_parent, int dist);
static void xsane_enhancement_by_histogram();
static void xsane_close_dialog_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_scan_dialog();
static void xsane_batch_scan_delete_callback(GtkWidget *widget, gpointer list);

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_bound_double(double *value, double min, double max)
{
  if (*value < min)
  {
    *value = min;
  }

  if (*value > max)
  {
    *value = max;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int authorization_flag;

static void xsane_authorization_button_callback(GtkWidget *widget, gpointer data)
{
  authorization_flag = (long) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_authorization_callback(SANE_String_Const resource,
                                         SANE_Char username[SANE_MAX_USERNAME_LEN],
                                         SANE_Char password[SANE_MAX_PASSWORD_LEN])
{
 GtkWidget *authorize_dialog, *vbox, *hbox, *button, *label;
 GtkWidget *username_widget, *password_widget;
 char buf[256];
 char *input;
 int len;

  authorize_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_position(GTK_WINDOW(authorize_dialog), GTK_WIN_POS_CENTER);
  gtk_widget_set_usize(authorize_dialog, 300, 190);
  gtk_window_set_policy(GTK_WINDOW(authorize_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(authorize_dialog), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1); /* -1 = cancel */
  sprintf(buf, "%s authorization", prog_name);
  gtk_window_set_title(GTK_WINDOW(authorize_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 10);
  gtk_container_add(GTK_CONTAINER(authorize_dialog), vbox);
  gtk_widget_show(vbox);

  sprintf(buf,"\n\nAuthorization required for %s\n", resource);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  /* ask for username */
  hbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new("Username :");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 20);
  gtk_widget_show(label);

  username_widget = gtk_entry_new_with_max_length(SANE_MAX_USERNAME_LEN-1);
  gtk_widget_set_usize(username_widget, 200, 0);
  gtk_entry_set_text(GTK_ENTRY(username_widget), "");
  gtk_box_pack_end(GTK_BOX(hbox), username_widget, FALSE, FALSE, 20);
  gtk_widget_show(username_widget);
  gtk_widget_show(hbox);


  /* ask for password */
  hbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new("Password :");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 20);
  gtk_widget_show(label);

  password_widget = gtk_entry_new_with_max_length(SANE_MAX_PASSWORD_LEN-1);
  gtk_entry_set_visibility(GTK_ENTRY(password_widget), FALSE); /* make entered text invisible */
  gtk_widget_set_usize(password_widget, 200, 0);
  gtk_entry_set_text(GTK_ENTRY(password_widget), "");
  gtk_box_pack_end(GTK_BOX(hbox), password_widget, FALSE, FALSE, 20);
  gtk_widget_show(password_widget);
  gtk_widget_show(hbox);

  /* buttons */
  hbox = gtk_hbox_new(TRUE, 10);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 30);

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) 1);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10);
  gtk_widget_show(button);

  gtk_widget_show(hbox);

  gtk_widget_show(authorize_dialog);


  username[0]=0;
  password[0]=0;

  authorization_flag = 0;

  /* wait for ok or cancel */
  while (authorization_flag == 0)
  {
    gtk_main_iteration();
  }

  if (authorization_flag == 1) /* 1=ok, -1=cancel */
  {
    input = gtk_entry_get_text(GTK_ENTRY(username_widget));
    len = strlen(input);
    memcpy(username, input, len);
    username[len] = 0;

    input = gtk_entry_get_text(GTK_ENTRY(password_widget));
    len = strlen(input);
    memcpy(password, input, len);
    password[len] = 0;
  }
  gtk_widget_destroy(authorize_dialog);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_progress_cancel(GtkWidget *widget, gpointer data)
{
  XsaneProgress_t *p = (XsaneProgress_t *) data;

  (*p->callback) ();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static XsaneProgress_t *xsane_progress_new(char *title, char *text, GtkSignalFunc callback, gpointer callback_data)
{
  GtkWidget *button, *label;
  GtkBox *vbox, *hbox;
  XsaneProgress_t *p;
  static const int progress_x = 5;
  static const int progress_y = 5;

  p = (XsaneProgress_t *) malloc(sizeof(XsaneProgress_t));
  p->callback = callback;

  p->shell = gtk_dialog_new();
  gtk_widget_set_uposition(p->shell, progress_x, progress_y);
  gtk_window_set_title(GTK_WINDOW (p->shell), title);
  vbox = GTK_BOX(GTK_DIALOG(p->shell)->vbox);
  hbox = GTK_BOX(GTK_DIALOG(p->shell)->action_area);

  gtk_container_border_width(GTK_CONTAINER (vbox), 7);

  label = gtk_label_new(text);
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start(vbox, label, FALSE, TRUE, 0);

  p->pbar = gtk_progress_bar_new();
  gtk_widget_set_usize(p->pbar, 200, 20);
  gtk_box_pack_start(vbox, p->pbar, TRUE, TRUE, 0);

  button = gtk_toggle_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT (button), "clicked", (GtkSignalFunc) xsane_progress_cancel, p);
  gtk_box_pack_start(hbox, button, TRUE, TRUE, 0);

  gtk_widget_show(label);
  gtk_widget_show(p->pbar);
  gtk_widget_show(button);
  gtk_widget_show(GTK_WIDGET (p->shell));
  return p;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_progress_free(XsaneProgress_t *p)
{
  if (p)
  {
    gtk_widget_destroy(p->shell);
    free (p);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_update(XsaneProgress_t *p, gfloat newval)
{
  if (p)
  {
    gtk_progress_bar_update(GTK_PROGRESS_BAR(p->pbar), newval);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_toggle_button_new_with_pixmap(GtkWidget *parent, const char *xpm_d[], const char *desc,
                                                int *state, void *xsane_toggle_button_callback)
{
  GtkWidget *button;
  GtkWidget *pixmapwidget;
  GdkBitmap *mask;
  GdkPixmap *pixmap;

  button = gtk_toggle_button_new();
  gsg_set_tooltip(dialog->tooltips, button, desc);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_container_add(GTK_CONTAINER(button), pixmapwidget);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);

  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), *state);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_toggle_button_callback, (GtkObject *)state);
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
  gtk_widget_show(button);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pixmap_new(GtkWidget *parent, char *title, int width, int height, XsanePixmap *hist)
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
 }

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_histogram_with_points(XsanePixmap *hist,
                                             SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                             int show_red, int show_green, int show_blue, int show_inten, double scale)
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
    rect.width  = HIST_WIDTH;
    rect.height = HIST_HEIGHT;

    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < HIST_WIDTH; i++)
    {
      inten = show_inten * count[i]       * scale;

      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[i]   * scale;
        green = show_green * count_green[i] * scale;
        blue  = show_blue  * count_blue[i]  * scale;
      }

      if (inten > HIST_HEIGHT)
      inten = HIST_HEIGHT;

      if (red > HIST_HEIGHT)
      red = HIST_HEIGHT;

      if (green > HIST_HEIGHT)
      green = HIST_HEIGHT;

      if (blue > HIST_HEIGHT)
      blue = HIST_HEIGHT;


      gdk_draw_rectangle(hist->pixmap, xsane.gc_red,   TRUE, i, HIST_HEIGHT - red,   XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_green, TRUE, i, HIST_HEIGHT - green, XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_blue,  TRUE, i, HIST_HEIGHT - blue,  XD, YD);
      gdk_draw_rectangle(hist->pixmap, xsane.gc_black, TRUE, i, HIST_HEIGHT - inten, XD, YD);
    }

    gtk_widget_draw(hist->pixmapwid, &rect);
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_histogram_with_lines(XsanePixmap *hist,
                                            SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                            int show_red, int show_green, int show_blue, int show_inten, double scale)
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
    rect.width  = HIST_WIDTH;
    rect.height = HIST_HEIGHT;

    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < HIST_WIDTH; i++)
    {
      inten = show_inten * count[i]       * scale;
      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[i]   * scale;
        green = show_green * count_green[i] * scale;
        blue  = show_blue  * count_blue[i]  * scale;
      }

      if (inten > HIST_HEIGHT)
      inten = HIST_HEIGHT;

      if (red > HIST_HEIGHT)
      red = HIST_HEIGHT;

      if (green > HIST_HEIGHT)
      green = HIST_HEIGHT;

      if (blue > HIST_HEIGHT)
      blue = HIST_HEIGHT;

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


      gdk_draw_line(hist->pixmap, xsane.gc_red,   i, HIST_HEIGHT - red,   i, HIST_HEIGHT - red0);
      gdk_draw_line(hist->pixmap, xsane.gc_green, i, HIST_HEIGHT - green, i, HIST_HEIGHT - green0);
      gdk_draw_line(hist->pixmap, xsane.gc_blue,  i, HIST_HEIGHT - blue,  i, HIST_HEIGHT - blue0);
      gdk_draw_line(hist->pixmap, xsane.gc_black, i, HIST_HEIGHT - inten, i, HIST_HEIGHT - inten0);
    }

    gtk_widget_draw(hist->pixmapwid, &rect);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_slider_level(XsaneSlider *slider)
{
 int i;
 guchar buf[XSANE_SLIDER_WIDTH*3];
 GdkRectangle rect;

  buf[0] = buf[1] = buf[2] = 0;
  buf[3+0] = buf[3+1] = buf[3+2]= 0;

  for (i=0; i<256; i++)
  {
    buf[3*i+0+6] = i * slider->r;
    buf[3*i+1+6] = i * slider->g;
    buf[3*i+2+6] = i * slider->b;
  }

  buf[258*3+0] = 255 * slider->r;
  buf[258*3+1] = 255 * slider->g;
  buf[258*3+2] = 255 * slider->b;

  buf[259*3+0] = 255 * slider->r;
  buf[259*3+1] = 255 * slider->g;
  buf[259*3+2] = 255 * slider->b;

  for (i=0; i<XSANE_SLIDER_HEIGHT; i++)
  {
    gtk_preview_draw_row(GTK_PREVIEW(slider->preview),buf, 0, i, XSANE_SLIDER_WIDTH);
  }

  rect.x=0;
  rect.y=0;
  rect.width  = XSANE_SLIDER_WIDTH;
  rect.height = XSANE_SLIDER_HEIGHT;

  gtk_widget_draw(slider->preview, &rect);
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_slider(GdkWindow *window, GdkGC *border_gc, GdkGC *fill_gc, int value) /* draw ^ */
{
 int y;

  for (y=0; y < XSANE_SLIDER_HEIGHT; y++)
  {
    gdk_draw_line(window, fill_gc, value + XSANE_SLIDER_OFFSET - y/2, y, value + XSANE_SLIDER_OFFSET + y/2, y);
  }

  gdk_draw_line(window, border_gc, value + XSANE_SLIDER_OFFSET, 0,
                                   value + XSANE_SLIDER_OFFSET - (XSANE_SLIDER_HEIGHT - 1)/2, XSANE_SLIDER_HEIGHT-1);
  gdk_draw_line(window, border_gc, value + XSANE_SLIDER_OFFSET, 0,
                                   value + XSANE_SLIDER_OFFSET + (XSANE_SLIDER_HEIGHT - 1)/2, XSANE_SLIDER_HEIGHT-1);
  gdk_draw_line(window, border_gc, value + XSANE_SLIDER_OFFSET - (XSANE_SLIDER_HEIGHT - 1)/2, XSANE_SLIDER_HEIGHT-1, 
                                   value + XSANE_SLIDER_OFFSET + (XSANE_SLIDER_HEIGHT - 1)/2, XSANE_SLIDER_HEIGHT-1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_clear_slider(XsaneSlider *slider)
{
/*
 GdkRectangle rect;

  rect.y=0;
  rect.width  = XSANE_SLIDER_HEIGHT+3;
  rect.height = XSANE_SLIDER_HEIGHT;

  rect.x= slider->position[0] - XSANE_SLIDER_HEIGHT/2 - 1;
  gtk_widget_draw(slider->preview, &rect);

  rect.x= slider->position[1] - XSANE_SLIDER_HEIGHT/2 - 1;
  gtk_widget_draw(slider->preview, &rect);

  rect.x= slider->position[2] - XSANE_SLIDER_HEIGHT/2 - 1;
  gtk_widget_draw(slider->preview, &rect);
*/
  xsane_draw_slider_level(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_redraw_slider(XsaneSlider *slider)
{
  xsane_draw_slider(slider->preview->window,
                    slider->preview->style->white_gc,
                    slider->preview->style->black_gc,
                    slider->position[0]);

  xsane_draw_slider(slider->preview->window,
                    slider->preview->style->white_gc,
                    slider->preview->style->dark_gc[GTK_STATE_NORMAL],
                    slider->position[1]);

  xsane_draw_slider(slider->preview->window,
                    slider->preview->style->black_gc,
                    slider->preview->style->white_gc,
                    slider->position[2]);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_slider(XsaneSlider *slider, double min, double mid, double max)
{
  slider->value[0] = min;
  slider->value[1] = mid;
  slider->value[2] = max;

  xsane_clear_slider(slider); 

  slider->position[0] = min * 2.55;
  slider->position[1] = mid * 2.55;
  slider->position[2] = max * 2.55;

  xsane_redraw_slider(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_slider(XsaneSlider *slider)
{
  xsane_clear_slider(slider); 

  slider->position[0] = 2.55 * slider->value[0];
  slider->position[1] = 2.55 * slider->value[1];
  slider->position[2] = 2.55 * slider->value[2];

  xsane_redraw_slider(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_sliders()
{
  xsane_update_slider(&xsane.slider_gray);

  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_update_slider(&xsane.slider_red);
    xsane_update_slider(&xsane.slider_green);
    xsane_update_slider(&xsane.slider_blue);

    xsane.slider_gray.active  = XSANE_SLIDER_ACTIVE; /* mark slider active */
    xsane.slider_red.active   = XSANE_SLIDER_ACTIVE; /* mark slider active */
    xsane.slider_green.active = XSANE_SLIDER_ACTIVE; /* mark slider active */
    xsane.slider_blue.active  = XSANE_SLIDER_ACTIVE; /* mark slider active */
  }
  else
  {
    xsane_clear_slider(&xsane.slider_red);
    xsane_clear_slider(&xsane.slider_green);
    xsane_clear_slider(&xsane.slider_blue);

    xsane.slider_red.active   = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_green.active = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_blue.active  = XSANE_SLIDER_INACTIVE; /* mark slider inactive */

    if (xsane.param.depth == 1)
    {
      xsane_clear_slider(&xsane.slider_gray);
      xsane.slider_gray.active = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    }
    else
    {
      xsane.slider_gray.active = XSANE_SLIDER_ACTIVE; /* mark slider active */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_slider_callback(GtkWidget *widget, GdkEvent *event, XsaneSlider *slider)
{
 GdkEventButton *button_event;
 GdkEventMotion *motion_event;
 int x, distance;
 int i = 0;
 int update = FALSE;

  if (slider->active == XSANE_SLIDER_INACTIVE)
  {
    return 0;
  }

  switch(event->type)
  {
    case GDK_EXPOSE:
      xsane_redraw_slider(slider);
     break;

    case GDK_BUTTON_PRESS:
      gtk_grab_add(widget);
      button_event = (GdkEventButton *) event;

      distance = G_MAXINT;
      for (i=0; i<3; i++)
      {
        if (fabs(button_event->x - slider->position[i]) < distance)
	{
	  slider->active = i;
	  distance = fabs(button_event->x - slider->position[i]);
	}
      }
      if (distance<10)
      {
        x = button_event->x;
        update = TRUE;
      }
      else
      {
        slider->active = XSANE_SLIDER_ACTIVE;
      }
     break;
    
    case GDK_BUTTON_RELEASE:
      gtk_grab_remove(widget);
      xsane_enhancement_by_histogram(); /* slider->active must be unchanged !!! */
      slider->active = XSANE_SLIDER_ACTIVE; /* ok, now we can reset it */
     break;

    case GDK_MOTION_NOTIFY:
      motion_event = (GdkEventMotion *) event;
      gdk_window_get_pointer(widget->window, &x, 0, 0);
      update = TRUE;
     break;

    default:
     break;
  }

  if (update)
  {
    switch(slider->active)
    {
      case 0:
	slider->value[0] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[0], 0.0, slider->value[1] - 1);
       break;

      case 1:
	slider->value[1] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[1], slider->value[0] + 1, slider->value[2] - 1);
       break;

      case 2:
	slider->value[2] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[2], slider->value[1] + 1, 100.0);
       break;

      default:
       break;
    }
    xsane_set_slider(slider, slider->value[0], slider->value[1], slider->value[2]);
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_create_slider(XsaneSlider *slider)
{
  slider->preview = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(slider->preview), XSANE_SLIDER_WIDTH, XSANE_SLIDER_HEIGHT);
  gtk_widget_set_events(slider->preview, XSANE_SLIDER_EVENTS);
  gtk_signal_connect(GTK_OBJECT(slider->preview), "event", GTK_SIGNAL_FUNC(xsane_slider_callback), slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
static void xsane_destroy_histogram()
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
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_calculate_auto_enhancement(SANE_Int *count_raw,
              SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue)
{	 /* calculate white, medium and black values for auto enhancement */
 int limit;
 int points;
 int min;
 int mid;
 int max;
 int val;
 int i;

    points = 0;
    for (i=0; i<256; i++)
    {
/*      points += count_raw[i] + count_raw_red[i] + count_raw_green[i] + count_raw_blue[i]; */
      points += log(1 + count_raw[i] + count_raw_red[i] + count_raw_green[i] + count_raw_blue[i]);
    }

    limit = points / 500;

    if (limit < 10)
    {
      limit = 10;
    }

    min = -1;
    val = 0;
    while ( (val < limit) && (min < 254) )
    {
      min++;
/*      val += count_raw[min] + count_raw_red[min] + count_raw_green[min] + count_raw_blue[min]; */
      val += log(1 + count_raw[min] + count_raw_red[min] + count_raw_green[min] + count_raw_blue[min]);
    }

    max = HIST_WIDTH;
    val = 0;
    while ( (val < limit) && (max > min + 1) )
    {
      max--;
/*      val += count_raw[max] + count_raw_red[max] + count_raw_green[max] + count_raw_blue[max]; */
      val += log(1 + count_raw[max] + count_raw_red[max] + count_raw_green[max] + count_raw_blue[max]);
    }

    limit = points / 2.0;

    mid = -1;
    val = 0;
    while ( (val < limit) && (mid < max - 1) )
    {
      mid++;
/*      val += count_raw[mid] + count_raw_red[mid] + count_raw_green[mid] + count_raw_blue[mid]; */
      val += log(1 + count_raw[mid] + count_raw_red[mid] + count_raw_green[mid] + count_raw_blue[mid]);
    }

    xsane.auto_white = max/2.55;
    xsane.auto_gray  = mid/2.55;
    xsane.auto_black = min/2.55;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_calculate_histogram()
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

  if (xsane.preview)
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
                                count_enh, count_enh_red, count_enh_green, count_enh_blue, x_min, y_min, x_max, y_max);

    if (xsane.param.depth > 1)
    {
      xsane_calculate_auto_enhancement(count_raw, count_raw_red, count_raw_green, count_raw_blue);
    }

    if (xsane.histogram_log)
    {
      for (i=0; i<=255; i++)
      {
        count_raw[i]       = (int) (50*log(1.0 + count_raw[i]));
        count_raw_red[i]   = (int) (50*log(1.0 + count_raw_red[i]));
        count_raw_green[i] = (int) (50*log(1.0 + count_raw_green[i]));
        count_raw_blue[i]  = (int) (50*log(1.0 + count_raw_blue[i]));

        count_enh[i]       = (int) (50*log(1.0 + count_enh[i]));
        count_enh_red[i]   = (int) (50*log(1.0 + count_enh_red[i]));
        count_enh_green[i] = (int) (50*log(1.0 + count_enh_green[i]));
        count_enh_blue[i]  = (int) (50*log(1.0 + count_enh_blue[i]));
      }
    }

    maxval_raw = 0;
    maxval_enh = 0;

    /* first and last 10 values are not used for calculating maximum value */
    for (i = 10 ; i < HIST_WIDTH - 10; i++)
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

    if (xsane.histogram_lines)
    {
      xsane_draw_histogram_with_lines(&xsane.histogram_raw, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      xsane_draw_histogram_with_lines(&xsane.histogram_enh, count_enh, count_enh_red, count_enh_green, count_enh_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
    }
    else
    {
      xsane_draw_histogram_with_points(&xsane.histogram_raw, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
      xsane_draw_histogram_with_points(&xsane.histogram_enh, count_enh, count_enh_red, count_enh_green, count_enh_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
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
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_histogram()
{
  if (preferences.show_histogram)
  {
    xsane_calculate_histogram();
    gtk_widget_show(xsane.histogram_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;

  *valuep = (GTK_TOGGLE_BUTTON(widget)->active != 0);
  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_gamma_curve(SANE_Int *gammadata, double gamma,
                              double brightness, double contrast, int numbers, int maxout)
{
 int i;
 double midin;
 double val;
 double m;
 double b;
 int maxin = numbers-1;

  midin = (int)(numbers / 2.0);

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midin;

  for (i=0; i <= maxin; i++)
  {
    val = ((double) i) - midin;
    val = val * m + b;
    xsane_bound_double(&val, 0.0, maxin);

    gammadata[i] = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_gamma()
{
  if (xsane.preview)
  {
    if (!xsane.preview_gamma_data_red)
    {
      xsane.preview_gamma_data_red   = malloc(256 * sizeof(SANE_Int));
      xsane.preview_gamma_data_green = malloc(256 * sizeof(SANE_Int));
      xsane.preview_gamma_data_blue  = malloc(256 * sizeof(SANE_Int));

      xsane.histogram_gamma_data_red   = malloc(256 * sizeof(SANE_Int));
      xsane.histogram_gamma_data_green = malloc(256 * sizeof(SANE_Int));
      xsane.histogram_gamma_data_blue  = malloc(256 * sizeof(SANE_Int));
    }

    xsane_create_gamma_curve(xsane.preview_gamma_data_red,
		 	     xsane.gamma * xsane.gamma_red * preferences.preview_gamma * preferences.preview_gamma_red,
			     xsane.brightness + xsane.brightness_red,
			     xsane.contrast + xsane.contrast_red, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_green,
			     xsane.gamma * xsane.gamma_green * preferences.preview_gamma * preferences.preview_gamma_green,
			     xsane.brightness + xsane.brightness_green,
			     xsane.contrast + xsane.contrast_green, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_blue,
			     xsane.gamma * xsane.gamma_blue * preferences.preview_gamma * preferences.preview_gamma_blue,
			     xsane.brightness + xsane.brightness_blue,
			     xsane.contrast + xsane.contrast_blue , 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_red,
		 	     xsane.gamma * xsane.gamma_red,
			     xsane.brightness + xsane.brightness_red,
			     xsane.contrast + xsane.contrast_red, 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_green,
			     xsane.gamma * xsane.gamma_green,
			     xsane.brightness + xsane.brightness_green,
			     xsane.contrast + xsane.contrast_green, 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_blue,
			     xsane.gamma * xsane.gamma_blue,
			     xsane.brightness + xsane.brightness_blue,
			     xsane.contrast + xsane.contrast_blue , 256, 255);


    preview_gamma_correction(xsane.preview,
                             xsane.preview_gamma_data_red, xsane.preview_gamma_data_green, xsane.preview_gamma_data_blue,
                             xsane.histogram_gamma_data_red, xsane.histogram_gamma_data_green, xsane.histogram_gamma_data_blue);

  }
  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_refresh_dialog(void *nothing)
{   
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_update()
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

    gtk_signal_emit_by_name(xsane.gamma_red_widget,        "changed"); 
    gtk_signal_emit_by_name(xsane.gamma_green_widget,      "changed"); 
    gtk_signal_emit_by_name(xsane.gamma_blue_widget,       "changed"); 

    gtk_signal_emit_by_name(xsane.brightness_red_widget,   "changed"); 
    gtk_signal_emit_by_name(xsane.brightness_green_widget, "changed"); 
    gtk_signal_emit_by_name(xsane.brightness_blue_widget,  "changed"); 

    gtk_signal_emit_by_name(xsane.contrast_red_widget,     "changed"); 
    gtk_signal_emit_by_name(xsane.contrast_green_widget,   "changed"); 
    gtk_signal_emit_by_name(xsane.contrast_blue_widget,    "changed"); 
  }

  /* update histogram slider */

  xsane_update_sliders();

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  gtk_signal_emit_by_name(xsane.gamma_widget,         "changed"); 
  gtk_signal_emit_by_name(xsane.brightness_widget,    "changed"); 
  gtk_signal_emit_by_name(xsane.contrast_widget,      "changed"); 
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gamma_to_histogram(double *min, double *mid, double *max,
                                     double contrast, double brightness, double gamma)
{
 double m;
 double b;

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * 50.0;

  if (m > 0)
  {
    *min = 50.0 - b/m;
    *mid = (100.0 * pow(0.5, gamma)-b) / m + 50.0;
    *max = (100.0-b)/m + 50.0;
  }
  else
  {
    *min = 0.0;
    *mid = 50.0;
    *max = 100.0;
  }

  xsane_bound_double(min, 0.0, 99.0);
  xsane_bound_double(max, 1.0, 100.0);
  xsane_bound_double(mid, *min+1, *max-1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_by_gamma()
{
 double min, mid, max;

  xsane_gamma_to_histogram(&min, &mid, &max, xsane.contrast, xsane.brightness, xsane.gamma);

  xsane.slider_gray.value[0] = min;
  xsane.slider_gray.value[1] = mid;
  xsane.slider_gray.value[2] = max;


  xsane_gamma_to_histogram(&min, &mid, &max,
                           xsane.contrast + xsane.contrast_red,
                           xsane.brightness + xsane.brightness_red,
                           xsane.gamma * xsane.gamma_red);

  xsane.slider_red.value[0] = min;
  xsane.slider_red.value[1] = mid;
  xsane.slider_red.value[2] = max;


  xsane_gamma_to_histogram(&min, &mid, &max,
                           xsane.contrast + xsane.contrast_green,
                           xsane.brightness + xsane.brightness_green,
                           xsane.gamma * xsane.gamma_green);

  xsane.slider_green.value[0] = min;
  xsane.slider_green.value[1] = mid;
  xsane.slider_green.value[2] = max;


  xsane_gamma_to_histogram(&min, &mid, &max,
                           xsane.contrast + xsane.contrast_blue,
                           xsane.brightness + xsane.brightness_blue,
                           xsane.gamma * xsane.gamma_blue);

  xsane.slider_blue.value[0] = min;
  xsane.slider_blue.value[1] = mid;
  xsane.slider_blue.value[2] = max;


  xsane_enhancement_update();
  xsane_update_gamma();
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

  xsane_enhancement_by_gamma();
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

  xsane_enhancement_by_gamma();
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
static void xsane_update_param(GSGDialog *dialog, void *arg)
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

  xsane.resolution = xsane.zoom * preferences.printer[preferences.printernr]->resolution;
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
  xsane_update_param(dialog, 0);
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
  xsane_update_param(dialog, 0);
  xsane.zoom = xsane.resolution / preferences.printer[preferences.printernr]->resolution;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_histogram_to_gamma(XsaneSlider *slider, double *contrast, double *brightness, double *gamma)
{
 double mid;
 double range;

  *contrast   = (10000.0 / (slider->value[2] - slider->value[0]) - 100.0);
  *brightness = - (slider->value[0] - 50.0) * (*contrast + 100.0)/50.0 - 100.0;

  mid   = slider->value[1] - slider->value[0];
  range = slider->value[2] - slider->value[0];

  *gamma = log(mid/range) / log(0.5);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_by_histogram()
{
 double gray_brightness;
 double gray_contrast;
 double gray_gamma;
 double brightness;
 double contrast;
 double gamma;

  xsane_histogram_to_gamma(&xsane.slider_gray, &gray_contrast, &gray_brightness, &gray_gamma);

  xsane.gamma      = gray_gamma;
  xsane.brightness = gray_brightness;
  xsane.contrast   = gray_contrast;

  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) ) /* rgb sliders active */
  {
    if (xsane.slider_gray.active < 0) /* gray slider not moved */
    {
      xsane_histogram_to_gamma(&xsane.slider_red, &contrast, &brightness, &gamma);

      xsane.gamma_red        = gamma / gray_gamma;
      xsane.brightness_red   = brightness - gray_brightness;
      xsane.contrast_red     = contrast - gray_contrast;

      xsane_histogram_to_gamma(&xsane.slider_green, &contrast, &brightness, &gamma);

      xsane.gamma_green      = gamma / gray_gamma;
      xsane.brightness_green = brightness - gray_brightness;
      xsane.contrast_green   = contrast - gray_contrast;

      xsane_histogram_to_gamma(&xsane.slider_blue, &contrast, &brightness, &gamma);

      xsane.gamma_blue       = gamma / gray_gamma;
      xsane.brightness_blue  = brightness - gray_brightness;
      xsane.contrast_blue    = contrast - gray_contrast;
    }
    else /* gray slider was moved in rgb-mode */
    {
      xsane_enhancement_by_gamma();
    }
  }

  xsane_enhancement_update();
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gamma_changed(GtkAdjustment *adj_data, double *val)
{
  *val = adj_data->value;
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_option_menu_lookup(GSGMenuItem menu_items[], const char *string)
{
  int i;

  for (i = 0; strcmp(menu_items[i].label, string) != 0; ++i);
  return i;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_option_menu_callback(GtkWidget * widget, gpointer data)
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

static void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc)
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
      if (i == 0)
      {
        gtk_widget_set_usize(item, 60, 0);
      }
      gtk_container_add(GTK_CONTAINER(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_option_menu_callback, menu_items + i);

      gtk_widget_show(item);

      menu_items[i].label = str_list[i];
      menu_items[i].elem  = elem;
      menu_items[i].index = i;
    }

  option_menu = gtk_option_menu_new();
  gsg_set_tooltip(dialog->tooltips, option_menu, desc);
  gtk_box_pack_end(GTK_BOX(parent), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_option_menu_lookup(menu_items, val));

  gtk_widget_show(option_menu);

  elem->widget = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scale_new(GtkBox *parent, char *labeltext, const char *desc,
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
  gsg_set_tooltip(dialog->tooltips, scale, desc);
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pixels */
  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
  /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
  gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);
  gtk_scale_set_digits(GTK_SCALE(scale), digits);
  gtk_box_pack_end(GTK_BOX(hbox), scale, FALSE, TRUE, 5); /* make scale not sizeable */

  if (xsane_scale_callback)
  {
    gtk_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_scale_callback, val);
  }

  gtk_widget_show(label);
  gtk_widget_show(scale);
  gtk_widget_show(hbox);

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scale_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
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
  gsg_set_tooltip(dialog->tooltips, scale, desc);
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pxiels */
  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
  /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
  gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);
  gtk_scale_set_digits(GTK_SCALE(scale), digits);
  gtk_box_pack_end(GTK_BOX(hbox), scale, TRUE, TRUE, 5); /* make scale sizeable */

  if (xsane_scale_callback)
  {
    gtk_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_scale_callback, val);
  }

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

static void xsane_modus_callback(GtkWidget *xsane_parent, int *num)
{
  xsane.xsane_mode = *num;
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_separator_new(GtkWidget *xsane_parent, int dist)
{
 GtkWidget *xsane_separator;

  xsane_separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(xsane_parent), xsane_separator, FALSE, FALSE, dist);
  gtk_widget_show(xsane_separator);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data)
{
  if (preferences.filename)
    free((void *) preferences.filename);
  preferences.filename = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_browse_filename_callback(GtkWidget *widget, gpointer data)
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

static void xsane_outputfilename_new(GtkWidget *vbox)
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
  gsg_set_tooltip(dialog->tooltips, text, DESC_FILENAME);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.filename);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_outputfilename_changed_callback, 0);

  xsane.outputfilename_entry = text;

  button = gtk_button_new_with_label("Browse");
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_browse_filename_callback, 0);

  gtk_widget_show(button);
  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_fine_mode_callback(GtkWidget * widget)
{
  xsane.fax_fine_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  if (xsane.fax_fine_mode)
  {
    xsane.resolution = 196;
  }
  else
  {
    xsane.resolution = 98;
  }

  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
static void xsane_enhancement_rgb_default_callback(GtkWidget * widget)
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

    xsane.slider_red.value[0] =   0.0;
    xsane.slider_red.value[1] =  50.0;
    xsane.slider_red.value[2] = 100.0;

    xsane.slider_green.value[0] =   0.0;
    xsane.slider_green.value[1] =  50.0;
    xsane.slider_green.value[2] = 100.0;

    xsane.slider_blue.value[0] =   0.0;
    xsane.slider_blue.value[1] =  50.0;
    xsane.slider_blue.value[2] = 100.0;

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  }
  else
  {
    xsane.slider_red.value[0] = xsane.slider_gray.value[0];
    xsane.slider_red.value[1] = xsane.slider_gray.value[1];
    xsane.slider_red.value[2] = xsane.slider_gray.value[2];

    xsane.slider_green.value[0] = xsane.slider_gray.value[0];
    xsane.slider_green.value[1] = xsane.slider_gray.value[1];
    xsane.slider_green.value[2] = xsane.slider_gray.value[2];

    xsane.slider_blue.value[0] = xsane.slider_gray.value[0];
    xsane.slider_blue.value[1] = xsane.slider_gray.value[1];
    xsane.slider_blue.value[2] = xsane.slider_gray.value[2];

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  }

  xsane_update_sliders();
  xsane_update_gamma();
  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_auto_enhancement_callback(GtkWidget * widget)
{
  xsane_calculate_histogram();

  xsane.slider_gray.value[0] = xsane.auto_black;
  xsane.slider_gray.value[1] = xsane.auto_gray;
  xsane.slider_gray.value[2] = xsane.auto_white;

  xsane.slider_red.value[0] = xsane.auto_black;
  xsane.slider_red.value[1] = xsane.auto_gray;
  xsane.slider_red.value[2] = xsane.auto_white;
  
  xsane.slider_green.value[0] = xsane.auto_black;
  xsane.slider_green.value[1] = xsane.auto_gray;
  xsane.slider_green.value[2] = xsane.auto_white;
  
  xsane.slider_blue.value[0] = xsane.auto_black;
  xsane.slider_blue.value[1] = xsane.auto_gray;
  xsane.slider_blue.value[2] = xsane.auto_white;

  xsane_enhancement_by_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_standard_options_callback(GtkWidget * widget)
{
  preferences.show_standard_options = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_standard_options)
  {
    gtk_widget_show(xsane.standard_options_shell);
  }
  else
  {
    gtk_widget_hide(xsane.standard_options_shell);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_advanced_options_callback(GtkWidget * widget)
{
  preferences.show_advanced_options = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_advanced_options)
  {
    gtk_widget_show(xsane.advanced_options_shell);
  }
  else
  {
    gtk_widget_hide(xsane.advanced_options_shell);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_histogram_callback(GtkWidget * widget)
{
  preferences.show_histogram = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  if (preferences.show_histogram)
  {
    xsane_update_histogram();
    gtk_widget_show(xsane.histogram_dialog);
  }
  else
  {
    gtk_widget_hide(xsane.histogram_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_printer_callback(GtkWidget *widget, gpointer data)
{
  preferences.printernr = (int) data;
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_update_xsane_callback()
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
/*  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, FALSE, FALSE, 0); */ /* make scales fixed */
  gtk_box_pack_start(GTK_BOX(xsane_hbox), xsane_vbox, TRUE, TRUE, 0); /* make scales sizeable */

  /* XSane Frame */

  xsane_frame = gtk_frame_new("XSane options");
  gtk_container_border_width(GTK_CONTAINER(xsane_frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(xsane_frame), GTK_SHADOW_ETCHED_IN);
/*  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_frame, FALSE, FALSE, 0); */ /* fixed frameheight */
  gtk_box_pack_start(GTK_BOX(xsane_vbox), xsane_frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(xsane_frame);

/*  xsane_vbox_xsane_modus = gtk_vbox_new(FALSE, 5); */
   xsane_vbox_xsane_modus = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(xsane_frame), xsane_vbox_xsane_modus);
  gtk_widget_show(xsane_vbox_xsane_modus);

/* scan copy fax selection */

  if (xsane.mode == STANDALONE)
  {
    xsane_hbox_xsane_modus = gtk_hbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(xsane_hbox_xsane_modus), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_hbox_xsane_modus, FALSE, FALSE, 0);

    xsane_label = gtk_label_new("XSane mode");
    gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_modus), xsane_label, FALSE, FALSE, 2);
    gtk_widget_show(xsane_label);

    xsane_modus_menu = gtk_menu_new();

    xsane_modus_item = gtk_menu_item_new_with_label("Scan");
    gtk_widget_set_usize(xsane_modus_item, 60, 0);
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_SCAN]);
    gtk_widget_show(xsane_modus_item);

    xsane_modus_item = gtk_menu_item_new_with_label("Copy");
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_COPY]);
    gtk_widget_show(xsane_modus_item);

#ifdef XSANE_TEST
    xsane_modus_item = gtk_menu_item_new_with_label("Fax");
    gtk_container_add(GTK_CONTAINER(xsane_modus_menu), xsane_modus_item);
    gtk_signal_connect(GTK_OBJECT(xsane_modus_item), "activate",
                       (GtkSignalFunc) xsane_modus_callback, &xsane_scanmode_number[XSANE_FAX]);
    gtk_widget_show(xsane_modus_item);
#endif

    xsane_modus_option_menu = gtk_option_menu_new();
    gsg_set_tooltip(dialog->tooltips, xsane_modus_option_menu, DESC_XSANE_MODE);
    gtk_box_pack_end(GTK_BOX(xsane_hbox_xsane_modus), xsane_modus_option_menu, FALSE, FALSE, 2);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_modus_option_menu), xsane_modus_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_modus_option_menu), xsane.xsane_mode);
    gtk_widget_show(xsane_modus_option_menu);
    gtk_widget_show(xsane_hbox_xsane_modus);


    dialog->xsanemode_widget = xsane_modus_option_menu;
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

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scanmode, opt->desc);
          }
           break;

           default:
            fprintf(stderr, "scanmode_selection: unknown type %d\n", opt->type);
        }
        gtk_widget_show(hbox);
      }
    }


    /* input selection */
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

            xsane_option_menu_new(hbox, (char **) opt->constraint.string_list, set, dialog->well_known.scansource, opt->desc);
           }
           break;

           default:
            fprintf(stderr, "scansource_selection: unknown type %d\n", opt->type);
        }
        gtk_widget_show(hbox);
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

        xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), resolution_xpm, DESC_RESOLUTION, min, max, quant, quant, 0.0, 0,
                                    &xsane.resolution, &xsane.resolution_widget, dialog->well_known.dpi, xsane_resolution_update);

        elem = dialog->element + dialog->well_known.dpi;
        elem->data = xsane.resolution_widget;
      }
    }
  }
  else if (xsane.xsane_mode == XSANE_COPY)
  {
   const SANE_Option_Descriptor *opt;
   GtkWidget *pixmapwidget, *hbox, *xsane_printer_option_menu, *xsane_printer_menu, *xsane_printer_item;
   GdkBitmap *mask;
   GdkPixmap *pixmap;
   int i;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), hbox, FALSE, FALSE, 2);

    pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) printer_xpm);
    pixmapwidget = gtk_pixmap_new(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
    gdk_pixmap_unref(pixmap);
    gtk_widget_show(pixmapwidget);

    xsane_printer_menu = gtk_menu_new();

    for (i=0; i < preferences.printerdefinitions; i++)
    {
      xsane_printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
      gtk_container_add(GTK_CONTAINER(xsane_printer_menu), xsane_printer_item);
      gtk_signal_connect(GTK_OBJECT(xsane_printer_item), "activate", (GtkSignalFunc) xsane_printer_callback, (void *) i);
      gtk_widget_show(xsane_printer_item);
    }

    xsane_printer_option_menu = gtk_option_menu_new();
    gsg_set_tooltip(dialog->tooltips, xsane_printer_option_menu, DESC_PRINTER_SELECT);
    gtk_box_pack_end(GTK_BOX(hbox), xsane_printer_option_menu, FALSE, FALSE, 2);
    gtk_widget_show(xsane_printer_option_menu);
    gtk_widget_show(hbox);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_printer_option_menu), xsane_printer_menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_printer_option_menu), preferences.printernr);


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
           min   = ((double) opt->constraint.range->min) / preferences.printer[preferences.printernr]->resolution;
           max   = ((double) opt->constraint.range->max) / preferences.printer[preferences.printernr]->resolution;
           quant = ((double) opt->constraint.range->quant) / preferences.printer[preferences.printernr]->resolution;
         break;

         case SANE_TYPE_FIXED:
           min   = SANE_UNFIX(opt->constraint.range->min) / preferences.printer[preferences.printernr]->resolution;
           max   = SANE_UNFIX(opt->constraint.range->max) / preferences.printer[preferences.printernr]->resolution;
           quant = SANE_UNFIX(opt->constraint.range->quant) / preferences.printer[preferences.printernr]->resolution;
         break;

         default:
          fprintf(stderr, "zoom_scale_update: unknown type %d\n", opt->type);
        }

        if (quant == 0)
        {
          quant = 0.01;
        }

        xsane.zoom = xsane.resolution / preferences.printer[preferences.printernr]->resolution;

        xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), zoom_xpm, DESC_ZOOM, min, max,
                                    quant, quant, 0.0, 2, &xsane.zoom, &xsane.zoom_widget, dialog->well_known.dpi, xsane_zoom_update);
      }
    }
  }
  else /* XSANE_FAX */
  {
   const SANE_Option_Descriptor *opt;

    xsane.resolution = 98;

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
    if (opt)
    {
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
      {
        xsane_button = gtk_check_button_new_with_label("Fine mode");
        gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_FAX_FINE_MODE);
        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(xsane_button), xsane.fax_fine_mode);
        gtk_box_pack_start(GTK_BOX(xsane_vbox_xsane_modus), xsane_button, FALSE, FALSE, 2);
        gtk_widget_show(xsane_button);
        gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_fax_fine_mode_callback, 0);

        if (xsane.fax_fine_mode)
        {
          xsane.resolution = 196;
        }
      }
    }
    xsane_fax_scan_dialog();
  }

   /* test if scanner gamma table is selected */

   xsane.scanner_gamma_gray  = FALSE;
   if (dialog->well_known.gamma_vector >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_gray  = TRUE;
     }
   }

   xsane.scanner_gamma_color  = FALSE;
   if (dialog->well_known.gamma_vector_r >0)
   {
    const SANE_Option_Descriptor *opt;

     opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);
     if (SANE_OPTION_IS_ACTIVE(opt->cap))
     {
       xsane.scanner_gamma_color  = TRUE;
     }
   }



  /* XSane Frame Enhancement */

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  if (xsane.param.depth == 1)
  {
    return(xsane_hbox);
  }

  xsane.slider_gray.active   = XSANE_SLIDER_ACTIVE; /* mark slider active */

  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_xpm, DESC_GAMMA, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                              &xsane.gamma, &xsane.gamma_widget, 0, xsane_gamma_changed);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_red_xpm, DESC_GAMMA_R, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_red  , &xsane.gamma_red_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_green_xpm, DESC_GAMMA_G, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_green, &xsane.gamma_green_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), Gamma_blue_xpm, DESC_GAMMA_B, 0.3, 3.0, 0.01, 0.01, 0.0, 2,
                                &xsane.gamma_blue , &xsane.gamma_blue_widget, 0, xsane_gamma_changed);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_xpm, DESC_BRIGHTNESS, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                              &xsane.brightness, &xsane.brightness_widget, 0, xsane_gamma_changed);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_red_xpm, DESC_BRIGHTNESS_R, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_red  , &xsane.brightness_red_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_green_xpm, DESC_BRIGHTNESS_G, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_green, &xsane.brightness_green_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), brightness_blue_xpm, DESC_BRIGHTNESS_B, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.brightness_blue,  &xsane.brightness_blue_widget, 0, xsane_gamma_changed);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_xpm, DESC_CONTRAST, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                              &xsane.contrast, &xsane.contrast_widget, 0, xsane_gamma_changed);
  if ( (xsane.xsane_color) && (!xsane.enhancement_rgb_default) )
  {
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_red_xpm, DESC_CONTRAST_R, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_red  , &xsane.contrast_red_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_green_xpm, DESC_CONTRAST_G, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_green, &xsane.contrast_green_widget, 0, xsane_gamma_changed);
    xsane_scale_new_with_pixmap(GTK_BOX(xsane_vbox_xsane_modus), contrast_blue_xpm, DESC_CONTRAST_B, -100.0, 100.0, 1.0, 1.0, 0.0, 0,
                                &xsane.contrast_blue,  &xsane.contrast_blue_widget, 0, xsane_gamma_changed);

    xsane_separator_new(xsane_vbox_xsane_modus, 2);
  }

  if (xsane.xsane_color)
  {
    xsane_button = gtk_check_button_new_with_label("RGB default");
    gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_RGB_DEFAULT);
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
  gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_ENH_AUTO);
  gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
  gtk_widget_show(xsane_button);
  gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_auto_enhancement_callback, 0);

  xsane_button = gtk_button_new_with_label("Default");
  gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_ENH_DEFAULT);
  gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
  gtk_widget_show(xsane_button);
  gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_restore_default, 0);

  xsane_button = gtk_button_new_with_label("Restore");
  gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_ENH_RESTORE);
  gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
  gtk_widget_show(xsane_button);
  gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_restore_saved, 0);

  xsane_button = gtk_button_new_with_label("Save");
  gsg_set_tooltip(dialog->tooltips, xsane_button, DESC_ENH_SAVE);
  gtk_box_pack_start(GTK_BOX(xsane_hbox_xsane_enhancement), xsane_button, TRUE, TRUE, 2);
  gtk_widget_show(xsane_button);
  gtk_signal_connect(GTK_OBJECT(xsane_button), "clicked", (GtkSignalFunc) xsane_enhancement_save, 0);

  xsane_update_histogram();

 return(xsane_hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_decode_devname(const char *encoded_devname, int n, char *buf)
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

static void xsane_gimp_query(void)
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
       "Oliver Rauch",
       "Oliver Rauch",
      "1998/1999",
      "<Toolbox>/Xtns/XSane/Device dialog...",
      "RGB, GRAY",
      PROC_EXTENSION,
      nargs, nreturn_vals,
      args, return_vals);

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);
  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    fprintf(stderr, "\n\n"
                    "%s warning:\n"
                    "  Sane major version number mismatch!\n"
                    "  xsane major version = %d\n"
                    "  backend major version = %d\n",
                    prog_name, SANE_V_MAJOR, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode));
    return;
  }

  sane_get_devices(&devlist, SANE_FALSE);

  for (i = 0; devlist[i]; ++i)
    {
      strcpy(name, "xsane-");
      if (encode_devname(devlist[i]->name, sizeof(name) - 6, name + 6) < 0)
	continue;	/* name too long... */

      strncpy(mpath, "<Toolbox>/Xtns/XSane/", sizeof(mpath));
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
         "Oliver Rauch",
         "Oliver Rauch",
	 "1998/1999", mpath, "RGB, GRAY", PROC_EXTENSION,
	 nargs, nreturn_vals, args, return_vals);
    }
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gimp_run(char *name, int nparams, GParam * param, int *nreturn_vals, GParam ** return_vals)
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
    if (xsane_decode_devname(name + 6, sizeof(devname), devname) < 0)
    {
      return;			/* name too long */
    }
    args[nargs++] = devname;
  }

  switch (run_mode)
  {
    case RUN_INTERACTIVE:
      xsane_interface(nargs, args);
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

static void xsane_update_preview(GSGDialog *dialog, void *arg)
{
  if (xsane.preview)
  {
    preview_update(xsane.preview);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_save(void)
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

static void xsane_new_printer()
{
  preferences.printernr = preferences.printerdefinitions++;

  preferences.printer[preferences.printernr] = calloc(sizeof(Preferences_printer_t), 1);

  preferences.printer[preferences.printernr]->name         = strdup(PRINTERNAME);
  preferences.printer[preferences.printernr]->command      = strdup(PRINTERCOMMAND);
  preferences.printer[preferences.printernr]->resolution   = 300;
  preferences.printer[preferences.printernr]->width        = 576;
  preferences.printer[preferences.printernr]->height       = 835;
  preferences.printer[preferences.printernr]->leftoffset   = 10;
  preferences.printer[preferences.printernr]->bottomoffset = 10;
  preferences.printer[preferences.printernr]->gamma        = 1.0;
  preferences.printer[preferences.printernr]->gamma_red    = 1.0;
  preferences.printer[preferences.printernr]->gamma_green  = 1.0;
  preferences.printer[preferences.printernr]->gamma_blue   = 1.0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_restore(void)
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
  {
    preferences.filename = strdup(OUTFILENAME);
  }

  if (preferences.printerdefinitions == 0)
  {
    xsane_new_printer();
  }

  if (!preferences.fax_command)
  {
    preferences.fax_command = strdup(FAXCOMMAND);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_quit(void)
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
  xsane_pref_save();
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

static gint xsane_standard_option_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_standard_options = FALSE;
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_advanced_option_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_advanced_options = FALSE;
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_histogram_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_histogram = FALSE;
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when window manager's "delete" (or "close") function is invoked.  */
static gint xsane_scan_win_delete(GtkWidget *w, gpointer data)
{
  xsane_quit();
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_preview_window_destroyed(GtkWidget *widget, gpointer call_data)
{
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(call_data), FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the preview button is pressed. */

static void xsane_scan_preview(GtkWidget * widget, gpointer call_data)
{
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    if (!xsane.preview)
    {
      xsane.preview = preview_new(dialog);
      if (xsane.preview && xsane.preview->top)
      {
        gtk_signal_connect(GTK_OBJECT(xsane.preview->top), "destroy", GTK_SIGNAL_FUNC(xsane_preview_window_destroyed), widget);
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
static void xsane_gimp_advance(void)
{
  if (++xsane.x >= xsane.param.pixels_per_line)
    {
      int tile_height = gimp_tile_height();

      xsane.x = 0;
      ++xsane.y;
      if (xsane.y % tile_height == 0)
	{
	  gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - tile_height,
				  xsane.param.pixels_per_line, tile_height);
	  if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
	    {
	      int height;

	      xsane.tile_offset %= 3;
	      if (!xsane.first_frame)
		{
		  /* get the data for the existing tile: */
		  height = tile_height;
		  if (xsane.y + height >= xsane.param.lines)
                  {
		    height = xsane.param.lines - xsane.y;
                  }
		  gimp_pixel_rgn_get_rect(&xsane.region, xsane.tile, 0, xsane.y, xsane.param.pixels_per_line, height);
		}
	    }
	  else
          {
	    xsane.tile_offset = 0;
          }
	}
    }
}

#endif /* HAVE_LIBGIMP_GIMP_H */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_read_image_data(gpointer data, gint source, GdkInputCondition cond)
{
  SANE_Handle dev = gsg_dialog_get_device (dialog);
  SANE_Status status;
  SANE_Int len;
  int i;
  char buf[255];

  if (xsane.param.depth <= 8)
  {
    static unsigned char buf8[32768];

    while (1)
    {
      status = sane_read(dev, (SANE_Byte *) buf8, sizeof(buf8), &len);
      if (status != SANE_STATUS_GOOD)
      {
        if (status == SANE_STATUS_EOF)
        {
          if (!xsane.param.last_frame)
          {
            xsane_start_scan();
            break; /* leave while loop */
          }
        }
        else
        {
          snprintf(buf, sizeof(buf), "Error during read: %s.", sane_strstatus(status));
          gsg_error(buf);
        }
        xsane_scan_done();
        return;
      }

      if (!len)
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.progress, xsane.bytes_read / (gfloat) xsane.num_bytes);

      if (xsane.input_tag < 0)
      {
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }
      }

      switch (xsane.param.format)
      {
        case SANE_FRAME_GRAY:
          if (xsane.mode == STANDALONE)
          {
           int i;
           char val;

            if ((!xsane.scanner_gamma_gray) && (xsane.param.depth > 1))
            {
              for (i=0; i < len; ++i)
              {
                val = xsane.gamma_data[(int) buf8[i]];
                fwrite(&val, 1, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf8, 1, len, xsane.out);
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

                  mask = buf8[i];
                  for (j = 7; j >= 0; --j)
                  {
                    u_char gl = (mask & (1 << j)) ? 0x00 : 0xff;
                    xsane.tile[xsane.tile_offset++] = gl;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                    {
                      break;
                    }
                  }
                }
               break;

              case 8:
                if (!xsane.scanner_gamma_gray)
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = xsane.gamma_data[(int) buf8[i]];
                    xsane_gimp_advance();
                  }
                }
                else
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = buf8[i];
                    xsane_gimp_advance();
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

            if (!xsane.scanner_gamma_color)
            {
              for (i=0; i < len; ++i)
              {
                if (dialog->pixelcolor == 0)
                {
                  val = xsane.gamma_data_red[(int) buf8[i]];
                  dialog->pixelcolor++;
                }
                else if (dialog->pixelcolor == 1)
                {
                  val = xsane.gamma_data_green[(int) buf8[i]];
                  dialog->pixelcolor++;
                }
                else
                {
                  val = xsane.gamma_data_blue[(int) buf8[i]];
                  dialog->pixelcolor = 0;
                }
                fwrite(&val, 1, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf8, 1, len, xsane.out);
            }
          }
#ifdef HAVE_LIBGIMP_GIMP_H
          else
          {
            switch (xsane.param.depth)
            {
              case 1:
                if (xsane.param.format == SANE_FRAME_RGB)
                {
                  goto bad_depth;
                }
                for (i = 0; i < len; ++i)
                {
                 u_char mask;
                 int j;

                  mask = buf8[i];
                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;
                    xsane.tile[xsane.tile_offset++] = gl;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                   break;
                  }
                }
               break;

              case 8:
                if (!xsane.scanner_gamma_color)
                {
                  for (i = 0; i < len; ++i)
                  {
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_red[(int) buf8[i]];
                    }
                    else if (xsane.tile_offset % 3 == 1)
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_green[(int) buf8[i]];
                    }
                    else
                    {
                      xsane.tile[xsane.tile_offset++] = xsane.gamma_data_blue[(int) buf8[i]];
                    }
 
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane_gimp_advance();
                    }
                  }
                }
                else
                {
                  for (i = 0; i < len; ++i)
                  {
                    xsane.tile[xsane.tile_offset++] = buf8[i];
                    if (xsane.tile_offset % 3 == 0)
                    {
                      xsane_gimp_advance();
                    }
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
          {
            for (i = 0; i < len; ++i)
            {
              fwrite(buf8 + i, 1, 1, xsane.out);
              fseek(xsane.out, 2, SEEK_CUR);
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

                  mask = buf8[i];
                  for (j = 0; j < 8; ++j)
                  {
                    u_char gl = (mask & 1) ? 0xff : 0x00;
                    mask >>= 1;
                    xsane.tile[xsane.tile_offset] = gl;
                    xsane.tile_offset += 3;
                    xsane_gimp_advance();
                    if (xsane.x == 0)
                    {
                      break;
                    }
                  }
                }
               break;

              case 8:
                for (i = 0; i < len; ++i)
                {
                  xsane.tile[xsane.tile_offset] = buf8[i];
                  xsane.tile_offset += 3;
                  xsane_gimp_advance();
                }
               break;
            }
          }
#endif /* HAVE_LIBGIMP_GIMP_H */
         break;

        default:
          fprintf(stderr, "%s.xsane_read_image_data: bad frame format %d\n", prog_name, xsane.param.format);
          xsane_scan_done();
          return;
      }
    }
  }
  else
  {
    static guint16 buf16[32768];
    char buf[255];
    char last = 0;
    int offset = 0;

    while (1)
    {
      if (offset) /* if we have had an odd number of bytes */
      {
        buf16[0] = last;
        status = sane_read(dev, (SANE_Byte *) (buf16 + 1), sizeof(buf16) - 1, &len);
        if (len)
        {
          len++;
        }
      }
      else /* last read we had an even number of bytes */
      {
        status = sane_read(dev, (SANE_Byte *) buf16, sizeof(buf16), &len);
      }

      if (len % 2) /* odd number of bytes */
      {
        len--;
        last = buf16[len];
        offset = 1;
      }
      else /* even number of bytes */
      {
        offset = 0;
      }


      if (status != SANE_STATUS_GOOD)
      {
        if (status == SANE_STATUS_EOF)
        {
          if (!xsane.param.last_frame)
          {
            xsane_start_scan();
            break; /* leave while loop */
          }
        }
        else
        {
          snprintf(buf, sizeof(buf), "Error during read: %s.", sane_strstatus(status));
          gsg_error(buf);
        }
        xsane_scan_done();
        return;
      }

      if (!len)
      {
        break; /* out of data for now, leave while loop */
      }

      xsane.bytes_read += len;
      xsane_progress_update(xsane.progress, xsane.bytes_read / (gfloat) xsane.num_bytes);

      if (xsane.input_tag < 0)
      {
        while (gtk_events_pending())
        {
          gtk_main_iteration();
        }
      }

      switch (xsane.param.format)
      {
        case SANE_FRAME_GRAY:
          if (xsane.mode == STANDALONE)
          {
           int i;
           guint16 val;

            if (!xsane.scanner_gamma_gray)
            {
              for (i=0; i < len/2; ++i)
              {
                val = xsane.gamma_data[buf16[i]];
                fwrite(&val, 2, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf16, 2, len/2, xsane.out);
            }
          }
         break;

        case SANE_FRAME_RGB:
          if (xsane.mode == STANDALONE)
          {
           int i;
           guint16 val;

            if (!xsane.scanner_gamma_color)
            {
              for (i=0; i < len/2; ++i)
              {
                if (dialog->pixelcolor == 0)
                {
                  val = xsane.gamma_data_red[buf16[i]];
                  dialog->pixelcolor++;
                }
                else if (dialog->pixelcolor == 1)
                {
                  val = xsane.gamma_data_green[buf16[i]];
                  dialog->pixelcolor++;
                }
                else
                {
                  val = xsane.gamma_data_blue[buf16[i]];
                  dialog->pixelcolor = 0;
                }
                fwrite(&val, 2, 1, xsane.out);
              }
            }
            else
            {
              fwrite(buf16, 2, len/2, xsane.out);
            }
          }
         break;

        case SANE_FRAME_RED:
        case SANE_FRAME_GREEN:
        case SANE_FRAME_BLUE:
          if (xsane.mode == STANDALONE)
          {
            for (i = 0; i < len/2; ++i)
            {
              fwrite(buf16 + i*2, 2, 1, xsane.out);
              fseek(xsane.out, 4, SEEK_CUR);
            }
          }
         break;

        default:
          fprintf(stderr, "%s.xsane_read_image_data: bad frame format %d\n", prog_name, xsane.param.format);
          xsane_scan_done();
          return;
      }
    }
  }

  return;

#ifdef HAVE_LIBGIMP_GIMP_H
bad_depth:

  snprintf(buf, sizeof(buf), "Cannot handle depth %d.", xsane.param.depth);
  gsg_error(buf);
  xsane_scan_done();
  return;
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static RETSIGTYPE xsane_sigpipe_handler(int signal)
/* this is to catch a broken pipe while writing to printercommand */
{
  xsane_cancel_save(0);
  xsane.broken_pipe = 1;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_scan_done(void)
{
  gtk_widget_set_sensitive(xsane.shell, TRUE);
  gtk_widget_set_sensitive(xsane.histogram_dialog, TRUE);
  gsg_set_sensitivity(dialog, TRUE);

  if (xsane.input_tag >= 0)
  {
    gdk_input_remove(xsane.input_tag);
    xsane.input_tag = -1;
  }

  if (!xsane.progress)
  {
    return;
  }

  /* remove progressbar */
  xsane_progress_free(xsane.progress);
  xsane.progress = 0;
  while(gtk_events_pending())
  {
    gtk_main_iteration();
  }


  /* we have to free the gamma tables if we used software gamma correction */
  
  if (xsane.gamma_data) 
  {
    free(xsane.gamma_data);
    xsane.gamma_data = 0;
  }

  if (xsane.gamma_data_red)
  {
    free(xsane.gamma_data_red);
    free(xsane.gamma_data_green);
    free(xsane.gamma_data_blue);

    xsane.gamma_data_red   = 0;
    xsane.gamma_data_green = 0;
    xsane.gamma_data_blue  = 0;
  }

  if (xsane.mode == STANDALONE)
    {
      fclose(xsane.out);
      xsane.out = 0;


      if ( (xsane.xsane_mode == XSANE_SCAN) && (xsane.xsane_output_format != XSANE_PNM) 
                                            && (xsane.xsane_output_format != XSANE_RAW16) )
      {
       FILE *outfile;
       FILE *infile;
       char copyfilename[PATH_MAX];
       char buf[256];

         /* open progressbar */
         snprintf(buf, sizeof(buf), "Saving image");
         xsane.progress = xsane_progress_new("Converting data....", buf, (GtkSignalFunc) xsane_cancel_save, 0);
         xsane_progress_update(xsane.progress, 0);
         while (gtk_events_pending())
         {
           gtk_main_iteration();
         }

         gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", "conversion", dialog->dev_name, ".tmp");
         infile = fopen(copyfilename, "r");
	 if (infile != 0)
	 {
	   fseek(infile, xsane.header_size, SEEK_SET);

#ifdef HAVE_LIBTIFF
	   if (xsane.xsane_output_format == XSANE_TIFF)		/* routines that want to have filename  for saving */
	   {
             xsane_save_tiff(preferences.filename, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                             xsane.param.lines, preferences.png_compression);
           }
	   else							/* routines that want to have filedescriptor for saving */
#endif
           {
             outfile = fopen(preferences.filename, "w");
             if (outfile != 0)
             {
               switch(xsane.xsane_output_format)
               {
#ifdef HAVE_LIBJPEG
	         case XSANE_JPEG:
                   xsane_save_jpeg(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                   xsane.param.lines, preferences.jpeg_quality);
                  break;
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
                 case XSANE_PNG:
                   if (xsane.param.depth <= 8)
		   {
                     xsane_save_png(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                    xsane.param.lines, preferences.png_compression);
                   }
                   else
                   {
                     xsane_save_png_16(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                       xsane.param.lines, preferences.png_compression);
                   }
                  break;

                 case XSANE_PNM16:
                   xsane_save_pnm_16(outfile, infile, xsane.xsane_color, xsane.param.depth, xsane.param.pixels_per_line,
                                     xsane.param.lines);
                  break;
#endif
#endif

	         case XSANE_PS: /* save postscript, use original size */
                 { 
                   float imagewidth  = xsane.param.pixels_per_line/(float)xsane.resolution; /* width in inch */
                   float imageheight = xsane.param.lines/(float)xsane.resolution; /* height in inch */

                    xsane_save_ps(outfile, infile,
                                  xsane.xsane_color /* gray, color */,
                                  xsane.param.depth /* bits */,
                                  xsane.param.pixels_per_line, xsane.param.lines,
                                  preferences.printer[preferences.printernr]->leftoffset + preferences.printer[preferences.printernr]->width/2 - imagewidth*36,
                                  preferences.printer[preferences.printernr]->bottomoffset + preferences.printer[preferences.printernr]->height/2 - imageheight*36,
                                  imagewidth, imageheight);
                  }
                  break;


                 default:
                   snprintf(buf, sizeof(buf),"Unknown file format for saving!\n" );
                   gsg_error(buf);
	       }
               fclose(outfile);
	     }
             else
             {
              char buf[256];

               snprintf(buf, sizeof(buf), "Failed to open `%s': %s", preferences.filename, strerror(errno));
               gsg_error(buf);
             }
           }
           fclose(infile);
           remove(copyfilename);
	 }
         else
         {
          char buf[256];

           snprintf(buf, sizeof(buf), "Failed to open imagefile for conversion: %s", preferences.filename, strerror(errno));
           gsg_error(buf);
         }
         xsane_progress_free(xsane.progress);
         xsane.progress = 0;

         while (gtk_events_pending())
         {
           gtk_main_iteration();
         }
      }
      else if (xsane.xsane_mode == XSANE_COPY)
      {
       FILE *outfile;
       FILE *infile;
       char copyfilename[PATH_MAX];
       char buf[256];

         /* open progressbar */
         snprintf(buf, sizeof(buf), "converting to postscript");
         xsane.progress = xsane_progress_new("Converting data....", buf, (GtkSignalFunc) xsane_cancel_save, 0);
         xsane_progress_update(xsane.progress, 0);
         while (gtk_events_pending())
         {
           gtk_main_iteration();
         }

         xsane.broken_pipe = 0;
         gsg_make_path(sizeof(copyfilename), copyfilename, "xsane", "conversion", dialog->dev_name, ".tmp");
         infile = fopen(copyfilename, "r");
         outfile = popen(preferences.printer[preferences.printernr]->command, "w");
	 if ((outfile != 0) && (infile != 0)) /* copy mode, use zoom size */
	 {
          struct SIGACTION act;
          float imagewidth  = xsane.param.pixels_per_line/(float)preferences.printer[preferences.printernr]->resolution; /* width in inch */
          float imageheight = xsane.param.lines/(float)preferences.printer[preferences.printernr]->resolution; /* height in inch */

           memset (&act, 0, sizeof (act)); /* define broken pipe handler */
           act.sa_handler = xsane_sigpipe_handler;
           sigaction (SIGPIPE, &act, 0);


	   fseek(infile, xsane.header_size, SEEK_SET);
           xsane_save_ps(outfile, infile,
                         xsane.xsane_color /* gray, color */,
                         xsane.param.depth /* bits */,
                         xsane.param.pixels_per_line, xsane.param.lines,
                         preferences.printer[preferences.printernr]->leftoffset + preferences.printer[preferences.printernr]->width/2 - imagewidth*36,
                         preferences.printer[preferences.printernr]->bottomoffset + preferences.printer[preferences.printernr]->height/2 - imageheight*36,
                         imagewidth, imageheight);
	 }
         else
         {
          char buf[256];

           if (!infile)
           {
             snprintf(buf, sizeof(buf), "Failed to open imagefile for conversion: %s", preferences.filename, strerror(errno));
             gsg_error(buf);
           }
           else if (!outfile)
           {
             snprintf(buf, sizeof(buf), "Failed to open pipe for executing printercommand");
             gsg_error(buf);
           }
         }

         if (xsane.broken_pipe)
         {
           snprintf(buf, sizeof(buf), "Failed to execute printercommand \"%s\"",
                    preferences.printer[preferences.printernr]->command);
           gsg_error(buf);
         }

         xsane_progress_free(xsane.progress);
         xsane.progress = 0;
         while (gtk_events_pending())
         {
           gtk_main_iteration();
         }

         if (infile)
         {
           fclose(infile);
           remove(copyfilename);
         }

         if (outfile)
         {
           pclose(outfile);
         }
      }
    }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
    {
      int remaining;

      /* GIMP mode */
      if (xsane.y > xsane.param.lines)
      {
	xsane.y = xsane.param.lines;
      }

      remaining = xsane.y % gimp_tile_height();
      if (remaining)
      {
	gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - remaining, xsane.param.pixels_per_line, remaining);
      }
      gimp_drawable_flush(xsane.drawable);
      gimp_display_new(xsane.image_ID);
      gimp_drawable_detach(xsane.drawable);
      free(xsane.tile);
      xsane.tile = 0;
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  xsane.header_size = 0;
  sane_cancel(gsg_dialog_get_device(dialog));
  
  if ( (preferences.increase_filename_counter) && (xsane.xsane_mode == XSANE_SCAN) && (xsane.mode == STANDALONE) )
  {
    xsane_increase_counter_in_filename(preferences.filename, preferences.skip_existing_numbers);
    gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), (char *) preferences.filename);
  }
  else if (xsane.xsane_mode == XSANE_FAX)
  {
    xsane_increase_counter_in_filename(preferences.filename, preferences.skip_existing_numbers);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_cancel(void)
{
  sane_cancel(gsg_dialog_get_device(dialog));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_start_scan(void)
{
  SANE_Status status;
  SANE_Handle dev = gsg_dialog_get_device(dialog);
  const char *frame_type = 0;
  char buf[256];
  int fd;

  gsg_set_sensitivity(dialog, FALSE); /* turn off buttons and sliders ... */
  gtk_widget_set_sensitive(xsane.shell, FALSE);
  gtk_widget_set_sensitive(xsane.histogram_dialog, FALSE);

#ifdef HAVE_LIBGIMP_GIMP_H
  if (xsane.mode == GIMP_EXTENSION && xsane.tile)
    {
      int height, remaining;

      /* write the last tile of the frame to the GIMP region: */

      if (xsane.y > xsane.param.lines)	/* sanity check */
      {
	xsane.y = xsane.param.lines;
      }

      remaining = xsane.y % gimp_tile_height();
      if (remaining)
      {
	gimp_pixel_rgn_set_rect(&xsane.region, xsane.tile, 0, xsane.y - remaining, xsane.param.pixels_per_line, remaining);
      }

      /* initialize the tile with the first tile of the GIMP region: */

      height = gimp_tile_height();
      if (height >= xsane.param.lines)
      {
	height = xsane.param.lines;
      }
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
              switch (xsane.param.depth)
	      {
                case 8: /* color 8 bit mode, write ppm header */
                  fprintf(xsane.out, "P6\n# SANE data follows\n%d %d\n255\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

		default: /* color, but not 8 bit mode, write as raw data because this is not defined in pnm */ 
                  fprintf(xsane.out, "SANE_RGB_RAW\n%d %d\n%d\n",
                          xsane.param.pixels_per_line, xsane.param.lines, (int) pow(2, xsane.param.depth) - 1);
              }
             break;

	    case SANE_FRAME_GRAY:
              switch (xsane.param.depth)
	      {
                case 1: /* 1 bit lineart mode, write pbm header */
                  fprintf(xsane.out, "P4\n# SANE data follows\n%d %d\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

                case 8: /* 8 bit grayscale mode, write pgm header */
                  fprintf(xsane.out, "P5\n# SANE data follows\n%d %d\n255\n",
                          xsane.param.pixels_per_line, xsane.param.lines);
                 break;

                default: /* grayscale mode but not 1 or 8 bit, write as raw data because this is not defined in pnm */
                  fprintf(xsane.out, "SANE_GRAYSCALE_RAW\n%d %d\n%d\n",
                          xsane.param.pixels_per_line, xsane.param.lines, (int) pow(2, xsane.param.depth) - 1);
              }
	     break;
	    }
	  xsane.header_size = ftell(xsane.out);
	}

      if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
      {
	fseek(xsane.out, xsane.header_size + xsane.param.format - SANE_FRAME_RED, SEEK_SET);
      }

      if (xsane.xsane_mode == XSANE_SCAN)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for `%s'...", frame_type, preferences.filename);
      }
      else if (xsane.xsane_mode == XSANE_COPY)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for photocopy ...", frame_type);
      }
      else if (xsane.xsane_mode == XSANE_FAX)
      {
        snprintf(buf, sizeof(buf), "Receiving %s data for fax ...", frame_type);
      }
    }
#ifdef HAVE_LIBGIMP_GIMP_H
  else
    {
      size_t tile_size;

      /* We are running under the GIMP */

      xsane.tile_offset = 0;
      tile_size = xsane.param.pixels_per_line * gimp_tile_height();
      if (xsane.param.format != SANE_FRAME_GRAY)
      {
	tile_size *= 3;	/* 24 bits/pixel */
      }
      if (xsane.tile)
      {
	xsane.first_frame = 0;
      }
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

      if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
      {
	xsane.tile_offset = xsane.param.format - SANE_FRAME_RED;
      }

      snprintf(buf, sizeof(buf), "Receiving %s data for GIMP...", frame_type);
    }
#endif /* HAVE_LIBGIMP_GIMP_H */

  dialog->pixelcolor = 0;

  if (xsane.progress)
  {
    xsane_progress_free(xsane.progress);
  }
  xsane.progress = xsane_progress_new("Scanning", buf, (GtkSignalFunc) xsane_cancel, 0);

  xsane.input_tag = -1;

  if (sane_set_io_mode(dev, SANE_TRUE) == SANE_STATUS_GOOD && sane_get_select_fd(dev, &fd) == SANE_STATUS_GOOD)
  {
    xsane.input_tag = gdk_input_add(fd, GDK_INPUT_READ, xsane_read_image_data, 0);
  }
  else
  {
    xsane_read_image_data(0, -1, GDK_INPUT_READ);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Invoked when the scan button is pressed */
static void xsane_scan_dialog(GtkWidget * widget, gpointer call_data)
{
  char buf[256];

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  if (xsane.mode == STANDALONE)  				/* We are running in standalone mode */
  {
   char *extension;

    if ( (xsane.xsane_mode == XSANE_SCAN) && (preferences.overwrite_warning) ) /* test if filename already used */
    {
     FILE *testfile;

      testfile = fopen(preferences.filename, "r");
      if (testfile) /* filename used: skip */
      {
       char buf[256];

        fclose(testfile);
        sprintf(buf, "File %s already exists\n", preferences.filename);
        if (gsg_decision("Warning:", buf, "Overwrite", "Cancel", 1 /* wait */) == FALSE)
        {
          return;
        }
      }
    }


    extension = strrchr(preferences.filename, '.');
    if (extension)
    {
      extension++;
    }

    xsane.xsane_output_format = XSANE_UNKNOWN;

    if (xsane.param.depth <= 8)
    {
      if (extension)
      {
        if ( (!strcasecmp(extension, "pnm")) || (!strcasecmp(extension, "ppm")) ||
             (!strcasecmp(extension, "pgm")) || (!strcasecmp(extension, "pbm")) )
        {
          xsane.xsane_output_format = XSANE_PNM;
        }
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        else if (!strcasecmp(extension, "png"))
        {
          xsane.xsane_output_format = XSANE_PNG;
        }
#endif
#endif
#ifdef HAVE_LIBJPEG
        else if ( (!strcasecmp(extension, "jpg")) || (!strcasecmp(extension, "jpeg")) )
        {
          xsane.xsane_output_format = XSANE_JPEG;
        }
#endif
        else if (!strcasecmp(extension, "ps"))
        {
          xsane.xsane_output_format = XSANE_PS;
        }
#ifdef HAVE_LIBTIFF
        else if ( (!strcasecmp(extension, "tif")) || (!strcasecmp(extension, "tiff")) )
        {
          xsane.xsane_output_format = XSANE_TIFF;
        }
#endif
      }
    }
    else /* depth >8 bpp */
    {
      if (extension)
      {
        if (!strcasecmp(extension, "raw"))
        {
          xsane.xsane_output_format = XSANE_RAW16;
        }
        else if ( (!strcasecmp(extension, "pnm")) || (!strcasecmp(extension, "ppm")) ||
             (!strcasecmp(extension, "pgm")) || (!strcasecmp(extension, "pbm")) )
        {
          xsane.xsane_output_format = XSANE_PNM16;
        }
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        else if (!strcasecmp(extension, "png"))
        {
          xsane.xsane_output_format = XSANE_PNG;
        }
#endif
#endif
      }
    }

    if ( (xsane.xsane_output_format == XSANE_UNKNOWN) && (xsane.xsane_mode == XSANE_SCAN) )
    {
      if (extension)
      {
        snprintf(buf, sizeof(buf), "Unsupported %d-bit output format: %s", xsane.param.depth, extension);
      }
      else
      {
        snprintf(buf, sizeof(buf), "No output format given !!!");
      }

      gsg_error(buf);
      return;
    }
      
    if ( (xsane.xsane_mode == XSANE_COPY) ||
         ((xsane.xsane_mode == XSANE_SCAN)  && (xsane.xsane_output_format != XSANE_PNM)
                                            && (xsane.xsane_output_format != XSANE_RAW16) ) )
    {
     char filename[PATH_MAX];

      gsg_make_path(sizeof(filename), filename, "xsane", "conversion", dialog->dev_name, ".tmp");
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
	   
  if (xsane.param.depth > 1) /* if depth > 1 use gamma correction */
  {
   int size;
   int gamma_gray_size, gamma_red_size, gamma_green_size, gamma_blue_size;
   int gamma_gray_max, gamma_red_max, gamma_green_max, gamma_blue_max;
   const SANE_Option_Descriptor *opt;

    size = (int) pow(2, xsane.param.depth);
    gamma_gray_size  = size;
    gamma_red_size   = size;
    gamma_green_size = size;
    gamma_blue_size  = size;

    size--; 
    gamma_gray_max   = size;
    gamma_red_max    = size;
    gamma_green_max  = size;
    gamma_blue_max   = size;

    if (xsane.scanner_gamma_gray) /* gamma table for gray available */
    {
      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);
      gamma_gray_size = opt->size / sizeof(opt->type);
      gamma_gray_max  = opt->constraint.range->max;
    }

    if (xsane.scanner_gamma_color) /* gamma table for red, green and blue available */
    {
     int gamma_red, gamma_green, gamma_blue;

      /* ok, scanner color gamma function is supported, so we do all conversions about that */
      /* we do not need any gamma tables while scanning, so we can free them after sending */
      /* the data to the scanner */

      /* if also gray gamma function is supported, set this to 1.0 to get the right colors */
      if (xsane.scanner_gamma_gray)
      {
        xsane.gamma_data = malloc(gamma_gray_size  * sizeof(SANE_Int));
        xsane_create_gamma_curve(xsane.gamma_data, 1.0, 0.0, 0.0, gamma_gray_size, gamma_gray_max);
        gsg_update_vector(dialog, dialog->well_known.gamma_vector, xsane.gamma_data);
        free(xsane.gamma_data);
        xsane.gamma_data = 0;
      }

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);
      gamma_red_size = opt->size / sizeof(opt->type);
      gamma_red_max  = opt->constraint.range->max;

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_g);
      gamma_green_size = opt->size / sizeof(opt->type);
      gamma_green_max  = opt->constraint.range->max;

      opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_b);
      gamma_blue_size = opt->size / sizeof(opt->type);
      gamma_blue_max  = opt->constraint.range->max;

      xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
      xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
      xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

      if (xsane.xsane_mode == XSANE_COPY)
      {
        gamma_red   = xsane.gamma * xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_red;
        gamma_green = xsane.gamma * xsane.gamma_green * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_green;
        gamma_blue  = xsane.gamma * xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_blue;
      }
      else
      {
        gamma_red   = xsane.gamma * xsane.gamma_red;
        gamma_green = xsane.gamma * xsane.gamma_green;
        gamma_blue  = xsane.gamma * xsane.gamma_blue;
      }

      xsane_create_gamma_curve(xsane.gamma_data_red,
		 	       gamma_red,
			       xsane.brightness + xsane.brightness_red,
			       xsane.contrast + xsane.contrast_red, gamma_red_size, gamma_red_max);

      xsane_create_gamma_curve(xsane.gamma_data_green,
			       gamma_green,
			       xsane.brightness + xsane.brightness_green,
			       xsane.contrast + xsane.contrast_green, gamma_green_size, gamma_green_max);

      xsane_create_gamma_curve(xsane.gamma_data_blue,
			       gamma_blue,
			       xsane.brightness + xsane.brightness_blue,
			       xsane.contrast + xsane.contrast_blue , gamma_blue_size, gamma_blue_max);

      gsg_update_vector(dialog, dialog->well_known.gamma_vector_r, xsane.gamma_data_red);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_g, xsane.gamma_data_green);
      gsg_update_vector(dialog, dialog->well_known.gamma_vector_b, xsane.gamma_data_blue);

      free(xsane.gamma_data_red);
      free(xsane.gamma_data_green);
      free(xsane.gamma_data_blue);

      xsane.gamma_data_red   = 0;
      xsane.gamma_data_green = 0;
      xsane.gamma_data_blue  = 0;
    }
    else if (xsane.scanner_gamma_gray) /* only scanner gray gamma function available */
    {
     int gamma;
      /* ok, the scanner only supports gray gamma function */
      /* if we are doing a grayscale scan everyting is ok, */
      /* for a color scan the software has to do the gamma correction set by the component slider */

      if (xsane.xsane_mode == XSANE_COPY)
      {
        gamma = xsane.gamma * preferences.printer[preferences.printernr]->gamma;
      }
      else
      {
        gamma = xsane.gamma;
      }

      xsane.gamma_data = malloc(gamma_gray_size * sizeof(SANE_Int));
      xsane_create_gamma_curve(xsane.gamma_data,
                               gamma, xsane.brightness, xsane.contrast,
                               gamma_gray_size, gamma_gray_max);

      gsg_update_vector(dialog, dialog->well_known.gamma_vector, xsane.gamma_data);
      free(xsane.gamma_data);
      xsane.gamma_data = 0;

      if (xsane.xsane_color) /* ok, we are doing a colorscan */
      {
        /* we have to create color gamma table for software conversion */
	/* but we only have to use color slider values, because gray slider value */
	/* is used by scanner gray gamma */

       int gamma_red, gamma_green, gamma_blue;

        xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
        xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
        xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

        if (xsane.xsane_mode == XSANE_COPY)
        {
          gamma_red   = xsane.gamma * xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_red;
          gamma_green = xsane.gamma * xsane.gamma_green * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_green;
          gamma_blue  = xsane.gamma * xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma * preferences.printer[preferences.printernr]->gamma_blue;
        }
        else
        {
          gamma_red   = xsane.gamma * xsane.gamma_red;
          gamma_green = xsane.gamma * xsane.gamma_green;
          gamma_blue  = xsane.gamma * xsane.gamma_blue;
        }
      
        xsane_create_gamma_curve(xsane.gamma_data_red,
                                 gamma_red, xsane.brightness_red, xsane.contrast_red,
                                 gamma_red_size, gamma_red_max);

        xsane_create_gamma_curve(xsane.gamma_data_green,
                                 gamma_green, xsane.brightness_green, xsane.contrast_green,
                                 gamma_green_size, gamma_green_max);

        xsane_create_gamma_curve(xsane.gamma_data_blue,
                                 gamma_blue, xsane.brightness_blue, xsane.contrast_blue,
                                 gamma_blue_size, gamma_blue_max);

        /* gamma tables are freed after scan */
      }

    }
    else /* scanner does not support any gamma correction */
    {
      /* ok, we have to do it on our own */

      if (xsane.xsane_color == 0) /* no color scan */
      {
       int gamma;

        if (xsane.xsane_mode == XSANE_COPY)
        {
          gamma = xsane.gamma * preferences.printer[preferences.printernr]->gamma;
        }
        else
        {
          gamma = xsane.gamma;
        }

        xsane.gamma_data = malloc(gamma_gray_size * sizeof(SANE_Int));
        xsane_create_gamma_curve(xsane.gamma_data,
                                 gamma, xsane.brightness, xsane.contrast,
                                 gamma_gray_size, gamma_gray_max);

        /* gamma table is freed after scan */
      }
      else /* color scan */
      {
       int gamma_red, gamma_green, gamma_blue;
        /* ok, we have to combin gray and color slider values */

        xsane.gamma_data_red   = malloc(gamma_red_size   * sizeof(SANE_Int));
        xsane.gamma_data_green = malloc(gamma_green_size * sizeof(SANE_Int));
        xsane.gamma_data_blue  = malloc(gamma_blue_size  * sizeof(SANE_Int));

        if (xsane.xsane_mode == XSANE_COPY)
        {
          gamma_red   = xsane.gamma * xsane.gamma_red   * preferences.printer[preferences.printernr]->gamma;
          gamma_green = xsane.gamma * xsane.gamma_green * preferences.printer[preferences.printernr]->gamma;
          gamma_blue  = xsane.gamma * xsane.gamma_blue  * preferences.printer[preferences.printernr]->gamma;
        }
        else
        {
          gamma_red   = xsane.gamma * xsane.gamma_red;
          gamma_green = xsane.gamma * xsane.gamma_green;
          gamma_blue  = xsane.gamma * xsane.gamma_blue;
        }

        xsane_create_gamma_curve(xsane.gamma_data_red,
                                 gamma_red,
                                 xsane.brightness + xsane.brightness_red,
                                 xsane.contrast + xsane.contrast_red, gamma_red_size, gamma_red_max);

        xsane_create_gamma_curve(xsane.gamma_data_green,
                                 gamma_green,
                                 xsane.brightness + xsane.brightness_green,
                                 xsane.contrast + xsane.contrast_green, gamma_green_size, gamma_green_max);

        xsane_create_gamma_curve(xsane.gamma_data_blue,
                                 gamma_blue,
                                 xsane.brightness + xsane.brightness_blue,
                                 xsane.contrast + xsane.contrast_blue , gamma_blue_size, gamma_blue_max);

        /* gamma tables are freed after scan */
      }

    }
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_start_scan();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0

static void xsane_zoom_in_preview(GtkWidget * widget, gpointer data)
{
  if (Selection.x1 >= Selection.x2 || Selection.y1 >= Selection.y2 || !Selection.active)
    return;

  Selection.active = FALSE;
  draw_selection(TRUE);

  gtk_ruler_set_range(GTK_RULER(xsane.hruler), Selection.x1, Selection.x2, 0, 20);
  gtk_ruler_set_range(GTK_RULER(xsane.vruler), Selection.y1, Selection.y2, 0, 20);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_zoom_out_preview(GtkWidget * widget, gpointer data)
{
  gtk_ruler_set_range(GTK_RULER(xsane.hruler), 0, Preview.PhysWidth, 0, 20);
  gtk_ruler_set_range(GTK_RULER(xsane.vruler), 0, Preview.PhysHeight, 0, 20);
}

#endif /* 0 */

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_files_exit_callback(GtkWidget *widget, gpointer data)
{
  xsane_quit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_files_build_menu(void)
{
  GtkWidget *menu, *item;

  menu = gtk_menu_new();

  item = gtk_menu_item_new();
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_widget_show(item);


  /* XSane about dialog */

  item = gtk_menu_item_new_with_label("About");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_about_dialog, 0);
  gtk_widget_show(item);


  /* XSane info dialog */

  item = gtk_menu_item_new_with_label("Info");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_info_dialog, 0);
  gtk_widget_show(item);

  
  /* Exit */

  item = gtk_menu_item_new_with_label("Exit");
  gtk_container_add(GTK_CONTAINER(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_files_exit_callback, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_set_unit_callback(GtkWidget *widget, gpointer data)
{
  const char *unit = data;
  double unit_conversion_factor = 1.0;

  if (strcmp(unit, "cm") == 0)
  {
    unit_conversion_factor = 10.0;
  }
  else if (strcmp(unit, "in") == 0)
  {
    unit_conversion_factor = 25.4;
  }

  preferences.length_unit = unit_conversion_factor;

  xsane_refresh_dialog(dialog);
  if (xsane.preview)
  {
/*    preview_update(xsane.preview); */
    preview_area_resize(xsane.preview->window);
  }

  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_int(GtkWidget *widget, int *val)
{
  char *start, *end;
  int v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = (int) strtol(start, &end, 10);
  if (end > start && v > 0)
  {
    *val = v;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_bool(GtkWidget *widget, int *val)
{
  *val = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_scale(GtkWidget *widget, double *val)
{
  *val = GTK_ADJUSTMENT(widget)->value;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_update_double(GtkWidget *widget, double *val)
{
  char *start, *end;
  double v;

  start = gtk_entry_get_text(GTK_ENTRY(widget));
  if (!start)
    return;

  v = strtod(start, &end);
  if (end > start && v > 0.0)
  {
    *val = v;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_update()
{
 char buf[256];
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_name_entry),    (char *) preferences.printer[preferences.printernr]->name);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_command_entry), (char *) preferences.printer[preferences.printernr]->command);

  sprintf(buf, "%d", preferences.printer[preferences.printernr]->resolution);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_resolution_entry), buf);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_width_entry), buf);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_height_entry), buf);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_leftoffset_entry), buf);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_bottomoffset_entry), buf);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_entry), buf);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_red_entry), buf);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_green_entry), buf);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(xsane_setup.printer_gamma_blue_entry), buf);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_callback(GtkWidget *widget, gpointer data)
{
  preferences.printernr = (int) data;
  xsane_setup_printer_update();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_menu_build(GtkWidget *option_menu)
{
 GtkWidget *printer_menu, *printer_item;
 int i;

  printer_menu = gtk_menu_new();

  for (i=0; i < preferences.printerdefinitions; i++)
  {
    printer_item = gtk_menu_item_new_with_label(preferences.printer[i]->name);
    gtk_container_add(GTK_CONTAINER(printer_menu), printer_item);
    gtk_signal_connect(GTK_OBJECT(printer_item), "activate", (GtkSignalFunc) xsane_setup_printer_callback, (void *) i);
    gtk_widget_show(printer_item);
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), printer_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), preferences.printernr);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_apply_changes(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;

  if (preferences.printer[preferences.printernr]->name)
  {
    free((void *) preferences.printer[preferences.printernr]->name);
  }
  preferences.printer[preferences.printernr]->name = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_name_entry)));

  if (preferences.printer[preferences.printernr]->command)
  {
    free((void *) preferences.printer[preferences.printernr]->command);
  }
  preferences.printer[preferences.printernr]->command = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.printer_command_entry)));

  xsane_update_int(xsane_setup.printer_resolution_entry,   &preferences.printer[preferences.printernr]->resolution);
  xsane_update_int(xsane_setup.printer_width_entry,        &preferences.printer[preferences.printernr]->width);
  xsane_update_int(xsane_setup.printer_height_entry,       &preferences.printer[preferences.printernr]->height);
  xsane_update_int(xsane_setup.printer_leftoffset_entry,   &preferences.printer[preferences.printernr]->leftoffset);
  xsane_update_int(xsane_setup.printer_bottomoffset_entry, &preferences.printer[preferences.printernr]->bottomoffset);

  xsane_update_double(xsane_setup.printer_gamma_entry,       &preferences.printer[preferences.printernr]->gamma);
  xsane_update_double(xsane_setup.printer_gamma_red_entry,   &preferences.printer[preferences.printernr]->gamma_red);
  xsane_update_double(xsane_setup.printer_gamma_green_entry, &preferences.printer[preferences.printernr]->gamma_green);
  xsane_update_double(xsane_setup.printer_gamma_blue_entry,  &preferences.printer[preferences.printernr]->gamma_blue);

  if (option_menu)
  {
    xsane_setup_printer_menu_build(option_menu);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_new(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;

  xsane_new_printer();
  xsane_setup_printer_update();

  xsane_setup_printer_menu_build(option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_printer_delete(GtkWidget *widget, gpointer data)
{
 GtkWidget *option_menu = (GtkWidget *) data;
 int i;

  preferences.printerdefinitions--;

  i = preferences.printernr;
  while (i < preferences.printerdefinitions)
  {
    memcpy(preferences.printer[i], preferences.printer[i+1], sizeof(Preferences_printer_t));
    i++;
  }

  if (preferences.printernr >= preferences.printerdefinitions)
  {
    preferences.printernr--;
  } 

  if (preferences.printerdefinitions == 0)
  {
    xsane_new_printer();
    preferences.printernr = 0;
  }

  xsane_setup_printer_update();

  xsane_setup_printer_menu_build(option_menu);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_preview_apply_changes(GtkWidget *widget, gpointer data)
{
  xsane_update_double(xsane_setup.preview_gamma_entry,            &preferences.preview_gamma);
  xsane_update_double(xsane_setup.preview_gamma_red_entry,        &preferences.preview_gamma_red);
  xsane_update_double(xsane_setup.preview_gamma_green_entry,      &preferences.preview_gamma_green);
  xsane_update_double(xsane_setup.preview_gamma_blue_entry,       &preferences.preview_gamma_blue);
  xsane_update_bool(xsane_setup.preview_preserve_button,          &preferences.preserve_preview);
  xsane_update_bool(xsane_setup.preview_own_cmap_button,          &preferences.preview_own_cmap);
  xsane_update_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_saving_apply_changes(GtkWidget *widget, gpointer data)
{
#ifdef HAVE_LIBJPEG
  xsane_update_scale(xsane_setup.jpeg_image_quality_scale, &preferences.jpeg_quality);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_update_scale(xsane_setup.pnm_image_compression_scale, &preferences.png_compression);
#endif
#endif

  xsane_update_bool(xsane_setup.overwrite_warning_button,         &preferences.overwrite_warning);
  xsane_update_bool(xsane_setup.increase_filename_counter_button, &preferences.increase_filename_counter);
  xsane_update_bool(xsane_setup.skip_existing_numbers_button,     &preferences.skip_existing_numbers);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_fax_apply_changes(GtkWidget *widget, gpointer data)
{
  if (preferences.fax_command)
  {
    free((void *) preferences.fax_command);
  }
  preferences.fax_command = strdup(gtk_entry_get_text(GTK_ENTRY(xsane_setup.fax_command_entry)));
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_options_ok_callback(GtkWidget *widget, gpointer data)
{
  xsane_setup_printer_apply_changes(0, 0);
  xsane_setup_preview_apply_changes(0, 0);
  xsane_setup_saving_apply_changes(0, 0);
  xsane_setup_fax_apply_changes(0, 0);


  gtk_widget_destroy((GtkWidget *)data);
  xsane_pref_save();

//  xsane_update_gamma();
  gsg_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_close_dialog_callback(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = data;

  gtk_widget_destroy(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_close_info_callback(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = data;

  gtk_widget_destroy(dialog);

  gtk_widget_set_sensitive(xsane.shell, TRUE);
  gtk_widget_set_sensitive(xsane.standard_options_shell, TRUE);
  gtk_widget_set_sensitive(xsane.advanced_options_shell, TRUE);;
  gtk_widget_set_sensitive(xsane.histogram_dialog, TRUE);

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn)
{
 GtkWidget *hbox, *label;

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_table_attach_defaults(GTK_TABLE(table), hbox, row, row+1, colomn, colomn+1);
  gtk_widget_show(hbox);

  label = gtk_label_new(text);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);
  gtk_widget_show(label);

 return label;
}
/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
static GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text)
{
 GtkWidget *hbox, *label;

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  label = gtk_label_new(text);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);
  gtk_widget_show(label);

 return label;
}
#endif
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_info_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *info_dialog, *vbox, *button, *label, *frame, *framebox, *hbox, *table;
 char buf[256];
 char *bufptr;

  sane_get_parameters(dialog->dev, &xsane.param); /* update xsane.param */

  info_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_position(GTK_WINDOW(info_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(info_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(info_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_info_callback), info_dialog);
  sprintf(buf, "%s info", prog_name);
  gtk_window_set_title(GTK_WINDOW(info_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_container_add(GTK_CONTAINER(info_dialog), vbox);
  gtk_widget_show(vbox);

  frame = gtk_frame_new("Scanner and backend:");
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(6, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  sprintf(buf, "Vendor:");
  label = xsane_info_table_text_new(table, buf, 0, 0);
  sprintf(buf, "%s", devlist[seldev]->vendor);
  label = xsane_info_table_text_new(table, buf, 1, 0);

  sprintf(buf, "Model:");
  label = xsane_info_table_text_new(table, buf, 0, 1);
  sprintf(buf, "%s", devlist[seldev]->model);
  label = xsane_info_table_text_new(table, buf, 1, 1);

  sprintf(buf, "Type:");
  label = xsane_info_table_text_new(table, buf, 0, 2);
  sprintf(buf, "%s", devlist[seldev]->type);
  label = xsane_info_table_text_new(table, buf, 1, 2);

  sprintf(buf, "Device:");
  label = xsane_info_table_text_new(table, buf, 0, 3);
  bufptr = strrchr(devlist[seldev]->name, ':');
  if (bufptr)
  {
    sprintf(buf, "%s", bufptr+1);
  }
  else
  {
    sprintf(buf, devlist[seldev]->name);
  }
  label = xsane_info_table_text_new(table, buf, 1, 3);

  sprintf(buf, "%s", devlist[seldev]->name);
  bufptr = strrchr(buf, ':');
  if (bufptr)
  {
    *bufptr = 0; 
    label = xsane_info_table_text_new(table, buf, 1, 4);
    sprintf(buf, "Loaded backend:");
    label = xsane_info_table_text_new(table, buf, 0, 4);
  }

  sprintf(buf, "Sane version:");
  label = xsane_info_table_text_new(table, buf, 0, 5);
  sprintf(buf, "%d.%d build %d",SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                                SANE_VERSION_MINOR(xsane.sane_backend_versioncode),
                                SANE_VERSION_BUILD(xsane.sane_backend_versioncode));
  label = xsane_info_table_text_new(table, buf, 1, 5);


  frame = gtk_frame_new("Recent values:");
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(4, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  if ((xsane.xsane_color) && (xsane.scanner_gamma_color))
  {
   const SANE_Option_Descriptor *opt;

    sprintf(buf,"Gamma correction by:");
    label = xsane_info_table_text_new(table, buf, 0, 0);
    sprintf(buf, "scanner");
    label = xsane_info_table_text_new(table, buf, 1, 0);

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector_r);

    sprintf(buf,"Gamma input depth:");
    label = xsane_info_table_text_new(table, buf, 0, 1);
    sprintf(buf,"%d bit", (int) (0.5 + log((double)opt->size / sizeof(opt->type)) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 1);

    sprintf(buf, "Gamma output depth:");
    label = xsane_info_table_text_new(table, buf, 0, 2);
    sprintf(buf, "%d bit", (int) (0.5 + log(opt->constraint.range->max+1.0) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else if ((!xsane.xsane_color) && (xsane.scanner_gamma_gray))
  {
   const SANE_Option_Descriptor *opt;

    sprintf(buf,"Gamma correction by:");
    label = xsane_info_table_text_new(table, buf, 0, 0);
    sprintf(buf, "scanner");
    label = xsane_info_table_text_new(table, buf, 1, 0);
    
    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.gamma_vector);

    sprintf(buf,"Gamma input depth:");
    label = xsane_info_table_text_new(table, buf, 0, 1);
    sprintf(buf,"%d bit", (int) (0.5 + log((double)opt->size / sizeof(opt->type)) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 1);

    sprintf(buf, "Gamma output depth:");
    label = xsane_info_table_text_new(table, buf, 0, 2);
    sprintf(buf, "%d bit", (int) (0.5 + log(opt->constraint.range->max+1.0) / log(2.0)));
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }
  else
  {
    sprintf(buf,"Gamma correction by:");
    label = xsane_info_table_text_new(table, buf, 0, 0);
    sprintf(buf, "software (xsane)");
    label = xsane_info_table_text_new(table, buf, 1, 0);

    sprintf(buf,"Gamma input depth:");
    label = xsane_info_table_text_new(table, buf, 0, 1);
    sprintf(buf, "%d bit", xsane.param.depth);
    label = xsane_info_table_text_new(table, buf, 1, 1);

    sprintf(buf, "Gamma output depth:");
    label = xsane_info_table_text_new(table, buf, 0, 2);
/*    sprintf(buf, "%d bit", 8); */
    sprintf(buf, "%d bit", xsane.param.depth); 
    label = xsane_info_table_text_new(table, buf, 1, 2);
  }

  sprintf(buf,"Scanner output depth:");
  label = xsane_info_table_text_new(table, buf, 0, 3);
  sprintf(buf, "%d bit", xsane.param.depth);
  label = xsane_info_table_text_new(table, buf, 1, 3);

  frame = gtk_frame_new("Xsane output formats:");
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show(frame);

  framebox = gtk_vbox_new(/* not homogeneous */ FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), framebox);
  gtk_widget_show(framebox);

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(framebox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);

  table = gtk_table_new(2, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
  gtk_widget_show(table);

  sprintf(buf, "8 bit output formats: ");
  label = xsane_info_table_text_new(table, buf, 0, 0);

  bufptr=buf;

#ifdef HAVE_LIBJPEG
  sprintf(bufptr, "JPEG, ");
  bufptr += strlen(bufptr);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  sprintf(bufptr, "PNG, ");
  bufptr += strlen(bufptr);
#endif
#endif

  sprintf(bufptr, "PNM, ");
  bufptr += strlen(bufptr);

  sprintf(bufptr, "PS, ");
  bufptr += strlen(bufptr);

#ifdef HAVE_LIBTIFF
  sprintf(bufptr, "TIFF, ");
  bufptr += strlen(bufptr);
#endif

  bufptr--;
  bufptr--;
  *bufptr = 0; /* erase last comma */

  label = xsane_info_table_text_new(table, buf, 1, 0);

  sprintf(buf, "16 bit output formats: ");
  label = xsane_info_table_text_new(table, buf, 0, 1);

  bufptr=buf;

  sprintf(bufptr, "experimental: ");
  bufptr += strlen(bufptr);

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  sprintf(bufptr, "PNG, ");
  bufptr += strlen(bufptr);
#endif
#endif

  sprintf(bufptr, "PNM, ");
  bufptr += strlen(bufptr);

  sprintf(bufptr, "RAW, ");
  bufptr += strlen(bufptr);

  bufptr--;
  bufptr--;
  *bufptr = 0; /* erase last comma */

  label = xsane_info_table_text_new(table, buf, 1, 1);

/*  gtk_label_set((GtkLabel *)label, "HALLO"); */

  button = gtk_button_new_with_label("Close");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_info_callback, info_dialog);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(info_dialog);

  gtk_widget_set_sensitive(xsane.shell, FALSE);
  gtk_widget_set_sensitive(xsane.standard_options_shell, FALSE);
  gtk_widget_set_sensitive(xsane.advanced_options_shell, FALSE);;
  gtk_widget_set_sensitive(xsane.histogram_dialog, FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_about_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *about_dialog, *vbox, *button, *label;
 char buf[256];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;


  about_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_position(GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(about_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(about_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_close_info_callback), about_dialog);
  sprintf(buf, "About %s", prog_name);
  gtk_window_set_title(GTK_WINDOW(about_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 5);
  gtk_container_add(GTK_CONTAINER(about_dialog), vbox);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(about_dialog);

  style = gtk_widget_get_style(about_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  sprintf(buf, "%s/xsane-logo.xpm", STRINGIFY(PATH_SANE_DATA_DIR));  
  pixmap = gdk_pixmap_create_from_xpm(about_dialog->window, &mask, bg_trans, buf);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);

  xsane_separator_new(vbox, 5);

  sprintf(buf,"XSane version: %s\n", XSANE_VERSION);
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  sprintf(buf,"1998-1999 by Oliver Rauch\n");
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  sprintf(buf,"Email: Oliver.Rauch@Wolfsburg.DE\n");
  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  button = gtk_button_new_with_label("Close");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_info_callback, about_dialog);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_widget_show(about_dialog);

  gtk_widget_set_sensitive(xsane.shell, FALSE);
  gtk_widget_set_sensitive(xsane.standard_options_shell, FALSE);
  gtk_widget_set_sensitive(xsane.advanced_options_shell, FALSE);;
  gtk_widget_set_sensitive(xsane.histogram_dialog, FALSE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_get_area_value(int option, float *val, SANE_Int *unit)
{
 const SANE_Option_Descriptor *opt;
 SANE_Handle dev;
 SANE_Word word;

  if (option <= 0)
  {
    return;
  }

  if (sane_control_option(dialog->dev, option, SANE_ACTION_GET_VALUE, &word, 0) == SANE_STATUS_GOOD)
  {
    dev = dialog->dev;
    opt = sane_get_option_descriptor (dev, option);

    if (unit)
    {
      *unit = opt->unit;
    }

    if (val)
    {
      if (opt->type == SANE_TYPE_FIXED)
      {
        *val = (float) word / 65536.0;
      }
      else
      {
        *val = (float) word;
      }
    }
  }
  else if (val)
  {
    *val = 0;
  }

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_delete_callback(GtkWidget *widget, gpointer list)
{
  gtk_list_remove_items(GTK_LIST(list), GTK_LIST(list)->selection);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_batch_scan_add_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 float tlx, tly, brx, bry;
 SANE_Int unit;
 char buf[255];
 

  xsane_get_area_value(dialog->well_known.coord[0], &tlx, &unit);
  xsane_get_area_value(dialog->well_known.coord[1], &tly, &unit);
  xsane_get_area_value(dialog->well_known.coord[2], &brx, &unit);
  xsane_get_area_value(dialog->well_known.coord[3], &bry, &unit);

  if (unit == SANE_UNIT_MM)
  {
    sprintf(buf, " top left (%7.2fmm, %7.2fmm), bottom right (%7.2fmm, %7.2fmm)", tlx, tly, brx, bry);
  }
  else
  {
    sprintf(buf, " top left (%5.0fpx, %5.0fpx), bottom right (%5.0fpx, %5.0fpx)", tlx, tly, brx, bry);
  }

  list_item = gtk_list_item_new_with_label(buf);
  gtk_container_add(GTK_CONTAINER(list), list_item);
  gtk_widget_show(list_item);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_TEST
static void xsane_batch_scan_dialog(GtkWidget *widget, gpointer data)
{
  GtkWidget *batch_scan_dialog, *batch_scan_vbox, *hbox, *button, *scrolled_window, *list;
  char buf[64];

  batch_scan_dialog = gtk_dialog_new();
  sprintf(buf, "%s batch scan", prog_name);
  gtk_window_set_title(GTK_WINDOW(batch_scan_dialog), buf);

  batch_scan_vbox = GTK_DIALOG(batch_scan_dialog)->vbox;

  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_usize(scrolled_window, 400, 200);
  gtk_container_add(GTK_CONTAINER(batch_scan_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
  gtk_container_add(GTK_CONTAINER(scrolled_window), list);
  gtk_widget_show(list);


  /* fill in action area: */
  hbox = GTK_DIALOG(batch_scan_dialog)->action_area;

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_dialog_callback, batch_scan_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Add area");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_add_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Delete");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_batch_scan_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(batch_scan_dialog);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_scan_delete_callback(GtkWidget *widget, gpointer list)
{
  gtk_list_remove_items(GTK_LIST(list), GTK_LIST(list)->selection);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_scan_dialog()
{
 GtkWidget *fax_dialog, *fax_scan_vbox, *hbox, *button, *scrolled_window, *list;
 char buf[64];

  fax_dialog = gtk_dialog_new();
  sprintf(buf, "%s fax page list", prog_name);
  gtk_window_set_title(GTK_WINDOW(fax_dialog), buf);

  fax_scan_vbox = GTK_DIALOG(fax_dialog)->vbox;

  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_usize(scrolled_window, 400, 200);
  gtk_container_add(GTK_CONTAINER(fax_scan_vbox), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
  gtk_container_add(GTK_CONTAINER(scrolled_window), list);
  gtk_widget_show(list);


  /* fill in action area: */
  hbox = GTK_DIALOG(fax_dialog)->action_area;

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_dialog_callback, fax_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Delete");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_scan_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(fax_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_setup_dialog(GtkWidget *widget, gpointer data)
{
 GtkWidget *setup_dialog, *setup_vbox, *vbox, *hbox, *button, *label, *text, *frame, *notebook;
 GtkWidget *printer_option_menu;
 char buf[64];

  setup_dialog = gtk_dialog_new();
  sprintf(buf, "%s setup", prog_name);
  gtk_window_set_title(GTK_WINDOW(setup_dialog), buf);

  setup_vbox = GTK_DIALOG(setup_dialog)->vbox;

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(setup_vbox), notebook, FALSE, FALSE, 0);
  gtk_widget_show(notebook);




  /* Printer options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Printer options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);



  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new("Printer selection:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  printer_option_menu = gtk_option_menu_new();
  gsg_set_tooltip(dialog->tooltips, printer_option_menu, DESC_PRINTER_SETUP);
  gtk_box_pack_end(GTK_BOX(hbox), printer_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(printer_option_menu);
  gtk_widget_show(hbox);

  xsane_setup_printer_menu_build(printer_option_menu);

  /* printername : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Name:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_NAME);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->name);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_name_entry = text;

  /* printcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Command:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.printer[preferences.printernr]->command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_command_entry = text;

  /* printerresolution : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Resolution (dpi):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_RESOLUTION);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->resolution);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_resolution_entry = text;


  xsane_separator_new(vbox, 2);


  /* printer width: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Width (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_WIDTH);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->width);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_width_entry = text;

  /* printer height: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Height (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_HEIGHT);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->height);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_height_entry = text;

  /* printer left offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Left offset (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_LEFTOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->leftoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_leftoffset_entry = text;

  /* printer bottom offset : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Bottom offset (1/72 inch):");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_BOTTOMOFFSET);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%d", preferences.printer[preferences.printernr]->bottomoffset);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_bottomoffset_entry = text;


  xsane_separator_new(vbox, 2);


  /* printer gamma: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma value:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_entry = text;

  /* printer gamma red: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma red:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_red);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_red_entry = text;

  /* printer gamma green: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma green:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_green);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_green_entry = text;

  /* printer gamma blue: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Printer gamma blue:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PRINTER_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  sprintf(buf, "%1.3g", preferences.printer[preferences.printernr]->gamma_blue);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.printer_gamma_blue_entry = text;


  xsane_separator_new(vbox, 4);

  /* "apply" "add printer" "delete printer" */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_apply_changes, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Add printer");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_new, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Delete printer");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_printer_delete, printer_option_menu);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* Saving options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Saving options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* overwrite warning */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Overwrite warning");
  gsg_set_tooltip(dialog->tooltips, button, DESC_OVERWRITE_WARNING);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.overwrite_warning);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.overwrite_warning_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Increase filename counter");
  gsg_set_tooltip(dialog->tooltips, button, DESC_INCREASE_COUNTER);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.increase_filename_counter);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.increase_filename_counter_button = button;

  /* increase filename counter */
  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Skip existing numbers");
  gsg_set_tooltip(dialog->tooltips, button, DESC_SKIP_EXISTING);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.skip_existing_numbers);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.skip_existing_numbers_button = button;


#ifdef HAVE_LIBJPEG
//  xsane_scale_new(GTK_BOX(vbox), "JPEG image quality", DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 1.0, 0.0, 0,
//                  &preferences.jpeg_quality, &scale, xsane_quality_changed);
  xsane_scale_new(GTK_BOX(vbox), "JPEG image quality", DESC_JPEG_QUALITY, 0.0, 100.0, 1.0, 1.0, 0.0, 0,
                  &preferences.jpeg_quality, (GtkObject **) &xsane_setup.jpeg_image_quality_scale, 0);
#endif

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_scale_new(GTK_BOX(vbox), "PNG image compression", DESC_PNG_COMPRESSION, 0.0, Z_BEST_COMPRESSION, 1.0, 1.0, 0.0, 0,
                  &preferences.png_compression, (GtkObject **) &xsane_setup.pnm_image_compression_scale, 0);
#endif
#endif

  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_saving_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);





  /* Preview options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Preview options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* preserve preview image: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Preserve preview image");
  gsg_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_PRESERVE);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preserve_preview);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_preserve_button = button;


  /* private colormap: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  button = gtk_check_button_new_with_label("Use private colormap");
  gsg_set_tooltip(dialog->tooltips, button, DESC_PREVIEW_COLORMAP);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), preferences.preview_own_cmap);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
  gtk_widget_show(button);
  gtk_widget_show(hbox);
  xsane_setup.preview_own_cmap_button = button;


  xsane_separator_new(vbox, 2);


  /* preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  sprintf(buf, "%1.3g", preferences.preview_gamma);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_entry = text;

  /* red preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma red:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  sprintf(buf, "%1.3g", preferences.preview_gamma_red);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_RED);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_red_entry = text;

  /* green preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma green:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  sprintf(buf, "%1.3g", preferences.preview_gamma_green);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_GREEN);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_green_entry = text;

  /* blue preview gamma correction value: */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label = gtk_label_new("Preview gamma blue:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  sprintf(buf, "%1.3g", preferences.preview_gamma_blue);
  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_PREVIEW_GAMMA_BLUE);
  gtk_widget_set_usize(text, 50, 0);
  gtk_entry_set_text(GTK_ENTRY(text), buf);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  xsane_setup.preview_gamma_blue_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_preview_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* Fax options notebook page */

  setup_vbox =  gtk_vbox_new(FALSE, 5);

  label = gtk_label_new("Fax options");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), setup_vbox, label);
  gtk_widget_show(setup_vbox);

  frame = gtk_frame_new(0);
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(setup_vbox), frame, TRUE, TRUE, 0); /* sizeable framehight */
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  /* faxcommand : */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label = gtk_label_new("Command:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  text = gtk_entry_new();
  gsg_set_tooltip(dialog->tooltips, text, DESC_FAX_COMMAND);
  gtk_widget_set_usize(text, 250, 0);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_command);
  gtk_box_pack_end(GTK_BOX(hbox), text, FALSE, FALSE, 2);
  gtk_widget_show(text);
  gtk_widget_show(hbox);
  xsane_setup.fax_command_entry = text;


  xsane_separator_new(vbox, 4);


  /* apply button */

  hbox = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = gtk_button_new_with_label("Apply");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_fax_apply_changes, 0);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(hbox);




  /* fill in action area: */
  hbox = GTK_DIALOG(setup_dialog)->action_area;

  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_setup_options_ok_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_dialog_callback, setup_dialog);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(setup_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_device_save(GtkWidget *widget, gpointer data)
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

static void xsane_pref_device_restore(void)
{
  char filename[PATH_MAX];
  int fd;

  gsg_make_path(sizeof(filename), filename, "xsane", 0, dialog->dev_name, ".rc");
  fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    return;
  }
  sanei_load_values(fd, dialog->dev);
  close(fd);

  if (dialog->well_known.dpi > 0)
  {
   const SANE_Option_Descriptor *opt;

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
  
  switch (opt->type)
  {
    case SANE_TYPE_INT:
    {
     SANE_Int dpi;
      sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
      xsane.resolution = dpi;
    }
    break;

    case SANE_TYPE_FIXED:
    {
     SANE_Fixed dpi;
      sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
      xsane.resolution = (int) SANE_UNFIX(dpi);
    }
    break;

    default:
     fprintf(stderr, "xsane_pref_device_restore: unknown type %d\n", opt->type);
    return;
  }
  }


  xsane_refresh_dialog(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_pref_toggle_tooltips(GtkWidget *widget, gpointer data)
{
  preferences.tooltips_enabled = (GTK_CHECK_MENU_ITEM(widget)->active != 0);
  gsg_set_tooltips(dialog, preferences.tooltips_enabled);
  xsane_pref_save();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_show_doc(GtkWidget *widget, gpointer data)
{
 char *name = (char *) data;
 char buf[256];
 FILE *ns_pipe;

  sprintf(buf, "netscape -no-about-splash -remote \"openFile(%s/sane-%s-doc.html)\" 2>&1",
               STRINGIFY(PATH_SANE_DATA_DIR), name);  
  ns_pipe = popen(buf, "r");

  if (ns_pipe)
  {
    while (!feof(ns_pipe))
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
      fgetc(ns_pipe); /* remove char from pipe */
    }

    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }

    if (pclose(ns_pipe))
    {
      sprintf(buf,"Ensure netscape is running!");
      gsg_error(buf);
    }
  }
  else
  {
    sprintf(buf,"Failed to execute netscape!");
    gsg_error(buf);
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_pref_build_menu(void)
{
  GtkWidget *menu, *item, *submenu, *subitem;

  menu = gtk_menu_new();


  /* XSane setup dialog */

  item = gtk_menu_item_new_with_label("Setup");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_setup_dialog, 0);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


#ifdef XSANE_TEST
  /* XSane batch scan dialog */

  item = gtk_menu_item_new_with_label("Batch scan");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_batch_scan_dialog, 0);
  gtk_widget_show(item);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
#endif



  /* show tooltips */

  item = gtk_check_menu_item_new_with_label("Show tooltips");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(item), preferences.tooltips_enabled);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", (GtkSignalFunc) xsane_pref_toggle_tooltips, 0);


  /* show histogram */

  xsane.show_histogram_widget = gtk_check_menu_item_new_with_label("Show histogram");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  gtk_menu_append(GTK_MENU(menu), xsane.show_histogram_widget);
  gtk_widget_show(xsane.show_histogram_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_histogram_widget), "toggled", (GtkSignalFunc) xsane_show_histogram_callback, 0);

  
  /* show standard options */

  xsane.show_standard_options_widget = gtk_check_menu_item_new_with_label("Show standard options");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_standard_options_widget), preferences.show_standard_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_standard_options_widget);
  gtk_widget_show(xsane.show_standard_options_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_standard_options_widget), "toggled", (GtkSignalFunc) xsane_show_standard_options_callback, 0);


  /* show advanced options */

  xsane.show_advanced_options_widget = gtk_check_menu_item_new_with_label("Show advanced options");
  gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(xsane.show_advanced_options_widget), preferences.show_advanced_options);
  gtk_menu_append(GTK_MENU(menu), xsane.show_advanced_options_widget);
  gtk_widget_show(xsane.show_advanced_options_widget);
  gtk_signal_connect(GTK_OBJECT(xsane.show_advanced_options_widget), "toggled", (GtkSignalFunc) xsane_show_advanced_options_callback, 0);

  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);


  /* length unit */

  item = gtk_menu_item_new_with_label("Length unit");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  submenu = gtk_menu_new();

  subitem = gtk_menu_item_new_with_label("millimeters");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_pref_set_unit_callback, "mm");
  gtk_widget_show(subitem);

  subitem = gtk_menu_item_new_with_label("centimeters");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_pref_set_unit_callback, "cm");
  gtk_widget_show(subitem);

  subitem = gtk_menu_item_new_with_label("inches");
  gtk_menu_append(GTK_MENU(submenu), subitem);
  gtk_signal_connect(GTK_OBJECT(subitem), "activate", (GtkSignalFunc) xsane_pref_set_unit_callback, "in");
  gtk_widget_show(subitem);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  xsane.length_unit_widget = item;


  /* insert separator: */

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);

  /* Save device setting */

  item = gtk_menu_item_new_with_label("Save device settings");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_pref_device_save, 0);
  gtk_widget_show(item);

  /* Restore device setting */

  item = gtk_menu_item_new_with_label("Restore device settings");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_pref_device_restore, 0);
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *xsane_help_build_menu(void)
{
  GtkWidget *menu, *item;
  char buf[256];
  char *bufptr;

  menu = gtk_menu_new();

  sprintf(buf, "%s", devlist[seldev]->name);
  bufptr = strrchr(buf, ':');
  if (bufptr)
  {
    *bufptr = 0; 
    bufptr = strrchr(buf, ':');
    if (bufptr)
    {
      bufptr++;
    }
    else
    {
      bufptr = buf;
    }

    xsane.backend = strdup(bufptr);

    item = gtk_menu_item_new_with_label("Show backend doc");
    gtk_menu_append(GTK_MENU(menu), item);
    gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) xsane.backend);
    gtk_widget_show(item);
  }

  item = gtk_menu_item_new_with_label("Show xsane doc");
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_show_doc, (void *) "xsane");
  gtk_widget_show(item);

  return menu;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void panel_build(GSGDialog *dialog)
{
  GtkWidget *xsane_hbox;
  GtkWidget *standard_hbox, *standard_vbox;
  GtkWidget *advanced_hbox, *advanced_vbox;
  GtkWidget *parent, *vbox, *button, *label;
  const SANE_Option_Descriptor *opt;
  SANE_Handle dev = dialog->dev;
  double dval, dmin, dmax, dquant;
  char *buf, str[16], title[256];
  GSGDialogElement *elem;
  SANE_Word quant, val;
  SANE_Status status;
  SANE_Int num_words;
  char **str_list;
  int i, j;
  int num_vector_opts = 0, *vector_opts;  xsane_hbox = NULL;

  /* reset well-known options: */
  dialog->well_known.scanmode        = -1;
  dialog->well_known.scansource      = -1;
  dialog->well_known.preview         = -1;
  dialog->well_known.dpi             = -1;
  dialog->well_known.coord[GSG_TL_X] = -1;
  dialog->well_known.coord[GSG_TL_Y] = -1;
  dialog->well_known.coord[GSG_BR_X] = -1;
  dialog->well_known.coord[GSG_BR_Y] = -1;
  dialog->well_known.gamma_vector    = -1;
  dialog->well_known.gamma_vector_r  = -1;
  dialog->well_known.gamma_vector_g  = -1;
  dialog->well_known.gamma_vector_b  = -1;
  dialog->well_known.bit_depth       = -1;


  /* standard options */
  standard_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(standard_hbox);
  standard_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(standard_vbox);
/*  gtk_box_pack_start(GTK_BOX(standard_hbox), standard_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(standard_hbox), standard_vbox, TRUE, TRUE, 0); /* make frame sizeable */

  /* advanced options */
  advanced_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(advanced_hbox);
  advanced_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_widget_show(advanced_vbox);
/*   gtk_box_pack_start(GTK_BOX(advanced_hbox), advanced_vbox, FALSE, FALSE, 0); */ /* make frame fixed */
  gtk_box_pack_start(GTK_BOX(advanced_hbox), advanced_vbox, TRUE, TRUE, 0); /* make frame sizeable */


  vector_opts = alloca(dialog->num_elements * sizeof (int));

  parent = standard_vbox;
  for (i = 1; i < dialog->num_elements; ++i)
    {
      opt = sane_get_option_descriptor (dev, i);
      if (!SANE_OPTION_IS_ACTIVE(opt->cap))
        continue;

      /* pick up well-known options as we go: */
      if (opt->name)
        {
          if (strcmp(opt->name, SANE_NAME_PREVIEW) == 0
              && opt->type == SANE_TYPE_BOOL)
            {
              dialog->well_known.preview = i;
              continue;
            }
          else if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) == 0
                   && opt->unit == SANE_UNIT_DPI && (opt->type == SANE_TYPE_INT || opt->type == SANE_TYPE_FIXED))
            dialog->well_known.dpi = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_MODE) == 0)
            dialog->well_known.scanmode = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) == 0)
            dialog->well_known.scansource = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_TL_X) == 0)
            dialog->well_known.coord[GSG_TL_X] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_TL_Y) == 0)
            dialog->well_known.coord[GSG_TL_Y] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_BR_X) == 0)
            dialog->well_known.coord[GSG_BR_X] = i;
          else if (strcmp (opt->name, SANE_NAME_SCAN_BR_Y) == 0)
            dialog->well_known.coord[GSG_BR_Y] = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR) == 0)
            dialog->well_known.gamma_vector = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_R) == 0)
            dialog->well_known.gamma_vector_r = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_G) == 0)
            dialog->well_known.gamma_vector_g = i;
          else if (strcmp (opt->name, SANE_NAME_GAMMA_VECTOR_B) == 0)
            dialog->well_known.gamma_vector_b = i;
          else if (strcmp (opt->name, SANE_NAME_BIT_DEPTH) == 0)
            dialog->well_known.bit_depth = i;
        }

      elem = dialog->element + i;
      elem->dialog = dialog;

      if (opt->unit == SANE_UNIT_NONE)
        strncpy(title, opt->title, sizeof (title));
      else
        snprintf(title, sizeof (title), "%s [%s]", opt->title, gsg_unit_string(opt->unit));

      switch (opt->type)
      {
        case SANE_TYPE_GROUP:
          /* group a set of options */
          vbox = standard_vbox;
          if (opt->cap & SANE_CAP_ADVANCED)
            vbox = advanced_vbox;
          parent = gsg_group_new (vbox, title);
          elem->widget = parent;
          break;

        case SANE_TYPE_BOOL:
          assert (opt->size == sizeof(SANE_Word));
          status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
            goto get_value_failed;
          gsg_button_new(parent, title, val, elem, dialog->tooltips, opt->desc);
          gtk_widget_show(parent->parent);
          break;

        case SANE_TYPE_INT:
          if (opt->size != sizeof(SANE_Word))
          {
            vector_opts[num_vector_opts++] = i;
            break;
          }
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
            goto get_value_failed;

          switch (opt->constraint_type)
            {
            case SANE_CONSTRAINT_RANGE:
              if (strcmp (opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a scale */
                quant = opt->constraint.range->quant;
                if (quant == 0)
                  quant = 1;
                gsg_scale_new(parent, title, val, opt->constraint.range->min, opt->constraint.range->max, quant,
                          (opt->cap & SANE_CAP_AUTOMATIC), elem, dialog->tooltips, opt->desc);
                gtk_widget_show (parent->parent);
              }
              break;

            case SANE_CONSTRAINT_WORD_LIST:
              /* use a "list-selection" widget */
              num_words = opt->constraint.word_list[0];
              str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
              for (j = 0; j < num_words; ++j)
                {
                  sprintf (str, "%d", opt->constraint.word_list[j + 1]);
                  str_list[j] = strdup (str);
                }
              str_list[j] = 0;
              sprintf (str, "%d", val);
              gsg_option_menu_new (parent, title, str_list, str, elem, dialog->tooltips, opt->desc);
              free (str_list);
              gtk_widget_show (parent->parent);
              break;

            default:
              fprintf(stderr, "panel_build: unknown constraint %d!\n", opt->constraint_type);
              break;
            }
          break;

        case SANE_TYPE_FIXED:
          if (opt->size != sizeof (SANE_Word))
            {
              vector_opts[num_vector_opts++] = i;
              break;
            }
          status = sane_control_option(dialog->dev, i, SANE_ACTION_GET_VALUE, &val, 0);
          if (status != SANE_STATUS_GOOD)
            goto get_value_failed;

          switch (opt->constraint_type)
            {
            case SANE_CONSTRAINT_RANGE:
              if (strcmp(opt->name, SANE_NAME_SCAN_RESOLUTION) != 0) /* do not show resolution */
              {
                /* use a scale */
                quant = opt->constraint.range->quant;
                if (quant == 0)
                quant = 1;
                dval = SANE_UNFIX (val);
                dmin = SANE_UNFIX (opt->constraint.range->min);
                dmax = SANE_UNFIX (opt->constraint.range->max);
                dquant = SANE_UNFIX (quant);
                if (opt->unit == SANE_UNIT_MM)
                {
                  dval /= preferences.length_unit;
                  dmin /= preferences.length_unit;
                  dmax /= preferences.length_unit;
                  dquant /= preferences.length_unit;
                }
                gsg_scale_new(parent, title, dval, dmin, dmax, dquant, (opt->cap & SANE_CAP_AUTOMATIC), elem,
                          dialog->tooltips, opt->desc);
                gtk_widget_show (parent->parent);
              }
              break;

            case SANE_CONSTRAINT_WORD_LIST:
              /* use a "list-selection" widget */
              num_words = opt->constraint.word_list[0];
              str_list = malloc ((num_words + 1) * sizeof (str_list[0]));
              for (j = 0; j < num_words; ++j)
              {
                sprintf (str, "%g", SANE_UNFIX (opt->constraint.word_list[j + 1]));
                str_list[j] = strdup (str);
              }
              str_list[j] = 0;
              sprintf (str, "%g", SANE_UNFIX (val));
              gsg_option_menu_new (parent, title, str_list, str, elem, dialog->tooltips, opt->desc);
              free (str_list);
              gtk_widget_show (parent->parent);
              break;

            default:
              fprintf (stderr, "panel_build: unknown constraint %d!\n", opt->constraint_type);
              break;
            }
          break;

        case SANE_TYPE_STRING:
          buf = malloc (opt->size);
          status = sane_control_option (dialog->dev, i, SANE_ACTION_GET_VALUE, buf, 0);
          if (status != SANE_STATUS_GOOD)
            {
              free (buf);
              goto get_value_failed;
            }

          switch (opt->constraint_type)
            {
            case SANE_CONSTRAINT_STRING_LIST:
              if ( (strcmp (opt->name, SANE_NAME_SCAN_MODE) != 0) &&  /* do not show scanmode */
                   (strcmp (opt->name, SANE_NAME_SCAN_SOURCE) != 0) ) /* do not show scansource */
              {
                /* use a "list-selection" widget */
                gsg_option_menu_new (parent, title,
                               (char **) opt->constraint.string_list, buf,
                               elem, dialog->tooltips, opt->desc);
                gtk_widget_show (parent->parent);
              }
              break;

            case SANE_CONSTRAINT_NONE:
              gsg_text_entry_new (parent, title, buf, elem, dialog->tooltips, opt->desc);
              gtk_widget_show (parent->parent);
              break;

            default:
              fprintf (stderr, "panel_build: unknown constraint %d!\n", opt->constraint_type);
              break;
            }
          free (buf);
          break;

        case SANE_TYPE_BUTTON:
          button = gtk_button_new ();
          gtk_signal_connect (GTK_OBJECT (button), "clicked", (GtkSignalFunc) gsg_push_button_callback, elem);
          gsg_set_tooltip(dialog->tooltips, button, opt->desc);

          label = gtk_label_new (title);
          gtk_container_add(GTK_CONTAINER (button), label);

          gtk_box_pack_start(GTK_BOX (parent), button, FALSE, TRUE, 0);

          gtk_widget_show(label);
          gtk_widget_show(button);

          elem->widget = button;
          gtk_widget_show(parent->parent);
          break;

        default:
          fprintf(stderr, "panel_build: Unknown type %d\n", opt->type);
          break;
      }
      continue;

    get_value_failed:
      {
        char msg[256];

        sprintf(msg, "Failed to obtain value of option %s: %s.", opt->name, sane_strstatus (status));
        gsg_error(msg);
      }
    }
  xsane_hbox = xsane_update_xsane_callback();

  gtk_container_add(GTK_CONTAINER(dialog->xsane_window), xsane_hbox);
  gtk_container_add(GTK_CONTAINER(dialog->standard_window), standard_hbox);
  gtk_container_add(GTK_CONTAINER(dialog->advanced_window), advanced_hbox);

  dialog->xsane_hbox    = xsane_hbox;
  dialog->standard_hbox = standard_hbox;
  dialog->advanced_hbox = advanced_hbox;

  xsane_update_histogram();
/*
  xsane_draw_slider_level(&xsane.slider_gray);
  xsane_draw_slider_level(&xsane.slider_red);
  xsane_draw_slider_level(&xsane.slider_green);
  xsane_draw_slider_level(&xsane.slider_blue);
*/
  xsane_update_sliders();

  if (xsane.length_unit_widget)
  {
   int unit;

    xsane_get_area_value(dialog->well_known.coord[0], 0, &unit);

    if (unit == SANE_UNIT_PIXEL)
    {
      gtk_widget_set_sensitive(xsane.length_unit_widget, FALSE);
    }
    else
    {
      gtk_widget_set_sensitive(xsane.length_unit_widget, TRUE);
    }
  }

  /* now add in vector editor, if necessary: */
/*
  if (num_vector_opts)
    vector_new (dialog, custom_gamma_vbox, num_vector_opts, vector_opts);
*/
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Create the main dialog box.  */

static void xsane_device_dialog(void)
{
  GtkWidget *hbox, *button, *frame;
  GtkWidget *main_dialog_window, *standard_dialog_window, *advanced_dialog_window;
  GtkWidget *menubar, *menubar_item;
  const gchar *devname;
  char buf[256];
  char windowname[255];
  char devicetext[255];
  char *textptr;
  GtkWidget *xsane_window;
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
  GtkWidget *xsane_color_hbox;
  GtkWidget *xsane_histogram_vbox;
  SANE_Int num_elements;
  SANE_Status status;
  SANE_Handle dev;



  devname = devlist[seldev]->name;

  status = sane_open(devname, &dev);
  if (status != SANE_STATUS_GOOD)
  {
    sprintf(buf, "Failed to open device `%s': %s.", devname, sane_strstatus(status));
    gsg_error(buf);
    return;
  }

  if (sane_control_option(dev, 0, SANE_ACTION_GET_VALUE, &num_elements, 0) != SANE_STATUS_GOOD)
  {
    gsg_error("Error obtaining option count.");
    sane_close(dev);
    return;
  }


  /* create device-text  for window titles */

  sprintf(devicetext, "%s", devlist[seldev]->model);
  textptr = devicetext + strlen(devicetext);
  while (*(textptr-1) == ' ')
  {
    textptr--;
  }

  *textptr = ':';
  textptr++;
  *textptr = 0;

  if (!strncmp(devname, "net:", 4))
  {
    sprintf(textptr, "net:");
    textptr = devicetext + strlen(devicetext);
  }

  sprintf(buf, ":%s", devname);
  sprintf(buf, "/%s", (strrchr(buf, ':')+1));
  sprintf(textptr, (strrchr(buf, '/')+1));


  /* create the xsane dialog box */

  xsane.shell = gtk_dialog_new();
  gtk_widget_set_uposition(xsane.shell, XSANE_DIALOG_POS_X, XSANE_DIALOG_POS_Y);
  gtk_widget_set_usize(xsane.shell, XSANE_DIALOG_WIDTH, XSANE_DIALOG_HEIGHT);
  sprintf(windowname, "%s %s %s", prog_name, XSANE_VERSION, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.shell), (char *) windowname);
                                              /* shrink grow auto_shrink */
  gtk_window_set_policy(GTK_WINDOW(xsane.shell), FALSE, TRUE, FALSE);
  gtk_signal_connect(GTK_OBJECT(xsane.shell), "delete_event", GTK_SIGNAL_FUNC(xsane_scan_win_delete), NULL);


  /* create the scanner standard options dialog box */

  xsane.standard_options_shell = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_widget_set_uposition(xsane.standard_options_shell, XSANE_DIALOG_POS_X, XSANE_DIALOG_POS_Y2);
  sprintf(windowname, "Standard options %s", devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.standard_options_shell), (char *) windowname);
                                                               /* shrink grow  auto_shrink */
  gtk_window_set_policy(GTK_WINDOW(xsane.standard_options_shell), FALSE, TRUE, FALSE);
  gtk_signal_connect(GTK_OBJECT(xsane.standard_options_shell), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_standard_option_win_delete), NULL);


  /* create the scanner advanced options dialog box */

  xsane.advanced_options_shell = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_widget_set_uposition(xsane.advanced_options_shell, XSANE_DIALOG_POS_X2, XSANE_DIALOG_POS_Y2);
  sprintf(windowname, "Advanced options %s", devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.advanced_options_shell), (char *) windowname);
                                                               /* shrink grow  auto_shrink */
  gtk_window_set_policy(GTK_WINDOW(xsane.advanced_options_shell), FALSE, TRUE, FALSE);
  gtk_signal_connect(GTK_OBJECT(xsane.advanced_options_shell), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_advanced_option_win_delete), NULL);


  /* create the main vbox */

  xsane_window = GTK_DIALOG(xsane.shell)->vbox;

  /* restore preferences */
  xsane_pref_restore();



  /* create the menubar */

  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(xsane_window), menubar, FALSE, FALSE, 0);

  /* "Files" submenu: */
  menubar_item = gtk_menu_item_new_with_label("File");
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_files_build_menu());
  gtk_widget_show(menubar_item);

  /* "Preferences" submenu: */
  menubar_item = gtk_menu_item_new_with_label("Preferences");
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_pref_build_menu());
  gtk_widget_show(menubar_item);

  /* "Help" submenu: */
  menubar_item = gtk_menu_item_new_with_label("Help");
  gtk_container_add(GTK_CONTAINER(menubar), menubar_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar_item), xsane_help_build_menu());
  gtk_widget_show(menubar_item);

  gtk_widget_show(menubar);


  /* xsane main window */

  xsane.main_dialog_scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xsane.main_dialog_scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(xsane_window), xsane.main_dialog_scrolled);
  gtk_widget_show(xsane.main_dialog_scrolled);

  xsane_vbox_main = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_main), 5);
  gtk_container_add(GTK_CONTAINER(xsane.main_dialog_scrolled), xsane_vbox_main);
  gtk_widget_show(xsane_vbox_main);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  main_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_main), main_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(main_dialog_window);

  gtk_widget_show(xsane_window);



  /* standard options */

  xsane_vbox_standard = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_standard), 5);
  gtk_container_add(GTK_CONTAINER(xsane.standard_options_shell), xsane_vbox_standard);
  gtk_widget_show(xsane_vbox_standard);

  /* create  a subwindow so the standard dialog keeps its position on rebuilds: */
  standard_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_standard), standard_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(standard_dialog_window);



  /* advanced options */

  xsane_vbox_advanced = gtk_vbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_vbox_advanced), 5);
  gtk_container_add(GTK_CONTAINER(xsane.advanced_options_shell), xsane_vbox_advanced);
  gtk_widget_show(xsane_vbox_advanced);

  /* create  a subwindow so the advanced dialog keeps its position on rebuilds: */
  advanced_dialog_window = gtk_hbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_advanced), advanced_dialog_window, TRUE, TRUE, 0);
  gtk_widget_show(advanced_dialog_window);



  /* realize window and set colors */

  gtk_widget_realize(xsane.shell);

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

  dialog = malloc(sizeof (*dialog));
  if (!dialog)
  {
    printf("Could not create dialog\n");
    return;
  }

  /* fill in dialog structure */

  memset(dialog, 0, sizeof(*dialog));

  dialog->xsane_window           = main_dialog_window;
  dialog->standard_window        = standard_dialog_window;
  dialog->advanced_window        = advanced_dialog_window;
  dialog->dev                    = dev;
  dialog->dev_name               = strdup(devname);
  dialog->num_elements           = num_elements;
  dialog->option_reload_callback = xsane_update_preview;
  dialog->option_reload_arg      = 0;
  dialog->param_change_callback  = xsane_update_param;
  dialog->param_change_arg       = 0;

  dialog->element = malloc(num_elements * sizeof(dialog->element[0]));
  memset(dialog->element, 0, num_elements * sizeof(dialog->element[0]));


  /* define tooltips colors */

  dialog->tooltips = gtk_tooltips_new();

  /* use black as foreground: */
  dialog->tooltips_fg.red   = 0;
  dialog->tooltips_fg.green = 0;
  dialog->tooltips_fg.blue  = 0;
  gdk_color_alloc(colormap, &dialog->tooltips_fg);

  /* postit yellow (khaki) as background: */
  dialog->tooltips_bg.red   = 61669;
  dialog->tooltips_bg.green = 59113;
  dialog->tooltips_bg.blue  = 35979;
  gdk_color_alloc(colormap, &dialog->tooltips_bg);

  gtk_tooltips_set_colors(dialog->tooltips, &dialog->tooltips_bg, &dialog->tooltips_fg);
  gsg_set_tooltips(dialog, preferences.tooltips_enabled);


  /* create histogram window */

  xsane.histogram_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_policy(GTK_WINDOW(xsane.histogram_dialog), FALSE, FALSE, FALSE);
  gtk_widget_set_uposition(xsane.histogram_dialog, XSANE_DIALOG_POS_X2, XSANE_DIALOG_POS_Y);
  gtk_signal_connect(GTK_OBJECT(xsane.histogram_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_histogram_win_delete), 0);
  sprintf(windowname, "%s histogram %s", prog_name, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.histogram_dialog), windowname);

  xsane_histogram_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_border_width(GTK_CONTAINER(xsane_histogram_vbox), 5);
  gtk_container_add(GTK_CONTAINER(xsane.histogram_dialog), xsane_histogram_vbox);
  gtk_widget_show(xsane_histogram_vbox);

  xsane_pixmap_new(xsane_histogram_vbox, "Raw image", 256, 100, &(xsane.histogram_raw));

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_gray.r = 1;
  xsane.slider_gray.g = 1;
  xsane.slider_gray.b = 1;
  xsane.slider_gray.active = -1;
  xsane_create_slider(&xsane.slider_gray);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_gray.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_gray.preview);
  gtk_widget_realize(xsane.slider_gray.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_red.r = 1;
  xsane.slider_red.g = 0;
  xsane.slider_red.b = 0;
  xsane.slider_red.active = -1;
  xsane_create_slider(&xsane.slider_red);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_red.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_red.preview);
  gtk_widget_realize(xsane.slider_red.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_green.r = 0;
  xsane.slider_green.g = 1;
  xsane.slider_green.b = 0;
  xsane.slider_green.active = -1;
  xsane_create_slider(&xsane.slider_green);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_green.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_green.preview);
  gtk_widget_realize(xsane.slider_green.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_blue.r = 0;
  xsane.slider_blue.g = 0;
  xsane.slider_blue.b = 1;
  xsane.slider_blue.active = -1;
  xsane_create_slider(&xsane.slider_blue);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_blue.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_blue.preview);
  gtk_widget_realize(xsane.slider_blue.preview);

  xsane_draw_slider_level(&xsane.slider_gray);
  xsane_draw_slider_level(&xsane.slider_red);
  xsane_draw_slider_level(&xsane.slider_green);
  xsane_draw_slider_level(&xsane.slider_blue);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane_pixmap_new(xsane_histogram_vbox, "Enhanced image", 256, 100, &(xsane.histogram_enh));

  xsane_color_hbox = gtk_hbox_new(TRUE, 5);
  gtk_container_border_width(GTK_CONTAINER(xsane_color_hbox), 5);
  gtk_container_add(GTK_CONTAINER(xsane_histogram_vbox), xsane_color_hbox);
  gtk_widget_show(xsane_color_hbox);

  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, intensity_xpm, DESC_HIST_INTENSITY,
                         &xsane.histogram_int,   xsane_histogram_toggle_button_callback);
  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, red_xpm, DESC_HIST_RED,
                         &xsane.histogram_red,   xsane_histogram_toggle_button_callback);
  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, green_xpm, DESC_HIST_GREEN,
                         &xsane.histogram_green, xsane_histogram_toggle_button_callback);
  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, blue_xpm, DESC_HIST_BLUE,
                         &xsane.histogram_blue,  xsane_histogram_toggle_button_callback);
  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, pixel_xpm, DESC_HIST_PIXEL,
                         &xsane.histogram_lines, xsane_histogram_toggle_button_callback);
  xsane_toggle_button_new_with_pixmap(xsane_color_hbox, log_xpm, DESC_HIST_LOG,
                         &xsane.histogram_log, xsane_histogram_toggle_button_callback);

  gtk_widget_show(xsane_color_hbox);



  panel_build(dialog); /* create backend dependend options */


  /* The info row */

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(hbox), 3);
  gtk_box_pack_start(GTK_BOX(xsane_window), hbox, FALSE, FALSE, 0);
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
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_scan_dialog, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  /* The Preview button */
  button = gtk_toggle_button_new_with_label("Preview Window");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_scan_preview, NULL);
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

  xsane_pref_device_restore();	/* restore device-settings */

  xsane_update_param(dialog, 0);

  if (preferences.show_standard_options)
  {
    gtk_widget_show(xsane.standard_options_shell);
  }

  if (preferences.show_advanced_options)
  {
    gtk_widget_show(xsane.advanced_options_shell);
  }

  gtk_widget_show(xsane.shell); /* call as last so focus is on it */

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  xsane_update_sliders();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_choose_dialog_ok_callback(void)
{
  gtk_signal_disconnect_by_func(GTK_OBJECT(choose_device_dialog), GTK_SIGNAL_FUNC(xsane_files_exit_callback), NULL);
  gtk_widget_destroy(choose_device_dialog);
  xsane_device_dialog();

  if (!dialog)
  {
    xsane_quit();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_select_device_callback(GtkWidget * widget, GdkEventButton *event, gpointer data)
{
  seldev = (long) data;
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    xsane_choose_dialog_ok_callback();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint32 xsane_choose_device(void)
{
 GtkWidget *main_vbox, *vbox, *hbox, *button, *device_frame, *device_vbox;
 GSList *owner;
 const SANE_Device *adev;
 gint i;
 char buf[256];
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkStyle *style;
 GdkColor *bg_trans;
 GdkFont *newfont=0;
 GdkFont *oldfont=0;
 char vendor[9];
 char model[17];
 char type[20];
 int j;

  choose_device_dialog = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(choose_device_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(choose_device_dialog), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(choose_device_dialog), "destroy", GTK_SIGNAL_FUNC(xsane_files_exit_callback), NULL);
  sprintf(buf, "%s device selection",prog_name);
  gtk_window_set_title(GTK_WINDOW(choose_device_dialog), buf);

  main_vbox = GTK_DIALOG(choose_device_dialog)->vbox;

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(vbox), 3);
  gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  /* xsane logo */
  gtk_widget_realize(choose_device_dialog);

  style = gtk_widget_get_style(choose_device_dialog);
  bg_trans = &style->bg[GTK_STATE_NORMAL];

  sprintf(buf, "%s/xsane-logo.xpm", STRINGIFY(PATH_SANE_DATA_DIR));  
  pixmap = gdk_pixmap_create_from_xpm(choose_device_dialog->window, &mask, bg_trans, buf);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(vbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);


  xsane_separator_new(vbox, 5);


  /* list the drivers with radiobuttons */
  device_frame = gtk_frame_new("Available devices:");
  gtk_box_pack_start(GTK_BOX(vbox), device_frame, FALSE, FALSE, 2);
  gtk_widget_show(device_frame);

  device_vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_border_width(GTK_CONTAINER(device_vbox), 3);
  gtk_container_add(GTK_CONTAINER(device_frame), device_vbox);

  gtk_widget_realize(device_vbox);
  style = gtk_widget_get_style(device_vbox);
  newfont = gdk_font_load("-misc-fixed-medium-r-semicondensed--13-100-*");
  if (newfont)
  {
    oldfont = style->font;
    style->font = newfont;
  }

  owner = NULL;
  for (i = 0; i < ndevs; i++)
  {
    adev = devlist[i];

    strncpy(vendor, adev->vendor, sizeof(vendor)-1);
    for (j = strlen(vendor); j < sizeof(vendor)-1; j++)
    { vendor[j]=' '; }
    vendor[j]=0;

    strncpy(model, adev->model, sizeof(model)-1);
    for (j = strlen(model); j < sizeof(model)-1; j++)
    { model[j]=' '; }
    model[j]=0;

    strncpy(type, adev->type, sizeof(type)-1);
    for (j = strlen(type); j < sizeof(type)-1; j++)
    { type[j]=' '; }
    type[j]=0;

    sprintf(buf, "%s %s %s [%s]", vendor, model, type, adev->name);
    button = gtk_radio_button_new_with_label(owner, (char *) buf);
    gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
                       (GtkSignalFunc) xsane_select_device_callback, (void *) (long) i);
    gtk_box_pack_start(GTK_BOX(device_vbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);
    owner = gtk_radio_button_group(GTK_RADIO_BUTTON(button));;
  }
  gtk_widget_show(device_vbox);

  /* The bottom row of buttons */
  hbox = GTK_DIALOG(choose_device_dialog)->action_area;

  /* The OK button */
  button = gtk_button_new_with_label("OK");
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_choose_dialog_ok_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  /* The Cancel button */
  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_files_exit_callback, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(choose_device_dialog);

  if (oldfont) /* I think this is not the planned way to do it, but otherwise I get a sigseg */
  {
    style->font = oldfont;
    gdk_font_unref(newfont);
  }
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_usage(void)
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

static void xsane_init(int argc, char **argv)
{
  char filename[PATH_MAX];
  struct stat st;

  gtk_init(&argc, &argv);
#ifdef HAVE_LIBGIMP_GIMP_H
  gtk_rc_parse(gimp_gtkrc());

  gdk_set_use_xshm(gimp_use_xshm());
#endif

  gsg_make_path(sizeof(filename), filename, 0, "xsane-style", 0, ".rc");
  if (stat(filename, &st) >= 0)
  {
    gtk_rc_parse(filename);
  }
  else
  {
    strncpy(filename, STRINGIFY(PATH_SANE_DATA_DIR) "/xsane-style.rc", sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
    if (stat(filename, &st) >= 0)
    {
      gtk_rc_parse(filename);
    }
  }

  sane_init(&xsane.sane_backend_versioncode, (void *) xsane_authorization_callback);
  if (SANE_VERSION_MAJOR(xsane.sane_backend_versioncode) != SANE_V_MAJOR)
  {
    fprintf(stderr, "\n\n"
                    "%s error:\n"
                    "  Sane major version number mismatch!\n"
                    "  xsane major version = %d\n"
                    "  backend major version = %d\n"
                    "*** PROGRAM ABORTED ***\n\n",
                    prog_name, SANE_V_MAJOR, SANE_VERSION_MAJOR(xsane.sane_backend_versioncode));
    return;
  }

  if (argc > 1)
  {
    int ch;

    while((ch = getopt_long(argc, argv, "ghV", long_options, 0)) != EOF)
    {
      switch(ch)
      {
        case 'g':
          printf("%s: GIMP support missing.\n", argv[0]);
          exit(0);

        case 'V':
          printf("xsane %s (sane %d.%d, package %s)\n",
                 XSANE_VERSION, 
                 SANE_VERSION_MAJOR(xsane.sane_backend_versioncode),
                 SANE_VERSION_MINOR(xsane.sane_backend_versioncode),
                 PACKAGE_VERSION);
          exit(0);

        case 'h':
        default:
          xsane_usage();
          exit(0);
      }
    }
  }

  sane_get_devices(&devlist, SANE_FALSE /* local and network devices */);

  /* if devicename is given try to identify it, if not found, open device list */
  if (optind < argc) 
  {
   int ndevs;

    for (ndevs = 0; devlist[ndevs]; ++ndevs)
    {
      if (!strncmp(devlist[ndevs]->name, argv[argc - 1], strlen(argv[argc - 1])))
      {
        seldev = ndevs;
        break; 
      }
    }

    if ((seldev < 0) && (argc > 1))
    {
     static SANE_Device dev;
     static const SANE_Device *device_list[] = { &dev, 0 };

      memset(&dev, 0, sizeof(dev));
      dev.name   = argv[argc - 1];
      dev.vendor = "unknown";
      dev.type   = "unknown";
      dev.model  = "unknown";

      devlist = device_list;
      seldev = 0;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_interface(int argc, char **argv)
{
  xsane.info_label = NULL;

  xsane_init(argc, argv);

  for (ndevs = 0; devlist[ndevs]; ++ndevs);

  if (seldev >= 0)
  {
    if (seldev >= ndevs)
    {
      fprintf(stderr, "%s: device %d is unavailable.\n", prog_name, seldev);
      xsane_quit();
    }

    xsane_device_dialog();

    if (!dialog)
    {
      xsane_quit();
    }
  }
  else
  {
    if (ndevs > 0)
    {
      seldev = 0;
      if (ndevs == 1)
      {
        xsane_device_dialog();
        if (!dialog)
	{
	  xsane_quit();
	}
      }
      else
      {
        xsane_choose_device();
      }
    }
    else
    {
      fprintf(stderr, "%s: no devices available.\n", prog_name);
      xsane_quit();
    }
  }

  gtk_main();
  sane_exit();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  dialog = 0;
  memset(&xsane, 0, sizeof(xsane)); /* set all values in xsane to 0 */

  xsane.mode                = STANDALONE;
  xsane.xsane_mode          = XSANE_SCAN;
  xsane.xsane_output_format = XSANE_PNM;

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
  xsane.slider_gray.value[2]  = 100.0;
  xsane.slider_gray.value[1]  = 50.0;
  xsane.slider_gray.value[0]  = 0.0;
  xsane.slider_red.value[2]   = 100.0;
  xsane.slider_red.value[1]   = 50.0;
  xsane.slider_red.value[0]   = 0.0;
  xsane.slider_green.value[2] = 100.0;
  xsane.slider_green.value[1] = 50.0;
  xsane.slider_green.value[0] = 0.0;
  xsane.slider_blue.value[2]  = 100.0;
  xsane.slider_blue.value[1]  = 50.0;
  xsane.slider_blue.value[0]  = 0.0;
  xsane.auto_white       = 100.0;
  xsane.auto_gray        = 50.0;
  xsane.auto_black       = 0.0;

  xsane.histogram_red    = 1;
  xsane.histogram_green  = 1;
  xsane.histogram_blue   = 1;
  xsane.histogram_int    = 1;
  xsane.histogram_log    = 1;

  xsane.xsane_color                = TRUE;
  xsane.scanner_gamma_color        = FALSE;
  xsane.scanner_gamma_gray         = FALSE;
  xsane.enhancement_rgb_default    = TRUE;

  prog_name = strrchr(argv[0], '/');
  if (prog_name)
  {
    ++prog_name;
  }
  else
  {
    prog_name = argv[0];
  }

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
    {
      xsane_interface(argc, argv);
    }
  }
#else
  xsane_interface(argc, argv);
#endif
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
