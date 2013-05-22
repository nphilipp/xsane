/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-back-gtk.c

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

/* ----------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-preferences.h"
#include "xsane-gamma.h"

/* ----------------------------------------------------------------------------------------------------------------- */

/* extern declarations */
extern void xsane_panel_build(void);

/* ----------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */
SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info);
const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option);
const char *xsane_back_gtk_unit_string(SANE_Unit unit);
void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const gchar *desc);
int xsane_back_gtk_make_path(size_t buf_size, char *buf, const char *prog_name, const char *dir_name,
                             const char *prefix, const char *dev_name, const char *postfix, int location);
void xsane_back_gtk_set_option(int opt_num, void *val, SANE_Action action);

static void xsane_back_gtk_panel_rebuild(void);
void xsane_set_sensitivity(SANE_Int sensitivity);
void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d);

/* ----------------------------------------------------------------------------------------------------------------- */
 
void xsane_bound_int(int *value, int min, int max)
{
  DBG(DBG_proc3, "xsane_bound_int\n");
 
  if (min > max)
  {
    int help = min;
    min = max;
    max = help;
  }
 
  if (*value < min)
  {
    *value = min;
  }
 
  if (*value > max)
  {
    *value = max;
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */
 
void xsane_bound_float(float *value, float min, float max)
{
  DBG(DBG_proc3, "xsane_bound_float\n");
 
  if (min > max)
  {
    double help = min;
    min = max;
    max = help;
  }
 
  if (*value < min)
  {
    *value = min;
  }
 
  if (*value > max)
  {
    *value = max;
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */
 
void xsane_bound_double(double *value, double min, double max)
{
  DBG(DBG_proc3, "xsane_bound_double\n");
 
  if (min > max)
  {
    double help = min;
    min = max;
    max = help;
  }
 
  if (*value < min)
  {
    *value = min;
  }
 
  if (*value > max)
  {
    *value = max;
  }
}
                       
/* ----------------------------------------------------------------------------------------------------------------- */

/* returns 1 if value is in bounds */ 
int xsane_check_bound_double(double value, double min, double max)
{
 int in_bounds = 1;

  DBG(DBG_proc3, "xsane_check_bound_double\n");
 
  if (min > max)
  {
    double help = min;
    min = max;
    max = help;
  }
 
  if (value < min)
  {
    in_bounds = 0;
  }
 
  if (value > max)
  {
    in_bounds = 0;
  }

 return (in_bounds);
}

/* ----------------------------------------------------------------------------------------------------------------- */

const SANE_Option_Descriptor *xsane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
  DBG(DBG_optdesc, "xsane_get_option_descriptor(%d)\n", option);

  if (option >= 0)
  {
    return sane_get_option_descriptor(handle, option);
  }
 return NULL;
}

/* ----------------------------------------------------------------------------------------------------------------- */

SANE_Status xsane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *val, SANE_Int *info)
{
  DBG(DBG_proc, "xsane_control_option(option = %d, action = %d)\n", option, action);

  if (option >= 0)
  {
   SANE_Status status;

#if 1
    /* I am not sure about a correct and intelligent way to handle an option that has not defined SANE_CAP_SOFT_DETECT */
    /* the test backend creates an option without SANE_CAP_SOFT_DETECT that causes an error message when I do not do the following */
    if (action == SANE_ACTION_GET_VALUE)
    {
     const SANE_Option_Descriptor *opt;

      opt = xsane_get_option_descriptor(xsane.dev, option);
      if ((opt) && (!(opt->cap & SANE_CAP_SOFT_DETECT)))
      {
        DBG(DBG_warning, "WARNING: xsane_control_option(option = %d, action = %d): SANE_CAP_SOFT_DETECT is not set\n", option, action);
        if (option > 0) /* continue for option == 0, otherwise we can not read this option */
        {
          return SANE_STATUS_GOOD;
        }
      }
    }
#endif

    status = sane_control_option(handle, option, action, val, info);
    if (status)
    {
      DBG(DBG_error, "ERROR: xsane_control_option(option = %d, action = %d) failed\n", option, action);
    }

    return status;
  }

 return SANE_STATUS_INVAL;
}

/* ----------------------------------------------------------------------------------------------------------------- */

const char *xsane_back_gtk_unit_string(SANE_Unit unit)
{
  DBG(DBG_proc, "xsane_back_gtk_unit_string\n");

  switch (unit)
  {
    case SANE_UNIT_NONE:	return "none";
    case SANE_UNIT_PIXEL:	return "px";
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
    case SANE_UNIT_MICROSECOND:	return "\302\265s"; /* UTF8 µs */
  }
  return 0;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_tooltip(GtkTooltips *tooltips, GtkWidget *widget, const gchar *desc)
{
  DBG(DBG_proc, "xsane_back_gtk_set_tooltip\n");

  if (desc && desc[0])
  {
    gtk_tooltips_set_tip(tooltips, widget, desc, 0);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_make_path(size_t buf_size, char *buf, const char *prog_name, const char *dir_name,
                             const char *prefix, const char *dev_name, const char *postfix, int location)
{
 size_t len, extra;
 int i;

  DBG(DBG_proc, "xsane_back_gtk_make_path\n");

  if (location == XSANE_PATH_LOCAL_SANE) /* make path to local file */
  {
    if (getenv(STRINGIFY(ENVIRONMENT_APPDATA_DIR_NAME)) != NULL)
    {
      snprintf(buf, buf_size-2, "%s%c.sane", getenv(STRINGIFY(ENVIRONMENT_APPDATA_DIR_NAME)), SLASH);
    }
    else
    {
      snprintf(buf, buf_size-2, "%s", STRINGIFY(XSANE_FIXED_APPDATA_DIR));
    }
    mkdir(buf, 0777);	/* ensure ~/.sane directory exists */
  }
  else if (location == XSANE_PATH_SYSTEM) /* make path to system file */
  {
    snprintf(buf, buf_size-2, "%s", STRINGIFY(PATH_SANE_DATA_DIR));
  }
  else /* make path to temporary file XSANE_PATH_TMP */
  {
    snprintf(buf, buf_size-2, "%s", preferences.tmp_path);
  }

  len = strlen(buf);

  buf[len++] = SLASH;

  if (prog_name)
  {
    extra = strlen(prog_name);
    if (len + extra + 2 >= buf_size)
    {
      goto filename_too_long;
    }

    memcpy(buf + len, prog_name, extra);
    len += extra;

    buf[len] = '\0';
    mkdir(buf, 0777);	/* ensure ~/.sane/PROG_NAME directory exists */

    buf[len++] = SLASH; /* OS/2 does not like slash at end of mktemp-path */
  }

  if (len >= buf_size)
  {
    goto filename_too_long;
  }

  if (dir_name)
  {
    extra = strlen(dir_name);
    if (len + extra + 2 >= buf_size)
    {
      goto filename_too_long;
    }

    memcpy(buf + len, dir_name, extra);
    len += extra;

    buf[len++] = SLASH;

    buf[len] = '\0';
    mkdir(buf, 0777);	/* ensure DIR_NAME directory exists */
  }

  if (len >= buf_size)
  {
    goto filename_too_long;
  }


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

  if (dev_name)
  {
      /* Turn devicename into valid filename by replacing slashes and other forbidden characters by "_", "_" gets "__", spaces are erased  */

    for (i = 0; dev_name[i]; ++i)
    {
      if (len + 2 >= buf_size)
      {
        goto filename_too_long;
      }

      switch (dev_name[i])
      {
        case '\\':	/* "\" -> "_" */
          buf[len++] = '_'; 
         break;

        case '/':	/* "/" -> "_" */
          buf[len++] = '_'; 
         break;

        case '*':	/* "*" -> "_" */
          buf[len++] = '_'; 
         break;

        case '?':	/* "?" -> "_" */
          buf[len++] = '_'; 
         break;

#ifdef _WIN32
        case ':':	/* ":" -> "_" */
          buf[len++] = '_'; 
         break;
#endif

#ifdef HAVE_OS2_H
        case ':':	/* ":" -> "_" */
          buf[len++] = '_'; 
         break;
#endif

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
  {
    goto filename_too_long;
  }

  if (location == XSANE_PATH_TMP) /* tmp dir, add uid */
  {
   char tmpbuf[TEXTBUFSIZE];
   uid_t uid;
   int fd;

    uid = getuid();
    snprintf(tmpbuf, sizeof(tmpbuf), "-%d-", (int) uid);
                                                             
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

    fd = mkstemp(buf); /* create unique filename and opens/creates the file */
    if (fd == -1)
    {
      xsane_back_gtk_error(ERR_CREATE_TEMP_FILE, FALSE);
     return -1;
    }
    close(fd); /* will be opened again later */
  }
  else
  {
    buf[len++] = '\0';
  }

  DBG(DBG_proc, "path = \"%s\"\n", buf);

  return 0;


filename_too_long:
  xsane_back_gtk_error(ERR_FILENAME_TOO_LONG, FALSE);
  errno = E2BIG;
 return -1;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_option(int opt_num, void *val, SANE_Action action)
{
 SANE_Status status;
 SANE_Int info;
 char buf[TEXTBUFSIZE];
 int old_channels = xsane.xsane_channels;
 int update_gamma = FALSE;

  DBG(DBG_proc, "xsane_back_gtk_set_option\n");

  status = xsane_control_option(xsane.dev, opt_num, action, val, &info);
  if (status != SANE_STATUS_GOOD)
  {
    snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_SET_OPTION, xsane_get_option_descriptor(xsane.dev, opt_num)->name,
             XSANE_STRSTATUS(status));
    xsane_back_gtk_error(buf, FALSE);
    return;
  }

  if (info & SANE_INFO_RELOAD_PARAMS)
  {
    xsane_update_param(0);
  }

  if (info & SANE_INFO_RELOAD_OPTIONS)
  {
    xsane_back_gtk_panel_rebuild();

    if (xsane.preview)
    {
      preview_update_surface(xsane.preview, 0);
    }

    update_gamma = TRUE; /* scanner gamma correction may have changed, medium may need update */
  }
  else if (info & SANE_INFO_INEXACT)
  {
    /* XXXXXXXXXXXXXX this also has to be handled XXXXXXXXXXXXXXX */
  }

  if (xsane.xsane_channels != old_channels)
  {
    /* we have to update gamma tables and histogram because medium settings */
    /* may have changed */
    update_gamma = TRUE;
  }

  if (update_gamma)
  {
    xsane_update_gamma_curve(TRUE);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_get_option_double(int option, double *val, SANE_Int *unit)
/* return values: */
/*  0 = OK */
/* -1 = option number < 0 */
/* -2 = failed to set option */
{
 const SANE_Option_Descriptor *opt;
 SANE_Handle dev;
 SANE_Word word;

  DBG(DBG_proc, "xsane_back_gtk_get_option_double\n");

  if (option <= 0)
  {
    return -1;
  }

  if (xsane_control_option(xsane.dev, option, SANE_ACTION_GET_VALUE, &word, 0) == SANE_STATUS_GOOD)
  {
    dev = xsane.dev;
    opt = xsane_get_option_descriptor(dev, option);

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

   return 0;
  }
  else if (val)
  {
    *val = 0;
  }
  return -2;
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_set_option_double(int option, double value)
{
 const SANE_Option_Descriptor *opt;
 SANE_Word word;

  DBG(DBG_proc, "xsane_set_option_double\n");

  if (option <= 0 || value <= -INF || value >= INF)
  {
    return -1;
  }

  opt = xsane_get_option_descriptor(xsane.dev, option);
  if (opt)
  {
    if (opt->type == SANE_TYPE_FIXED)
    {
      word = SANE_FIX(value);
    }
    else
    {
      word = value + 0.5;
    }

    if (xsane_control_option(xsane.dev, option, SANE_ACTION_SET_VALUE, &word, 0))
    {
      return -2;
    }
  }
  else
  {
    return -1;
  }

 return 0;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static int xsane_back_gtk_decision_delete_event(GtkWidget * widget, GdkEvent *event, gpointer data)
{
 gint *decision_flag = (gint *) data;

  DBG(DBG_proc, "xsane_back_gtk_decision_delete_event\n");

  xsane.back_gtk_message_dialog_active--;

  if (decision_flag)
  {
    *decision_flag = -1;
  }

 return FALSE; /* continue with original delete even routine */
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_decision_ok_callback(GtkWidget *widget, gpointer data)
{
 gint *decision_flag = (gint *) data;

  DBG(DBG_proc, "xsane_back_gtk_decision_ok_callback\n");

  gtk_widget_destroy(widget->parent->parent->parent->parent);
  xsane.back_gtk_message_dialog_active--;

  if (decision_flag)
  {
    *decision_flag = 1;
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_decision_reject_callback(GtkWidget *widget, gpointer data)
{
 gint *decision_flag = (gint *) data;

  DBG(DBG_proc, "xsane_back_gtk_decision_reject_callback\n");

  gtk_widget_destroy(widget->parent->parent->parent->parent);
  xsane.back_gtk_message_dialog_active--;

  if (decision_flag)
  {
    *decision_flag = -1;
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

gint xsane_back_gtk_decision(gchar *title, gchar **xpm_d,  gchar *message, gchar *oktext, gchar *rejecttext, int wait)
{
 GtkWidget *main_vbox, *hbox, *label, *button, *frame;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 GtkWidget *pixmapwidget;
 GtkWidget *decision_dialog;
 GtkAccelGroup *accelerator_group;
 gint decision_flag;
 gint *decision_flag_ptr = NULL;

  DBG(DBG_proc, "xsane_back_gtk_decision\n");

  if (wait)
  {
    decision_flag_ptr = &decision_flag;
  }

  xsane.back_gtk_message_dialog_active++;
  decision_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(decision_dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_title(GTK_WINDOW(decision_dialog), title);
  g_signal_connect(GTK_OBJECT(decision_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_back_gtk_decision_delete_event), (void *) decision_flag_ptr);

  xsane_set_window_icon(decision_dialog, 0);

  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(decision_dialog), accelerator_group);

  /* create a frame */
  frame = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(decision_dialog), frame);
  gtk_widget_show(frame);

  /* create the main vbox */
  main_vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 5);
  gtk_widget_show(main_vbox);
  gtk_container_add(GTK_CONTAINER(frame), main_vbox);

  /* create a horizontal box to put the icon and the text insode */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

  /* the info icon */
  if (xpm_d)
  {
    pixmap = gdk_pixmap_create_from_xpm_d(decision_dialog->window, &mask, xsane.bg_trans, xpm_d);
    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 10);
    gtk_widget_show(pixmapwidget);
    gdk_drawable_unref(pixmap);
    gdk_drawable_unref(mask);
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
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_back_gtk_decision_ok_callback, (void *) decision_flag_ptr);
  gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, TRUE, 5);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);


  if (rejecttext) /* the rejection button */
  {
    button = gtk_button_new_with_label(rejecttext);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_back_gtk_decision_reject_callback, (void *) decision_flag_ptr);
    gtk_box_pack_end(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    gtk_widget_show(button);
  }

  /* if rejectbutton is available then the following command is valid for the reject button */
  /* otherwise it is valid for the ok button */
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);

  gtk_widget_show(hbox);
  gtk_widget_show(decision_dialog);


  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

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

void xsane_back_gtk_ipc_dialog_callback(gpointer data, gint source, GdkInputCondition cond)
{
 char message[TEXTBUFSIZE];
 size_t bytes;

  DBG(DBG_proc, "xsane_back_gtk_message\n");

  bytes = read(xsane.ipc_pipefd[0], message, sizeof(message)-1);
  message[bytes] = 0;

  xsane_back_gtk_decision(ERR_HEADER_CHILD_PROCESS_ERROR, (gchar **) error_xpm, message, BUTTON_CLOSE, 0 /* no reject text */, FALSE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_message(gchar *title, gchar **icon_xpm, gchar *message, int wait)
{
  DBG(DBG_proc, "xsane_back_gtk_message\n");

  xsane_back_gtk_decision(title, icon_xpm, message, BUTTON_CLOSE, 0 /* no reject text */, wait);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_error(gchar *error, int wait)
{
  DBG(DBG_proc, "xsane_back_gtk_error: %s\n", error);

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

void xsane_back_gtk_warning(gchar *warning, int wait)
{
  DBG(DBG_proc, "xsane_back_gtk_warning: %s\n", warning);

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

void xsane_back_gtk_info(gchar *info, int wait)
{
  DBG(DBG_proc, "xsane_back_gtk_info: %s\n", info);

  if (wait)
  {
   SANE_Int old_sensitivity = xsane.sensitivity;

    xsane_set_sensitivity(FALSE);
    xsane_back_gtk_message(ERR_HEADER_INFO, (gchar**) info_xpm, info, wait);
    xsane_set_sensitivity(old_sensitivity);
  }
  else
  {
    xsane_back_gtk_message(ERR_HEADER_INFO, (gchar**) info_xpm, info, wait);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_filetype_menu_set_history(GtkWidget *xsane_filetype_option_menu, char *filetype)
{
 int filetype_nr;
 int select_item;

  filetype_nr = 0;
  select_item = 0;

#ifdef HAVE_LIBJPEG
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_JPEG)) )
  {
    select_item = filetype_nr;
  }
#endif

  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PDF)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PNG)) )
  {
    select_item = filetype_nr;
  }
#endif
#endif

  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PNM)) )
  {
    select_item = filetype_nr;
  }

  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PS)) )
  {
    select_item = filetype_nr;
  }

#ifdef SUPPORT_RGBA
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_RGBA)) )
  {
    select_item = filetype_nr;
  }
#endif

  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_TEXT)) )
  {
    select_item = filetype_nr;
  }

#ifdef HAVE_LIBTIFF
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_TIFF)) )
  {
    select_item = filetype_nr;
  }
