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
#include "xsane-front-gtk.h"
#include "xsane-back-gtk.h"
#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-icons-def.h"
#include "xsane-text.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif


/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

static void xsane_bound_double(double *value, double min, double max);
void xsane_clear_histogram(XsanePixmap *hist);
static void xsane_draw_histogram_with_points(XsanePixmap *hist, int invert,
                                             SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                             int show_red, int show_green, int show_blue, int show_inten, double scale);
static void xsane_draw_histogram_with_lines(XsanePixmap *hist, int invert,
                                            SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                            int show_red, int show_green, int show_blue, int show_inten, double scale);
void xsane_draw_slider_level(XsaneSlider *slider);
static void xsane_set_slider(XsaneSlider *slider, double min, double mid, double max);
void xsane_update_slider(XsaneSlider *slider);
void xsane_update_sliders(void);
static gint xsane_slider_callback(GtkWidget *widget, GdkEvent *event, XsaneSlider *slider);
void xsane_create_slider(XsaneSlider *slider);
void xsane_create_histogram(GtkWidget *parent, const char *title, int width, int height, XsanePixmap *hist);
void xsane_destroy_histogram(void);
static void xsane_calculate_auto_enhancement(int negative,
              SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue);
void xsane_calculate_histogram(void);
void xsane_update_histogram(void);
void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data);
void xsane_create_gamma_curve(SANE_Int *gammadata, int negative, double gamma,
                              double brightness, double contrast, int numbers, int maxout);
void xsane_update_gamma(void);
static void xsane_enhancement_update(void);
static void xsane_gamma_to_histogram(double *min, double *mid, double *max,
                                     double contrast, double brightness, double gamma);
void xsane_enhancement_by_gamma(void);
void xsane_enhancement_restore_default(void);
void xsane_enhancement_restore(void);
void xsane_enhancement_store(void);
static void xsane_histogram_to_gamma(XsaneSlider *slider, double *contrast, double *brightness, double *gamma);
void xsane_enhancement_by_histogram(void);
static gint xsane_histogram_win_delete(GtkWidget *widget, gpointer data);
void xsane_create_histogram_dialog(const char *devicetext);

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

void xsane_clear_histogram(XsanePixmap *hist)
{
 GdkRectangle rect;

  if(hist->pixmap)
  {
    rect.x=0;
    rect.y=0;
    rect.width  = HIST_WIDTH;
    rect.height = HIST_HEIGHT;

    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);
    gtk_widget_draw(hist->pixmapwid, &rect);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_histogram_with_points(XsanePixmap *hist, int invert,
                                             SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                             int show_red, int show_green, int show_blue, int show_inten, double scale)
{
 GdkRectangle rect;
 int i;
 int inten, red, green, blue;
 int colval;

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
      if (invert)
      {
        colval = 255-i;
      }
      else
      {
        colval = i;
      }

      inten = show_inten * count[colval]       * scale;

      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[colval]   * scale;
        green = show_green * count_green[colval] * scale;
        blue  = show_blue  * count_blue[colval]  * scale;
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

static void xsane_draw_histogram_with_lines(XsanePixmap *hist, int invert,
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
 int colval;

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
      if (invert)
      {
        colval = 255-i;
      }
      else
      {
        colval = i;
      }

      inten = show_inten * count[colval]       * scale;

      if (xsane.xsane_color)
      {
        red   = show_red   * count_red[colval]   * scale;
        green = show_green * count_green[colval] * scale;
        blue  = show_blue  * count_blue[colval]  * scale;
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

void xsane_establish_slider(XsaneSlider *slider)
{
 int x, y, pos, len;
 guchar buf[XSANE_SLIDER_WIDTH*3];
 GdkRectangle rect;

  buf[0] = buf[1] = buf[2] = 0;
  buf[3+0] = buf[3+1] = buf[3+2]= 0;

  for (x=0; x<256; x++)
  {
    buf[3*x+0+6] = x * slider->r;
    buf[3*x+1+6] = x * slider->g;
    buf[3*x+2+6] = x * slider->b;
  }

  buf[258*3+0] = 255 * slider->r;
  buf[258*3+1] = 255 * slider->g;
  buf[258*3+2] = 255 * slider->b;

  buf[259*3+0] = 255 * slider->r;
  buf[259*3+1] = 255 * slider->g;
  buf[259*3+2] = 255 * slider->b;

  for (y=0; y<XSANE_SLIDER_HEIGHT; y++)
  {
    pos = slider->position[0]-y/2;
    len = y;
    if (pos<-2)
    {
      len = len + pos + 2;
      pos = -2;
    }
    pos = pos * 3 + 6;

    for (x=0; x<=len; x++)
    {
      if ((x == 0) || (x == len) || (y == XSANE_SLIDER_HEIGHT-1))
      {
        buf[pos++] = 255;
        buf[pos++] = 255;
        buf[pos++] = 255;
      }
      else
      {
        buf[pos++] = 0;
        buf[pos++] = 0;
        buf[pos++] = 0;
      }
    }


    pos = slider->position[1]-y/2;
    len = y;
    pos = pos * 3 + 6;

    for (x=0; x<=len; x++)
    {
      if ((x == 0) || (x == len) || (y == XSANE_SLIDER_HEIGHT-1))
      {
        buf[pos++] = 255;
        buf[pos++] = 255;
        buf[pos++] = 255;
      }
      else
      {
        buf[pos++] = 128;
        buf[pos++] = 128;
        buf[pos++] = 128;
      }
    }


    pos = slider->position[2]-y/2;
    len = y;
    if (pos+len>257)
    {
      len = 257 - pos;
    }
    pos = pos * 3 + 6;

    for (x=0; x<=len; x++)
    {
      if ((x == 0) || (x == len) || (y == XSANE_SLIDER_HEIGHT-1))
      {
        buf[pos++] = 0;
        buf[pos++] = 0;
        buf[pos++] = 0;
      }
      else
      {
        buf[pos++] = 255;
        buf[pos++] = 255;
        buf[pos++] = 255;
      }
    }

    gtk_preview_draw_row(GTK_PREVIEW(slider->preview),buf, 0, y, XSANE_SLIDER_WIDTH);
  }

  rect.x=0;
  rect.y=0;
  rect.width  = XSANE_SLIDER_WIDTH;
  rect.height = XSANE_SLIDER_HEIGHT;

  gtk_widget_draw(slider->preview, &rect);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_draw_slider_level(XsaneSlider *slider)
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

static void xsane_set_slider(XsaneSlider *slider, double min, double mid, double max)
{
  slider->value[0] = min;
  slider->value[1] = mid;
  slider->value[2] = max;

  slider->position[0] = min * 2.55;
  slider->position[1] = mid * 2.55;
  slider->position[2] = max * 2.55;

  xsane_establish_slider(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_slider(XsaneSlider *slider)
{
  slider->position[0] = 2.55 * slider->value[0];
  slider->position[1] = 2.55 * slider->value[1];
  slider->position[2] = 2.55 * slider->value[2];

  xsane_establish_slider(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_sliders()
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
    xsane_draw_slider_level(&xsane.slider_red);   /* remove slider */
    xsane_draw_slider_level(&xsane.slider_green); /* remove slider */
    xsane_draw_slider_level(&xsane.slider_blue);  /* remove slider */

    xsane.slider_red.active   = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_green.active = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_blue.active  = XSANE_SLIDER_INACTIVE; /* mark slider inactive */

    if (xsane.param.depth == 1)
    {
      xsane_draw_slider_level(&xsane.slider_gray); /* remove slider */
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

void xsane_create_slider(XsaneSlider *slider)
{
  slider->preview = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(slider->preview), XSANE_SLIDER_WIDTH, XSANE_SLIDER_HEIGHT);
  gtk_widget_set_events(slider->preview, XSANE_SLIDER_EVENTS);
  gtk_signal_connect(GTK_OBJECT(slider->preview), "event", GTK_SIGNAL_FUNC(xsane_slider_callback), slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_histogram(GtkWidget *parent, const char *title, int width, int height, XsanePixmap *hist)
{
  GdkBitmap *mask=NULL;

   hist->frame     = gtk_frame_new(title);
   hist->pixmap    = gdk_pixmap_new(xsane.histogram_dialog->window, width, height, -1);
   hist->pixmapwid = gtk_pixmap_new(hist->pixmap, mask);
   gtk_container_add(GTK_CONTAINER(hist->frame), hist->pixmapwid);
   gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, width, height);

   gtk_box_pack_start(GTK_BOX(parent), hist->frame, FALSE, FALSE, 2);
   gtk_widget_show(hist->pixmapwid);
   gtk_widget_show(hist->frame);
 }                           

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
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
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_calculate_auto_enhancement(int negative,
              SANE_Int *count_raw, SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue)
{	 /* calculate white, medium and black values for auto enhancement */
 int limit, limit_mid;
 int points, points_mix, points_red, points_green, points_blue;
 int min, mid, max;
 int min_red, mid_red, max_red;
 int min_green, mid_green, max_green;
 int min_blue, mid_blue, max_blue;
 int val;
 int i;

  if (xsane.preview)
  {
    points       = 0;
    points_mix   = 0;
    points_red   = 0;
    points_green = 0;
    points_blue  = 0;

    for (i=0; i<256; i++)
    {
      points       += count_raw[i];
      points_mix   += 10 * log(1 + count_raw[i] + count_raw_red[i] + count_raw_green[i] + count_raw_blue[i]);
      points_red   += 10 * log(1 + count_raw_red[i]);
      points_green += 10 * log(1 + count_raw_green[i]);
      points_blue  += 10 * log(1 + count_raw_blue[i]);
    }

    limit = 1 + points / 5000;

    /* ----- gray ----- */

    min = -1;
    val = 0;
    while ( (val/4 < limit) && (min < 254) )
    {
      min++;
      val += count_raw[min] + count_raw_red[min] + count_raw_green[min] + count_raw_blue[min];
    }

    max = HIST_WIDTH;
    val = 0;
    while ( (val/4 < limit) && (max > min + 1) )
    {
      max--;
      val += count_raw[max] + count_raw_red[max] + count_raw_green[max] + count_raw_blue[max];
    }

    limit_mid = points_mix / 2.0;

    mid = 0;
    val = 0;
    while ( (val < limit_mid) && (mid < max - 2) )
    {
      mid++;
      val += 10 * log(1 + count_raw[mid] + count_raw_red[mid] + count_raw_green[mid] + count_raw_blue[mid]);
    }

    /* ----- red ----- */

    min_red = -1;
    val     = 0;
    while ( (val < limit) && (min_red < 254) )
    {
      min_red++;
      val += count_raw_red[min_red];
    }

    max_red = HIST_WIDTH;
    val     = 0;
    while ( (val < limit) && (max_red > min_red + 1) )
    {
      max_red--;
      val += count_raw_red[max_red];
    }

    limit_mid = points_red / 2.0;

    mid_red = 0;
    val     = 0;
    while ( (val < limit_mid) && (mid_red < max_red - 2) )
    {
      mid_red++;
      val += 10 * log(1 + count_raw_red[mid_red]);
    }

    /* ----- green ----- */

    min_green = -1;
    val       = 0;
    while ( (val < limit) && (min_green < 254) )
    {
      min_green++;
      val += count_raw_green[min_green];
    }

    max_green = HIST_WIDTH;
    val       = 0;
    while ( (val < limit) && (max_green > min_green + 1) )
    {
      max_green--;
      val += count_raw_green[max_green];
    }

    limit_mid = points_green / 2.0;

    mid_green = 0;
    val     = 0;
    while ( (val < limit_mid) && (mid_green < max_green - 2) )
    {
      mid_green++;
      val += 10 * log(1 + count_raw_green[mid_green]);
    }

    /* ----- blue ----- */

    min_blue = -1;
    val      = 0;
    while ( (val < limit) && (min_blue < 254) )
    {
      min_blue++;
      val += count_raw_blue[min_blue];
    }

    max_blue = HIST_WIDTH;
    val       = 0;
    while ( (val < limit) && (max_blue > min_blue + 1) )
    {
      max_blue--;
      val += count_raw_blue[max_blue];
    }

    limit_mid = points_blue / 2.0;

    mid_blue = 0;
    val      = 0;
    while ( (val < limit_mid) && (mid_blue < max_blue - 2) )
    {
      mid_blue++;
      val += 10 * log(1 + count_raw_blue[mid_blue]);
    }

    if (negative)
    {
      xsane.auto_white = (255-min)/2.55;
      xsane.auto_gray  = (255-mid)/2.55;
      xsane.auto_black = (255-max)/2.55;

      xsane.auto_white_red = (255-min_red)/2.55;
      xsane.auto_gray_red  = (255-mid_red)/2.55;
      xsane.auto_black_red = (255-max_red)/2.55;

      xsane.auto_white_green = (255-min_green)/2.55;
      xsane.auto_gray_green  = (255-mid_green)/2.55;
      xsane.auto_black_green = (255-max_green)/2.55;

      xsane.auto_white_blue = (255-min_blue)/2.55;
      xsane.auto_gray_blue  = (255-mid_blue)/2.55;
      xsane.auto_black_blue = (255-max_blue)/2.55;
    }
    else /* positive */
    {
      xsane.auto_white = max/2.55;
      xsane.auto_gray  = mid/2.55;
      xsane.auto_black = min/2.55;

      xsane.auto_white_red = max_red/2.55;
      xsane.auto_gray_red  = mid_red/2.55;
      xsane.auto_black_red = min_red/2.55;

      xsane.auto_white_green = max_green/2.55;
      xsane.auto_gray_green  = mid_green/2.55;
      xsane.auto_black_green = min_green/2.55;

      xsane.auto_white_blue = max_blue/2.55;
      xsane.auto_gray_blue  = mid_blue/2.55;
      xsane.auto_black_blue = min_blue/2.55;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_calculate_histogram(void)
{
 SANE_Int *count_raw;
 SANE_Int *count_raw_red;
 SANE_Int *count_raw_green;
 SANE_Int *count_raw_blue;
 SANE_Int *count_enh;
 SANE_Int *count_enh_red;
 SANE_Int *count_enh_green;
 SANE_Int *count_enh_blue;
 int i;
 int maxval_raw;
 int maxval_enh;
 int maxval;
 double scale;

  /* at first reset auto enhancement values */

  xsane.auto_black = 0.0;
  xsane.auto_gray  = 50.0;
  xsane.auto_white = 100.0;

  xsane.auto_black_red = 0.0;
  xsane.auto_gray_red  = 50.0;
  xsane.auto_white_red = 100.0;

  xsane.auto_black_green = 0.0;
  xsane.auto_gray_green  = 50.0;
  xsane.auto_white_green = 100.0;

  xsane.auto_black_blue = 0.0;
  xsane.auto_gray_blue  = 50.0;
  xsane.auto_white_blue = 100.0;

  if (xsane.preview) /* preview window exists? */
  {
    count_raw       = calloc(256, sizeof(SANE_Int));
    count_raw_red   = calloc(256, sizeof(SANE_Int));
    count_raw_green = calloc(256, sizeof(SANE_Int));
    count_raw_blue  = calloc(256, sizeof(SANE_Int));
    count_enh       = calloc(256, sizeof(SANE_Int));
    count_enh_red   = calloc(256, sizeof(SANE_Int));
    count_enh_green = calloc(256, sizeof(SANE_Int));
    count_enh_blue  = calloc(256, sizeof(SANE_Int));

    preview_calculate_histogram(xsane.preview, count_raw, count_raw_red, count_raw_green, count_raw_blue,
                                count_enh, count_enh_red, count_enh_green, count_enh_blue);

    if (xsane.param.depth > 1)
    {
      xsane_calculate_auto_enhancement(xsane.negative, count_raw, count_raw_red, count_raw_green, count_raw_blue);
    }

    if (xsane.histogram_log) /* logarithmical display */
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
      xsane_draw_histogram_with_lines(&xsane.histogram_raw, xsane.negative,
                         count_raw, count_raw_red, count_raw_green, count_raw_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);

      xsane_draw_histogram_with_lines(&xsane.histogram_enh, 0 /* negative is done by gamma table */,
                        count_enh, count_enh_red, count_enh_green, count_enh_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);
    }
    else
    {
      xsane_draw_histogram_with_points(&xsane.histogram_raw, xsane.negative,
                        count_raw, count_raw_red, count_raw_green, count_raw_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale);

      xsane_draw_histogram_with_points(&xsane.histogram_enh, 0 /*negative is done by gamma table */,
                        count_enh, count_enh_red, count_enh_green, count_enh_blue,
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
  else
  {
    xsane_clear_histogram(&xsane.histogram_raw);
    xsane_clear_histogram(&xsane.histogram_enh);
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

void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;

  *valuep = (GTK_TOGGLE_BUTTON(widget)->active != 0);
  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_gamma_curve(SANE_Int *gammadata, int negative, double gamma,
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

  if (negative)
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) (maxin - i)) - midin;
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
    }
  }
  else /* positive */
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i) - midin;
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_gamma(void)
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

    xsane_create_gamma_curve(xsane.preview_gamma_data_red, xsane.negative,
		 	     xsane.gamma * xsane.gamma_red * preferences.preview_gamma * preferences.preview_gamma_red,
			     xsane.brightness + xsane.brightness_red,
			     xsane.contrast + xsane.contrast_red, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_green, xsane.negative,
			     xsane.gamma * xsane.gamma_green * preferences.preview_gamma * preferences.preview_gamma_green,
			     xsane.brightness + xsane.brightness_green,
			     xsane.contrast + xsane.contrast_green, 256, 255);

    xsane_create_gamma_curve(xsane.preview_gamma_data_blue, xsane.negative,
			     xsane.gamma * xsane.gamma_blue * preferences.preview_gamma * preferences.preview_gamma_blue,
			     xsane.brightness + xsane.brightness_blue,
			     xsane.contrast + xsane.contrast_blue , 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_red, xsane.negative,
		 	     xsane.gamma * xsane.gamma_red,
			     xsane.brightness + xsane.brightness_red,
			     xsane.contrast + xsane.contrast_red, 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_green, xsane.negative,
			     xsane.gamma * xsane.gamma_green,
			     xsane.brightness + xsane.brightness_green,
			     xsane.contrast + xsane.contrast_green, 256, 255);

    xsane_create_gamma_curve(xsane.histogram_gamma_data_blue, xsane.negative,
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

static void xsane_enhancement_update()
{
 guint sig_changed = gtk_signal_lookup("changed", GTK_OBJECT_TYPE(xsane.gamma_widget));
 
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

    gtk_signal_emit(xsane.gamma_red_widget,        sig_changed); 
    gtk_signal_emit(xsane.gamma_green_widget,      sig_changed); 
    gtk_signal_emit(xsane.gamma_blue_widget,       sig_changed); 

    gtk_signal_emit(xsane.brightness_red_widget,   sig_changed); 
    gtk_signal_emit(xsane.brightness_green_widget, sig_changed); 
    gtk_signal_emit(xsane.brightness_blue_widget,  sig_changed); 

    gtk_signal_emit(xsane.contrast_red_widget,     sig_changed); 
    gtk_signal_emit(xsane.contrast_green_widget,   sig_changed); 
    gtk_signal_emit(xsane.contrast_blue_widget,    sig_changed); 
  }

  /* update histogram slider */

  xsane_update_sliders();

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  gtk_signal_emit(xsane.gamma_widget,      sig_changed); 
  gtk_signal_emit(xsane.brightness_widget, sig_changed); 
  gtk_signal_emit(xsane.contrast_widget,   sig_changed); 
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

void xsane_enhancement_by_gamma(void)
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

void xsane_enhancement_restore_default()
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

void xsane_enhancement_restore()
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

  xsane.enhancement_rgb_default = preferences.xsane_rgb_default;
  xsane.negative                = preferences.xsane_negative;

  xsane_refresh_dialog(dialog);
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_store()
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

  preferences.xsane_rgb_default      = xsane.enhancement_rgb_default;
  preferences.xsane_negative         = xsane.negative;
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

void xsane_enhancement_by_histogram(void)
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

static gint xsane_histogram_win_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  preferences.show_histogram = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  return TRUE;
}                                    

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_histogram_dialog(const char *devicetext)
{
 char windowname[255];
 GtkWidget *xsane_color_hbox;
 GtkWidget *xsane_histogram_vbox;  
 GdkColor color_black;
 GdkColor color_red;
 GdkColor color_green;
 GdkColor color_blue;
 GdkColor color_backg;
 GdkColormap *colormap;
 GtkStyle *style;

  xsane.histogram_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_policy(GTK_WINDOW(xsane.histogram_dialog), FALSE, FALSE, FALSE);
  gtk_widget_set_uposition(xsane.histogram_dialog, XSANE_DIALOG_POS_X2, XSANE_DIALOG_POS_Y);
  gtk_signal_connect(GTK_OBJECT(xsane.histogram_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_histogram_win_delete), 0);
  sprintf(windowname, "%s %s", WINDOW_HISTOGRAM, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.histogram_dialog), windowname);

  xsane_histogram_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_histogram_vbox), 5);
  gtk_container_add(GTK_CONTAINER(xsane.histogram_dialog), xsane_histogram_vbox);
  gtk_widget_show(xsane_histogram_vbox);


  /* set gc for histogram drawing */
  gtk_widget_realize(xsane.histogram_dialog); /* realize dialog to get colors and style */

  style = gtk_widget_get_style(xsane.histogram_dialog);
/*
  style = gtk_rc_get_style(xsane.histogram_dialog);
  style = gtk_widget_get_default_style();
*/

  xsane.gc_trans = style->bg_gc[GTK_STATE_NORMAL];
  xsane.bg_trans = &style->bg[GTK_STATE_NORMAL];   

  colormap = gdk_window_get_colormap(xsane.histogram_dialog->window);

  xsane.gc_black  = gdk_gc_new(xsane.histogram_dialog->window);
  color_black.red   = 0;
  color_black.green = 0;
  color_black.blue  = 0;
  gdk_color_alloc(colormap, &color_black);
  gdk_gc_set_foreground(xsane.gc_black, &color_black);

  xsane.gc_red   = gdk_gc_new(xsane.histogram_dialog->window);
  color_red.red   = 40000;
  color_red.green = 10000;
  color_red.blue  = 10000;
  gdk_color_alloc(colormap, &color_red);
  gdk_gc_set_foreground(xsane.gc_red, &color_red);

  xsane.gc_green = gdk_gc_new(xsane.histogram_dialog->window);
  color_green.red   = 10000;
  color_green.green = 40000;
  color_green.blue  = 10000;
  gdk_color_alloc(colormap, &color_green);
  gdk_gc_set_foreground(xsane.gc_green, &color_green);

  xsane.gc_blue  = gdk_gc_new(xsane.histogram_dialog->window);
  color_blue.red   = 10000;
  color_blue.green = 10000;
  color_blue.blue  = 40000;
  gdk_color_alloc(colormap, &color_blue);
  gdk_gc_set_foreground(xsane.gc_blue, &color_blue);

  xsane.gc_backg  = gdk_gc_new(xsane.histogram_dialog->window);
  color_backg.red   = 50000;
  color_backg.green = 50000;
  color_backg.blue  = 50000;
  gdk_color_alloc(colormap, &color_backg);
  gdk_gc_set_foreground(xsane.gc_backg, &color_backg);                              


  /* add histogram images and sliders */

  xsane_create_histogram(xsane_histogram_vbox, FRAME_RAW_IMAGE, 256, 100, &(xsane.histogram_raw));

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

  xsane_create_histogram(xsane_histogram_vbox, FRAME_ENHANCED_IMAGE, 256, 100, &(xsane.histogram_enh));

  xsane_color_hbox = gtk_hbox_new(TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_color_hbox), 5);
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

}

/* ---------------------------------------------------------------------------------------------------------------------- */

