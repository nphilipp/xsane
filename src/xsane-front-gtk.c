/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-front-gtk.c

   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998-2001 Oliver Rauch
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
#include "xsane-preferences.h"
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"
#include <md5.h>

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_get_bounds(const SANE_Option_Descriptor *opt, double *minp, double *maxp);
double xsane_find_best_resolution(int well_known_option, double dpi);
int xsane_set_resolution(int well_known_option, double resolution);
void xsane_set_all_resolutions(void);
void xsane_define_maximum_output_size();
void xsane_close_dialog_callback(GtkWidget *widget, gpointer data);
void xsane_authorization_button_callback(GtkWidget *widget, gpointer data);
gint xsane_authorization_callback(SANE_String_Const resource,
                                  SANE_Char username[SANE_MAX_USERNAME_LEN],
                                  SANE_Char password[SANE_MAX_PASSWORD_LEN]);
void xsane_progress_cancel(GtkWidget *widget, gpointer data);
void xsane_progress_new(char *bar_text, char *info, GtkSignalFunc callback);
void xsane_progress_update(gfloat newval);
void xsane_progress_clear();
GtkWidget *xsane_vendor_pixmap_new(GdkWindow *window, GtkWidget *parent);
GtkWidget *xsane_toggle_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                         int *state, void *xsane_toggle_button_callback);
GtkWidget *xsane_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                        void *xsane_button_callback, gpointer data);
void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc,
                           void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
void xsane_option_menu_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkObject **data, int option,
                                       void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
void xsane_scale_new(GtkBox *parent, char *labeltext, const char *desc,
                     float min, float max, float quant, float step, float page_step,
                     int digits, double *val, GtkObject **data, void *xsane_scale_callback, SANE_Int settable);
void xsane_scale_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                 float min, float max, float quant, float step, float page_step, int digits,
                                 double *val, GtkObject **data, int option, void *xsane_scale_callback, SANE_Int settable);
void xsane_separator_new(GtkWidget *xsane_parent, int dist);
GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn);
GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text);
void xsane_refresh_dialog(void);
void xsane_set_sensitivity(SANE_Int sensitivity);
void xsane_update_param(void *arg);
void xsane_define_output_filename(void);
void xsane_identify_output_format(char **ext);

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_get_bounds(const SANE_Option_Descriptor *opt, double *minp, double *maxp)
{
 double min, max;
 int i;

  DBG(DBG_proc, "xsane_get_bounds\n");

  min = -INF;
  max =  INF;
  switch (opt->constraint_type)
  {
    case SANE_CONSTRAINT_RANGE:
      min = opt->constraint.range->min;
      max = opt->constraint.range->max;
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      min =  INF;
      max = -INF;

      for (i = 1; i <= opt->constraint.word_list[0]; ++i)
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
     break;

    default:
     break;
  }

  if (opt->type == SANE_TYPE_FIXED)
  {
    if (min > -INF && min < INF)
    {
      min = SANE_UNFIX (min);
    }
    if (max > -INF && max < INF)
    {
      max = SANE_UNFIX (max);
    }
  }
  *minp = min;
  *maxp = max;
}                                                        

/* ---------------------------------------------------------------------------------------------------------------------- */