#endif
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_filetype_option_menu), select_item);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_back_gtk_filetype_menu_new(char *filetype, GtkSignalFunc filetype_callback)
{
 GtkWidget *xsane_filetype_menu, *xsane_filetype_item;
 GtkWidget *xsane_filetype_option_menu;
 int filetype_nr;
 int select_item;

  xsane_filetype_menu = gtk_menu_new();

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_BY_EXT);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_BY_EXT);
  gtk_widget_show(xsane_filetype_item);

  filetype_nr = 0;
  select_item = 0;

#ifdef HAVE_LIBJPEG
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_JPEG);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_JPEG);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_JPEG)) )
  {
    select_item = filetype_nr;
  }
#endif

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PDF);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_PDF);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PDF)) )
  {
    select_item = filetype_nr;
  }

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNG);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_PNG);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PNG)) )
  {
    select_item = filetype_nr;
  }
#endif
#endif

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNM);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_PNM);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PNM)) )
  {
    select_item = filetype_nr;
  }

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PS);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_PS);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_PS)) )
  {
    select_item = filetype_nr;
  }

#ifdef SUPPORT_RGBA
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_RGBA);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_RGBA);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_RGBA)) )
  {
    select_item = filetype_nr;
  }
#endif

  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TEXT);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_TEXT);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_TEXT)) )
  {
    select_item = filetype_nr;
  }

