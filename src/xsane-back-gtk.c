/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-back-gtk.c

   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998-2000 Oliver Rauch
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

/* ----------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-back-gtk.h"
#include "xsane-preferences.h"
#include "xsane-gamma.h"

/* ----------------------------------------------------------------------------------------------------------------- */

/* extern declarations */
extern void xsane_panel_build(GSGDialog *dialog);

/* ----------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */
SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info);
const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option);
const char *xsane_back_gtk_unit_string(SANE_Unit unit);
void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const char *desc);
int xsane_back_gtk_make_path(size_t buf_size, char *buf, const char *prog_name, const char *dir_name,
                             const char *prefix, const char *dev_name, const char *postfix, int location);
void xsane_back_gtk_set_option(GSGDialog * dialog, int opt_num, void *val, SANE_Action action);

static void xsane_back_gtk_panel_rebuild(GSGDialog *dialog);
void xsane_set_sensitivity(SANE_Int sensitivity);
void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d);

/* ----------------------------------------------------------------------------------------------------------------- */

const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
  if (option >= 0)
  {
    return sane_get_option_descriptor(handle, option);
  }
 return NULL;
}

/* ----------------------------------------------------------------------------------------------------------------- */

SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info)
{
  if (option >= 0)
  {
    return sane_control_option(handle, option, action, val, info);
  }

 return SANE_STATUS_INVAL;
}

/* ----------------------------------------------------------------------------------------------------------------- */

const char *xsane_back_gtk_unit_string(SANE_Unit unit)
{
  switch (unit)
  {
    case SANE_UNIT_NONE:	return "none";
    case SANE_UNIT_PIXEL:	return "pixel";
    case SANE_UNIT_BIT:		return "bit";
    case SANE_UNIT_DPI:		return "dpi";
    case SANE_UNIT_PERCENT:	return "%";
    case SANE_UNIT_MM:
      if (preferences.length_unit > 9.9 && preferences.length_unit < 10.1)
      {
	return "cm";
      }
      else if (preferences.length_unit > 25.3 && preferences.length_unit < 25.5)
      {
	return "in";
      }
      return "mm";
    case SANE_UNIT_MICROSECOND:	return "\265s";
  }
  return 0;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const char *desc)
{
  if (desc && desc[0])
  {
    gtk_tooltips_set_tip(tooltips, widget, desc, 0);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_make_path(size_t buf_size, char *buf, const char *prog_name, const char *dir_name,
                             const char *prefix, const char *dev_name, const char *postfix, int location)
{
  struct passwd *pw;
  size_t len, extra;
  int i;

  if (location == XSANE_PATH_LOCAL_SANE) /* make path to local file */
  {
    pw = getpwuid(getuid()); /* get homedirectory */
    if (!pw)
    {
      snprintf(buf, buf_size, "%s %s", ERR_HOME_DIR, strerror(errno));
      xsane_back_gtk_error(buf, FALSE);
      return -1;
    }

    snprintf(buf, buf_size, "%s/.sane", pw->pw_dir);
    mkdir(buf, 0777);	/* ensure ~/.sane directory exists */
  }
  else if (location == XSANE_PATH_SYSTEM) /* make path to system file */
  {
    snprintf(buf, buf_size, "%s", STRINGIFY(PATH_SANE_DATA_DIR));
  }
  else /* make path to temporary file */
  {
    snprintf(buf, buf_size, "%s", preferences.tmp_path);
  }

  len = strlen(buf);

  if (prog_name)
  {
    extra = strlen(prog_name);
    if (len + extra + 1 >= buf_size)
    {
      goto filename_too_long;
    }

    buf[len++] = '/';
    memcpy(buf + len, prog_name, extra);
    len += extra;
    buf[len] = '\0';
    mkdir(buf, 0777);	/* ensure ~/.sane/PROG_NAME directory exists */
  }
  if (len >= buf_size)
  {
    goto filename_too_long;
  }

  buf[len++] = '/';


  if (dir_name)
  {
    extra = strlen(dir_name);
    if (len + extra + 1 >= buf_size)
    {
      goto filename_too_long;
    }

    buf[len++] = '/';
    memcpy(buf + len, dir_name, extra);
    len += extra;
    buf[len] = '\0';
    mkdir(buf, 0777);	/* ensure DIR_NAME directory exists */
  }

  if (len >= buf_size)
  {
    goto filename_too_long;
  }

  buf[len++] = '/';


  if (prefix)
  {
    extra = strlen(prefix);
    if (len + extra >= buf_size)
    {
      goto filename_too_long;
    }

    memcpy(buf + len, prefix, extra);
    len += extra;
  }

  if (location == XSANE_PATH_TMP) /* tmp dir, add uid */
  {
   char tmpbuf[256];
   uid_t uid;
   int rnd;

    uid = getuid();
    snprintf(tmpbuf, sizeof(tmpbuf), "%d-", uid);
                                                             
    extra = strlen(tmpbuf);
    if (len + extra >= buf_size)
    {
      goto filename_too_long;
    }

    memcpy(buf + len, tmpbuf, extra);
    len += extra;

    if (len + 7 >= buf_size)
    {
      goto filename_too_long;
    }

    memcpy(buf + len, "XXXXXX", 6);		/* create unique filename */
    len += 6;
    buf[len] = '\0';
    memcpy(buf, mktemp(buf), len);

    rnd = random() & 65535;			/* add random number */
    snprintf(tmpbuf, sizeof(tmpbuf), "%05d-", rnd);
    memcpy(buf+len, tmpbuf, strlen(tmpbuf));
    len += 6;
  }

  if (dev_name)
  {
      /* Turn devicename into valid filename by replacing slashes by "_", "_" gets "__", spaces are erased  */

    for (i = 0; dev_name[i]; ++i)
    {
      if (len + 2 >= buf_size)
      {
        goto filename_too_long;
      }

      switch (dev_name[i])
      {
        case '/':	/* "/" -> "_" */
          buf[len++] = '_'; 
         break;

        case ' ':	/* erase " " */
         break;

        case '_':	/* "_" -> "__" */
          buf[len++] = '_';
          /* fall through */
        default:
          buf[len++] = dev_name[i];
         break;
      }
    }
  }

  if (postfix)
  {
    extra = strlen(postfix);
    if (len + extra >= buf_size)
    {
      goto filename_too_long;
    }
    memcpy(buf + len, postfix, extra);
    len += extra;
  }
  if (len >= buf_size)
    goto filename_too_long;

  buf[len++] = '\0';
  return 0;

filename_too_long:
  xsane_back_gtk_error(ERR_FILENAME_TOO_LONG, FALSE);
  errno = E2BIG;
  return -1;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_option(GSGDialog * dialog, int opt_num, void *val, SANE_Action action)
{
  SANE_Status status;
  SANE_Int info;
  char buf[256];

  status = xsane_control_option(dialog->dev, opt_num, action, val, &info);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_SET_OPTION, xsane_get_option_descriptor(dialog->dev, opt_num)->name,
             XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, FALSE);
    return;
  }

  if ((info & SANE_INFO_RELOAD_PARAMS) && dialog->param_change_callback)
  {
    (*dialog->param_change_callback) (dialog, dialog->param_change_arg);
  }

  if (info & SANE_INFO_RELOAD_OPTIONS)
  {
    xsane_back_gtk_panel_rebuild(dialog);
    if (dialog->option_reload_callback)
    {
      (*dialog->option_reload_callback) (dialog, dialog->option_reload_arg);
    }
    xsane_enhancement_by_gamma(); /* WARNING: THIS IS A TEST xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_close_dialog_callback(GtkWidget * widget, gpointer data)
{
  gtk_widget_destroy(data);
  xsane_back_gtk_message_dialog_active = 0;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static gint decision_flag;
static GtkWidget *decision_dialog;

void xsane_back_gtk_decision_callback(GtkWidget * widget, gpointer data)
{
  gtk_widget_destroy(decision_dialog);
  xsane_back_gtk_message_dialog_active = 0;
  decision_flag = (long) data;
}

/* ----------------------------------------------------------------------------------------------------------------- */

gint xsane_back_gtk_decision(gchar *title, gchar **xpm_d,  gchar *message, gchar *oktext, gchar *rejecttext, gint wait)
{
 GtkWidget *main_vbox, *hbox, *label, *button;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 GtkWidget *pixmapwidget;

  if (xsane_back_gtk_message_dialog_active)
  {
    fprintf(stderr, "%s: %s\n", title, message);
    return TRUE;
  }
  xsane_back_gtk_message_dialog_active = 1;
  decision_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_position(GTK_WINDOW(decision_dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title(GTK_WINDOW(decision_dialog), title);
  gtk_signal_connect(GTK_OBJECT(decision_dialog), "delete_event",
                     GTK_SIGNAL_FUNC(xsane_back_gtk_decision_callback), (void *) -1); /* -1 = cancel */

  xsane_set_window_icon(decision_dialog, 0);

  /* create the main vbox */
  main_vbox = gtk_vbox_new(TRUE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 5);
  gtk_widget_show(main_vbox);

  gtk_container_add(GTK_CONTAINER(decision_dialog), main_vbox);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

  /* the info icon */
  if (xpm_d)
  {
    pixmap = gdk_pixmap_create_from_xpm_d(decision_dialog->window, &mask, xsane.bg_trans, xpm_d);
    pixmapwidget = gtk_pixmap_new(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 10);
    gtk_widget_show(pixmapwidget);
    gdk_pixmap_unref(pixmap);
  }

  /* the message */
  label = gtk_label_new(message);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  gtk_widget_show(hbox);


  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

  /* the confirmation button */
  button = gtk_button_new_with_label(oktext);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_back_gtk_decision_callback, (void *) 1 /* confirm */);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);


  if (rejecttext) /* the rejection button */
  {
    button = gtk_button_new_with_label(rejecttext);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_back_gtk_decision_callback, (void *) -1 /* reject */);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    gtk_widget_show(button);
  }
  gtk_widget_show(hbox);

  gtk_widget_show(decision_dialog);

  if (!wait)
  {
    return TRUE;
  }

  decision_flag = 0;

  while (decision_flag == 0)
  {
    gtk_main_iteration();
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  if (decision_flag == 1)
  {
    return TRUE;
  }

  return FALSE;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_message(gchar *title, gchar **icon_xpm, gchar *message, gint wait)
{
  xsane_back_gtk_decision(title, icon_xpm, message, BUTTON_OK, 0 /* no reject text */, wait);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_error(gchar *error, gint wait)
{
  if (wait)
  {
   SANE_Int old_sensitivity = xsane.sensitivity;

    xsane_set_sensitivity(FALSE);
    xsane_back_gtk_message(ERR_HEADER_ERROR, (gchar**) error_xpm, error, wait);
    xsane_set_sensitivity(old_sensitivity);
  }
  else
  {
    xsane_back_gtk_message(ERR_HEADER_ERROR, (gchar **) error_xpm, error, wait);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_warning(gchar *warning, gint wait)
{
  if (wait)
  {
   SANE_Int old_sensitivity = xsane.sensitivity;

    xsane_set_sensitivity(FALSE);
    xsane_back_gtk_message(ERR_HEADER_WARNING, (gchar**) warning_xpm, warning, wait);
    xsane_set_sensitivity(old_sensitivity);
  }
  else
  {
    xsane_back_gtk_message(ERR_HEADER_WARNING, (gchar**) warning_xpm, warning, wait);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_get_filename_button_clicked(GtkWidget *w, gpointer data)
{
  int *clicked = data;
  *clicked = 1;
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_get_filename(const char *label, const char *default_name, size_t max_len, char *filename, int show_fileopts)
{
  int cancel = 0, ok = 0, destroy = 0;
  GtkWidget *fileselection;

  fileselection = gtk_file_selection_new((char *) label);

  gtk_signal_connect(GTK_OBJECT(fileselection),
                     "destroy", GTK_SIGNAL_FUNC(xsane_back_gtk_get_filename_button_clicked), &destroy);         
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselection)->cancel_button),
		     "clicked", (GtkSignalFunc) xsane_back_gtk_get_filename_button_clicked, &cancel);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselection)->ok_button),
		     "clicked", (GtkSignalFunc) xsane_back_gtk_get_filename_button_clicked, &ok);
  if (default_name)
  {
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselection), (char *) default_name);
  }

  if (show_fileopts)
  {
    gtk_file_selection_show_fileop_buttons(GTK_FILE_SELECTION(fileselection));
  }
  else
  {
    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(fileselection));
  }

  gtk_widget_show(fileselection);

  while (!cancel && !ok && !destroy)
  {
    if (!gtk_events_pending())
    {
      usleep(100000);
    }
    gtk_main_iteration();
  }

  if (ok)
  {
   size_t len, cwd_len;
   char *cwd;

    strncpy(filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselection)), max_len - 1);
    filename[max_len - 1] = '\0';

    len = strlen(filename);
    cwd = alloca(len + 2);
    getcwd(cwd, len + 1);
    cwd_len = strlen(cwd);
    cwd[cwd_len++] = '/';
    cwd[cwd_len] = '\0';
    if (strncmp(filename, cwd, cwd_len) == 0)
    {
      memcpy(filename, filename + cwd_len, len - cwd_len + 1);
    }
  }

  if (!destroy)
  {
    gtk_widget_destroy(fileselection);
  }

  return ok ? 0 : -1;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static gint xsane_back_gtk_autobutton_update(GtkWidget *widget, GSGDialogElement *elem)
{
  GSGDialog *dialog = elem->dialog;
  int opt_num = elem - dialog->element;
  const SANE_Option_Descriptor *opt;
  SANE_Status status;
  SANE_Word val;
  char buf[256];

  opt = xsane_get_option_descriptor(dialog->dev, opt_num);
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    xsane_back_gtk_set_option(dialog, opt_num, 0, SANE_ACTION_SET_AUTO);
  }
  else
  {
    status = xsane_control_option(dialog->dev, opt_num, SANE_ACTION_GET_VALUE, &val, 0);
    if (status != SANE_STATUS_GOOD)
    {
      snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_GET_OPTION, opt->name, XSANE_STRSTATUS(status));
      xsane_back_gtk_error(buf, FALSE);
    }
    xsane_back_gtk_set_option(dialog, opt_num, &val, SANE_ACTION_SET_VALUE);
  }
  return FALSE;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_autobutton_new(GtkWidget *parent, GSGDialogElement *elem,
		GtkWidget *label, GtkTooltips *tooltips)
{
  GtkWidget *button, *alignment;

  button = gtk_check_button_new();
  gtk_container_set_border_width(GTK_CONTAINER(button), 0);
  gtk_widget_set_usize(button, 20, 20);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_back_gtk_autobutton_update, elem);
  xsane_back_gtk_set_tooltip(tooltips, button, "Turns on automatic mode.");

  alignment = gtk_alignment_new(0.0, 1.0, 0.5, 0.5);
  gtk_container_add(GTK_CONTAINER(alignment), button);

  gtk_box_pack_end(GTK_BOX(parent), label, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(parent), alignment, FALSE, FALSE, 2);

  gtk_widget_show(alignment);
  gtk_widget_show(button);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static gint xsane_back_gtk_button_update(GtkWidget * widget, GSGDialogElement * elem)
{
  GSGDialog *dialog = elem->dialog;
  int opt_num = elem - dialog->element;
  const SANE_Option_Descriptor *opt;
  SANE_Word val = SANE_FALSE;

  opt = xsane_get_option_descriptor(dialog->dev, opt_num);
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    val = SANE_TRUE;
  }
  xsane_back_gtk_set_option(dialog, opt_num, &val, SANE_ACTION_SET_VALUE);

  return FALSE;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_button_new(GtkWidget * parent, const char *name, SANE_Word val,
	    GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
  GtkWidget *button;

  button = gtk_check_button_new_with_label((char *) name);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), val);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_back_gtk_button_update, elem);
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, TRUE, 0);
  gtk_widget_show(button);
  xsane_back_gtk_set_tooltip(tooltips, button, desc);

  gtk_widget_set_sensitive(button, settable);

  elem->widget = button;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_scale_update(GtkAdjustment * adj_data, GSGDialogElement * elem)
{
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  SANE_Word val, new_val;
  int opt_num;
  double d;

  opt_num = elem - dialog->element;
  opt = xsane_get_option_descriptor(dialog->dev, opt_num);
  switch(opt->type)
  {
    case SANE_TYPE_INT:
      val = adj_data->value + 0.5;
      break;

    case SANE_TYPE_FIXED:
      d = adj_data->value;
      if (opt->unit == SANE_UNIT_MM)
      {
	d *= preferences.length_unit;
      }
      val = SANE_FIX(d);
      break;

    default:
      fprintf(stderr, "xsane_back_gtk_scale_update: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
      return;
  }

  xsane_back_gtk_set_option(dialog, opt_num, &val, SANE_ACTION_SET_VALUE);
  xsane_control_option(dialog->dev, opt_num, SANE_ACTION_GET_VALUE, &new_val, 0);
  if (new_val != val)
  {
    val = new_val;
    goto value_changed;
  }
 return;			/* value didn't change */

value_changed:
  switch(opt->type)
    {
    case SANE_TYPE_INT:
      adj_data->value = val;
      break;

    case SANE_TYPE_FIXED:
      d = SANE_UNFIX(val);
      if (opt->unit == SANE_UNIT_MM)
      {
	d /= preferences.length_unit;
      }
      adj_data->value = d;
      break;

    default:
      break;
    }
  /* Let widget know that value changed _again_.  This must converge
     quickly---otherwise things would get very slow very quickly (as
     in "infinite recursion"): */
  gtk_signal_emit_by_name(GTK_OBJECT(adj_data), "value_changed");
 return;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_scale_new(GtkWidget * parent, const char *name, gfloat val,
	   gfloat min, gfloat max, gfloat quant, int automatic,
	   GSGDialogElement * elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
  GtkWidget *hbox, *label, *scale;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  elem->data = gtk_adjustment_new(val, min, max, quant, quant*10, 0.0);
  scale = gtk_hscale_new(GTK_ADJUSTMENT(elem->data));
  xsane_back_gtk_set_tooltip(tooltips, scale, desc);
  gtk_widget_set_usize(scale, 150, 0);

  if (automatic)
  {
    xsane_back_gtk_autobutton_new(hbox, elem, scale, tooltips);
  }
  else
  {
    gtk_box_pack_end(GTK_BOX(hbox), scale, FALSE, FALSE, 0); /* make scales fixed */
/*    gtk_box_pack_end(GTK_BOX(hbox), scale, TRUE, TRUE, 0); */ /* make scales sizeable */
  }

  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
  gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);

  if (quant - (int) quant == 0.0)
  {
    gtk_scale_set_digits(GTK_SCALE(scale), 0);
  }
  else
  {
    /* set number of digits in dependacne of quantization */
    gtk_scale_set_digits(GTK_SCALE(scale), (int) log10(1/quant)+0.8);
  }

  gtk_signal_connect(elem->data, "value_changed", (GtkSignalFunc) xsane_back_gtk_scale_update, elem);

  gtk_widget_show(label);
  gtk_widget_show(scale);
  gtk_widget_show(hbox);

  gtk_widget_set_sensitive(scale, settable);

  elem->widget = scale;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_push_button_callback(GtkWidget * widget, gpointer data)
{
  GSGDialogElement *elem = data;
  GSGDialog *dialog = elem->dialog;
  int opt_num;

  opt_num = elem - dialog->element;
  xsane_back_gtk_set_option(dialog, opt_num, 0, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static int xsane_back_gtk_option_menu_lookup(GSGMenuItem menu_items[], const char *string)
{
  int i;

  for (i = 0; strcmp(menu_items[i].label, string) != 0; ++i);

 return i;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_option_menu_callback(GtkWidget * widget, gpointer data)
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
  opt = xsane_get_option_descriptor(dialog->dev, opt_num);
  switch(opt->type)
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
      fprintf(stderr, "xsane_back_gtk_option_menu_callback: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
      break;
  }
  xsane_back_gtk_set_option(dialog, opt_num, valp, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_option_menu_new(GtkWidget *parent, const char *name, char *str_list[],
		 const char *val, GSGDialogElement * elem,
		 GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *hbox, *label, *option_menu, *menu, *item;
 GSGMenuItem *menu_items;
 int i, num_items;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  for (num_items = 0; str_list[num_items]; ++num_items);
  menu_items = malloc(num_items * sizeof(menu_items[0]));

  menu = gtk_menu_new();
  for (i = 0; i < num_items; ++i)
  {
    item = gtk_menu_item_new_with_label(_BGT(str_list[i]));
    gtk_container_add(GTK_CONTAINER(menu), item);
    gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_back_gtk_option_menu_callback, menu_items + i);

    gtk_widget_show(item);

    menu_items[i].label = str_list[i];
    menu_items[i].elem = elem;
    menu_items[i].index = i;
  }

  option_menu = gtk_option_menu_new();
  gtk_box_pack_end(GTK_BOX(hbox), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_back_gtk_option_menu_lookup(menu_items, val));
  xsane_back_gtk_set_tooltip(tooltips, option_menu, desc);

  gtk_widget_show(label);
  gtk_widget_show(option_menu);
  gtk_widget_show(hbox);

  gtk_widget_set_sensitive(option_menu, settable);

  elem->widget = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_text_entry_callback(GtkWidget *w, gpointer data)
{
  GSGDialogElement *elem = data;
  const SANE_Option_Descriptor *opt;
  GSGDialog *dialog = elem->dialog;
  gchar *text;
  int opt_num;
  char *buf;

  opt_num = elem - dialog->element;
  opt = xsane_get_option_descriptor(dialog->dev, opt_num);

  buf = alloca(opt->size);
  buf[0] = '\0';

  text = gtk_entry_get_text(GTK_ENTRY(elem->widget));
  if (text)
  {
    strncpy(buf, text, opt->size);
  }
  buf[opt->size - 1] = '\0';

  xsane_back_gtk_set_option(dialog, opt_num, buf, SANE_ACTION_SET_VALUE);

  if (strcmp(buf, text) != 0) /* the backend modified the option value; update widget: */
  {
    gtk_entry_set_text(GTK_ENTRY(elem->widget), buf);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_text_entry_new(GtkWidget * parent, const char *name, const char *val, GSGDialogElement *elem,
		        GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
  GtkWidget *hbox, *text, *label;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  text = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text), (char *) val);
/*  gtk_box_pack_start(GTK_BOX(hbox), text, FALSE, TRUE, 0); */ /* text entry fixed */
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0); /* text entry sizeable */
  gtk_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_back_gtk_text_entry_callback, elem);
  xsane_back_gtk_set_tooltip(tooltips, text, desc);

  gtk_widget_show(hbox);
  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_widget_set_sensitive(text, settable);

  elem->widget = text;
}

/* ----------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_back_gtk_group_new(GtkWidget *parent, const char * title)
{
  GtkWidget * frame, * vbox;

  frame = gtk_frame_new((char *) title);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(parent), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new(FALSE, 4);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);
  return vbox;
}

/* ----------------------------------------------------------------------------------------------------------------- */
#if 0
static GtkWidget* xsane_back_gtk_curve_new(GSGDialog *dialog, int optnum)
{
  const SANE_Option_Descriptor * opt;
  gfloat fmin, fmax, val, *vector;
  SANE_Word *optval, min, max;
  GtkWidget *curve, *gamma;
  SANE_Status status;
  SANE_Handle dev;
  int i, optlen;

  gamma = gtk_gamma_curve_new();
  curve = GTK_GAMMA_CURVE(gamma)->curve;
  dev = dialog->dev;

  opt    = xsane_get_option_descriptor(dev, optnum);
  optlen = opt->size / sizeof(SANE_Word);
  vector = alloca(optlen * (sizeof(vector[0]) + sizeof(optval[0])));
  optval = (SANE_Word *) (vector + optlen);

  min = max = 0;
  switch(opt->constraint_type)
  {
    case SANE_CONSTRAINT_RANGE:
      min = opt->constraint.range->min;
      max = opt->constraint.range->max;
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      if (opt->constraint.word_list[0] > 1)
      {
        min = max = opt->constraint.word_list[1];
        for (i = 2; i < opt->constraint.word_list[0]; ++i)
        {
          if (opt->constraint.word_list[i] < min)
          {
            min = opt->constraint.word_list[i];
          }

	  if (opt->constraint.word_list[i] > max)
          {
            max = opt->constraint.word_list[i];
          }
        }
      }
      break;

    default:
      break;
  }
  if (min == max)
  {
    fprintf(stderr, "xsane_back_gtk_curve_new: %s: `%s'\n", WARN_NO_VALUE_CONSTRAINT, opt->name);
    fmin = 0;
    fmax = 255;
  }
  else if (opt->type == SANE_TYPE_FIXED)
  {
    fmin = SANE_UNFIX(min);
    fmax = SANE_UNFIX(max);
  }
  else
  {
    fmin = min;
    fmax = max;
  }
  gtk_curve_set_range(GTK_CURVE(curve), 0, optlen - 1, fmin, fmax);

  status = xsane_control_option(dev, optnum, SANE_ACTION_GET_VALUE, optval, 0);
  if (status == SANE_STATUS_GOOD)
  {
    for (i = 0; i < optlen; ++i)
    {
      if (opt->type == SANE_TYPE_FIXED)
      {
        val = SANE_UNFIX(optval[i]);
      }
      else
      {
        val = optval[i];
      }
      vector[i] = val;
    }
    gtk_curve_set_vector(GTK_CURVE(curve), optlen, vector);
  }
  else
  {
    gtk_widget_set_sensitive(gamma, FALSE);
  }

  return gamma;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_vector_new(GSGDialog * dialog, GtkWidget *vbox, int num_vopts, int *vopts)
{
  GtkWidget *notebook, *label, *curve;
  const SANE_Option_Descriptor *opt;
  int i;

  notebook = gtk_notebook_new();
  gtk_container_set_border_width(GTK_CONTAINER(notebook), 4);
  gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

  for (i = 0; i < num_vopts; ++i)
  {
    opt = xsane_get_option_descriptor(dialog->dev, vopts[i]);

    label = gtk_label_new((char *) opt->title);
    vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    gtk_widget_show(vbox);
    gtk_widget_show(label);

    curve = xsane_back_gtk_curve_new(dialog, vopts[i]);
    gtk_container_set_border_width(GTK_CONTAINER(curve), 4);
    gtk_box_pack_start(GTK_BOX(vbox), curve, TRUE, TRUE, 0);
    gtk_widget_show(curve);

    dialog->element[vopts[i]].widget = curve;
  }
  gtk_widget_show(notebook);
}
#endif
/* ----------------------------------------------------------------------------------------------------------------- */
#if 0
static void tooltips_destroy(GSGDialog * dialog)
{
  gtk_object_unref(GTK_OBJECT(dialog->tooltips));
  dialog->tooltips = 0;
}
#endif
/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_panel_destroy(GSGDialog * dialog)
{
  const SANE_Option_Descriptor *opt;
  GSGDialogElement *elem;
  int i, j;

  gtk_widget_destroy(dialog->xsane_hbox);
  gtk_widget_destroy(dialog->standard_hbox);
  gtk_widget_destroy(dialog->advanced_hbox);

  /* free the menu labels of integer/fix-point word-lists: */
  for (i = 0; i < dialog->num_elements; ++i)
  {
    if (dialog->element[i].menu)
    {
      opt = xsane_get_option_descriptor(dialog->dev, i);
      elem = dialog->element + i;
      if (opt->type != SANE_TYPE_STRING)
      {
        for (j = 0; j < elem->menu_size; ++j)
        {
          if (elem->menu[j].label)
          {
            free(elem->menu[j].label);
            elem->menu[j].label = 0;
          }
        }
        free(elem->menu);
        elem->menu = 0;
      }
    }
  }
  memset(dialog->element, 0, dialog->num_elements * sizeof(dialog->element[0]));
}

/* ----------------------------------------------------------------------------------------------------------------- */

/* When an setting an option changes the dialog, everything may
   change: the option titles, the activity-status of the option, its
   constraints or what not.  Thus, rather than trying to be clever in
   detecting what exactly changed, we use a brute-force method of
   rebuilding the entire dialog.  */

static void xsane_back_gtk_panel_rebuild(GSGDialog * dialog)
{
  xsane_back_gtk_panel_destroy(dialog);
  xsane_panel_build(dialog);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_refresh_dialog(GSGDialog *dialog)
{
  xsane_back_gtk_panel_rebuild(dialog);
  if (dialog->param_change_callback)
  {
    (*dialog->param_change_callback) (dialog, dialog->param_change_arg);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_update_scan_window(GSGDialog *dialog)
{
  const SANE_Option_Descriptor *opt;
  double old_val, new_val;
  GSGDialogElement *elem;
  SANE_Status status;
  SANE_Word word;
  int i, optnum;
  char str[64];

  for (i = 0; i < 4; ++i)
  {
    if (dialog->well_known.coord[i] > 0)
    {
      optnum = dialog->well_known.coord[i];
      elem = dialog->element + optnum;
      opt = xsane_get_option_descriptor(dialog->dev, optnum);

      status = xsane_control_option(dialog->dev, optnum, SANE_ACTION_GET_VALUE, &word, 0);
      if (status != SANE_STATUS_GOOD)
      {
         continue; /* sliently ignore errors */
      }

      switch(opt->constraint_type)
      {
        case SANE_CONSTRAINT_RANGE:
          if (opt->type == SANE_TYPE_INT)
          {
            old_val = GTK_ADJUSTMENT(elem->data)->value;
            new_val = word;
            GTK_ADJUSTMENT(elem->data)->value = new_val;
          }
          else
          {
            old_val = GTK_ADJUSTMENT(elem->data)->value;
            new_val = SANE_UNFIX(word);
	    if (opt->unit == SANE_UNIT_MM)
            {
              new_val /= preferences.length_unit;
            }
            GTK_ADJUSTMENT(elem->data)->value = new_val;
          }

          if (old_val != new_val)
          {
	      gtk_signal_emit_by_name(GTK_OBJECT(elem->data), "value_changed");
          }
         break;

	case SANE_CONSTRAINT_WORD_LIST:
	  if (opt->type == SANE_TYPE_INT)
          {
	    sprintf(str, "%d", word);
          }
	  else
          {
	    sprintf(str, "%g", SANE_UNFIX(word));
          }
	  /* XXX maybe we should call this only when the value changes... */
	  gtk_option_menu_set_history(GTK_OPTION_MENU(elem->widget), xsane_back_gtk_option_menu_lookup(elem->menu, str));
	 break;

	default:
	 break;
      }
    }
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

/* Ensure sure the device has up-to-date option values.  Except for
   vectors, all option values are kept current.  Vectors are
   downloaded into the device during this call.  */
void xsane_back_gtk_sync(GSGDialog *dialog)
{
  const SANE_Option_Descriptor *opt;
  gfloat val, *vector;
  SANE_Word *optval;
  int i, j, optlen;
  GtkWidget *curve;

  for (i = 1; i < dialog->num_elements; ++i)
  {
    opt = xsane_get_option_descriptor(dialog->dev, i);

    if (!SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      continue;
    }

    if (opt->type != SANE_TYPE_INT && opt->type != SANE_TYPE_FIXED)
    {
      continue;
    }

    if (opt->size == sizeof(SANE_Word))
    {
      continue;
    }

    /* ok, we're dealing with an active vector */

    optlen = opt->size / sizeof(SANE_Word);
    optval = alloca(optlen * sizeof(optval[0]));
    vector = alloca(optlen * sizeof(vector[0]));

    curve = GTK_GAMMA_CURVE(dialog->element[i].widget)->curve;
    gtk_curve_get_vector(GTK_CURVE(curve), optlen, vector);
    for (j = 0; j < optlen; ++j)
    {
      val = vector[j];
      if (opt->type == SANE_TYPE_FIXED)
      {
        optval[j] = SANE_FIX(val);
      }
      else
      {
        optval[j] = val + 0.5;
      }
    }

    xsane_back_gtk_set_option(dialog, i, optval, SANE_ACTION_SET_VALUE);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_update_vector(GSGDialog *dialog, int opt_num, SANE_Int *vector)
{
  const SANE_Option_Descriptor *opt;
  gfloat val;
  SANE_Word *optval;
  int j, optlen;

  if (opt_num < 1)
    return; /* not defined */

  opt = xsane_get_option_descriptor(dialog->dev, opt_num);
  if (!SANE_OPTION_IS_ACTIVE(opt->cap))
  {
    return; /* inactive */
  }

  if (opt->type != SANE_TYPE_INT && opt->type != SANE_TYPE_FIXED)
  {
    return;
  }

  if (opt->size == sizeof(SANE_Word))
  {
    return;
  }

  /* ok, we're dealing with an active vector */

  optlen = opt->size / sizeof(SANE_Word);
  optval = alloca(optlen * sizeof(optval[0]));
  for (j = 0; j < optlen; ++j)
  {
    val = vector[j];
    if (opt->type == SANE_TYPE_FIXED)
    {
      optval[j] = SANE_FIX(val);
    }
    else
    {
      optval[j] = val + 0.5;
    }
  }

  xsane_back_gtk_set_option(dialog, opt_num, optval, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_tooltips(GSGDialog *dialog, int enable)
{
  if (!dialog->tooltips)
  {
    return;
  }

  if (enable)
  {
    gtk_tooltips_enable(dialog->tooltips);
  }
  else
  {
    gtk_tooltips_disable(dialog->tooltips);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_sensitivity(GSGDialog *dialog, int sensitive)
{
  const SANE_Option_Descriptor *opt;
  int i;

  for (i = 0; i < dialog->num_elements; ++i)
  {
    opt = xsane_get_option_descriptor(dialog->dev, i);

    if (!SANE_OPTION_IS_ACTIVE(opt->cap) || !SANE_OPTION_IS_SETTABLE(opt->cap) ||
        opt->type == SANE_TYPE_GROUP || !dialog->element[i].widget)
    {
      continue;
    }

    if (!(opt->cap & SANE_CAP_ALWAYS_SETTABLE))
    {
      gtk_widget_set_sensitive(dialog->element[i].widget, sensitive);
    }
  }

  if (dialog)
  {
    if (dialog->xsanemode_widget)
    {
      gtk_widget_set_sensitive(dialog->xsanemode_widget, sensitive); 
    }
  }

  while (gtk_events_pending())
  {
   gtk_main_iteration();
  }   
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_sensitivity(SANE_Int sensitivity)
{
  if (xsane.shell)
  {
    gtk_widget_set_sensitive(xsane.menubar, sensitivity); 
    gtk_widget_set_sensitive(dialog->xsane_window, sensitivity); 
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), sensitivity);
    gtk_widget_set_sensitive(xsane.standard_options_shell, sensitivity);
    gtk_widget_set_sensitive(xsane.advanced_options_shell, sensitivity);
    gtk_widget_set_sensitive(xsane.histogram_dialog, sensitivity);
  }

  if (xsane.preview)
  {
    gtk_widget_set_sensitive(xsane.preview->button_box, sensitivity);   /* button box at top of window */
#if 1
    gtk_widget_set_sensitive(xsane.preview->viewport, sensitivity);     /* Preview image selection */
#endif
    gtk_widget_set_sensitive(xsane.preview->start, sensitivity);        /* Acquire preview button */
  }

  if (xsane.fax_dialog)
  {
    gtk_widget_set_sensitive(xsane.fax_dialog, sensitivity);
  }

#if 0
  if (dialog)
  {
    xsane_back_gtk_set_sensitivity(dialog, sensitivity);
  }
#endif

  while (gtk_events_pending()) /* make sure set_sensitivity is displayed */
  {
    gtk_main_iteration();
  }

  xsane.sensitivity = sensitivity;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_destroy_dialog(GSGDialog * dialog)
{
  SANE_Handle dev = dialog->dev;

  xsane_back_gtk_panel_destroy(dialog);
  free((void *) dialog->dev_name);
  free(dialog->element);
  free(dialog);

  sane_close(dev);
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d)
{
 GdkPixmap *pixmap;
 GdkBitmap *mask;

  gtk_widget_realize(gtk_window);
  if (xpm_d)
  {
    pixmap = gdk_pixmap_create_from_xpm_d(gtk_window->window, &mask, xsane.bg_trans, xpm_d);
  }
  else
  {
    if (xsane.window_icon_pixmap)
    {
      pixmap = xsane.window_icon_pixmap;
      mask   = xsane.window_icon_mask;
    }
    else
    {
      pixmap = gdk_pixmap_create_from_xpm_d(gtk_window->window, &mask, xsane.bg_trans, (gchar **) xsane_window_icon_xpm);
    }
  }

  gdk_window_set_icon(gtk_window->window, 0, pixmap, mask);
}

/* ---------------------------------------------------------------------------------------------------------------------- */ 