double xsane_find_best_resolution(int well_known_option, double dpi)
{
 const SANE_Option_Descriptor *opt;
 double bestdpi;

  DBG(DBG_proc, "xsane_find_best_resolution\n");

  opt = xsane_get_option_descriptor(xsane.dev, well_known_option);

  if (!opt)
  {
    return -1.0; /* option does not exits */
  }

  if (opt->constraint_type == SANE_CONSTRAINT_RANGE)
  {
   double quant=0;
   double min=0;
   double max=0;

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
        fprintf(stderr, "find_best_resolution: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
    }

    bestdpi = dpi;

    if (quant != 0) /* make sure selected value fits into quantisation */
    {
     int factor;
     double diff;

      factor = (int) (dpi - min) / quant;

      diff = dpi - min - factor * quant;
      bestdpi = min + factor * quant;

      if (diff >quant/2.0)
      {
        bestdpi += quant;
      }
    }

    if (bestdpi < min)
    {
      bestdpi = min;
    }

    if (bestdpi > max)
    {
      bestdpi = max;
    }
  }
  else if (opt->constraint_type == SANE_CONSTRAINT_WORD_LIST)
  {
   SANE_Word diff;
   SANE_Word val;
   int items;
   int i;

    items = opt->constraint.word_list[0];

    bestdpi = opt->constraint.word_list[1];
    if (opt->type == SANE_TYPE_FIXED)
    {
      bestdpi = SANE_UNFIX(bestdpi);
    }

    diff = abs(bestdpi - dpi);

    for (i=1; i<=items; i++)
    {
      val = opt->constraint.word_list[i];
      if (opt->type == SANE_TYPE_FIXED)
      {
        val = SANE_UNFIX(val);
      }

      if (abs(val - dpi) < diff)
      {
        diff = abs(val - dpi);
        bestdpi = val;
      }
    }
  }
  else
  {
    fprintf(stderr, "find_best_resolution: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
    return -1; /* error */
  }

  return bestdpi;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_set_resolution(int well_known_option, double resolution)
{
 const SANE_Option_Descriptor *opt;
 double bestdpi;
 SANE_Word dpi;

  DBG(DBG_proc, "xsane_set_resolution\n");

  opt = xsane_get_option_descriptor(xsane.dev, well_known_option);

  if (!opt)
  {
    return -1; /* option does not exits */
  }

  if (!SANE_OPTION_IS_ACTIVE(opt->cap))
  {
    return -1; /* option is not active */
  }

  bestdpi = xsane_find_best_resolution(well_known_option, resolution);

  if (bestdpi < 0)
  {
     fprintf(stderr, "set_resolution: %s\n", ERR_FAILED_SET_RESOLUTION);
    return -1;
  }

  switch (opt->type)
  {
    case SANE_TYPE_INT:
      dpi = bestdpi;
    break;

    case SANE_TYPE_FIXED:
      dpi = SANE_FIX(bestdpi);
    break;

    default:
     fprintf(stderr, "set_resolution: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
    return 1; /* error */
  }

  xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_SET_VALUE, &dpi, 0);
  return 0; /* everything is ok */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_all_resolutions(void)
{
 int printer_resolution;

  DBG(DBG_proc, "xsane_set_all_resolutions\n");

#if 0
  xsane_set_resolution(xsane.well_known.dpi_y, xsane.resolution_y); /* set y resolution if possible */
  if (xsane_set_resolution(xsane.well_known.dpi_x, xsane.resolution_x)) /* set x resolution if possible */
  {
    xsane_set_resolution(xsane.well_known.dpi, xsane.resolution); /* set common resolution if necessary */
    xsane.resolution_x = xsane.resolution;
    xsane.resolution_y = xsane.resolution;
  }
#else
  if (xsane_set_resolution(xsane.well_known.dpi_y, xsane.resolution_y)) /* set y resolution if possible */
  {
    xsane_set_resolution(xsane.well_known.dpi, xsane.resolution); /* set common resolution if necessary */
    xsane.resolution_x = xsane.resolution;
    xsane.resolution_y = xsane.resolution;
  }
  else
  {
    xsane_set_resolution(xsane.well_known.dpi_x, xsane.resolution_x); /* set x resolution if possible */
  }
#endif

  switch (xsane.param.format)
  {
    case SANE_FRAME_GRAY:
      if (xsane.param.depth == 1)
      {
        printer_resolution = preferences.printer[preferences.printernr]->lineart_resolution;
      }
      else
      {
        printer_resolution = preferences.printer[preferences.printernr]->grayscale_resolution;
      }
    break;

    case SANE_FRAME_RGB:
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    default:
      printer_resolution = preferences.printer[preferences.printernr]->color_resolution;
    break;
  }

  xsane.zoom   = xsane.resolution   / printer_resolution;
  xsane.zoom_x = xsane.resolution_x / printer_resolution;
  xsane.zoom_y = xsane.resolution_y / printer_resolution; 
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_define_maximum_output_size()
{
 const SANE_Option_Descriptor *opt;

  DBG(DBG_proc, "xsane_define_maximum_output_size\n");

  opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.coord[0]);

  if ( (opt) && (opt->unit== SANE_UNIT_MM) )
  {
    switch(xsane.xsane_mode)
    {
      case XSANE_SCAN:

        xsane_define_output_filename();
        xsane_identify_output_format(0);

        if (xsane.xsane_output_format == XSANE_PS) 
        {
          if (preferences.psrotate) /* rotate: landscape */
          {
            preview_set_maximum_output_size(xsane.preview, preferences.psfile_height, preferences.psfile_width);
          }
          else /* do not rotate: portrait */
          {
            preview_set_maximum_output_size(xsane.preview, preferences.psfile_width, preferences.psfile_height);
          }
        }
        else
        {
          preview_set_maximum_output_size(xsane.preview, INF, INF);
        }
       break;

      case XSANE_COPY:
        if (preferences.psrotate) /* rotate: landscape */
        {
          preview_set_maximum_output_size(xsane.preview,
                                          preferences.printer[preferences.printernr]->height / xsane.zoom_y,
                                          preferences.printer[preferences.printernr]->width  / xsane.zoom_x);
        }
        else /* do not rotate: portrait */
        {
          preview_set_maximum_output_size(xsane.preview,
                                          preferences.printer[preferences.printernr]->width  / xsane.zoom_x,
                                          preferences.printer[preferences.printernr]->height / xsane.zoom_y);
        }
       break;

      case XSANE_FAX:
        preview_set_maximum_output_size(xsane.preview, preferences.fax_width, preferences.fax_height);
       break;

      default:
        preview_set_maximum_output_size(xsane.preview, INF, INF);
    }
  }
  else
  {
    preview_set_maximum_output_size(xsane.preview, INF, INF);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_close_dialog_callback(GtkWidget *widget, gpointer data)
{
 GtkWidget *dialog_widget = data;

  DBG(DBG_proc, "xsane_close_dialog_callback\n");

  gtk_widget_destroy(dialog_widget);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int authorization_flag;

void xsane_authorization_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_authorization_button_callback\n");

  authorization_flag = (long) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

gint xsane_authorization_callback(SANE_String_Const resource,
                                  SANE_Char username[SANE_MAX_USERNAME_LEN],
                                  SANE_Char password[SANE_MAX_PASSWORD_LEN])
{
 GtkWidget *authorize_dialog, *vbox, *hbox, *button, *label;
 GtkWidget *username_widget, *password_widget;
 char buf[SANE_MAX_PASSWORD_LEN+SANE_MAX_USERNAME_LEN+128];
 char *input;
 char *resource_string;
 int len;
 int resource_len;
 int secure_password_transmission = 0;
 char password_filename[PATH_MAX];
 struct stat password_stat;
 FILE *password_file;
 unsigned char md5digest[16];
 int query_user = 1;

  DBG(DBG_proc, "xsane_authorization_callback\n");

  if (strstr(resource, "$MD5$") != NULL) /* secure password authorisation */
  {
    DBG(DBG_info, "xsane_authorization_callback: secure (MD5) password transmission requested\n");
    secure_password_transmission = 1;
    resource_len = strstr(resource, "$MD5$") - resource;
  }
  else
  {
    DBG(DBG_info, "xsane_authorization_callback: insecure password transmission requested\n");
    resource_len = strlen(resource);
  }
  resource_string = alloca(resource_len+1);
  snprintf(resource_string, resource_len+1, "%s", resource);

  xsane_back_gtk_make_path(sizeof(password_filename), password_filename, NULL, NULL, "pass", NULL, NULL, XSANE_PATH_LOCAL_SANE);

  /* if password transmission is secure and file ~/.sane/pass exists and it's permissions are x00 then
     try to read username and pasword for resource from that file */
  if ((stat(password_filename, &password_stat) == 0) && (secure_password_transmission))
  {
    if ((password_stat.st_mode & 63) != 0) /* 63 = 0x077 */
    {
      snprintf(buf, sizeof(buf), ERR_PASSWORD_FILE_INSECURE, password_filename);
      xsane_back_gtk_error(buf, TRUE);
    }
    else /* ok, password file has secure permissions, we can use it */
    {
      password_file = fopen(password_filename, "r");

      if (password_file)
      {
        DBG(DBG_info, "xsane authorization: opened %s as password file\n", password_filename);

        while (fgets(buf, sizeof(buf), password_file))
        {
         char *stored_username;
         char *stored_password;
         char *stored_resource;
         char *marker;

          marker = strrchr(buf, '\n');
          if (marker)
          {
            *marker = 0; /* remove \n at end of read line */
          }

          marker = strrchr(buf, '\r');
          if (marker)
          {
            *marker = 0; /* remove \r at end of read line (eg for windows file) */
          }

          marker = strchr(buf, ':');
          if (marker)
          {
            stored_username = buf;
            *marker = 0; /* set \0 to end of stored_username */
            stored_password = marker + 1;

            marker = strchr(stored_password, ':');
            if (marker)
            {
              *marker = 0; /* set \0 to end of stored_password */
              stored_resource = marker + 1;

              if (strcmp(stored_resource, resource_string) == 0) /* password file resource equals requested resource */
              {
                strcpy(username, stored_username);
                strcpy(password, stored_password);
                query_user = 0;
              }
            }
          }
        }

        fclose(password_file);
      }
      else
      {
        DBG(DBG_info, "xsane authorization: could not open existing password file %s\n", password_filename);
      }
    }
  }
  else
  {
    DBG(DBG_info, "xsane authorization: password file %s does not exist\n", password_filename);
  }

  if (query_user)
  {
    authorize_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_position(GTK_WINDOW(authorize_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_policy(GTK_WINDOW(authorize_dialog), FALSE, FALSE, FALSE);
    gtk_signal_connect(GTK_OBJECT(authorize_dialog), "delete_event",
                       GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1); /* -1 = cancel */
    snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_AUTHORIZE);
    gtk_window_set_title(GTK_WINDOW(authorize_dialog), buf);
    xsane_set_window_icon(authorize_dialog, 0);

    vbox = gtk_vbox_new(/* not homogeneous */ FALSE, 10); /* y-space between all box items */
    gtk_container_add(GTK_CONTAINER(authorize_dialog), vbox);
    gtk_widget_show(vbox);

    /* print resourece string */
    snprintf(buf, sizeof(buf), "\n\n%s %s", TEXT_AUTHORIZATION_REQ, resource_string);
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0); /* y-space around authorization text */
    gtk_widget_show(label);

    /* print securety of password transmission */
    if (secure_password_transmission)
    {
      snprintf(buf, sizeof(buf), "%s\n", TEXT_AUTHORIZATION_SECURE);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%s\n", TEXT_AUTHORIZATION_INSECURE);
    }
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0); /* y-space around authorization text */
    gtk_widget_show(label);

    /* ask for username */
    hbox = gtk_hbox_new(FALSE, 10); /* x-space between label and input filed */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); /* y-space around inner items */

    label = gtk_label_new(TEXT_USERNAME);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10); /* x-space around label */
    gtk_widget_show(label);

    username_widget = gtk_entry_new_with_max_length(SANE_MAX_USERNAME_LEN-1);
    gtk_widget_set_usize(username_widget, 250, 0);
    gtk_entry_set_text(GTK_ENTRY(username_widget), "");
    gtk_box_pack_end(GTK_BOX(hbox), username_widget, FALSE, FALSE, 10); /* x-space around input filed */
    gtk_widget_show(username_widget);
    gtk_widget_show(hbox);


    /* ask for password */
    hbox = gtk_hbox_new(FALSE, 10); /* x-space between label and input filed */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); /* y-space around inner items */

    label = gtk_label_new(TEXT_PASSWORD);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10); /* x-space around label */
    gtk_widget_show(label);

    password_widget = gtk_entry_new_with_max_length(SANE_MAX_PASSWORD_LEN-1);
    gtk_entry_set_visibility(GTK_ENTRY(password_widget), FALSE); /* make entered text invisible */
    gtk_widget_set_usize(password_widget, 250, 0);
    gtk_entry_set_text(GTK_ENTRY(password_widget), "");
    gtk_box_pack_end(GTK_BOX(hbox), password_widget, FALSE, FALSE, 10); /* x-space around input filed */
    gtk_widget_show(password_widget);
    gtk_widget_show(hbox);

    /* buttons */
    hbox = gtk_hbox_new(TRUE, 10); /* x-space between buttons */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);  /* y-space around buttons */

    button = gtk_button_new_with_label(BUTTON_OK);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10); /* x-space around OK-button */
    gtk_widget_grab_default(button);
    gtk_widget_show(button);

    button = gtk_button_new_with_label(BUTTON_CANCEL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10); /* x-space around cancel-button */
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
  }

  if (secure_password_transmission)
  {
    DBG(DBG_info, "xsane authorization: calculating md5digest of password\n");

    snprintf(buf, sizeof(buf), "%s%s", (strstr(resource, "$MD5$")) + 5, password); /* random string from backend + password */
    md5_buffer(buf, strlen(buf), md5digest); /* calculate md5digest */
#if 0 /* makes problems with WIN32 */
    memset(password, 0, SANE_MAX_PASSWORD_LEN); /* clear password */
#endif

    sprintf(password, "$MD5$%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            md5digest[0],  md5digest[1],  md5digest[2],  md5digest[3],
            md5digest[4],  md5digest[5],  md5digest[6],  md5digest[7],
            md5digest[8],  md5digest[9],  md5digest[10], md5digest[11],
            md5digest[12], md5digest[13], md5digest[14], md5digest[15]);
  }         

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_cancel(GtkWidget *widget, gpointer data)
{
 GtkSignalFunc callback = data;

  DBG(DBG_proc, "xsane_progress_cancel\n");

  (callback)();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_new(char *bar_text, char *info, GtkSignalFunc callback)
{
  DBG(DBG_proc, "xsane_progress_new\n");

  gtk_label_set(GTK_LABEL(xsane.info_label), info);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), bar_text); 
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.progress_bar), 0.0);
  gtk_widget_set_sensitive(GTK_WIDGET(xsane.cancel_button), TRUE);
  gtk_signal_connect(GTK_OBJECT(xsane.cancel_button), "clicked", (GtkSignalFunc) xsane_progress_cancel, callback);
  xsane.cancel_callback = callback;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_clear()
{
  DBG(DBG_proc, "xsane_progress_clear\n");

  /* do not call xsane_update_param() here because it overrites the good scanning parameters with bad guessed ones */
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), ""); 
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.progress_bar), 0.0);
  gtk_widget_set_sensitive(GTK_WIDGET(xsane.cancel_button), FALSE);

  if (xsane.cancel_callback)
  {
    gtk_signal_disconnect_by_func(GTK_OBJECT(xsane.cancel_button), (GtkSignalFunc) xsane_progress_cancel, xsane.cancel_callback);
    xsane.cancel_callback = 0;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_update(gfloat newval)
{
  DBG(DBG_proc, "xsane_progress_update\n");

  if (newval < 0.0)
  {
    newval = 0.0;
  }

  if (newval > 1.0)
  {
    newval = 1.0;
  }

  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.progress_bar), newval);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

