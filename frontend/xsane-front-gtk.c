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
#include "xsane-back-gtk.h"
#include "xsane.h"
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-text.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_set_resolution(void);
void xsane_close_dialog_callback(GtkWidget *widget, gpointer data);
void xsane_authorization_button_callback(GtkWidget *widget, gpointer data);
gint xsane_authorization_callback(SANE_String_Const resource,
                                  SANE_Char username[SANE_MAX_USERNAME_LEN],
                                  SANE_Char password[SANE_MAX_PASSWORD_LEN]);
void xsane_progress_cancel(GtkWidget *widget, gpointer data);
XsaneProgress_t *xsane_progress_new(char *title, char *text, GtkSignalFunc callback, gpointer callback_data);
void xsane_progress_free(XsaneProgress_t *p);
void xsane_progress_update(XsaneProgress_t *p, gfloat newval);
void xsane_toggle_button_new_with_pixmap(GtkWidget *parent, const char *xpm_d[], const char *desc,
                                         int *state, void *xsane_toggle_button_callback);
GtkWidget *xsane_button_new_with_pixmap(GtkWidget *parent, const char *xpm_d[], const char *desc,
                                        void *xsane_button_callback, gpointer data);
void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc);
void xsane_scale_new(GtkBox *parent, char *labeltext, const char *desc,
                     float min, float max, float quant, float step, float xxx,
                     int digits, double *val, GtkObject **data, void *xsane_scale_callback);
void xsane_scale_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
                                 float min, float max, float quant, float step, float xxx,
                                 int digits, double *val, GtkObject **data, int option, void *xsane_scale_callback);
void xsane_option_menu_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkObject **data, int option, void *xsane_scale_callback);
void xsane_separator_new(GtkWidget *xsane_parent, int dist);
GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn);
GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_resolution(void)
{
 const SANE_Option_Descriptor *opt;
 SANE_Word dpi;

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
  sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_SET_VALUE, &dpi, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_close_dialog_callback(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = data;

  gtk_widget_destroy(dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int authorization_flag;

void xsane_authorization_button_callback(GtkWidget *widget, gpointer data)
{
  authorization_flag = (long) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

gint xsane_authorization_callback(SANE_String_Const resource,
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
  snprintf(buf, sizeof(buf), "%s authorization", prog_name);
  gtk_window_set_title(GTK_WINDOW(authorize_dialog), buf);

  vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 10);
  gtk_container_add(GTK_CONTAINER(authorize_dialog), vbox);
  gtk_widget_show(vbox);

  snprintf(buf, sizeof(buf), "\n\nAuthorization required for %s\n", resource);
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

void xsane_progress_cancel(GtkWidget *widget, gpointer data)
{
  XsaneProgress_t *p = (XsaneProgress_t *) data;

  (*p->callback) ();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

XsaneProgress_t *xsane_progress_new(char *title, char *text, GtkSignalFunc callback, gpointer callback_data)
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

void xsane_progress_free(XsaneProgress_t *p)
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

void xsane_toggle_button_new_with_pixmap(GtkWidget *parent, const char *xpm_d[], const char *desc,
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

GtkWidget *xsane_button_new_with_pixmap(GtkWidget *parent, const char *xpm_d[], const char *desc,
                                  void *xsane_button_callback, gpointer data)
{
  GtkWidget *button;
  GtkWidget *pixmapwidget;
  GdkBitmap *mask;
  GdkPixmap *pixmap;

  button = gtk_button_new();
  gsg_set_tooltip(dialog->tooltips, button, desc);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_container_add(GTK_CONTAINER(button), pixmapwidget);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);

  if (xsane_button_callback)
  {
    gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_button_callback, data);
  }
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  return(button);
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

void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc)
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

void xsane_scale_new(GtkBox *parent, char *labeltext, const char *desc,
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

void xsane_scale_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
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

void xsane_option_menu_new_with_pixmap(GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkObject **data, int option, void *xsane_option_menu_callback)
{
 GtkWidget *hbox;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.shell->window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);

  xsane_option_menu_new(hbox, str_list, val, option, desc);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_separator_new(GtkWidget *xsane_parent, int dist)
{
 GtkWidget *xsane_separator;

  xsane_separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(xsane_parent), xsane_separator, FALSE, FALSE, dist);
  gtk_widget_show(xsane_separator);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn)
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
GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text)
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