#ifdef HAVE_LIBTIFF
  xsane_filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TIFF);
  gtk_container_add(GTK_CONTAINER(xsane_filetype_menu), xsane_filetype_item);
  g_signal_connect(GTK_OBJECT(xsane_filetype_item), "activate", filetype_callback, (void *) XSANE_FILETYPE_TIFF);
  gtk_widget_show(xsane_filetype_item);
  filetype_nr++;
  if ( (filetype) && (!strcasecmp(filetype, XSANE_FILETYPE_TIFF)) )
  {
    select_item = filetype_nr;
  }
#endif

  xsane_filetype_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_filetype_option_menu, DESC_FILETYPE);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_filetype_option_menu), xsane_filetype_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_filetype_option_menu), select_item);

 return (xsane_filetype_option_menu);
}

/* ----------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_back_gtk_cms_function_menu_new(int select_cms_function, GtkSignalFunc cms_function_menu_callback)
{
 GtkWidget *xsane_cms_function_menu, *xsane_cms_function_item;
 GtkWidget *xsane_cms_function_option_menu;

  xsane_cms_function_menu = gtk_menu_new();

  xsane_cms_function_item = gtk_menu_item_new_with_label(MENU_ITEM_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE);
  if (cms_function_menu_callback)
  {
    g_signal_connect(GTK_OBJECT(xsane_cms_function_item), "activate", (GtkSignalFunc) cms_function_menu_callback, (void *) XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE);
  }
  gtk_container_add(GTK_CONTAINER(xsane_cms_function_menu), xsane_cms_function_item);
  gtk_widget_show(xsane_cms_function_item);

  xsane_cms_function_item = gtk_menu_item_new_with_label(MENU_ITEM_CMS_FUNCTION_CONVERT_TO_SRGB);
  if (cms_function_menu_callback)
  {
    g_signal_connect(GTK_OBJECT(xsane_cms_function_item), "activate", (GtkSignalFunc) cms_function_menu_callback, (void *) XSANE_CMS_FUNCTION_CONVERT_TO_SRGB);
  }
  gtk_container_add(GTK_CONTAINER(xsane_cms_function_menu), xsane_cms_function_item);
  gtk_widget_show(xsane_cms_function_item);

  xsane_cms_function_item = gtk_menu_item_new_with_label(MENU_ITEM_FUNCTION_CONVERT_TO_WORKING_CS);
  if (cms_function_menu_callback)
  {
    g_signal_connect(GTK_OBJECT(xsane_cms_function_item), "activate", (GtkSignalFunc) cms_function_menu_callback, (void *) XSANE_CMS_FUNCTION_CONVERT_TO_WORKING_CS);
  }
  gtk_container_add(GTK_CONTAINER(xsane_cms_function_menu), xsane_cms_function_item);
  gtk_widget_show(xsane_cms_function_item);

  xsane_cms_function_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_cms_function_option_menu, DESC_CMS_FUNCTION);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_cms_function_option_menu), xsane_cms_function_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_cms_function_option_menu), select_cms_function);

 return (xsane_cms_function_option_menu);
}

/* ----------------------------------------------------------------------------------------------------------------- */

#ifdef __GTK_FILE_CHOOSER_H__

GtkWidget *filechooser;
char *filechooser_filetype = NULL;

static void xsane_back_gtk_filetype2_callback(GtkWidget *widget, gpointer data)
{
 char *extension, *chooser_filename;
 char filename[PATH_MAX];
 char *basename;
 char *new_filetype = (char *) data;
 int pos;

  DBG(DBG_proc, "xsane_filetype2_callback\n");

  chooser_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooser));

  if ((new_filetype) && (*new_filetype))
  {
    extension = strrchr(chooser_filename, '.');

    if ((extension) && (extension != chooser_filename))
    {
      if ( (!strcasecmp(extension, ".pnm"))  || (!strcasecmp(extension, ".raw"))
        || (!strcasecmp(extension, ".png"))  || (!strcasecmp(extension, ".ps"))
        || (!strcasecmp(extension, ".pdf"))  || (!strcasecmp(extension, ".rgba"))
        || (!strcasecmp(extension, ".tiff")) || (!strcasecmp(extension, ".tif"))
        || (!strcasecmp(extension, ".text")) || (!strcasecmp(extension, ".txt"))
        || (!strcasecmp(extension, ".jpg"))  || (!strcasecmp(extension, ".jpeg"))
         ) /* remove filetype extension */
      {
        *extension = 0; /* remove extension */
      }
    }
    snprintf(filename, sizeof(filename), "%s%s", chooser_filename, new_filetype);
  }
  else
  {
    snprintf(filename, sizeof(filename), "%s", chooser_filename);
  }

  if (filechooser_filetype)
  {
    free(filechooser_filetype);
    filechooser_filetype = NULL;
  }

  if (data)
  {
    filechooser_filetype = strdup(new_filetype); 
  }


  basename = filename;

  for (pos = strlen(filename) - 1; pos > 0; pos--)
  {
    if (filename[pos] == '/')
    {
      filename[pos]=0;

      basename = filename+pos+1;
     break;
    }
  }

  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filechooser), basename);

  g_free(chooser_filename);
}

/* ----------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_get_filename(const char *label, const char *default_name, size_t max_len, char *filename, char **filetype, int *cms_function,
                                XsaneFileChooserAction action, int show_extra_widgets, int enable_filters, int activate_filter)
{
 int ok = 0;
 GtkWidget *xsane_filetype_option_menu;
 GtkWidget *xsane_cms_function_option_menu = xsane_cms_function_option_menu;
 gint result;
 const gchar *accept_text = NULL;
 const gchar *reject_text = NULL;
 GtkFileChooserAction chooser_action = GTK_FILE_CHOOSER_ACTION_OPEN;
 GtkResponseType accept_code;
 GtkResponseType reject_code;
 char buf[PATH_MAX];


  DBG(DBG_proc, "xsane_back_gtk_get_filename\n");

  if (filechooser)
  {
    gdk_beep();
    return -1; /* cancel => do not allow to open more than one filechooser dialog */
  }

  switch (action)
  {
    default:
    case XSANE_FILE_CHOOSER_ACTION_OPEN:
      chooser_action = GTK_FILE_CHOOSER_ACTION_OPEN;
      accept_text = GTK_STOCK_OPEN;
      accept_code = GTK_RESPONSE_ACCEPT;
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;

    case XSANE_FILE_CHOOSER_ACTION_SELECT_OPEN:
      chooser_action = GTK_FILE_CHOOSER_ACTION_OPEN;
      accept_text = GTK_STOCK_OK;
      accept_code = GTK_RESPONSE_ACCEPT;
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;

    case XSANE_FILE_CHOOSER_ACTION_SAVE:
      chooser_action = GTK_FILE_CHOOSER_ACTION_SAVE;
      accept_text = GTK_STOCK_SAVE;
      accept_code = GTK_RESPONSE_ACCEPT;
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;

    case XSANE_FILE_CHOOSER_ACTION_SELECT_SAVE:
      chooser_action = GTK_FILE_CHOOSER_ACTION_SAVE;
      accept_text = GTK_STOCK_OK;
      accept_code = GTK_RESPONSE_ACCEPT;
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;

    case XSANE_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      chooser_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      accept_text = GTK_STOCK_OK;
      accept_code = GTK_RESPONSE_ACCEPT;
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;

    case XSANE_FILE_CHOOSER_ACTION_SELECT_PROJECT:
      chooser_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      accept_text = GTK_STOCK_OK;
      accept_code = GTK_RESPONSE_NO; /* when we would use ACCEPT, OK, YES or APPLY then the filechooser_dialog would create non existant directories */
      reject_text = GTK_STOCK_CANCEL;
      reject_code = GTK_RESPONSE_CANCEL;
     break;
  }

  filechooser = gtk_file_chooser_dialog_new (label,
				      NULL,
				      chooser_action,
				      reject_text, reject_code,
				      accept_text, accept_code,
				      NULL);

  xsane_set_window_icon(filechooser, 0);

  gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filechooser), TRUE);


  /* add paths to filechooser */
  if (getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)) != NULL)
  {
    snprintf(buf, sizeof(buf)-2, "%s", getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)));
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(filechooser), buf, NULL);
  }

  if (getcwd(buf, sizeof(buf)))
  {
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(filechooser), buf, NULL);
  }


  if (enable_filters & XSANE_FILE_FILTER_ALL) /* filter: all files */
  {
   GtkFileFilter *filter;

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*");

    gtk_file_filter_set_name(filter, FILE_FILTER_ALL_FILES);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

    if (activate_filter == XSANE_FILE_FILTER_ALL)
    {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(filechooser), filter);
    }
  }

  if (enable_filters & XSANE_FILE_FILTER_DRC) /* filter: device rc */
  {
   GtkFileFilter *filter;

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[dD][rR][cC]");

    gtk_file_filter_set_name(filter, FILE_FILTER_DRC);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

    if (activate_filter == XSANE_FILE_FILTER_DRC)
    {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(filechooser), filter);
    }

    /* add path to filechooser */
    if (getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)) != NULL)
    {
      gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(filechooser), getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)), NULL);
    }
  }

  if (enable_filters & XSANE_FILE_FILTER_ICM) /* filter: color management profiles */
  {
   GtkFileFilter *filter;

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[iI][cC][cCmM]");

    gtk_file_filter_set_name(filter, FILE_FILTER_ICM);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

    if (activate_filter == XSANE_FILE_FILTER_ICM)
    {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(filechooser), filter);
    }

    /* add path to filechooser */
    if (getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)) != NULL)
    {
      snprintf(buf, sizeof(buf)-2, "%s%c.color%cicc", getenv(STRINGIFY(ENVIRONMENT_HOME_DIR_NAME)), SLASH, SLASH);
      gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(filechooser), buf, NULL);
    }
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(filechooser), "/usr/share/color/icc", NULL);
  }

  if (enable_filters & XSANE_FILE_FILTER_IMAGES) /* filter: images */
  {
   GtkFileFilter *filter;

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][gG]");
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][eE][gG]");
    gtk_file_filter_add_pattern(filter, "*.[pP][nN][gG]");
    gtk_file_filter_add_pattern(filter, "*.[tT][iI][fF]");
    gtk_file_filter_add_pattern(filter, "*.[tT][iI][fF][fF]");
    gtk_file_filter_add_pattern(filter, "*.[pP][sS]");
    gtk_file_filter_add_pattern(filter, "*.[pP][dD][fF]");
    gtk_file_filter_add_pattern(filter, "*.[pP][nN][mM]");
    gtk_file_filter_add_pattern(filter, "*.[pP][bB][mM]");
    gtk_file_filter_add_pattern(filter, "*.[pP][gG][mM]");
    gtk_file_filter_add_pattern(filter, "*.[pP][pP][mM]");

    gtk_file_filter_set_name(filter, FILE_FILTER_IMAGES);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

    if (activate_filter == XSANE_FILE_FILTER_IMAGES)
    {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(filechooser), filter);
    }
  }

  if (enable_filters & XSANE_FILE_FILTER_BATCHLIST) /* filter: color management profiles */
  {
   GtkFileFilter *filter;

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[xX][bV][lL]");

    gtk_file_filter_set_name(filter, FILE_FILTER_XBL);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), filter);

    if (activate_filter == XSANE_FILE_FILTER_BATCHLIST)
    {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(filechooser), filter);
    }
  }

  /* set default filename */
  if (default_name) /* select file */
  {
   const char *basename = default_name;
   char *path;
   int pos;

    DBG(DBG_info, "xsane_back_gtk_get_filename: default_name =%s\n", default_name);

    path = strdup(default_name);
    for (pos = strlen(path)-1; pos > 0; pos--)
    {
      if (path[pos] == '/')
      {
        path[pos]=0;

	basename = path+pos+1;
       break;
      }
    }

    if (pos)
    {
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), path);
    }

    if ((action == XSANE_FILE_CHOOSER_ACTION_SAVE) || (action == XSANE_FILE_CHOOSER_ACTION_SELECT_SAVE))
    {
      gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filechooser), (char *) basename);
    }
    else
    {
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filechooser), (char *) default_name);
    }
  }


  /* add filetype menu */

  if (show_extra_widgets)
  {
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *label;

    vbox = gtk_vbox_new(FALSE, 15);
    gtk_widget_show(vbox);

    if (show_extra_widgets & XSANE_GET_FILENAME_SHOW_FILETYPE)
    {
      DBG(DBG_info, "xsane_back_gtk_get_filename: showing filetype menu\n");
  
      if (filechooser_filetype)
      {
        free(filechooser_filetype);
      }
  
      if ((filetype) && (*filetype))
      {
        filechooser_filetype = strdup(*filetype);
      }
      else
      {
        filechooser_filetype = NULL;
      }
  
      hbox = gtk_hbox_new(FALSE, 2);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  
      label = gtk_label_new(TEXT_FILETYPE);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
      gtk_widget_show(label);
  
      xsane_filetype_option_menu = xsane_back_gtk_filetype_menu_new(filechooser_filetype, (GtkSignalFunc) xsane_back_gtk_filetype2_callback);
      gtk_box_pack_start(GTK_BOX(hbox), xsane_filetype_option_menu, TRUE, TRUE, 2);
      gtk_widget_show(xsane_filetype_option_menu);
    }

#ifdef HAVE_LIBLCMS 
    if ((cms_function) && (show_extra_widgets & XSANE_GET_FILENAME_SHOW_CMS_FUNCTION))
    {
      hbox = gtk_hbox_new(FALSE, 2);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  
      label = gtk_label_new(TEXT_CMS_FUNCTION);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
      gtk_widget_show(label);
  
      xsane_cms_function_option_menu = xsane_back_gtk_cms_function_menu_new(*cms_function, NULL);
      gtk_box_pack_start(GTK_BOX(hbox), xsane_cms_function_option_menu, TRUE, TRUE, 2);
      gtk_widget_show(xsane_cms_function_option_menu);
    }
#endif

    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(filechooser), vbox);
  }


  gtk_widget_show(filechooser);

  result = gtk_dialog_run(GTK_DIALOG(filechooser));

  DBG(DBG_info, "xsane_back_gtk_get_filename: gtk_dialog_run() returned with result=%d\n", result);

  if (result == accept_code)
  {
   char *chooser_filename;

    if ((filetype) && (*filetype))
    {
      free(*filetype);
      *filetype = NULL;
    }

    if (filechooser_filetype)
    {
      if (filetype)
      {
        *filetype = strdup(filechooser_filetype);
      }
    }

#ifdef HAVE_LIBLCMS 
    if ((cms_function) && (show_extra_widgets & XSANE_GET_FILENAME_SHOW_CMS_FUNCTION))
    {
      *cms_function = gtk_option_menu_get_history(GTK_OPTION_MENU(xsane_cms_function_option_menu));

      DBG(DBG_info, "selected cms_function = %d\n", *cms_function);
    }
#endif

    chooser_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooser));
    strncpy(filename, chooser_filename, max_len - 1);
    g_free(chooser_filename);

    filename[max_len - 1] = '\0';

    ok = TRUE;
  }

  gtk_widget_destroy(filechooser);
  filechooser = NULL;

 return ok ? 0 : -1;
}

#else

GtkWidget *fileselection;
char *fileselection_filetype = NULL;

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_get_filename_button_clicked(GtkWidget *w, gpointer data)
{
 int *clicked = data;

  DBG(DBG_proc, "xsane_back_gtk_get_filename_button_clicked\n");
  *clicked = 1;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_filetype_callback(GtkWidget *widget, gpointer data)
{
 char *extension, *filename;
 char buffer[PATH_MAX];
 char *new_filetype = (char *) data;

  DBG(DBG_proc, "xsane_filetype_callback\n");

  filename = strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselection)));

  if ((new_filetype) && (*new_filetype))
  {
    extension = strrchr(filename, '.');

    if ((extension) && (extension != filename))
    {
      if ( (!strcasecmp(extension, ".pnm"))  || (!strcasecmp(extension, ".raw"))
        || (!strcasecmp(extension, ".png"))  || (!strcasecmp(extension, ".ps"))
        || (!strcasecmp(extension, ".pdf"))  || (!strcasecmp(extension, ".rgba"))
        || (!strcasecmp(extension, ".tiff")) || (!strcasecmp(extension, ".tif"))
        || (!strcasecmp(extension, ".text")) || (!strcasecmp(extension, ".txt"))
        || (!strcasecmp(extension, ".jpg"))  || (!strcasecmp(extension, ".jpeg"))
         ) /* remove filetype extension */
      {
        *extension = 0; /* remove extension */
      }
    }
    snprintf(buffer, sizeof(buffer), "%s%s", filename, new_filetype);
    free(filename);
    filename = strdup(buffer);
  }

  if (fileselection_filetype)
  {
    free(fileselection_filetype);
    fileselection_filetype = NULL;
  }

  if (data)
  {
    fileselection_filetype = strdup(new_filetype); 
  }

  gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselection), filename);

  free(filename);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_back_gtk_get_filename(const char *label, const char *default_name, size_t max_len, char *filename, char **filetype, int *cms_function,
                                XsaneFileChooserAction action, int show_filetype_menu, int enable_filters, int activate_filter)
{
 int cancel = 0, ok = 0, destroy = 0;
 GtkAccelGroup *accelerator_group;
 GtkWidget *xsane_filetype_option_menu;
 int show_fileopts = 0;
 int select_directory = 0;

  if (action == XSANE_FILE_CHOOSER_ACTION_SELECT_FOLDER)
  {
    select_directory = TRUE;
  }
  else
  {
    show_fileopts = TRUE;
  }

  DBG(DBG_proc, "xsane_back_gtk_get_filename\n");

  if (fileselection)
  {
    gdk_beep();
    return -1; /* cancel => do not allow to open more than one fileselection dialog */
  }

  fileselection = gtk_file_selection_new((char *) label);
  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(fileselection), accelerator_group);

  g_signal_connect(GTK_OBJECT(fileselection), "destroy", GTK_SIGNAL_FUNC(xsane_back_gtk_get_filename_button_clicked), &destroy);

  g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselection)->cancel_button), "clicked", (GtkSignalFunc) xsane_back_gtk_get_filename_button_clicked, &cancel);
  gtk_widget_add_accelerator(GTK_FILE_SELECTION(fileselection)->cancel_button, "clicked",
                              accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);

  g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselection)->ok_button), "clicked", (GtkSignalFunc) xsane_back_gtk_get_filename_button_clicked, &ok);

  if (select_directory)
  {
    DBG(DBG_info, "xsane_back_gtk_get_filename: select directory\n");
    gtk_widget_hide(GTK_FILE_SELECTION(fileselection)->file_list->parent);
    gtk_widget_hide(GTK_FILE_SELECTION(fileselection)->fileop_del_file);
    gtk_widget_hide(GTK_FILE_SELECTION(fileselection)->fileop_ren_file);
    gtk_widget_hide(GTK_FILE_SELECTION(fileselection)->selection_entry);

    gtk_widget_set_size_request(GTK_FILE_SELECTION(fileselection)->dir_list, 280, 230);

    if (default_name) /* add "/." to end of directory name so that the gtkfilesel* behaves correct */
    {
     char directory_name[PATH_MAX];

      snprintf(directory_name, sizeof(directory_name), "%s%c", default_name, SLASH);
      DBG(DBG_info, "xsane_back_gtk_get_filename: directory_name =%s\n", directory_name);
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselection), (char *) directory_name);
    }
  }
  else if (default_name) /* select file */
  {
    DBG(DBG_info, "xsane_back_gtk_get_filename: default_name =%s\n", default_name);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselection), (char *) default_name);
  }

  if (show_fileopts)
  {
    DBG(DBG_info, "xsane_back_gtk_get_filename: showing file-options\n");
    gtk_file_selection_show_fileop_buttons(GTK_FILE_SELECTION(fileselection));
  }
  else
  {
    DBG(DBG_info, "xsane_back_gtk_get_filename: hiding file-options\n");
    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(fileselection));
  }

  if (show_filetype_menu)
  {
   GtkWidget *hbox;
   GtkWidget *vbox;
   GtkWidget *label;

    DBG(DBG_info, "xsane_back_gtk_get_filename: showing filetype menu\n");

    if (fileselection_filetype)
    {
      free(fileselection_filetype);
    }

    if ((filetype) && (*filetype))
    {
      fileselection_filetype = strdup(*filetype);
    }
    else
    {
      fileselection_filetype = NULL;
    }

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(fileselection)->action_area), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new(TEXT_FILETYPE);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    xsane_filetype_option_menu = xsane_back_gtk_filetype_menu_new(fileselection_filetype, (GtkSignalFunc) xsane_back_gtk_filetype_callback);
    gtk_box_pack_start(GTK_BOX(hbox), xsane_filetype_option_menu, TRUE, TRUE, 2);
    gtk_widget_show(xsane_filetype_option_menu);
  }

  gtk_widget_show(fileselection);

  DBG(DBG_info, "xsane_back_gtk_get_filename: waiting for user action\n");
  while (!cancel && !ok && !destroy)
  {
    gtk_main_iteration();
  }

  if (ok)
  {
   size_t len, cwd_len;
   char *cwd;

    DBG(DBG_info, "ok button pressed\n");

    if ((filetype) && (*filetype))
    {
      free(*filetype);
      *filetype = NULL;
    }

    if (fileselection_filetype)
    {
      if (filetype)
      {
        *filetype = strdup(fileselection_filetype);
      }

      free(fileselection_filetype);
      fileselection_filetype = NULL;
    }

    strncpy(filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselection)), max_len - 1);

#ifndef HAVE_GTK2
    /* in gtk1 we have to remove the text that is defined in the selection entry to get a proper behaviour */
    if (select_directory)
    {
      *(filename+strlen(filename)-strlen(gtk_entry_get_text(GTK_ENTRY(GTK_FILE_SELECTION(fileselection)->selection_entry)))) = '\0';
    }
#endif

    filename[max_len - 1] = '\0';

    len = strlen(filename);

    cwd = alloca(len + 2); /* alloca => memory is freed on return */
    getcwd(cwd, len + 1);
    cwd_len = strlen(cwd);
    cwd[cwd_len++] = '/';
    cwd[cwd_len] = '\0';

    DBG(DBG_info, "xsane_back_gtk_get_filename: full path filename = %s\n", filename);
  }

  if (!destroy)
  {
    gtk_widget_destroy(fileselection);
  }

  fileselection = NULL;

  return ok ? 0 : -1;
}
#endif

/* ----------------------------------------------------------------------------------------------------------------- */

static gint xsane_back_gtk_autobutton_update(GtkWidget *widget, DialogElement *elem)
{
 int opt_num = elem - xsane.element;
 const SANE_Option_Descriptor *opt;
 SANE_Status status;
 SANE_Word val;
 char buf[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_back_gtk_autobutton_update\n");

  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    xsane_back_gtk_set_option(opt_num, 0, SANE_ACTION_SET_AUTO);

    gtk_widget_set_sensitive(elem->widget, FALSE);

    if (elem->widget2)
    {
      gtk_widget_set_sensitive(elem->widget2, FALSE);
    }
  }
  else
  {
    gtk_widget_set_sensitive(elem->widget, TRUE);

    if (elem->widget2)
    {
      gtk_widget_set_sensitive(elem->widget2, TRUE);
    }

    status = xsane_control_option(xsane.dev, opt_num, SANE_ACTION_GET_VALUE, &val, 0);
    if (status != SANE_STATUS_GOOD)
    {
      snprintf(buf, sizeof(buf), "%s %s: %s.", ERR_GET_OPTION, opt->name, XSANE_STRSTATUS(status));
      xsane_back_gtk_error(buf, FALSE);
    }
    xsane_back_gtk_set_option(opt_num, &val, SANE_ACTION_SET_VALUE);
  }
  return FALSE;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_autobutton_new(GtkWidget *parent, DialogElement *elem,
		GtkTooltips *tooltips)
{
 GtkWidget *button;

  DBG(DBG_proc, "xsane_back_gtk_autobutton_new\n");

  button = gtk_check_button_new();
  gtk_container_set_border_width(GTK_CONTAINER(button), 0);
  gtk_widget_set_size_request(button, 20, 20);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_back_gtk_autobutton_update, elem);
  xsane_back_gtk_set_tooltip(tooltips, button, DESC_AUTOMATIC);

  gtk_box_pack_end(GTK_BOX(parent), button, FALSE, FALSE, 2);
  gtk_widget_show(button);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static gint xsane_back_gtk_button_update(GtkWidget * widget, DialogElement * elem)
{
 int opt_num = elem - xsane.element;
 const SANE_Option_Descriptor *opt;
 SANE_Word val = SANE_FALSE;

  DBG(DBG_proc, "xsane_back_gtk_button_update\n");

  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
  if (GTK_TOGGLE_BUTTON(widget)->active)
  {
    val = SANE_TRUE;
  }
  xsane_back_gtk_set_option(opt_num, &val, SANE_ACTION_SET_VALUE);

  return FALSE;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_button_new(GtkWidget * parent, const char *name, SANE_Word val,
	    DialogElement * elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *button;

  DBG(DBG_proc, "xsane_back_gtk_button_new\n");

  button = gtk_check_button_new_with_label((char *) name);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), val);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_back_gtk_button_update, elem);
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, TRUE, 0);
  gtk_widget_show(button);
  xsane_back_gtk_set_tooltip(tooltips, button, desc);

  gtk_widget_set_sensitive(button, settable);

  elem->widget  = button;
}

/* ----------------------------------------------------------------------------------------------------------------- */