/* add the scanner vendor´s logo if available */
/* when the logo is not available use the xsane logo */
GtkWidget *xsane_vendor_pixmap_new(GdkWindow *window, GtkWidget *parent)
{
 char filename[PATH_MAX];
 GtkWidget *hbox, *vbox;
 GtkWidget *pixmapwidget = NULL;
 GdkBitmap *mask;
 GdkPixmap *pixmap = NULL;

  if (xsane.devlist[xsane.selected_dev]->vendor)
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, xsane.devlist[xsane.selected_dev]->vendor, "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, xsane.bg_trans, filename);
  }

  if (!pixmap) /* vendor logo not available, use backend logo */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, xsane.backend, "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, xsane.bg_trans, filename);
  }

  if (!pixmap) /* backend logo not available, use xsane logo */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, "sane-xsane", "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, xsane.bg_trans, filename);
  }

  if (pixmap) /* ok, we have a pixmap, so let´s show it */
  {
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);   

    hbox = gtk_hbox_new(TRUE /* homogeneous */ , 2);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
    gtk_widget_show(hbox);

    pixmapwidget = gtk_pixmap_new(pixmap, mask); /* now add the pixmap */
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE /* expand */, FALSE /* fill */, 2);
    gdk_pixmap_unref(pixmap);
    gtk_widget_show(pixmapwidget);
  }

 return pixmapwidget;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_toggle_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                         int *state, void *xsane_toggle_button_callback)
{
 GtkWidget *button;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_toggle_button_new_with_pixmap\n");

  button = gtk_toggle_button_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, desc);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_container_add(GTK_CONTAINER(button), pixmapwidget);
  gtk_widget_show(pixmapwidget);
  gdk_pixmap_unref(pixmap);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *state);
  gtk_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_toggle_button_callback, (GtkObject *) state);
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

 return button;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                  void *xsane_button_callback, gpointer data)
{
 GtkWidget *button;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_button_new_with_pixmap\n");

  button = gtk_button_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, desc);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
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

  DBG(DBG_proc, "xsane_option_menu_lookup\n");

  for (i = 0; strcmp(menu_items[i].label, string) != 0; ++i);
  return i;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_option_menu_callback(GtkWidget *widget, gpointer data)
{
 GSGMenuItem *menu_item = data;
 GSGDialogElement *elem = menu_item->elem;
 const SANE_Option_Descriptor *opt;
 int opt_num;
 double dval;
 SANE_Word val;
 void *valp = &val;

  DBG(DBG_proc, "xsane_option_menu_callback\n");

  opt_num = elem - xsane.element;
  opt = xsane_get_option_descriptor(xsane.dev, opt_num);
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
      fprintf(stderr, "xsane_option_menu_callback: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
      break;
  }
  xsane_back_gtk_set_option(opt_num, valp, SANE_ACTION_SET_VALUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc,
                           void *option_menu_callback, SANE_Int settable, const gchar *widget_name)
{
 GtkWidget *option_menu, *menu, *item;
 GSGMenuItem *menu_items;
 GSGDialogElement *elem;
 int i, num_items;

  DBG(DBG_proc, "xsane_option_menu_new\n");

  elem = xsane.element + option_number;

  for (num_items = 0; str_list[num_items]; ++num_items);
  menu_items = malloc(num_items * sizeof(menu_items[0]));

  menu = gtk_menu_new();
  if (widget_name)
  {
    gtk_widget_set_name(menu, widget_name);
  }

  for (i = 0; i < num_items; ++i)
  {
    item = gtk_menu_item_new_with_label(_BGT(str_list[i]));
    gtk_container_add(GTK_CONTAINER(menu), item);

    if (option_menu_callback)
    {
      gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) option_menu_callback, menu_items + i);
    }
    else
    {
      gtk_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_option_menu_callback, menu_items + i);
    }

    gtk_widget_show(item);

    menu_items[i].label = str_list[i];
    menu_items[i].elem  = elem;
    menu_items[i].index = i;
  }

  option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, option_menu, desc);
  gtk_box_pack_end(GTK_BOX(parent), option_menu, FALSE, FALSE, 2);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_option_menu_lookup(menu_items, val));

  gtk_widget_show(option_menu);

  gtk_widget_set_sensitive(option_menu, settable);

  elem->widget = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkObject **data, int option,
                                       void *option_menu_callback, SANE_Int settable, const gchar *widget_name)
{
 GtkWidget *hbox;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_option_menu_new_with_pixmap\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);

  xsane_option_menu_new(hbox, str_list, val, option, desc, option_menu_callback, settable, widget_name);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scale_new(GtkBox *parent, char *labeltext, const char *desc,
                     float min, float max, float quant, float page_step, float page_size,
                     int digits, double *val, GtkObject **data, void *xsane_scale_callback, SANE_Int settable)
{
 GtkWidget *hbox;
 GtkWidget *label;
 GtkWidget *scale;

  DBG(DBG_proc, "xsane_scale_new\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  label = gtk_label_new(labeltext);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  *data = gtk_adjustment_new(*val, min, max, quant, page_step, page_size);
  scale = gtk_hscale_new(GTK_ADJUSTMENT(*data));
  xsane_back_gtk_set_tooltip(xsane.tooltips, scale, desc);
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pixels */
  gtk_range_set_update_policy(GTK_RANGE(scale), preferences.gtk_update_policy);
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

  gtk_widget_set_sensitive(scale, settable);  

}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_scale_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                 float min, float max, float quant, float page_step, float page_size,
                                 int digits, double *val, GtkObject **data, int option, void *xsane_scale_callback, SANE_Int settable)
{
 GtkWidget *hbox;
 GtkWidget *scale;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_scale_new_with_pixmap\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 0);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);

  *data = gtk_adjustment_new(*val, min, max, quant, page_step, page_size);
  scale = gtk_hscale_new(GTK_ADJUSTMENT(*data));
  xsane_back_gtk_set_tooltip(xsane.tooltips, scale, desc);
  gtk_widget_set_usize(scale, 201, 0); /* minimum scale with = 201 pixels */
  gtk_range_set_update_policy(GTK_RANGE(scale), preferences.gtk_update_policy);
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

  gtk_widget_set_sensitive(scale, settable);  

  gdk_pixmap_unref(pixmap);

  if (option)
  {
   GSGDialogElement *elem;

    elem=xsane.element + option;
    elem->data   = *data;
    elem->widget = scale;
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_separator_new(GtkWidget *xsane_parent, int dist)
{
 GtkWidget *xsane_separator;

  DBG(DBG_proc, "xsane_separator_new\n");

  xsane_separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(xsane_parent), xsane_separator, FALSE, FALSE, dist);
  gtk_widget_show(xsane_separator);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn)
{
 GtkWidget *hbox, *label;

  DBG(DBG_proc, "xsane_info_table_text_new\n");

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

  DBG(DBG_proc, "xsane_info_text_new\n");

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

void xsane_refresh_dialog(void)
{
  DBG(DBG_proc, "xsane_refresh_dialog\n");

  xsane_back_gtk_refresh_dialog();
}                    

/* ---------------------------------------------------------------------------------------------------------------------- */

/* Update the info line with the latest size information and update histogram.  */
void xsane_update_param(void *arg)
{
 gchar buf[200];
 const char *unit;

  DBG(DBG_proc, "xsane_update_param\n");

  if (!xsane.info_label)
  {
    return;
  }

  if (xsane.block_update_param) /* if we change more than one value, we only want to update all once */
  {
    return;
  }

  if (xsane.preview)
  {
    preview_update_surface(xsane.preview, 0);
  }

  if (sane_get_parameters(xsane.dev, &xsane.param) == SANE_STATUS_GOOD)
  {
   float size = xsane.param.bytes_per_line * xsane.param.lines;

    unit = "B";

    if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
    {
      size *= 3.0;
    }

    if (size >= 1024.0 * 1024.0)
    {
      size /= 1024.0 * 1024.0;
      unit = "MB";
    }
    else if (size >= 1024.0)
    {
      size /= 1024.0;
      unit = "KB";
    }
    snprintf(buf, sizeof(buf), "%d x %d (%1.1f %s)", xsane.param.pixels_per_line, xsane.param.lines, size, unit);

    if (xsane.param.format == SANE_FRAME_GRAY)
    {
      xsane.xsane_color = 0;
    }
#ifdef SUPPORT_RGBA
    else if (xsane.param.format == SANE_FRAME_RGBA)
    {
      xsane.xsane_color = 4;
    }
#endif
    else /* RGB */
    {
      xsane.xsane_color = 3;
    }
  }
  else 
  {
    snprintf(buf, sizeof(buf), TEXT_INVALID_PARAMS);
  }

  gtk_label_set(GTK_LABEL(xsane.info_label), buf);


  if (xsane.preview->surface_unit == SANE_UNIT_MM)
  {
   double dx, dy;

    unit = xsane_back_gtk_unit_string(xsane.preview->surface_unit);

    dx = fabs(xsane.preview->selection.coordinate[2] - xsane.preview->selection.coordinate[0]) / preferences.length_unit;
    dy = fabs(xsane.preview->selection.coordinate[3] - xsane.preview->selection.coordinate[1]) / preferences.length_unit;

    snprintf(buf, sizeof(buf), "%1.2f %s x %1.2f %s", dx, unit, dy, unit);
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), buf);
  }

  xsane_update_histogram();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_define_output_filename(void)
{
 char buffer[256];

  DBG(DBG_proc, "xsane_define_output_filename\n");

  if (xsane.output_filename)
  {
    free(xsane.output_filename);
    xsane.output_filename = 0;
  }

  if (!xsane.force_filename)
  {
    if (xsane.filetype)
    {
      snprintf(buffer, sizeof(buffer), "%s%s", preferences.filename, xsane.filetype);
      xsane.output_filename = strdup(buffer);
    }
    else
    {
      xsane.output_filename = strdup(preferences.filename);
    }
  }
  else
  {
      xsane.output_filename = strdup(xsane.external_filename);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_identify_output_format(char **ext)
{
 char *extension;

  DBG(DBG_proc, "xsane_identify_output_format\n");

  extension = strrchr(xsane.output_filename, '.');
  if (extension)
  {
    extension++; /* skip "." */
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
#ifdef SUPPORT_RGBA
      else if (!strcasecmp(extension, "rgba"))
      {
        xsane.xsane_output_format = XSANE_RGBA;
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
#ifdef HAVE_LIBTIFF
      else if ( (!strcasecmp(extension, "tif")) || (!strcasecmp(extension, "tiff")) )
      {
        xsane.xsane_output_format = XSANE_TIFF;
      }
#endif
#ifdef SUPPORT_RGBA
      else if (!strcasecmp(extension, "rgba"))
      {
        xsane.xsane_output_format = XSANE_RGBA;
      }
#endif
    }
  }

  if (ext)
  {
    if (extension)
    {
      *ext = strdup(extension);
    }
    else
    {
      *ext = 0;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_change_working_directory(void)
{
 char filename[PATH_MAX];
 char windowname[256];

  DBG(DBG_proc, "xsane_change_working_directory\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_CHANGE_WORKING_DIR, xsane.device_text);
  if (getcwd(filename, sizeof(filename)))
  {
    xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, FALSE);
    if (chdir(filename))
    {
     char buf[256];

      snprintf(buf, sizeof(buf), "%s %s (%s).", ERR_CHANGE_WORKING_DIR, filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
      xsane_set_sensitivity(TRUE);
      return;
    }
  }

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */
