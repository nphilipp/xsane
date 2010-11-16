/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-gamma.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2010 Oliver Rauch
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

#include "xsane.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif


/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_clear_histogram(XsanePixmap *hist);
static void xsane_draw_histogram_with_points(XsanePixmap *hist,
                                             SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                             int show_red, int show_green, int show_blue, int show_inten, double scale);
static void xsane_draw_histogram_with_lines(XsanePixmap *hist, 
                                            SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                            int show_red, int show_green, int show_blue, int show_inten, double scale);
void xsane_draw_slider_level(XsaneSlider *slider);
static void xsane_set_slider(XsaneSlider *slider, double min, double mid, double max);
void xsane_update_slider(XsaneSlider *slider);
void xsane_update_sliders(void);
static gint xsane_slider_callback(GtkWidget *widget, GdkEvent *event, XsaneSlider *slider);
void xsane_create_slider(XsaneSlider *slider);
void xsane_create_histogram(GtkWidget *parent, const char *title, int width, int height, XsanePixmap *hist);
/* void xsane_get_free_gamma_curve(gfloat *free_color_gamma_data, SANE_Int *gammadata, */
void xsane_get_free_gamma_curve(gfloat *free_color_gamma_data, u_char *gammadata,
                                int negative, double gamma, double brightness, double contrast,
                                int len, int maxout);
static void xsane_calculate_auto_enhancement(SANE_Int *count_raw,
            SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue);
void xsane_calculate_raw_histogram(void);
void xsane_calculate_enh_histogram(void);
void xsane_update_histogram(int update_raw);
void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data);
void xsane_create_preview_threshold_curve(u_char *gammadata, double threshold, int numbers);
void xsane_create_preview_gamma_curve(u_char *gammadata, int negative, double gamma,
                                      double brightness, double contrast,
                                      double medium_shadow, double medium_highlight, double medium_gamma, 
                                      int numbers);
void xsane_create_gamma_curve(SANE_Int *gammadata,
                              int negative, double gamma, double brightness, double contrast,
                              double medium_shadow, double medium_highlight, double medium_gamma, 
                              int numbers, int maxout);
void xsane_update_gamma_curve(int update_raw);
static void xsane_enhancement_update(void);
static void xsane_gamma_to_histogram(double *min, double *mid, double *max,
                                     double contrast, double brightness, double gamma);
void xsane_enhancement_by_gamma(void);
void xsane_enhancement_restore_default(void);
void xsane_enhancement_restore(void);
void xsane_enhancement_store(void);
static int xsane_histogram_to_gamma(XsaneSlider *slider, double *contrast, double contrast_offset, double *brightness, double brightness_offset, double *gamma, double gamma_multiplier);
void xsane_enhancement_by_histogram(int update_gamma);
static gint xsane_histogram_win_delete(GtkWidget *widget, gpointer data);
void xsane_create_histogram_dialog(const char *devicetext);
#ifdef HAVE_WORKING_GTK_GAMMACURVE
static gint xsane_gamma_win_delete(GtkWidget *widget, gpointer data);
#endif
void xsane_create_gamma_dialog(const char *devicetext);
void xsane_update_gamma_dialog(void);
void xsane_set_auto_enhancement(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_clear_histogram(XsanePixmap *hist)
{
  DBG(DBG_proc, "xsane_clear_histogram\n");

  if(hist->pixmap)
  {
    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);
    gtk_widget_queue_draw(hist->pixmapwid);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_histogram_with_points(XsanePixmap *hist,
                                             SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                             int show_red, int show_green, int show_blue, int show_inten, double scale)
{
 int i;
 int inten, red, green, blue;

  DBG(DBG_proc, "xsane_draw_histogram_with_points\n");

#define XD 1
#define YD 2

  if(hist->pixmap)
  {
    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < HIST_WIDTH; i++)
    {
      inten = show_inten * count[i] * scale;

      if (xsane.xsane_channels > 1)
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

#ifdef HAVE_GTK2
    gtk_widget_queue_draw(hist->pixmapwid);

    if (hist->pixmapwid->window)
    {
      gdk_window_process_updates(hist->pixmapwid->window, FALSE);
    }
#else
    {
     GdkRectangle rect;

      rect.x=0;
      rect.y=0;
      rect.width  = HIST_WIDTH;
      rect.height = HIST_HEIGHT;

      gtk_widget_draw(hist->pixmapwid, &rect);
    }
#endif
  }
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_draw_histogram_with_lines(XsanePixmap *hist,
                                            SANE_Int *count, SANE_Int *count_red, SANE_Int *count_green, SANE_Int *count_blue,
                                            int show_red, int show_green, int show_blue, int show_inten, double scale)
{
 int i, j, k;
 int inten, red, green, blue;
 int inten0=0, red0=0, green0=0, blue0=0;
 int val[4];
 int val2[4];
 int color[4];
 int val_swap;
 int color_swap;

  DBG(DBG_proc, "xsane_draw_histogram_with_lines\n");

  if (hist->pixmap)
  {
    gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, HIST_WIDTH, HIST_HEIGHT);

    red   = 0;
    green = 0;
    blue  = 0;

    for (i=0; i < HIST_WIDTH; i++)
    {
      inten = show_inten * count[i] * scale;

      if (xsane.xsane_channels > 1)
      {
        red   = show_red   * count_red[i]   * scale;
        green = show_green * count_green[i] * scale;
        blue  = show_blue  * count_blue[i]  * scale;
      }

      if (inten > HIST_HEIGHT)
      {
        inten = HIST_HEIGHT;
      }

      if (red > HIST_HEIGHT)
      {
        red = HIST_HEIGHT;
      }

      if (green > HIST_HEIGHT)
      {
        green = HIST_HEIGHT;
      }

      if (blue > HIST_HEIGHT)
      {
        blue = HIST_HEIGHT;
      }

      val[0] = red;   color[0] = 0;
      val[1] = green; color[1] = 1;
      val[2] = blue;  color[2] = 2;
      val[3] = inten; color[3] = 3;

      for (j = 0; j < 3; j++)
      {
        for (k = j + 1; k < 4; k++)
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
      val2[0] = val[1] + 1;
      val2[1] = val[2] + 1;
      val2[2] = val[3] + 1;
      val2[3] = 0;

      for (j = 0; j < 4; j++)
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

#ifdef HAVE_GTK2
    gtk_widget_queue_draw(hist->pixmapwid);

    if (hist->pixmapwid->window)
    {
      gdk_window_process_updates(hist->pixmapwid->window, FALSE);
    }
#else
    {
     GdkRectangle rect;

      rect.x=0;
      rect.y=0;
      rect.width  = HIST_WIDTH;
      rect.height = HIST_HEIGHT;

      gtk_widget_draw(hist->pixmapwid, &rect);
    }
#endif
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_establish_slider(XsaneSlider *slider)
{
 int x, y, pos, len;
 guchar buf[XSANE_SLIDER_WIDTH*3];

  DBG(DBG_proc, "xsane_establish_slider\n");

  buf[0] = buf[1] = buf[2] = 0;
  buf[3+0] = buf[3+1] = buf[3+2]= 0;

  for (x = 0; x < 256; x++)
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

  for (y = 0; y < XSANE_SLIDER_HEIGHT; y++)
  {
    pos = slider->position[0]-y/2;
    len = y;
    if (pos<-2)
    {
      len = len + pos + 2;
      pos = -2;
    }
    pos = pos * 3 + 6;

    for (x = 0; x <= len; x++)
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

    for (x = 0; x <= len; x++)
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

#ifdef HAVE_GTK2
    gtk_widget_queue_draw(slider->preview);

    if (slider->preview->window)
    {
      gdk_window_process_updates(slider->preview->window, FALSE);
    }
#else
    {
     GdkRectangle rect;

      rect.x=0;
      rect.y=0;
      rect.width  = HIST_WIDTH;
      rect.height = HIST_HEIGHT;

      gtk_widget_draw(slider->preview, &rect);
    }
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_draw_slider_level(XsaneSlider *slider)
{
 int i;
 guchar buf[XSANE_SLIDER_WIDTH*3];

  DBG(DBG_proc, "xsane_draw_slider_level\n");

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

#ifdef HAVE_GTK2
    gtk_widget_queue_draw(slider->preview);

    if (slider->preview->window)
    {
      gdk_window_process_updates(slider->preview->window, FALSE);
    }
#else
    {
     GdkRectangle rect;

      rect.x=0;
      rect.y=0;
      rect.width  = HIST_WIDTH;
      rect.height = HIST_HEIGHT;

      gtk_widget_draw(slider->preview, &rect);
    }
#endif
}
/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_set_slider(XsaneSlider *slider, double min, double mid, double max)
{
  DBG(DBG_proc, "xsane_set_slider\n");

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
  DBG(DBG_proc, "xsane_update_slider\n");

  slider->position[0] = 2.55 * slider->value[0];
  slider->position[1] = 2.55 * slider->value[1];
  slider->position[2] = 2.55 * slider->value[2];

  xsane_establish_slider(slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_sliders()
{
  DBG(DBG_proc, "xsane_update_sliders\n");

  xsane_update_slider(&xsane.slider_gray);

  if ( (xsane.xsane_channels > 1) && (!xsane.enhancement_rgb_default) && (!xsane.enable_color_management))
  {
    xsane_update_slider(&xsane.slider_red);
    xsane_update_slider(&xsane.slider_green);
    xsane_update_slider(&xsane.slider_blue);

    xsane.slider_gray.active  &= ~XSANE_SLIDER_INACTIVE; /* mark slider active */
    xsane.slider_red.active   &= ~XSANE_SLIDER_INACTIVE; /* mark slider active */
    xsane.slider_green.active &= ~XSANE_SLIDER_INACTIVE; /* mark slider active */
    xsane.slider_blue.active  &= ~XSANE_SLIDER_INACTIVE; /* mark slider active */
  }
  else
  {
    xsane_draw_slider_level(&xsane.slider_red);   /* remove slider */
    xsane_draw_slider_level(&xsane.slider_green); /* remove slider */
    xsane_draw_slider_level(&xsane.slider_blue);  /* remove slider */

    xsane.slider_red.active   = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_green.active = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    xsane.slider_blue.active  = XSANE_SLIDER_INACTIVE; /* mark slider inactive */

    if ((xsane.param.depth == 1) || (xsane.enable_color_management))
    {
      xsane_draw_slider_level(&xsane.slider_gray); /* remove slider */
      xsane.slider_gray.active = XSANE_SLIDER_INACTIVE; /* mark slider inactive */
    }
    else
    {
      xsane.slider_gray.active &= ~XSANE_SLIDER_INACTIVE; /* mark slider active */
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_batch_scan_gamma_event()
{
  DBG(DBG_proc, "xsane_batch_scan_gamma_event\n");

  xsane_batch_scan_update_icon_list(); /* update gamma of batch scan icons */

  gtk_timeout_remove(xsane.batch_scan_gamma_timer);
  xsane.batch_scan_gamma_timer = 0;

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_slider_hold_event()
{
  DBG(DBG_proc, "xsane_slider_hold_event\n");

  xsane_enhancement_by_histogram(TRUE);

  gtk_timeout_remove(xsane.slider_timer);
  xsane.slider_timer = 0;

  if (xsane.slider_timer_restart)
  {
    xsane.slider_timer = gtk_timeout_add(XSANE_CONTINUOUS_HOLD_TIME, xsane_slider_hold_event, 0);
    xsane.slider_timer_restart = FALSE;
  }

 return FALSE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_slider_callback(GtkWidget *widget, GdkEvent *event, XsaneSlider *slider)
{
 GdkEventButton *button_event;
 GdkEventMotion *motion_event;
 int distance;
 int i = 0;
 static int update = FALSE;
 static int x;

  DBG(DBG_proc, "xsane_slider_callback\n");

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
	  slider->active = i + 1;
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
      xsane_enhancement_by_histogram(TRUE); /* slider->active must be unchanged !!! */
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
    update = FALSE;
    switch(slider->active)
    {
      case 1:
	slider->value[0] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[0], 0.0, slider->value[1] - 1);
       break;

      case 2:
	slider->value[1] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[1], slider->value[0] + 1, slider->value[2] - 1);
       break;

      case 3:
	slider->value[2] = (x-XSANE_SLIDER_OFFSET) / 2.55;
	xsane_bound_double(&slider->value[2], slider->value[1] + 1, 100.0);
       break;

      default:
       break;
    }
    xsane_set_slider(slider, slider->value[0], slider->value[1], slider->value[2]);

    if (preferences.gtk_update_policy == GTK_UPDATE_CONTINUOUS)
    {
      /* call xsane_enhancement_by_histogram by event handler */
      if (!xsane.slider_timer)
      {
        xsane.slider_timer = gtk_timeout_add(XSANE_CONTINUOUS_HOLD_TIME, xsane_slider_hold_event, 0);
      }
      else
      {
        xsane.slider_timer_restart = TRUE;
      }
    }
    else if (preferences.gtk_update_policy == GTK_UPDATE_DELAYED)
    {
      if (xsane.slider_timer) /* hold timer active? then remove it, we had a motion */
      {
        gtk_timeout_remove(xsane.slider_timer);
      }          
      /* call xsane_slider_hold_event if mouse is not moved for ??? ms */
      xsane.slider_timer = gtk_timeout_add(XSANE_HOLD_TIME, xsane_slider_hold_event, 0);
    }
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_slider(XsaneSlider *slider)
{
  DBG(DBG_proc, "xsane_create_slider\n");

  slider->preview = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(slider->preview), XSANE_SLIDER_WIDTH, XSANE_SLIDER_HEIGHT);
  gtk_widget_set_events(slider->preview, XSANE_SLIDER_EVENTS);
  g_signal_connect(GTK_OBJECT(slider->preview), "event", GTK_SIGNAL_FUNC(xsane_slider_callback), slider);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_histogram(GtkWidget *parent, const char *title, int width, int height, XsanePixmap *hist)
{
  GdkBitmap *mask=NULL;

  DBG(DBG_proc, "xsane_create_histogram\n");

   hist->frame     = gtk_frame_new(title);
   hist->pixmap    = gdk_pixmap_new(xsane.histogram_dialog->window, width, height, -1);
   hist->pixmapwid = gtk_image_new_from_pixmap(hist->pixmap, mask);
   gtk_container_add(GTK_CONTAINER(hist->frame), hist->pixmapwid);
   gdk_draw_rectangle(hist->pixmap, xsane.gc_backg, TRUE, 0, 0, width, height);

   gtk_box_pack_start(GTK_BOX(parent), hist->frame, FALSE, FALSE, 2);
   gtk_widget_show(hist->pixmapwid);
   gtk_widget_show(hist->frame);
 }                           

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_calculate_auto_enhancement(SANE_Int *count_raw,
            SANE_Int *count_raw_red, SANE_Int *count_raw_green, SANE_Int *count_raw_blue)
{	 /* calculate white, medium and black values for auto enhancement */
 int limit, limit_mid;
 int points, points_mix, points_red, points_green, points_blue;
 int min, mid, max;
 int min_red, mid_red, max_red;
 int min_green, mid_green, max_green;
 int min_blue, mid_blue, max_blue;
 int val;
 int i;

  DBG(DBG_proc, "xsane_calculate_auto_enhancement\n");

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
    while ( (val/4 < limit) && (min < 253) )
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
    while ( (val < limit_mid) && (mid < max - 1) )
    {
      mid++;
      val += 10 * log(1 + count_raw[mid] + count_raw_red[mid] + count_raw_green[mid] + count_raw_blue[mid]);
    }

    xsane_bound_int(&mid, min, max);

    /* ----- red ----- */

    min_red = -1;
    val     = 0;
    while ( (val < limit) && (min_red < 253) )
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
    while ( (val < limit_mid) && (mid_red < max_red - 1) )
    {
      mid_red++;
      val += 10 * log(1 + count_raw_red[mid_red]);
    }

    xsane_bound_int(&mid_red, min_red, max_red);

    /* ----- green ----- */

    min_green = -1;
    val       = 0;
    while ( (val < limit) && (min_green < 253) )
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
    while ( (val < limit_mid) && (mid_green < max_green - 1) )
    {
      mid_green++;
      val += 10 * log(1 + count_raw_green[mid_green]);
    }

    xsane_bound_int(&mid_green, min_green, max_green);

    /* ----- blue ----- */

    min_blue = -1;
    val      = 0;
    while ( (val < limit) && (min_blue < 253) )
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
    while ( (val < limit_mid) && (mid_blue < max_blue - 1) )
    {
      mid_blue++;
      val += 10 * log(1 + count_raw_blue[mid_blue]);
    }

    xsane_bound_int(&mid_blue, min_blue, max_blue);

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

  DBG(DBG_proc, "xsane.auto_white       = %f\n", xsane.auto_white);
  DBG(DBG_proc, "xsane.auto_gray        = %f\n", xsane.auto_gray);
  DBG(DBG_proc, "xsane.auto_black       = %f\n", xsane.auto_black);
  DBG(DBG_proc, "xsane.auto_white_red   = %f\n", xsane.auto_white_red);
  DBG(DBG_proc, "xsane.auto_gray_red    = %f\n", xsane.auto_gray_red);
  DBG(DBG_proc, "xsane.auto_black_red   = %f\n", xsane.auto_black_red);
  DBG(DBG_proc, "xsane.auto_white_green = %f\n", xsane.auto_white_green);
  DBG(DBG_proc, "xsane.auto_gray_green  = %f\n", xsane.auto_gray_green);
  DBG(DBG_proc, "xsane.auto_black_green = %f\n", xsane.auto_black_green);
  DBG(DBG_proc, "xsane.auto_white_blue  = %f\n", xsane.auto_white_blue);
  DBG(DBG_proc, "xsane.auto_gray_blue   = %f\n", xsane.auto_gray_blue);
  DBG(DBG_proc, "xsane.auto_black_blue  = %f\n", xsane.auto_black_blue);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_calculate_raw_histogram(void)
{
 SANE_Int *count_raw;
 SANE_Int *count_raw_red;
 SANE_Int *count_raw_green;
 SANE_Int *count_raw_blue;
 int i;
 int maxval_raw;
 double scale_raw;

  DBG(DBG_proc, "xsane_calculate_raw_histogram\n");

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

    preview_calculate_raw_histogram(xsane.preview, count_raw, count_raw_red, count_raw_green, count_raw_blue);

    if (xsane.param.depth > 1)
    {
      xsane_calculate_auto_enhancement(count_raw, count_raw_red, count_raw_green, count_raw_blue);
    }

    if (xsane.histogram_log) /* logarithmical display */
    {
      for (i=0; i<=255; i++)
      {
        count_raw[i]       = (int) (50*log(1.0 + count_raw[i]));
        count_raw_red[i]   = (int) (50*log(1.0 + count_raw_red[i]));
        count_raw_green[i] = (int) (50*log(1.0 + count_raw_green[i]));
        count_raw_blue[i]  = (int) (50*log(1.0 + count_raw_blue[i]));
      }
    }

    maxval_raw = 1; /* we do not use 0 here because we divide through this varaible later */

    /* first and last 10 values are not used for calculating maximum value */
    for (i = 10 ; i < HIST_WIDTH - 10; i++)
    {
      if (count_raw[i]       > maxval_raw) { maxval_raw = count_raw[i]; }
      if (count_raw_red[i]   > maxval_raw) { maxval_raw = count_raw_red[i]; }
      if (count_raw_green[i] > maxval_raw) { maxval_raw = count_raw_green[i]; }
      if (count_raw_blue[i]  > maxval_raw) { maxval_raw = count_raw_blue[i]; }
    }
    scale_raw = 100.0/maxval_raw;

    if (xsane.histogram_lines)
    {
      xsane_draw_histogram_with_lines(&xsane.histogram_raw,
                                      count_raw, count_raw_red, count_raw_green, count_raw_blue,
                                      xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue,
                                      xsane.histogram_int, scale_raw);
    }
    else
    {
      xsane_draw_histogram_with_points(&xsane.histogram_raw,
                                       count_raw, count_raw_red, count_raw_green, count_raw_blue,
                                       xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue,
                                       xsane.histogram_int, scale_raw);
    }

    free(count_raw_blue);
    free(count_raw_green);
    free(count_raw_red);
    free(count_raw);
  }
  else
  {
    xsane_clear_histogram(&xsane.histogram_raw);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_calculate_enh_histogram(void)
{
 SANE_Int *count_enh;
 SANE_Int *count_enh_red;
 SANE_Int *count_enh_green;
 SANE_Int *count_enh_blue;
 int i;
 int maxval_enh;
 double scale_enh;

  DBG(DBG_proc, "xsane_calculate_enh_histogram\n");

  if (xsane.preview) /* preview window exists? */
  {
    count_enh       = calloc(256, sizeof(SANE_Int));
    count_enh_red   = calloc(256, sizeof(SANE_Int));
    count_enh_green = calloc(256, sizeof(SANE_Int));
    count_enh_blue  = calloc(256, sizeof(SANE_Int));

    preview_calculate_enh_histogram(xsane.preview, count_enh, count_enh_red, count_enh_green, count_enh_blue);

    if (xsane.histogram_log) /* logarithmical display */
    {
      for (i=0; i<=255; i++)
      {
        count_enh[i]       = (int) (50*log(1.0 + count_enh[i]));
        count_enh_red[i]   = (int) (50*log(1.0 + count_enh_red[i]));
        count_enh_green[i] = (int) (50*log(1.0 + count_enh_green[i]));
        count_enh_blue[i]  = (int) (50*log(1.0 + count_enh_blue[i]));
      }
    }

    maxval_enh = 1;

    /* first and last 10 values are not used for calculating maximum value */
    for (i = 10 ; i < HIST_WIDTH - 10; i++)
    {
      if (count_enh[i]       > maxval_enh) { maxval_enh = count_enh[i]; }
      if (count_enh_red[i]   > maxval_enh) { maxval_enh = count_enh_red[i]; }
      if (count_enh_green[i] > maxval_enh) { maxval_enh = count_enh_green[i]; }
      if (count_enh_blue[i]  > maxval_enh) { maxval_enh = count_enh_blue[i]; }
    }
    scale_enh = 100.0/maxval_enh;

    if (xsane.histogram_lines)
    {
      xsane_draw_histogram_with_lines(&xsane.histogram_enh,
                        count_enh, count_enh_red, count_enh_green, count_enh_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale_enh);
    }
    else
    {
      xsane_draw_histogram_with_points(&xsane.histogram_enh,
                        count_enh, count_enh_red, count_enh_green, count_enh_blue,
                        xsane.histogram_red, xsane.histogram_green, xsane.histogram_blue, xsane.histogram_int, scale_enh);
    }

    free(count_enh_blue);
    free(count_enh_green);
    free(count_enh_red);
    free(count_enh);
  }
  else
  {
    xsane_clear_histogram(&xsane.histogram_enh);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_histogram(int update_raw)
{
  DBG(DBG_proc, "xsane_update_histogram\n");

  if (preferences.show_histogram)
  {
    if (update_raw)
    {
      xsane_calculate_raw_histogram();
    }
    xsane_calculate_enh_histogram();

    gtk_widget_show(xsane.histogram_dialog);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_histogram_toggle_button_callback(GtkWidget *widget, gpointer data)
{
  int *valuep = data;

  DBG(DBG_proc, "xsane_histogram_toggle_button_callback\n");

  *valuep = (GTK_TOGGLE_BUTTON(widget)->active != 0);
  xsane_update_histogram(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_preview_threshold_curve(u_char *gammadata, double threshold, int numbers)
{
 int i;
 int maxin = numbers-1;
 int threshold_level;

  DBG(DBG_proc, "xsane_create_preview_threshold_curve\n");

  xsane_bound_double(&threshold, 0.0, 100.0);
  threshold_level = maxin * threshold / 100.0;

  for (i=0; i < threshold_level; i++)
  {
    gammadata[i] = 0;
  }

  for (i=threshold_level; i <= maxin; i++)
  {
    gammadata[i] = 255;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#if 0
void xsane_create_preview_gamma_curve(u_char *gammadata, int negative, double gamma,
                                      double brightness, double contrast,
                                      double medium_shadow, double medium_highlight, double medium_gamma, 
                                      int numbers)
{
 int i;
 double midin;
 double val;
 double m;
 double b;
 double medium_m;
 double medium_mid;
 int maxin = numbers-1;
 double clip_shadow, clip_highlight;
 double m_shadow, m_highlight;
 double s2 = 0.0, s3 = 0.0; 
 double h2 = 0.0, h3 = 0.0;
 double clip_alpha = 0.34; /* 1/3 ... 1/2 are allowed */
 int medium_range;
 int unclipped_range;
 double medium_shadow_val, medium_highlight_val;
 double medium_highlight_val_from_maxin;
 double clip_highlight_from_maxin;

  medium_range    = (medium_highlight - medium_shadow)/ 100.0 * maxin;

  m_shadow = 1.0;
  m_highlight = 1.0;

  medium_shadow_val = medium_shadow/100.0 * maxin;
  medium_highlight_val = medium_highlight/100.0 * maxin;
  medium_highlight_val_from_maxin = maxin - medium_highlight_val;

  for (i=1; i<10; i++)
  {
    clip_shadow = clip_alpha * m_shadow * medium_shadow_val;
    clip_highlight_from_maxin = clip_alpha * (m_highlight * medium_highlight_val_from_maxin);
    clip_highlight = maxin - clip_highlight_from_maxin;
    unclipped_range = clip_highlight - clip_shadow;

    m_shadow = (4 * m_shadow + unclipped_range / medium_range / medium_gamma) / 5;
    m_highlight = (4 * m_highlight + unclipped_range / medium_range) / 5;
  }
  m_shadow = 1.0;
  m_highlight = 1.0;

  /* soft clipping constants for shadow of medium */

  if (medium_shadow_val)
  {
    clip_shadow = clip_alpha * m_shadow * medium_shadow_val;
    s2 = 3 * clip_shadow / (medium_shadow_val * medium_shadow_val) - m_shadow / medium_shadow_val;
    s3 = m_shadow / (medium_shadow_val * medium_shadow_val)  - 2 * clip_shadow / (medium_shadow_val * medium_shadow_val * medium_shadow_val);

    DBG(DBG_info2, "\n");
    DBG(DBG_info2, "maxin = %d\n", maxin);
    DBG(DBG_info2, "m_shadow          = %f\n", m_shadow);
    DBG(DBG_info2, "medium_shadow_val = %d\n", medium_shadow_val);
    DBG(DBG_info2, "clip_shadow       = %f\n", clip_shadow);
    DBG(DBG_info2, "s2                = %f\n", s2);
    DBG(DBG_info2, "s3                = %f\n", s3);
    DBG(DBG_info2, "s2*shadow^2 + s3*shadow^3 = %f\n", s2 * medium_shadow_val * medium_shadow_val + s3 * medium_shadow_val * medium_shadow_val* medium_shadow_val);
  }
  else
  {
    clip_shadow = 0;
  }


  /* soft clipping constants for highlight of medium */

  if (medium_highlight_val < maxin)
  {
    medium_highlight_val_from_maxin = maxin - medium_highlight_val;
    clip_highlight_from_maxin = clip_alpha * (m_highlight * medium_highlight_val_from_maxin);
    clip_highlight = maxin - clip_highlight_from_maxin;
    h2 = 3 * clip_highlight_from_maxin / (medium_highlight_val_from_maxin * medium_highlight_val_from_maxin) - m_highlight / medium_highlight_val_from_maxin;
    h3 = m_highlight / (medium_highlight_val_from_maxin * medium_highlight_val_from_maxin)  - 2 * clip_highlight_from_maxin / (medium_highlight_val_from_maxin * medium_highlight_val_from_maxin * medium_highlight_val_from_maxin);

    DBG(DBG_info2, "\n");
    DBG(DBG_info2, "maxin = %d\n", maxin);
    DBG(DBG_info2, "m_highlight          = %f\n", m_highlight);
    DBG(DBG_info2, "medium_highlight_val = %d\n", medium_highlight_val);
    DBG(DBG_info2, "clip_highlight       = %f\n", clip_highlight);
    DBG(DBG_info2, "h2                   = %f\n", h2);
    DBG(DBG_info2, "h3                   = %f\n", h3);
    DBG(DBG_info2, "h2*highlight^2 + h3*highlight^3 = %f\n", h2 * (maxin - medium_highlight_val) * (maxin - medium_highlight_val) + h3 * (maxin - medium_highlight_val) * (maxin - medium_highlight_val) * (maxin - medium_highlight_val));
  }
  else
  {
    clip_highlight = maxin;
  }

  /* standard gamma constants for medium */

  unclipped_range = clip_highlight   - clip_shadow;
  DBG(DBG_info2, "medium_range = %d\n", medium_range);
  DBG(DBG_info2, "unclipped_range = %d\n", unclipped_range);

  medium_m   = 100.0/(medium_highlight - medium_shadow);
  medium_mid = (medium_shadow + medium_highlight)/200.0 * maxin;

  DBG(DBG_proc, "xsane_create_preview_gamma_curve(neg=%d, gam=%3.2f, bri=%3.2f, ctr=%3.2f, nrs=%d)\n",
                 negative, gamma, brightness, contrast, numbers);

  if (contrast < -100.0)
  {
    contrast = -100.0;
  }

  midin = (int)(numbers / 2.0);

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midin;

  if (negative)
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;
      val = maxin - val; /* invert */

      if (i < medium_shadow_val)
      {
        val = maxin - s2 * i * i - s3 * i * i * i;
      }
      else if (i > medium_highlight_val)
      {
        val = h2 * (maxin - i) * (maxin - i) + h3 * (maxin - i) * (maxin - i) * (maxin - i);
      }
      else
      {
        xsane_bound_double(&val, 0.0, maxin);
        val = (maxin - clip_highlight) + unclipped_range * pow( val/maxin, (1.0/medium_gamma) );
      }

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = (u_char) (255.99999 * pow( ceil(val)/maxin, (1.0/gamma) ));
    }
  }
  else /* positive */
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;

      if (i < medium_shadow_val)
      {
        val = s2 * i * i + s3 * i * i * i;
      }
      else if (i > medium_highlight_val)
      {
        val = maxin - (h2 * (maxin - i) * (maxin - i) + h3 * (maxin - i) * (maxin - i) * (maxin - i));
      }
      else
      {
        xsane_bound_double(&val, 0.0, maxin);
        val = clip_shadow + unclipped_range * pow( val/maxin, (1.0/medium_gamma) );
      }

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = (u_char) (255.99999 * pow( val/maxin, (1.0/gamma) ));
    }
  }
}

#else

void xsane_create_preview_gamma_curve(u_char *gammadata, int negative, double gamma,
                                      double brightness, double contrast,
                                      double medium_shadow, double medium_highlight, double medium_gamma, 
                                      int numbers)
{
 int i;
 double midin;
 double val;
 double m;
 double b;
 double medium_m;
 double medium_mid;
 int maxin = numbers-1;

  medium_m   = 100.0/(medium_highlight - medium_shadow);
  medium_mid = (medium_shadow + medium_highlight)/200.0 * maxin;

  DBG(DBG_proc, "xsane_create_preview_gamma_curve(neg=%d, gam=%3.2f, bri=%3.2f, ctr=%3.2f, nrs=%d)\n",
                 negative, gamma, brightness, contrast, numbers);

  if (contrast < -100.0)
  {
    contrast = -100.0;
  }

  midin = (int)(numbers / 2.0);

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midin;

  if (negative)
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;
      xsane_bound_double(&val, 0.0, maxin);
      val = maxin - val; /* invert */
      val = maxin * pow( val/maxin, (1.0/medium_gamma) );

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = (u_char) (255.99999 * pow( ceil(val)/maxin, (1.0/gamma) ));
    }
  }
  else /* positive */
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;
      xsane_bound_double(&val, 0.0, maxin);
      val = maxin * pow( val/maxin, (1.0/medium_gamma) );

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);

      gammadata[i] = (u_char) (255.99999 * pow( val/maxin, (1.0/gamma) ));
    }
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_gamma_curve(SANE_Int *gammadata,
                              int negative, double gamma, double brightness, double contrast,
                              double medium_shadow, double medium_highlight, double medium_gamma, 
                              int numbers, int maxout)
{
 int i;
 double midin;
 double val;
 double m;
 double b;
 double medium_m;
 double medium_mid;
 int maxin = numbers-1;

  DBG(DBG_proc, "xsane_create_gamma_curve(neg=%d, gam=%3.2f, bri=%3.2f, ctr=%3.2f, "
                "mshd=%3.2f, mhig=%3.2f, mgam=%3.2f, "
                "nrs=%d, max=%d)\n",
                 negative, gamma, brightness, contrast,
                 medium_shadow, medium_highlight, medium_gamma,
                 numbers, maxout);

  midin = (int)(numbers / 2.0);

  if (contrast < -100.0)
  {
    contrast = -100.0;
  }

  medium_m   = 100.0/(medium_highlight - medium_shadow);
  medium_mid = (medium_shadow + medium_highlight)/200.0 * maxin;

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midin;

  if (negative)
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;
      xsane_bound_double(&val, 0.0, maxin);
      val = maxin - val; /* invert */
      val = maxin * pow( val/maxin, (1.0/medium_gamma) );

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);
      gammadata[i] = (int) (maxout * pow( val/maxin, (1.0/gamma) ));
    }
  }
  else /* positive */
  {
    for (i=0; i <= maxin; i++)
    {
      val = ((double) i);

      /* medium correction */
      val = (val - medium_mid) * medium_m + midin;
      xsane_bound_double(&val, 0.0, maxin);
      val = maxin * pow( val/maxin, (1.0/medium_gamma) );

      val = val - midin;

      /* user correction */
      val = val * m + b;
      xsane_bound_double(&val, 0.0, maxin);
      gammadata[i] = (int) (maxout * pow( val/maxin, (1.0/gamma) ));
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_gamma_curve(int update_raw)
{
  DBG(DBG_proc, "xsane_update_gamma_curve\n");

  if (xsane.preview)
  {
    if (!xsane.preview_gamma_data_red)
    {
      xsane.preview_gamma_size = pow(2, preferences.preview_gamma_input_bits);

      xsane.preview_gamma_data_red   = malloc(xsane.preview_gamma_size);
      xsane.preview_gamma_data_green = malloc(xsane.preview_gamma_size);
      xsane.preview_gamma_data_blue  = malloc(xsane.preview_gamma_size);

      xsane.histogram_gamma_data_red   = malloc(xsane.preview_gamma_size);
      xsane.histogram_gamma_data_green = malloc(xsane.preview_gamma_size);
      xsane.histogram_gamma_data_blue  = malloc(xsane.preview_gamma_size);

      xsane.histogram_medium_gamma_data_red   = malloc(xsane.preview_gamma_size);
      xsane.histogram_medium_gamma_data_green = malloc(xsane.preview_gamma_size);
      xsane.histogram_medium_gamma_data_blue  = malloc(xsane.preview_gamma_size);
    }

    if (xsane.preview->calibration)
    {
     double pgamma_red;
     double pgamma_green;
     double pgamma_blue;

      pgamma_red   = preferences.preview_gamma * preferences.preview_gamma_red;
      pgamma_green = preferences.preview_gamma * preferences.preview_gamma_green;
      pgamma_blue  = preferences.preview_gamma * preferences.preview_gamma_blue;

      xsane_create_preview_gamma_curve(xsane.preview_gamma_data_red,   0, pgamma_red,
                                       0.0, 0.0,  0.0, 100.0, 1.0, xsane.preview_gamma_size);
      xsane_create_preview_gamma_curve(xsane.preview_gamma_data_green, 0, pgamma_green,
                                       0.0, 0.0,  0.0, 100.0, 1.0, xsane.preview_gamma_size);
      xsane_create_preview_gamma_curve(xsane.preview_gamma_data_blue,  0, pgamma_blue,
                                       0.0, 0.0,  0.0, 100.0, 1.0, xsane.preview_gamma_size);
    }
    else if (xsane.param.depth == 1) /* for lineart mode with grayscale preview scan */
    {
      xsane_create_preview_threshold_curve(xsane.preview_gamma_data_red,   xsane.threshold, xsane.preview_gamma_size);
      xsane_create_preview_threshold_curve(xsane.preview_gamma_data_green, xsane.threshold, xsane.preview_gamma_size);
      xsane_create_preview_threshold_curve(xsane.preview_gamma_data_blue,  xsane.threshold, xsane.preview_gamma_size);

      xsane_create_preview_threshold_curve(xsane.histogram_gamma_data_red,   xsane.threshold, xsane.preview_gamma_size);
      xsane_create_preview_threshold_curve(xsane.histogram_gamma_data_green, xsane.threshold, xsane.preview_gamma_size);
      xsane_create_preview_threshold_curve(xsane.histogram_gamma_data_blue,  xsane.threshold, xsane.preview_gamma_size);
    }
    else /* multi bit mode */
    {
     double pgamma_red   = 1.0;
     double pgamma_green = 1.0;
     double pgamma_blue  = 1.0;

      if ((xsane.mode != XSANE_GIMP_EXTENSION) || (!preferences.disable_gimp_preview_gamma))
      {
        pgamma_red   = preferences.preview_gamma * preferences.preview_gamma_red;
        pgamma_green = preferences.preview_gamma * preferences.preview_gamma_green;
        pgamma_blue  = preferences.preview_gamma * preferences.preview_gamma_blue;
      }


#ifdef HAVE_WORKING_GTK_GAMMACURVE
#if 1
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_gray)->curve),  65536, xsane.free_gamma_data);
#if 1
{
 int i;
  for (i=0; i<100; i++)
  {
    printf("%1.6f ", xsane.free_gamma_data[i]);
    if (i / 10.0 == i / 10)
    {
      printf("\n");
    }
  }
  printf("\n");
  for (i=65435; i<65536; i++)
  {
    printf("%1.6f ", xsane.free_gamma_data[i]);
    if (i / 10.0 == i / 10)
    {
      printf("\n");
    }
  }
  printf("\n");
  printf("\n");
}
#endif
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_red)->curve),   65536, xsane.free_gamma_data_red);
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_green)->curve), 65536, xsane.free_gamma_data_green);
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_blue)->curve),  65536, xsane.free_gamma_data_blue);  
#endif

      xsane_get_free_gamma_curve(xsane.free_gamma_data_red, xsane.preview_gamma_data_red, xsane.negative,
                                 xsane.gamma * xsane.gamma_red * pgamma_red,
                                 xsane.brightness + xsane.brightness_red,
                                 xsane.contrast + xsane.contrast_red, xsane.preview_gamma_size, 255);  

      xsane_get_free_gamma_curve(xsane.free_gamma_data_green, xsane.preview_gamma_data_green, xsane.negative,
                                 xsane.gamma * xsane.gamma_green * pgamma_green,
                                 xsane.brightness + xsane.brightness_green,
                                 xsane.contrast + xsane.contrast_green, xsane.preview_gamma_size, 255);

      xsane_get_free_gamma_curve(xsane.free_gamma_data_blue, xsane.preview_gamma_data_blue, xsane.negative,
                                 xsane.gamma * xsane.gamma_blue * pgamma_blue,
                                 xsane.brightness + xsane.brightness_blue,
                                 xsane.contrast + xsane.contrast_blue , xsane.preview_gamma_size, 255);               


      xsane_get_free_gamma_curve(xsane.free_gamma_data_red, xsane.histogram_gamma_data_red, xsane.negative,
                                 xsane.gamma * xsane.gamma_red,
                                 xsane.brightness + xsane.brightness_red,
                                 xsane.contrast + xsane.contrast_red, xsane.preview_gamma_size, 255);  

      xsane_get_free_gamma_curve(xsane.free_gamma_data_green, xsane.histogram_gamma_data_green, xsane.negative,
                                 xsane.gamma * xsane.gamma_green,
                                 xsane.brightness + xsane.brightness_green,
                                 xsane.contrast + xsane.contrast_green, xsane.preview_gamma_size, 255);

      xsane_get_free_gamma_curve(xsane.free_gamma_data_blue, xsane.histogram_gamma_data_blue, xsane.negative,
                                 xsane.gamma * xsane.gamma_blue,
                                 xsane.brightness + xsane.brightness_blue,
                                 xsane.contrast + xsane.contrast_blue , xsane.preview_gamma_size, 255);               
#else
      if ( ( ( (xsane.xsane_channels > 1) && xsane.scanner_gamma_color ) || /* color scan and gamma table for red, green and blue available */
             xsane.scanner_gamma_gray ) && /* grayscale scan and gamma table for gray available */
           (!xsane.no_preview_medium_gamma) ) /* do not use gamma table when disabled */
      {
        DBG(DBG_info, "creating preview gamma tables without medium correction\n");

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_red, xsane.negative,
                                         xsane.gamma * xsane.gamma_red * pgamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_green, xsane.negative,
                                         xsane.gamma * xsane.gamma_green * pgamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_blue, xsane.negative,
                                         xsane.gamma * xsane.gamma_blue * pgamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);


        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_red, xsane.negative,
                                         xsane.gamma * xsane.gamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_green, xsane.negative,
                                         xsane.gamma * xsane.gamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_blue, xsane.negative,
                                         xsane.gamma * xsane.gamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         0.0, 100.0, 1.0,
                                         xsane.preview_gamma_size);

        if (update_raw) /* to speed up things here */
        {
          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_red, xsane.negative,
                                           1.0, 0.0, 0.0, 0.0, 100.0, 1.0,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_green, xsane.negative,
                                           1.0, 0.0, 0.0, 0.0, 100.0, 1.0,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_blue, xsane.negative,
                                           1.0, 0.0, 0.0, 0.0, 100.0, 1.0,
                                           xsane.preview_gamma_size);
        }
      }
      else if (xsane.xsane_channels > 1) /* color scan, no color scanner gamma tables available */
      {
        DBG(DBG_info, "creating preview gamma tables with medium correction\n");

        xsane.medium_changed = FALSE;

        preview_display_valid(xsane.preview);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_red, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_red * pgamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_green, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_green * pgamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_blue * pgamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue,
                                         xsane.preview_gamma_size);


        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_red, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_green, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue,
                                         xsane.preview_gamma_size);


        if (update_raw) /* to speed up things here */
        {
          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_red, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_red, xsane.medium_highlight_red, xsane.medium_gamma_red,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_green, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_green, xsane.medium_highlight_green, xsane.medium_gamma_green,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_blue, xsane.medium_highlight_blue, xsane.medium_gamma_blue,
                                           xsane.preview_gamma_size);
        }
      }
      else /* grayscale scan, no gray scanner gamma table available */
      {
        DBG(DBG_info, "creating preview gamma tables with medium correction\n");

        xsane.medium_changed = FALSE;

        preview_display_valid(xsane.preview);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_red, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_red * pgamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_green, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_green * pgamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.preview_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_blue * pgamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);


        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_red, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_red,
                                         xsane.brightness + xsane.brightness_red,
                                         xsane.contrast + xsane.contrast_red,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_green, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_green,
                                         xsane.brightness + xsane.brightness_green,
                                         xsane.contrast + xsane.contrast_green,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);

        xsane_create_preview_gamma_curve(xsane.histogram_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                         xsane.gamma * xsane.gamma_blue,
                                         xsane.brightness + xsane.brightness_blue,
                                         xsane.contrast + xsane.contrast_blue,
                                         xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                         xsane.preview_gamma_size);


        if (update_raw) /* to speed up things here */
        {
          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_red, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_green, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                           xsane.preview_gamma_size);

          xsane_create_preview_gamma_curve(xsane.histogram_medium_gamma_data_blue, xsane.negative != xsane.medium_negative,
                                           1.0, 0.0, 0.0,
                                           xsane.medium_shadow_gray, xsane.medium_highlight_gray, xsane.medium_gamma_gray,
                                           xsane.preview_gamma_size);
        }
      }
#endif
    }

    preview_gamma_correction(xsane.preview, preferences.preview_gamma_input_bits,
                             xsane.preview_gamma_data_red, xsane.preview_gamma_data_green, xsane.preview_gamma_data_blue,
                             xsane.histogram_gamma_data_red, xsane.histogram_gamma_data_green, xsane.histogram_gamma_data_blue,
                             xsane.histogram_medium_gamma_data_red, xsane.histogram_medium_gamma_data_green, xsane.histogram_medium_gamma_data_blue);

    xsane_update_histogram(update_raw);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_enhancement_update(void)
{
  DBG(DBG_proc, "xsane_enhancement_update\n");

  if (xsane.param.depth == 1) /* lineart? no gamma */
  {
    return;
  } 

  if (xsane.enable_color_management) /* color management? no gamma */
  {
    return;
  } 

  xsane.block_enhancement_update = TRUE;

  gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.gamma_widget),      xsane.gamma);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.brightness_widget), xsane.brightness);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.contrast_widget),   xsane.contrast);

  if ( (xsane.xsane_channels > 1) && (!xsane.enhancement_rgb_default) )
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.gamma_red_widget),      xsane.gamma_red);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.brightness_red_widget), xsane.brightness_red);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.contrast_red_widget),   xsane.contrast_red);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.gamma_green_widget),      xsane.gamma_green);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.brightness_green_widget), xsane.brightness_green);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.contrast_green_widget),   xsane.contrast_green);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.gamma_blue_widget),      xsane.gamma_blue);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.brightness_blue_widget), xsane.brightness_blue);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(xsane.contrast_blue_widget),   xsane.contrast_blue);
  }

  xsane.block_enhancement_update = FALSE;

  xsane_update_sliders(); /* update histogram slider */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_gamma_to_histogram(double *min, double *mid, double *max,
                                     double contrast, double brightness, double gamma)
{
 double m;
 double b;

  DBG(DBG_proc, "xsane_gamma_to_histogram\n");

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

  xsane_bound_double(min, 0.0, 98.0);
  xsane_bound_double(max, *min+1, 100.0);
  xsane_bound_double(mid, *min+1, *max-1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_by_gamma(void)
{
 double min, mid, max;
 double contrast, brightness, gamma;

  DBG(DBG_proc, "xsane_enhancement_by_gamma\n");

  xsane_gamma_to_histogram(&min, &mid, &max, xsane.contrast, xsane.brightness, xsane.gamma);

  xsane.slider_gray.value[0] = min;
  xsane.slider_gray.value[1] = mid;
  xsane.slider_gray.value[2] = max;


  /* red */
  contrast   = xsane.contrast   + xsane.contrast_red;
  brightness = xsane.brightness + xsane.brightness_red;
  gamma      = xsane.gamma * xsane.gamma_red;

  if (contrast < xsane.contrast_min)
  {
    contrast = xsane.contrast_min;
  }

  xsane_gamma_to_histogram(&min, &mid, &max, contrast, brightness, gamma);


  xsane.slider_red.value[0] = min;
  xsane.slider_red.value[1] = mid;
  xsane.slider_red.value[2] = max;


  /* green */
  contrast   = xsane.contrast   + xsane.contrast_green;
  brightness = xsane.brightness + xsane.brightness_green;
  gamma      = xsane.gamma * xsane.gamma_green;

  if (contrast < xsane.contrast_min)
  {
    contrast = xsane.contrast_min;
  }

  xsane_gamma_to_histogram(&min, &mid, &max, contrast, brightness, gamma);

  xsane.slider_green.value[0] = min;
  xsane.slider_green.value[1] = mid;
  xsane.slider_green.value[2] = max;


  /* blue */
  contrast   = xsane.contrast   + xsane.contrast_blue;
  brightness = xsane.brightness + xsane.brightness_blue;
  gamma      = xsane.gamma * xsane.gamma_blue;

  if (contrast < xsane.contrast_min)
  {
    contrast = xsane.contrast_min;
  }

  xsane_gamma_to_histogram(&min, &mid, &max, contrast, brightness, gamma);

  xsane.slider_blue.value[0] = min;
  xsane.slider_blue.value[1] = mid;
  xsane.slider_blue.value[2] = max;


  xsane_enhancement_update();
  xsane_update_gamma_curve(FALSE);

  if (xsane.batch_scan_gamma_timer)
  {
    gtk_timeout_remove(xsane.batch_scan_gamma_timer);
  }
  xsane.batch_scan_gamma_timer = gtk_timeout_add(XSANE_CONTINUOUS_HOLD_TIME * 4, xsane_batch_scan_gamma_event, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_restore_default()
{
  DBG(DBG_proc, "xsane_enhancement_restore_default\n");

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
  DBG(DBG_proc, "xsane_enhancement_restore\n");

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

  xsane_refresh_dialog();
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_store()
{
  DBG(DBG_proc, "xsane_enhancement_store\n");

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

static int xsane_histogram_to_gamma(XsaneSlider *slider,
                                    double *contrast,   double contrast_offset,
                                    double *brightness, double brightness_offset,
                                    double *gamma,      double gamma_multiplier)
{
 double mid;
 double range;
 int correct_bound = ((slider->active == XSANE_SLIDER_ACTIVE) || (slider->active == XSANE_SLIDER_INACTIVE)); /* slider not moved */

  DBG(DBG_proc, "xsane_histogram_to_gamma(correct_bound = %d)\n", correct_bound);

  *contrast = (10000.0 / (slider->value[2] - slider->value[0]) - 100.0);
  if (correct_bound)
  {
    xsane_bound_double(contrast, xsane.contrast_min + contrast_offset, xsane.contrast_max + contrast_offset);
  }

  *brightness = - (slider->value[0] - 50.0) * (*contrast + 100.0)/50.0 - 100.0;
  if (correct_bound)
  {
    xsane_bound_double(brightness, xsane.brightness_min + brightness_offset, xsane.brightness_max + brightness_offset);
  }

  mid   = slider->value[1] - slider->value[0];
  range = slider->value[2] - slider->value[0];

  *gamma = log(mid/range) / log(0.5);

  if (correct_bound)
  {
    xsane_bound_double(gamma, XSANE_GAMMA_MIN * gamma_multiplier, XSANE_GAMMA_MAX * gamma_multiplier);
    return 1; /* in bound */
  }
  else if (xsane_check_bound_double(*contrast, xsane.contrast_min + contrast_offset, xsane.contrast_max + contrast_offset) &&
           xsane_check_bound_double(*brightness, xsane.brightness_min + brightness_offset, xsane.brightness_max + brightness_offset) &&
           xsane_check_bound_double(*gamma, XSANE_GAMMA_MIN * gamma_multiplier, XSANE_GAMMA_MAX * gamma_multiplier))
  {
    return 1; /* in bound */
  }

  return 0; /* out of bound */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_enhancement_by_histogram(int update_gamma)
{
 double gray_brightness;
 double gray_contrast;
 double gray_gamma;
 double brightness;
 double contrast;
 double gamma;

  DBG(DBG_proc, "xsane_enhancement_by_histogram\n");

  if (xsane_histogram_to_gamma(&xsane.slider_gray, &gray_contrast, 0, &gray_brightness, 0, &gray_gamma, 1.0))
  {
    if (update_gamma)
    {
      xsane.gamma = gray_gamma;
    }

    xsane.brightness = gray_brightness;
    xsane.contrast   = gray_contrast;
  }

  if ( (xsane.xsane_channels > 1) && (!xsane.enhancement_rgb_default) ) /* rgb sliders active */
  {
    if ((xsane.slider_gray.active == XSANE_SLIDER_ACTIVE) ||
        (xsane.slider_gray.active == XSANE_SLIDER_INACTIVE)) /* gray slider not moved */
    {
      /* red */
      if (xsane_histogram_to_gamma(&xsane.slider_red, &contrast, gray_contrast, &brightness, gray_brightness, &gamma, gray_gamma))
      {
        if (update_gamma)
        {
          xsane.gamma_red = gamma / gray_gamma;
        }
        xsane.brightness_red = brightness - gray_brightness;
        xsane.contrast_red   = contrast - gray_contrast;
      }

      /* green */
      if (xsane_histogram_to_gamma(&xsane.slider_green, &contrast, gray_contrast, &brightness, gray_brightness, &gamma, gray_gamma))
      {
        if (update_gamma)
        {
          xsane.gamma_green = gamma / gray_gamma;
        }
        xsane.brightness_green = brightness - gray_brightness;
        xsane.contrast_green   = contrast - gray_contrast;
      }

      /* blue */
      if (xsane_histogram_to_gamma(&xsane.slider_blue, &contrast, gray_contrast, &brightness, gray_brightness, &gamma, gray_gamma))
      {
        if (update_gamma)
        {
          xsane.gamma_blue = gamma / gray_gamma;
        }
        xsane.brightness_blue = brightness - gray_brightness;
        xsane.contrast_blue   = contrast - gray_contrast;
      }

    }
  }

  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_histogram_win_delete(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_histogram_win_delete\n");

  if (preferences.show_histogram)
  {
    xsane_window_get_position(xsane.histogram_dialog, &xsane.histogram_dialog_posx, &xsane.histogram_dialog_posy);
    gtk_window_move(GTK_WINDOW(xsane.histogram_dialog), xsane.histogram_dialog_posx, xsane.histogram_dialog_posy);
  }

  gtk_widget_hide(widget);
  preferences.show_histogram = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_histogram_widget), preferences.show_histogram);
  return TRUE;
}                                    

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_histogram_dialog(const char *devicetext)
{
 char windowname[TEXTBUFSIZE];
 GtkWidget *xsane_color_hbox;
 GtkWidget *xsane_histogram_vbox;  
 GtkWidget *button;  
 GdkColor color_black;
 GdkColor color_red;
 GdkColor color_green;
 GdkColor color_blue;
 GdkColor color_backg;
 GdkColormap *colormap;
 GtkStyle *style;

  DBG(DBG_proc, "xsane_create_histogram_dialog\n");

  xsane.histogram_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(xsane.histogram_dialog), FALSE);
  gtk_window_move(GTK_WINDOW(xsane.histogram_dialog), XSANE_HISTOGRAM_DIALOG_POS_X, XSANE_HISTOGRAM_DIALOG_POS_Y);
  g_signal_connect(GTK_OBJECT(xsane.histogram_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_histogram_win_delete), NULL);
  sprintf(windowname, "%s %s", WINDOW_HISTOGRAM, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.histogram_dialog), windowname);
  xsane_set_window_icon(xsane.histogram_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(xsane.histogram_dialog), xsane.accelerator_group);

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

  colormap = gdk_drawable_get_colormap(xsane.histogram_dialog->window);

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
  xsane.slider_gray.active = XSANE_SLIDER_ACTIVE;
  xsane_create_slider(&xsane.slider_gray);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_gray.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_gray.preview);
  gtk_widget_realize(xsane.slider_gray.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_red.r = 1;
  xsane.slider_red.g = 0;
  xsane.slider_red.b = 0;
  xsane.slider_red.active = XSANE_SLIDER_ACTIVE;
  xsane_create_slider(&xsane.slider_red);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_red.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_red.preview);
  gtk_widget_realize(xsane.slider_red.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_green.r = 0;
  xsane.slider_green.g = 1;
  xsane.slider_green.b = 0;
  xsane.slider_green.active = XSANE_SLIDER_ACTIVE;
  xsane_create_slider(&xsane.slider_green);
  gtk_box_pack_start(GTK_BOX(xsane_histogram_vbox), xsane.slider_green.preview, FALSE, FALSE, 0);
  gtk_widget_show(xsane.slider_green.preview);
  gtk_widget_realize(xsane.slider_green.preview);

  xsane_separator_new(xsane_histogram_vbox, 0);

  xsane.slider_blue.r = 0;
  xsane.slider_blue.g = 0;
  xsane.slider_blue.b = 1;
  xsane.slider_blue.active = XSANE_SLIDER_ACTIVE;
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

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, intensity_xpm, DESC_HIST_INTENSITY,
                                               &xsane.histogram_int,   xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_I, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, red_xpm, DESC_HIST_RED,
                                               &xsane.histogram_red,   xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_R, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, green_xpm, DESC_HIST_GREEN,
                                               &xsane.histogram_green, xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_G, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, blue_xpm, DESC_HIST_BLUE,
                                               &xsane.histogram_blue,  xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_B, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, pixel_xpm, DESC_HIST_PIXEL,
                                               &xsane.histogram_lines, xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_M, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  button = xsane_toggle_button_new_with_pixmap(xsane.histogram_dialog->window, xsane_color_hbox, log_xpm, DESC_HIST_LOG,
                                               &xsane.histogram_log, xsane_histogram_toggle_button_callback);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_L, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  gtk_widget_show(xsane_color_hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#ifdef HAVE_WORKING_GTK_GAMMACURVE
/* xsane_get_free_gamma_curve transforms gamma table with 65536 entries and value range 0.0-1.0 to requested gamma table */
/* it combines the color gamma table given by gamma_widget and the gray gamma table (xsane.gamma_curve_gray) */
/* void xsane_get_free_gamma_curve(gfloat *free_color_gamma_data, SANE_Int *gammadata, */
void xsane_get_free_gamma_curve(gfloat *free_color_gamma_data, u_char *gammadata,
                                int negative, double gamma, double brightness, double contrast,
                                int len, int maxout)
{
 int i;
 gfloat factor;
 double midin;
 double val;
 double m;
 double b;
 int maxin = len-1;

  DBG(DBG_proc, "xsane_get_free_gamma_curve\n");
  DBG(DBG_proc, "xsane_get_free_gamma_curve(neg=%d, gam=%3.2f, bri=%3.2f, ctr=%3.2f, nrs=%d, max=%d\n",
                 negative, gamma, brightness, contrast, len, maxout);

  if (contrast < -100.0)
  {
    contrast = -100.0;
  }

  midin = (int)(len / 2.0);

  m = 1.0 + contrast/100.0;
  b = (1.0 + brightness/100.0) * midin;

  factor = 65536.0 / len;

  if (1) /* xxxxxxxxxxxx colors available */
  {
    if (negative)
    {
      DBG(DBG_proc, "xsane_get_free_gamma_curve: color transformation, negative\n");

      for (i=0; i <= maxin; i++)
      {
        val = ((double) (maxin - i)) - midin;
        val = val * m + b;
        xsane_bound_double(&val, 0.0, maxin);

        val = maxout * xsane.free_gamma_data[(int) (65535 * free_color_gamma_data[(int) (val * factor)])];           
        val = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
        gammadata[i] = val;
      }
    }
    else /* positive */
    {
      DBG(DBG_proc, "xsane_get_free_gamma_curve: color transformation, positive\n");

      for (i=0; i <= maxin; i++)
      {
        val = ((double) i) - midin;
        val = val * m + b;
        xsane_bound_double(&val, 0.0, maxin);

        val = maxout * xsane.free_gamma_data[(int) (65535 * free_color_gamma_data[(int) (val * factor)])];           
        val = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
        gammadata[i] = val;
      }
    }
  }
  else
  {
    if (negative)
    {
      DBG(DBG_proc, "xsane_get_free_gamma_curve: gray transformation, negative\n");

      for (i=0; i <= maxin; i++)
      {
        val = ((double) (maxin - i)) - midin;
        val = val * m + b;
        xsane_bound_double(&val, 0.0, maxin);

        val = maxout * xsane.free_gamma_data[(int) (val * factor)];
        val = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
        gammadata[i] = val;
      }
    }
    else /* positive */
    {
      DBG(DBG_proc, "xsane_get_free_gamma_curve: gray transformation, positive\n");

      for (i=0; i <= maxin; i++)
      {
        val = ((double) i) - midin;
        val = val * m + b;
        xsane_bound_double(&val, 0.0, maxin);

        val = maxout * xsane.free_gamma_data[(int) (val * factor)];
        val = 0.5 + maxout * pow( val/maxin, (1.0/gamma) );
        gammadata[i] = val;
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_gamma_win_delete(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_gamma_win_delete\n");

  gtk_widget_hide(widget);
  preferences.show_gamma = FALSE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.show_gamma_widget), preferences.show_gamma);
  return TRUE;
}                                    

/* ---------------------------------------------------------------------------------------------------------------------- */
/* xsane_gamma_curve_notebook_page_new creates a notebook page with a gamma curve
   of 65536 entries and a value range from 0.0-1.0 */

GtkWidget* xsane_gamma_curve_notebook_page_new(GtkWidget *notebook, char *title)
{
 gfloat fmin, fmax, *vector;
 GtkWidget *curve, *gamma, *vbox, *label;
 int  optlen;

  DBG(DBG_proc, "xsane_back_gtk_curve_new\n");

  optlen = 65536;
  fmin = 0.0;
  fmax = 1.0;

  label = gtk_label_new((char *) title);
  vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show(vbox);
  gtk_widget_show(label);

  gamma = gtk_gamma_curve_new();
  gtk_widget_set_size_request(gamma, -1, 256);
  curve = GTK_GAMMA_CURVE(gamma)->curve;

  vector = alloca(optlen * sizeof(vector[0]));

  gtk_curve_set_range(GTK_CURVE(curve), 0, optlen - 1, fmin, fmax);
#if 0
  gtk_curve_maintain_accuracy(GTK_CURVE(curve), 1.0);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(gamma), 4);
  gtk_box_pack_start(GTK_BOX(vbox), gamma, TRUE, TRUE, 0);
  gtk_widget_show(gamma);

  return gamma;
}                   

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_create_gamma_dialog(const char *devicetext)
{
 char windowname[TEXTBUFSIZE];
 GtkWidget *xsane_vbox_gamma, *notebook;

  DBG(DBG_proc, "xsane_create_free_gamma_dialog\n");

  xsane.free_gamma_data       = calloc(65536, sizeof(xsane.free_gamma_data[0]));
  xsane.free_gamma_data_red   = calloc(65536, sizeof(xsane.free_gamma_data_red[0]));
  xsane.free_gamma_data_green = calloc(65536, sizeof(xsane.free_gamma_data_green[0]));
  xsane.free_gamma_data_blue  = calloc(65536, sizeof(xsane.free_gamma_data_green[0]));

  xsane.gamma_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(xsane.gamma_dialog), FALSE);
  gtk_window_move(GTK_WINDOW(xsane.gamma_dialog), XSANE_GAMMA_POS_X, XSANE_GAMMA_POS_Y);
  g_signal_connect(GTK_OBJECT(xsane.gamma_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_gamma_win_delete), NULL);
  sprintf(windowname, "%s %s", WINDOW_GAMMA, devicetext);
  gtk_window_set_title(GTK_WINDOW(xsane.gamma_dialog), windowname);
  xsane_set_window_icon(xsane.gamma_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(xsane.gamma_dialog), xsane.accelerator_group);

  xsane_vbox_gamma = gtk_vbox_new(TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(xsane_vbox_gamma), 5);
  gtk_container_add(GTK_CONTAINER(xsane.gamma_dialog), xsane_vbox_gamma);
  gtk_widget_show(xsane_vbox_gamma);

  notebook = gtk_notebook_new();
  gtk_container_set_border_width(GTK_CONTAINER(notebook), 4);
  gtk_box_pack_start(GTK_BOX(xsane_vbox_gamma), notebook, TRUE, TRUE, 0);   
  gtk_widget_show(notebook);

  xsane.gamma_curve_gray  = xsane_gamma_curve_notebook_page_new(notebook, "Gamma gray");
  xsane.gamma_curve_red   = xsane_gamma_curve_notebook_page_new(notebook, "Gamma red");
  xsane.gamma_curve_green = xsane_gamma_curve_notebook_page_new(notebook, "Gamma green");
  xsane.gamma_curve_blue  = xsane_gamma_curve_notebook_page_new(notebook, "Gamma blue");

  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_gray)->curve),  65536, xsane.free_gamma_data);
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_red)->curve),   65536, xsane.free_gamma_data_red);
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_green)->curve), 65536, xsane.free_gamma_data_green);
  gtk_curve_get_vector(GTK_CURVE(GTK_GAMMA_CURVE(xsane.gamma_curve_blue)->curve),  65536, xsane.free_gamma_data_blue);

}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_update_gamma_dialog()
{
  DBG(DBG_proc, "xsane_update_gamma_dialog\n");

  if (preferences.show_gamma)
  {
    gtk_widget_show(xsane.gamma_dialog);
  }
}
#endif


/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_auto_enhancement()
{
  DBG(DBG_proc, "xsane_set_auto_enhancement\n");
  xsane.slider_gray.value[0] = xsane.auto_black;
  xsane.slider_gray.value[1] = xsane.auto_gray;
  xsane.slider_gray.value[2] = xsane.auto_white;

  if (xsane.enhancement_rgb_default) /* set same values for color components */
  {
    xsane.slider_red.value[0] = xsane.auto_black;
    xsane.slider_red.value[1] = xsane.auto_gray;
    xsane.slider_red.value[2] = xsane.auto_white;
  
    xsane.slider_green.value[0] = xsane.auto_black;
    xsane.slider_green.value[1] = xsane.auto_gray;
    xsane.slider_green.value[2] = xsane.auto_white;
  
    xsane.slider_blue.value[0] = xsane.auto_black;
    xsane.slider_blue.value[1] = xsane.auto_gray;
    xsane.slider_blue.value[2] = xsane.auto_white;
  }
  else /* set different values for each color component */
  {
    xsane.slider_red.value[0] = xsane.auto_black_red;
    xsane.slider_red.value[1] = xsane.auto_gray_red;
    xsane.slider_red.value[2] = xsane.auto_white_red;
  
    xsane.slider_green.value[0] = xsane.auto_black_green;
    xsane.slider_green.value[1] = xsane.auto_gray_green;
    xsane.slider_green.value[2] = xsane.auto_white_green;
  
    xsane.slider_blue.value[0] = xsane.auto_black_blue;
    xsane.slider_blue.value[1] = xsane.auto_gray_blue;
    xsane.slider_blue.value[2] = xsane.auto_white_blue;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* set medium values as gamma/contrast/brightness */
void xsane_apply_medium_definition_as_enhancement(Preferences_medium_t *medium)
{
  xsane.gamma      = medium->gamma_gray;
  xsane.contrast   = 10000.0 / (medium->highlight_gray  - medium->shadow_gray)  - 100.0;
  xsane.brightness = - (medium->shadow_gray  - 50.0) * (xsane.contrast + 100.0) / 50.0 - 100.0;

  xsane.gamma_red   = medium->gamma_red   / xsane.gamma;
  xsane.gamma_green = medium->gamma_green / xsane.gamma;
  xsane.gamma_blue  = medium->gamma_blue  / xsane.gamma;

  xsane.contrast_red   = 10000.0 / (medium->highlight_red   - medium->shadow_red)   - 100.0 - xsane.contrast;
  xsane.contrast_green = 10000.0 / (medium->highlight_green - medium->shadow_green) - 100.0 - xsane.contrast;
  xsane.contrast_blue  = 10000.0 / (medium->highlight_blue  - medium->shadow_blue)  - 100.0 - xsane.contrast;

  xsane.brightness_red   = - (medium->shadow_red   - 50.0) * (xsane.contrast + xsane.contrast_red   + 100.0) / 50.0 - 100.0 - xsane.brightness;
  xsane.brightness_green = - (medium->shadow_green - 50.0) * (xsane.contrast + xsane.contrast_green + 100.0) / 50.0 - 100.0 - xsane.brightness;
  xsane.brightness_blue  = - (medium->shadow_blue  - 50.0) * (xsane.contrast + xsane.contrast_blue  + 100.0) / 50.0 - 100.0 - xsane.brightness;

  xsane.negative = medium->negative;

  if (xsane.negative)
  {
    xsane.brightness       = -xsane.brightness;
    xsane.brightness_red   = -xsane.brightness_red;
    xsane.brightness_green = -xsane.brightness_green;
    xsane.brightness_blue  = -xsane.brightness_blue;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* set medium values */
void xsane_set_medium(Preferences_medium_t *medium)
{
 const SANE_Option_Descriptor *opt;
 int shadow_gray, shadow_red, shadow_green, shadow_blue;
 int highlight_gray, highlight_red, highlight_green, highlight_blue;


  if (!medium)
  {
    DBG(DBG_proc, "xsane_set_medium: no medium given, using default values\n");

    xsane.medium_gamma_gray  = 1.0;
    xsane.medium_gamma_red   = 1.0;
    xsane.medium_gamma_green = 1.0;
    xsane.medium_gamma_blue  = 1.0;
 
    xsane.medium_shadow_gray  = 0.0;
    xsane.medium_shadow_red   = 0.0;
    xsane.medium_shadow_green = 0.0;
    xsane.medium_shadow_blue  = 0.0;
 
    xsane.medium_highlight_gray  = 100.0;
    xsane.medium_highlight_red   = 100.0;
    xsane.medium_highlight_green = 100.0;
    xsane.medium_highlight_blue  = 100.0;
 
    xsane.medium_negative = 0;

   return;
  }
 
  DBG(DBG_proc, "xsane_set_medium: setting values for %s\n", medium->name);

  xsane.medium_gamma_red   = medium->gamma_red;
  xsane.medium_gamma_green = medium->gamma_green;
  xsane.medium_gamma_blue  = medium->gamma_blue;

  xsane.medium_shadow_red   = medium->shadow_red;
  xsane.medium_shadow_green = medium->shadow_green;
  xsane.medium_shadow_blue  = medium->shadow_blue;

  xsane.medium_highlight_red   = medium->highlight_red;
  xsane.medium_highlight_green = medium->highlight_green;
  xsane.medium_highlight_blue  = medium->highlight_blue;

  xsane.medium_gamma_gray      = medium->gamma_gray;
  xsane.medium_shadow_gray     = medium->shadow_gray;
  xsane.medium_highlight_gray  = medium->highlight_gray;
 
  xsane.medium_negative = medium->negative;
 
#if 1
return;
#endif
 
  opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.shadow);
  if (!opt)
  {
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.shadow_r);
  }
  else if (!opt)
  {
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.highlight);
  }
  else if (!opt)
  {
    opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.highlight_r);
  }
  else if (!opt)
  {
    DBG(DBG_info, "xsane_set_medium_callback: no shadow/highlight values available\n");
   return;
  }

  if (opt->type == SANE_TYPE_FIXED)
  {
    shadow_gray     = SANE_FIX(medium->shadow_gray);
    shadow_red      = SANE_FIX(medium->shadow_red);
    shadow_green    = SANE_FIX(medium->shadow_green);
    shadow_blue     = SANE_FIX(medium->shadow_blue);
    highlight_gray  = SANE_FIX(medium->highlight_gray);
    highlight_red   = SANE_FIX(medium->highlight_red);
    highlight_green = SANE_FIX(medium->highlight_green);
    highlight_blue  = SANE_FIX(medium->highlight_blue);
  }
  else if (opt->type == SANE_TYPE_INT)
  {
    shadow_gray     = (int) medium->shadow_gray;
    shadow_red      = (int) medium->shadow_red;
    shadow_green    = (int) medium->shadow_green;
    shadow_blue     = (int) medium->shadow_blue;
    highlight_gray  = (int) medium->highlight_gray;
    highlight_red   = (int) medium->highlight_red;
    highlight_green = (int) medium->highlight_green;
    highlight_blue  = (int) medium->highlight_blue;
  }
  else
  {
    DBG(DBG_info, "xsane_set_medium_callback: unknown type of shadow/highlight: %d\n", opt->type);
   return;
  }

  /* shadow values */ 
  if (!xsane_control_option(xsane.dev, xsane.well_known.shadow,      SANE_ACTION_SET_VALUE, &shadow_gray, 0))
  {
    xsane.medium_shadow_gray = 0.0; /* we are using hardware shadow */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.shadow_r,    SANE_ACTION_SET_VALUE, &shadow_red, 0))
  {
    xsane.medium_shadow_red = 0.0; /* we are using hardware shadow */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.shadow_g,    SANE_ACTION_SET_VALUE, &shadow_green, 0))
  {
    xsane.medium_shadow_green = 0.0; /* we are using hardware shadow */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.shadow_b,    SANE_ACTION_SET_VALUE, &shadow_blue, 0))
  {
    xsane.medium_shadow_blue = 0.0; /* we are using hardware shadow */
  }                                                                                                         

  /* highlight values */
  if (!xsane_control_option(xsane.dev, xsane.well_known.highlight,   SANE_ACTION_SET_VALUE, &highlight_gray, 0))
  {
    xsane.medium_highlight_gray = 100.0; /* we are using hardware highlight */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.highlight_r, SANE_ACTION_SET_VALUE, &highlight_red, 0))
  {
    xsane.medium_highlight_red = 100.0; /* we are using hardware highlight */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.highlight_g, SANE_ACTION_SET_VALUE, &highlight_green, 0))
  {
    xsane.medium_highlight_green = 100.0; /* we are using hardware highlight */
  }
 
  if (!xsane_control_option(xsane.dev, xsane.well_known.highlight_b, SANE_ACTION_SET_VALUE, &highlight_blue, 0))
  {
    xsane.medium_highlight_blue = 100.0; /* we are using hardware highlight */
  }
 
  xsane_back_gtk_refresh_dialog();
}                                                                                                             

/* ---------------------------------------------------------------------------------------------------------------------- */