/* called from xsane_back_gtk_value_new and xsane_back_gtk_range_new */
static void xsane_back_gtk_value_update(GtkAdjustment *adj_data, DialogElement *elem)
{
 const SANE_Option_Descriptor *opt;
 SANE_Word val, new_val;
 int opt_num;
 double d;

  DBG(DBG_proc, "xsane_back_gtk_value_update\n");

  opt_num = elem - xsane.element;
  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
  switch(opt->type)
  {
    case SANE_TYPE_INT:
     val = adj_data->value; /* OLD:  + 0.5 but this mad problems with negative values */
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
      DBG(DBG_error, "xsane_back_gtk_value_update: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
     return;
  }

  xsane_back_gtk_set_option(opt_num, &val, SANE_ACTION_SET_VALUE);
  xsane_control_option(xsane.dev, opt_num, SANE_ACTION_GET_VALUE, &new_val, 0);
#if 1
  val = new_val;
  switch(opt->type)
  {
    case SANE_TYPE_INT:
      if (new_val != val)
      {
        adj_data->value = val;
        g_signal_emit_by_name(GTK_OBJECT(adj_data), "value_changed");
      }
     break;

    case SANE_TYPE_FIXED:
      if (abs(new_val - val) > 1) /* tolarate 1/65536 error, instead of: if (new_val != val) */
      {
        d = SANE_UNFIX(val);
        if (opt->unit == SANE_UNIT_MM)
        {
          d /= preferences.length_unit;
        }
        adj_data->value = d;
        g_signal_emit_by_name(GTK_OBJECT(adj_data), "value_changed");
      }
     break;

    default:
     break;
  }
#endif
#if 0
  if (abs(new_val - val) > 1) /* tolarate 1/65536 error, instead of: if (new_val != val) */
  {
    val = new_val;
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
    g_signal_emit_by_name(GTK_OBJECT(adj_data), "value_changed");
  }
#endif

 return;			/* value didn't change */
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_range_display_value_right_callback(GtkAdjustment *adjust, gpointer data)
{
 gchar buf[TEXTBUFSIZE];
 int digits = (int) data;
 GtkLabel *label;
 
  snprintf(buf, sizeof(buf), "%1.*f", digits, adjust->value);
  label = (GtkLabel *) gtk_object_get_data(GTK_OBJECT(adjust), "value-label");
  gtk_label_set_text(label, buf);
}                                                                                                                                
/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_range_new(GtkWidget *parent, const char *name, gfloat val,
	   gfloat min, gfloat max, gfloat quant, int automatic,
	   DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *hbox, *label, *slider = NULL, *spinbutton, *value_label;
 int digits;

  DBG(DBG_proc, "xsane_back_gtk_range_new(%s)\n", name);

  if (quant - (int) quant == 0.0)
  {
    digits = 0;
  }
  else
  {
    digits = (int) (log10(1/quant)+0.8); /* set number of digits in dependance of quantization */
  }

  if (digits < 0)
  {
    digits = 0;
  }

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  elem->data = gtk_adjustment_new(val, min, max, quant, quant*10, 0);

  /* value label */
  if (preferences.show_range_mode & 8)
  {
    value_label = gtk_label_new("");
    gtk_widget_set_size_request(value_label, 45, -1);
    gtk_box_pack_end(GTK_BOX(hbox), value_label, FALSE, FALSE, 1);
 
    g_signal_connect(elem->data, "value_changed", (GtkSignalFunc) xsane_back_gtk_range_display_value_right_callback, (void *) digits);
    gtk_object_set_data(GTK_OBJECT(elem->data), "value-label", value_label);
    g_signal_emit_by_name(GTK_OBJECT(elem->data), "value_changed"); /* update value */
    gtk_widget_show(value_label);
    gtk_widget_set_sensitive(value_label, settable);
  }
 
  /* spinbutton */
  if (preferences.show_range_mode & 4)
  {
#ifndef HAVE_GTK2
    if (digits > 5)
    {
      digits = 5;
    }
#endif
    spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(elem->data), 0, digits);

    if (preferences.show_range_mode & 3) /* slider also visible */
    {
      gtk_widget_set_size_request(spinbutton, 70, -1);
    }
    else /* slider not visible */
    {
      gtk_widget_set_size_request(spinbutton, 100, -1);
    }

    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, desc);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    gtk_box_pack_end(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 5); /* make spinbutton not sizeable */
    gtk_widget_show(spinbutton);
    gtk_widget_set_sensitive(spinbutton, settable);
    elem->widget = spinbutton;
  }

  /* slider */
  if (preferences.show_range_mode & 3)
  {
    if (preferences.show_range_mode & 1) /* bit 0 (val 1) : scale */
    {
      slider = gtk_hscale_new(GTK_ADJUSTMENT(elem->data));
      gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    }
    else /* bit 1 (val 2) : scrollbar */
    {
      slider = gtk_hscrollbar_new(GTK_ADJUSTMENT(elem->data));
    }
    xsane_back_gtk_set_tooltip(xsane.tooltips, slider, desc);
    gtk_widget_set_size_request(slider, 140, -1);
    /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
    gtk_range_set_update_policy(GTK_RANGE(slider), preferences.gtk_update_policy);
    gtk_box_pack_end(GTK_BOX(hbox), slider, FALSE, FALSE, 5); /* make slider not sizeable */
    gtk_widget_show(slider);
    gtk_widget_set_sensitive(slider, settable);
  }

  if (automatic)
  {
    xsane_back_gtk_autobutton_new(hbox, elem, tooltips);
  }

  g_signal_connect(elem->data, "value_changed", (GtkSignalFunc) xsane_back_gtk_value_update, elem);

  gtk_widget_show(label);
  gtk_widget_show(hbox);

  if (elem->widget)
  {
    elem->widget2 = slider; /* widget is used by spinbutton */
  }
  else
  {
    elem->widget  = slider; /* we do not have a spinbutton */
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_value_new(GtkWidget *parent, const char *name, gfloat val,
	   gfloat quant, int automatic,
	   DialogElement *elem, GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *hbox, *label, *spinbutton;
 int digits;

  DBG(DBG_proc, "xsane_back_gtk_value_new(%s)\n", name);

  if (quant - (int) quant == 0.0)
  {
    digits = 0;
  }
  else
  {
    digits = (int) (log10(1/quant)+0.8); /* set number of digits in dependance of quantization */
  }

  if (digits < 0)
  {
    digits = 0;
  }

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  elem->data = gtk_adjustment_new(val, -1e29, 1e29, 1, 10, 0);

  /* spinbutton */
#ifndef HAVE_GTK2
  if (digits > 5)
  {
    digits = 5;
  }
#endif
  spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(elem->data), 0, digits);

  if (preferences.show_range_mode & 3) /* sliders are visible */
  {
    gtk_widget_set_size_request(spinbutton, 70, -1);
  }
  else /* sliders not visible */
  {
    gtk_widget_set_size_request(spinbutton, 100, -1);
  }

  xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, desc);
  gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
  gtk_box_pack_end(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 5); /* make spinbutton not sizeable */
  gtk_widget_show(spinbutton);
  gtk_widget_set_sensitive(spinbutton, settable);
  elem->widget = spinbutton;

  if (automatic)
  {
    xsane_back_gtk_autobutton_new(hbox, elem, tooltips);
  }

  g_signal_connect(elem->data, "value_changed", (GtkSignalFunc) xsane_back_gtk_value_update, elem);

  gtk_widget_show(label);
  gtk_widget_show(hbox);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_push_button_callback(GtkWidget * widget, gpointer data)
{
 DialogElement *elem = data;
 int opt_num;

  DBG(DBG_proc, "xsane_back_gtk_push_button_callback\n");

  opt_num = elem - xsane.element;
  xsane_back_gtk_set_option(opt_num, 0, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static int xsane_back_gtk_option_menu_lookup(MenuItem menu_items[], const char *string)
{
 int i;

  DBG(DBG_proc, "xsane_back_gtk_option_menu_lookup\n");

  for (i = 0; (menu_items[i].label) && strcmp(menu_items[i].label, string) != 0; ++i);

 return i;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_option_menu_callback(GtkWidget * widget, gpointer data)
{
 MenuItem *menu_item = data;
 DialogElement *elem = menu_item->elem;
 const SANE_Option_Descriptor *opt;
 int opt_num;
 double dval;
 SANE_Word val;
 void *valp = &val;

  DBG(DBG_proc, "xsane_back_gtk_option_menu_callback\n");

  opt_num = elem - xsane.element;
  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
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
      DBG(DBG_error, "xsane_back_gtk_option_menu_callback: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
     break;
  }
  xsane_back_gtk_set_option(opt_num, valp, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_option_menu_new(GtkWidget *parent, const char *name, char *str_list[],
		 const char *val, DialogElement *elem,
		 GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *hbox, *label, *option_menu, *menu, *item;
 MenuItem *menu_items;
 int i, num_items;

  DBG(DBG_proc, "xsane_back_gtk_option_menu_new(%s)\n", name);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  for (num_items = 0; str_list[num_items]; ++num_items)
  {
  }

  menu_items = malloc((num_items + 1) * sizeof(menu_items[0]));

  menu = gtk_menu_new();
  for (i = 0; i < num_items; ++i)
  {
    item = gtk_menu_item_new_with_label(_BGT(str_list[i]));
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_back_gtk_option_menu_callback, menu_items + i);

    gtk_widget_show(item);

    menu_items[i].label = str_list[i];
    menu_items[i].elem = elem;
    menu_items[i].index = i;
  }

  /* add empty element as end of list marker */
  menu_items[i].label = NULL;
  menu_items[i].elem = NULL;
  menu_items[i].index = 0;

  option_menu = gtk_option_menu_new();
  gtk_box_pack_end(GTK_BOX(hbox), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_back_gtk_option_menu_lookup(menu_items, val));
  xsane_back_gtk_set_tooltip(tooltips, option_menu, desc);

  gtk_widget_show(label);
  gtk_widget_show(option_menu);
  gtk_widget_show(hbox);

  gtk_widget_set_sensitive(option_menu, settable);

  elem->widget  = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_text_entry_callback(GtkWidget *w, gpointer data)
{
 DialogElement *elem = data;
 const SANE_Option_Descriptor *opt;
 const gchar *text;
 int opt_num;
 char *buf;

  DBG(DBG_proc, "xsane_back_gtk_text_entry_callback\n");

  opt_num = elem - xsane.element;
  opt = xsane_get_option_descriptor(xsane.dev, opt_num);

  buf = alloca(opt->size);
  buf[0] = '\0';

  text = gtk_entry_get_text(GTK_ENTRY(elem->widget));
  if (text)
  {
    strncpy(buf, text, opt->size);
  }
  buf[opt->size - 1] = '\0';

  xsane_back_gtk_set_option(opt_num, buf, SANE_ACTION_SET_VALUE);

  if (strcmp(buf, text) != 0) /* the backend modified the option value; update widget: */
  {
    gtk_entry_set_text(GTK_ENTRY(elem->widget), buf);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_text_entry_new(GtkWidget * parent, const char *name, const char *val, DialogElement *elem,
		        GtkTooltips *tooltips, const char *desc, SANE_Int settable)
{
 GtkWidget *hbox, *text, *label;

  DBG(DBG_proc, "xsane_back_gtk_text_entry_new\n");

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

  label = gtk_label_new((char *) name);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  text = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text), (char *) val);
/*  gtk_box_pack_start(GTK_BOX(hbox), text, FALSE, TRUE, 0); */ /* text entry fixed */
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0); /* text entry sizeable */
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_back_gtk_text_entry_callback, elem);
  xsane_back_gtk_set_tooltip(tooltips, text, desc);

  gtk_widget_show(hbox);
  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_widget_set_sensitive(text, settable);

  elem->widget  = text;
}

/* ----------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_back_gtk_group_new(GtkWidget *parent, const char *title)
{
 GtkWidget * frame, * vbox;

  DBG(DBG_proc, "xsane_back_gtk_group_new(%s)\n", title);

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
static void tooltips_destroy(void)
{
  DBG(DBG_proc, "tooltips_destroy\n");

  gtk_object_unref(GTK_OBJECT(xsane.tooltips));
  xsane.tooltips = 0;
}
#endif
/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_panel_destroy(void)
{
 const SANE_Option_Descriptor *opt;
 DialogElement *elem;
 int i, j;

  DBG(DBG_proc, "xsane_back_gtk_panel_destroy\n");

  if (!xsane.xsane_hbox)
  {
    DBG(DBG_proc, "xsane_back_gtk_panel_destroy: panel does not exist\n");
   return;
  }

  gtk_widget_destroy(xsane.xsane_hbox);
  gtk_widget_destroy(xsane.standard_hbox);
  gtk_widget_destroy(xsane.advanced_hbox);

  xsane.xsane_hbox    = NULL;
  xsane.standard_hbox = NULL;
  xsane.advanced_hbox = NULL;

  /* free the menu labels of integer/fix-point word-lists: */
  for (i = 0; i < xsane.num_elements; ++i)
  {
    if (xsane.element[i].menu)
    {
      opt = xsane_get_option_descriptor(xsane.dev, i);
      elem = xsane.element + i;
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
  memset(xsane.element, 0, xsane.num_elements * sizeof(xsane.element[0]));
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_back_gtk_panel_rebuild(void)
{
  DBG(DBG_proc, "xsane_back_gtk_panel_rebuild\n");

  xsane_back_gtk_panel_destroy();
  xsane_panel_build();
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_refresh_dialog(void)
{
  DBG(DBG_proc, "xsane_back_gtk_refresh_dialog\n");

  xsane_back_gtk_panel_rebuild();
  xsane_update_param(0);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_update_scan_window(void)
{
 const SANE_Option_Descriptor *opt;
 double old_val, new_val;
 DialogElement *elem;
 SANE_Status status;
 SANE_Word word;
 int i, optnum;
 char str[64];

  DBG(DBG_proc, "xsane_back_gtk_update_scan_window\n");

  for (i = 0; i < 4; ++i)
  {
    if (xsane.well_known.coord[i] > 0)
    {
      optnum = xsane.well_known.coord[i];
      elem = xsane.element + optnum;
      opt = xsane_get_option_descriptor(xsane.dev, optnum);

      status = xsane_control_option(xsane.dev, optnum, SANE_ACTION_GET_VALUE, &word, 0);
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

          if (old_val != new_val) /* XXX dangerous comparison of doubles, we should allow tiny differences */
          {
	      g_signal_emit_by_name(GTK_OBJECT(elem->data), "value_changed");
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

void xsane_back_gtk_update_vector(int opt_num, SANE_Int *vector)
{
 const SANE_Option_Descriptor *opt;
 gfloat val;
 SANE_Word *optval;
 int j, optlen;

  DBG(DBG_proc, "xsane_back_gtk_update_vector\n");

  if (opt_num < 1)
    return; /* not defined */

  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
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

  xsane_back_gtk_set_option(opt_num, optval, SANE_ACTION_SET_VALUE);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_tooltips(int enable)
{
  DBG(DBG_proc, "xsane_back_gtk_set_tooltips\n");

  if (!xsane.tooltips)
  {
    return;
  }

  if (enable)
  {
    gtk_tooltips_enable(xsane.tooltips);
  }
  else
  {
    gtk_tooltips_disable(xsane.tooltips);
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_set_sensitivity(int sensitive)
{
 const SANE_Option_Descriptor *opt;
 int i;

  DBG(DBG_proc, "xsane_back_gtk_set_sensitivity\n");

  for (i = 0; i < xsane.num_elements; ++i)
  {
    opt = xsane_get_option_descriptor(xsane.dev, i);

    if (!SANE_OPTION_IS_ACTIVE(opt->cap) || !SANE_OPTION_IS_SETTABLE(opt->cap) ||
        opt->type == SANE_TYPE_GROUP || !xsane.element[i].widget)
    {
      continue;
    }

  //  if (!(opt->cap & SANE_CAP_ALWAYS_SETTABLE))
    {
      gtk_widget_set_sensitive(xsane.element[i].widget, sensitive);
    }
  }

  if (xsane.xsanemode_widget)
  {
    gtk_widget_set_sensitive(xsane.xsanemode_widget, sensitive); 
  }

  while (gtk_events_pending())
  {
   gtk_main_iteration();
  }   
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_sensitivity(SANE_Int sensitivity)
{
  DBG(DBG_proc, "xsane_set_sensitivity(%d)\n", sensitivity);

  if (xsane.dialog)
  {
    /* clear or rebuild histogram */
    if (sensitivity)
    {
      xsane_update_histogram(TRUE /* update raw */);
    }
    else
    {
      xsane_clear_histogram(&xsane.histogram_raw);
      xsane_clear_histogram(&xsane.histogram_enh);
    }

    gtk_widget_set_sensitive(xsane.menubar, sensitivity); 
    gtk_widget_set_sensitive(xsane.xsane_window, sensitivity); 
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), sensitivity);
    gtk_widget_set_sensitive(xsane.standard_options_dialog, sensitivity);
    gtk_widget_set_sensitive(xsane.advanced_options_dialog, sensitivity);
    gtk_widget_set_sensitive(xsane.histogram_dialog, sensitivity);
#ifdef HAVE_WORKING_GTK_GAMMACURVE
    gtk_widget_set_sensitive(xsane.gamma_dialog, sensitivity);
#endif
  }

  if (xsane.preview)
  {
    gtk_widget_set_sensitive(xsane.preview->button_box, sensitivity);   /* button box at top of window */
    gtk_widget_set_sensitive(xsane.preview->menu_box, sensitivity);     /* menu box at top of window */
#if 1
    gtk_widget_set_sensitive(xsane.preview->viewport, sensitivity);     /* Preview image selection */
#endif
    gtk_widget_set_sensitive(xsane.preview->start, sensitivity);        /* Acquire preview button */
  }

  if (xsane.project_dialog)
  {
    /* do not change sensitivity of project_dialog, we want the progress bar */
    /* to be sensitive */
    gtk_widget_set_sensitive(xsane.project_box, sensitivity);
    gtk_widget_set_sensitive(xsane.project_exists, sensitivity);
    gtk_widget_set_sensitive(xsane.project_entry_box, sensitivity);
  }

  if (xsane.batch_scan_dialog)
  {
    gtk_widget_set_sensitive(xsane.batch_scan_button_box, sensitivity);
    gtk_widget_set_sensitive(xsane.batch_scan_action_box, sensitivity);
  }

  xsane.sensitivity = sensitivity;
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_back_gtk_destroy_dialog(void)
{
 SANE_Handle dev = xsane.dev;

  DBG(DBG_proc, "xsane_back_gtk_destroy_dialog\n");

  xsane_back_gtk_panel_destroy();
  free((void *) xsane.dev_name);
  free(xsane.element);

  sane_close(dev);
}
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_window_icon(GtkWidget *gtk_window, gchar **xpm_d)
{
 GdkPixmap *pixmap;
 GdkBitmap *mask;

  DBG(DBG_proc, "xsane_set_window_icon\n");

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
