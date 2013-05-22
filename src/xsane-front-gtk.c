/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-front-gtk.c

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

int xsane_parse_options(char *options, char *argv[]);
void xsane_get_bounds(const SANE_Option_Descriptor *opt, double *minp, double *maxp);
double xsane_find_best_resolution(int well_known_option, double dpi);
double xsane_set_resolution(int well_known_option, double resolution);
void xsane_set_all_resolutions(void);
void xsane_define_maximum_output_size();
void xsane_close_dialog_callback(GtkWidget *widget, gpointer data);
void xsane_authorization_button_callback(GtkWidget *widget, gpointer data);
gint xsane_authorization_callback(SANE_String_Const resource,
                                  SANE_Char username[SANE_MAX_USERNAME_LEN],
                                  SANE_Char password[SANE_MAX_PASSWORD_LEN]);
void xsane_progress_cancel(GtkWidget *widget, gpointer data);
void xsane_progress_new(char *bar_text, char *info, GtkSignalFunc callback, int *cancel_data_pointer);
void xsane_progress_update(gfloat newval);
void xsane_progress_clear();
void xsane_progress_bar_set_fraction(GtkProgressBar *progress_bar, gdouble fraction);
GtkWidget *xsane_vendor_pixmap_new(GdkWindow *window, GtkWidget *parent);
GtkWidget *xsane_toggle_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                         int *state, void *xsane_toggle_button_callback);
GtkWidget *xsane_button_new_with_pixmap(GdkWindow *window, GtkWidget *parent, const char *xpm_d[], const char *desc,
                                        void *xsane_button_callback, gpointer data);
void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc,
                           void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
void xsane_option_menu_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkWidget **data, int option,
                                       void *option_menu_callback, SANE_Int settable, const gchar *widget_name);
void xsane_range_new(GtkBox *parent, char *labeltext, const char *desc,
                     float min, float max, float quant, float page_step,
                     int digits, double *val, GtkWidget **data, void *xsane_range_callback, SANE_Int settable);
void xsane_range_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                 float min, float max, float quant, float page_step, int digits,
                                 double *val, GtkWidget **data, int option, void *xsane_range_callback, SANE_Int settable);
static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data);
void xsane_set_filename(gchar *filename);
void xsane_separator_new(GtkWidget *xsane_parent, int dist);
void xsane_vseparator_new(GtkWidget *xsane_parent, int dist);
GtkWidget *xsane_info_table_text_new(GtkWidget *table, gchar *text, int row, int colomn);
GtkWidget *xsane_info_text_new(GtkWidget *parent, gchar *text);
void xsane_refresh_dialog(void);
void xsane_update_param(void *arg);
void xsane_define_output_filename(void);
int xsane_identify_output_format(char *filename, char *filetype, char **ext);

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_parse_options(char *options, char *argv[])
{
 int optpos = 0;
 int bufpos = 0;
 int arg    = 0;
 char buf[TEXTBUFSIZE];
 
  DBG(DBG_proc, "xsane_parse_options\n");
 
  while (options[optpos] != 0)
  {
    switch(options[optpos])
    {
      case ' ':
        buf[bufpos] = 0;
        argv[arg++] = strdup(buf);
        bufpos = 0;
        optpos++;
       break;
 
      case '\"':
        optpos++; /* skip " */
        while ((options[optpos] != 0) && (options[optpos] != '\"'))
        {
          buf[bufpos++] = options[optpos++];
        }
        optpos++; /* skip " */
       break;
 
      default:
        buf[bufpos++] = options[optpos++];
       break;
    }
  }
  buf[bufpos] = 0;
  argv[arg++] = strdup(buf);

 return arg;
}

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
      min = SANE_UNFIX(min);
    }
    if (max > -INF && max < INF)
    {
      max = SANE_UNFIX(max);
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
        DBG(DBG_error, "find_best_resolution: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
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
    DBG(DBG_error, "find_best_resolution: %s %d\n", ERR_UNKNOWN_CONSTRAINT_TYPE, opt->constraint_type);
    return -1; /* error */
  }

  return bestdpi;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

double xsane_set_resolution(int well_known_option, double resolution)
{
 const SANE_Option_Descriptor *opt;
 double bestdpi;
 SANE_Word dpi;

  DBG(DBG_proc, "xsane_set_resolution\n");

  opt = xsane_get_option_descriptor(xsane.dev, well_known_option);

  if (!opt)
  {
    return -1.0; /* option does not exits */
  }

  if (!SANE_OPTION_IS_ACTIVE(opt->cap))
  {
    return -1.0; /* option is not active */
  }

  bestdpi = xsane_find_best_resolution(well_known_option, resolution);

  if (bestdpi < 0)
  {
     DBG(DBG_error, "set_resolution: %s\n", ERR_FAILED_SET_RESOLUTION);
    return -1.0;
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
     DBG(DBG_error, "set_resolution: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
    return -1.0; /* error */
  }

  /* it makes problems to call xsane_back_gtk_set_option. This would allow a */
  /* panel_rebuild that can mess up a lot at this place*/
  xsane_control_option(xsane.dev, well_known_option, SANE_ACTION_SET_VALUE, &dpi, 0);

 return (bestdpi); /* everything is ok */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_all_resolutions(void)
{
 int printer_resolution;
 double new_resolution;

  DBG(DBG_proc, "xsane_set_all_resolutions\n");

  new_resolution = xsane_set_resolution(xsane.well_known.dpi_y, xsane.resolution_y); /* set y resolution if possible */
  if (new_resolution < 0) /* set y resolution not possible */
  {
    new_resolution = xsane_set_resolution(xsane.well_known.dpi, xsane.resolution); /* set common resolution if necessary */
    if (new_resolution > 0)
    {
      xsane.resolution   = new_resolution;
      xsane.resolution_x = new_resolution;
      xsane.resolution_y = new_resolution;
    }
    else
    {
      xsane.resolution   = 72.0;
      xsane.resolution_x = 72.0;
      xsane.resolution_y = 72.0;
    }
  }
  else /* we were able to set y resolution */
  {
    xsane.resolution_y = new_resolution;
    new_resolution = xsane_set_resolution(xsane.well_known.dpi_x, xsane.resolution_x); /* set x resolution if possible */
    xsane.resolution_x = new_resolution;
  }

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
      case XSANE_SAVE:

        xsane_define_output_filename();
        xsane.xsane_output_format = xsane_identify_output_format(xsane.output_filename, preferences.filetype, 0);

        preview_set_maximum_output_size(xsane.preview, INF, INF, 0);
       break;

      case XSANE_VIEWER:
        preview_set_maximum_output_size(xsane.preview, INF, INF, 0);
       break;

      case XSANE_COPY:
        if (preferences.paper_orientation >= 8) /* rotate: landscape */
        {
          preview_set_maximum_output_size(xsane.preview,
                                          preferences.printer[preferences.printernr]->height / xsane.zoom,
                                          preferences.printer[preferences.printernr]->width  / xsane.zoom,
                                          preferences.paper_orientation);
        }
        else /* do not rotate: portrait */
        {
          preview_set_maximum_output_size(xsane.preview,
                                          preferences.printer[preferences.printernr]->width  / xsane.zoom,
                                          preferences.printer[preferences.printernr]->height / xsane.zoom,
                                          preferences.paper_orientation);
        }
       break;

      case XSANE_FAX:
        preview_set_maximum_output_size(xsane.preview, preferences.fax_width, preferences.fax_height, 0);
       break;

      case XSANE_EMAIL:
        preview_set_maximum_output_size(xsane.preview, INF, INF, 0);
       break;

      default:
        preview_set_maximum_output_size(xsane.preview, INF, INF, 0);
    }
  }
  else
  {
    preview_set_maximum_output_size(xsane.preview, INF, INF, 0);
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
 const gchar *input;
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
        /* file format: "username:password:resource" */
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
    authorize_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(authorize_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(authorize_dialog), FALSE);
    g_signal_connect(GTK_OBJECT(authorize_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1); /* -1 = cancel */
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
    gtk_widget_set_size_request(username_widget, 250, -1);
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
    gtk_widget_set_size_request(password_widget, 250, -1);
    gtk_entry_set_text(GTK_ENTRY(password_widget), "");
    gtk_box_pack_end(GTK_BOX(hbox), password_widget, FALSE, FALSE, 10); /* x-space around input filed */
    gtk_widget_show(password_widget);
    gtk_widget_show(hbox);

    /* buttons */
    hbox = gtk_hbox_new(TRUE, 10); /* x-space between buttons */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);  /* y-space around buttons */

#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
    button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10); /* x-space around cancel-button */
    gtk_widget_show(button);

#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
    button = gtk_button_new_with_label(BUTTON_OK);
#endif
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    g_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(xsane_authorization_button_callback), (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10); /* x-space around OK-button */
    gtk_widget_grab_default(button);
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

void xsane_progress_bar_set_fraction(GtkProgressBar *progress_bar, gdouble fraction)
{
   if (fraction < 0.0)
   { 
     fraction = 0.0; 
   }

   if (fraction > 1.0)
   { 
     fraction = 1.0; 
   }

#ifdef HAVE_GTK2
  if ((fraction - gtk_progress_bar_get_fraction(progress_bar) > XSANE_PROGRESS_BAR_MIN_DELTA_PERCENT) || (fraction == 0.0) || (fraction > 0.99))
  {
    gtk_progress_bar_set_fraction(progress_bar, fraction);
#if 0
    /* this produces jumping scrollbars when we use it instead of the gtk_main_iteration loop */
    if (GTK_WIDGET_DRAWABLE(progress_bar))
    {
      gdk_window_process_updates(progress_bar->progress.widget.window, TRUE);
    }
#endif
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
  }
#else
  if ((fraction - gtk_progress_get_current_percentage(&progress_bar->progress) > XSANE_PROGRESS_BAR_MIN_DELTA_PERCENT) || (fraction == 0.0) || (fraction > 0.99))
  {
    gtk_progress_bar_update(progress_bar, fraction);

    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
  }
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_cancel(GtkWidget *widget, gpointer data)
{
 void *cancel_data_pointer;
 GtkFunction callback = (GtkFunction) data;

  DBG(DBG_proc, "xsane_progress_cancel\n");

  cancel_data_pointer = gtk_object_get_data(GTK_OBJECT(widget), "progress-cancel-data-pointer");

  (callback)(cancel_data_pointer);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_progress_new(char *bar_text, char *info, GtkSignalFunc callback, int *cancel_data_pointer)
{
  DBG(DBG_proc, "xsane_progress_new\n");

  gtk_label_set(GTK_LABEL(xsane.info_label), info);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), bar_text); 
  gtk_progress_bar_update(GTK_PROGRESS_BAR(xsane.progress_bar), 0.0);
  gtk_widget_set_sensitive(GTK_WIDGET(xsane.cancel_button), TRUE);
  gtk_object_set_data(GTK_OBJECT(xsane.cancel_button), "progress-cancel-data-pointer", cancel_data_pointer);
  g_signal_connect(GTK_OBJECT(xsane.cancel_button), "clicked", (GtkSignalFunc) xsane_progress_cancel, callback);
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
    g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.cancel_button), (GtkSignalFunc) xsane_progress_cancel, xsane.cancel_callback);
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
 GdkBitmap *mask = NULL;
 GdkPixmap *pixmap = NULL;
 GdkColor *bg_trans = NULL;

  if (xsane.devlist[xsane.selected_dev]->vendor)
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, xsane.devlist[xsane.selected_dev]->vendor, "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, bg_trans, filename);
  }

  if (!pixmap) /* vendor logo not available, use backend logo */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, xsane.backend, "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, bg_trans, filename);
  }

  if (!pixmap) /* backend logo not available, use xsane logo */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", NULL, NULL, "sane-xsane", "-logo.xpm", XSANE_PATH_SYSTEM);
    pixmap = gdk_pixmap_create_from_xpm(window, &mask, bg_trans, filename);
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

    pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask); /* now add the pixmap */
    gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE /* expand */, FALSE /* fill */, 2);
    gdk_drawable_unref(pixmap);
    gdk_drawable_unref(mask);
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
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_container_add(GTK_CONTAINER(button), pixmapwidget);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *state);
  g_signal_connect(GTK_OBJECT(button), "toggled", (GtkSignalFunc) xsane_toggle_button_callback, (GtkObject *) state);
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
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_container_add(GTK_CONTAINER(button), pixmapwidget);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  if (xsane_button_callback)
  {
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_button_callback, data);
  }
  gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  return(button);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_option_menu_lookup(MenuItem menu_items[], const char *string)
{
 int i;

  DBG(DBG_proc, "xsane_option_menu_lookup\n");

  for (i = 0; (menu_items[i].label) && strcmp(menu_items[i].label, string); ++i)
  {
  }

 return i;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_option_menu_callback(GtkWidget *widget, gpointer data)
{
 MenuItem *menu_item = data;
 DialogElement *elem = menu_item->elem;
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
      DBG(DBG_error, "xsane_option_menu_callback: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
     break;
  }
  xsane_back_gtk_set_option(opt_num, valp, SANE_ACTION_SET_VALUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_new(GtkWidget *parent, char *str_list[], const char *val, int option_number, const char *desc,
                           void *option_menu_callback, SANE_Int settable, const gchar *widget_name)
{
 GtkWidget *option_menu, *menu, *item;
 MenuItem *menu_items;
 DialogElement *elem;
 int i, num_items;

  DBG(DBG_proc, "xsane_option_menu_new\n");

  elem = xsane.element + option_number;

  for (num_items = 0; str_list[num_items]; ++num_items)
  {
  }

  menu_items = malloc((num_items + 1) * sizeof(menu_items[0]));

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
      g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) option_menu_callback, menu_items + i);
    }
    else
    {
      g_signal_connect(GTK_OBJECT(item), "activate", (GtkSignalFunc) xsane_option_menu_callback, menu_items + i);
    }

    gtk_widget_show(item);

    menu_items[i].label = str_list[i];
    menu_items[i].elem  = elem;
    menu_items[i].index = i;
  }

  /* add empty element as end of list marker */
  menu_items[i].label = NULL;
  menu_items[i].elem  = NULL;
  menu_items[i].index = 0;

  option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, option_menu, desc);
  gtk_box_pack_end(GTK_BOX(parent), option_menu, TRUE, TRUE, 5);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), xsane_option_menu_lookup(menu_items, val));

  gtk_widget_show(option_menu);

  gtk_widget_set_sensitive(option_menu, settable);

  elem->widget  = option_menu;
  elem->menu_size = num_items;
  elem->menu = menu_items;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_option_menu_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                       char *str_list[], const char *val,
                                       GtkWidget **data, int option,
                                       void *option_menu_callback, SANE_Int settable, const gchar *widget_name)
{
 GtkWidget *hbox;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_option_menu_new_with_pixmap\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);

  xsane_option_menu_new(hbox, str_list, val, option, desc, option_menu_callback, settable, widget_name);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_range_display_value_right_callback(GtkAdjustment *adjust, gpointer data)
{
 gchar buf[TEXTBUFSIZE];
 int digits = (int) data;
 GtkLabel *label;

  snprintf(buf, sizeof(buf), "%1.*f", digits, adjust->value);
  label = (GtkLabel *) gtk_object_get_data(GTK_OBJECT(adjust), "value-label"); 
  gtk_label_set_text(label, buf);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_range_new(GtkBox *parent, char *labeltext, const char *desc,
                     float min, float max, float quant, float page_step,
                     int digits, double *val, GtkWidget **data, void *xsane_range_callback, SANE_Int settable)
{
 GtkWidget *hbox;
 GtkWidget *label;
 GtkWidget *slider = NULL;
 GtkWidget *spinbutton;
 GtkWidget *value_label;

  DBG(DBG_proc, "xsane_range_new\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 2);

  label = gtk_label_new(labeltext);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  *data = (GtkWidget *) gtk_adjustment_new(*val, min, max, quant, page_step, 0);

  /* value label */
  if (preferences.show_range_mode & 8)
  {
    value_label = gtk_label_new("");
    gtk_widget_set_size_request(value_label, 35, -1);
    gtk_box_pack_end(GTK_BOX(hbox), value_label, FALSE, FALSE, 1);
    
    g_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_range_display_value_right_callback, (void *) digits);
    gtk_object_set_data(GTK_OBJECT(*data), "value-label", value_label); 
    g_signal_emit_by_name(GTK_OBJECT(*data), "value_changed"); /* update value */
    gtk_widget_show(value_label);
    gtk_widget_set_sensitive(value_label, settable);  
  }

  /* spinbutton */
  if (preferences.show_range_mode & 4)
  {
    spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(*data), 0, digits);
    if (preferences.show_range_mode & 3) /* slider also visible */
    {
      gtk_widget_set_size_request(spinbutton, 60, -1);
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
  }

  /* slider */
  if (preferences.show_range_mode & 3)
  {
    if (preferences.show_range_mode & 1) /* bit 0 (val 1) : scale */
    {
      slider = gtk_hscale_new(GTK_ADJUSTMENT(*data));
      gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
      gtk_scale_set_digits(GTK_SCALE(slider), digits);
    }
    else /* bit 1 (val 2) : scrollbar */
    {
      slider = gtk_hscrollbar_new(GTK_ADJUSTMENT(*data));
    }
    xsane_back_gtk_set_tooltip(xsane.tooltips, slider, desc);
    gtk_widget_set_size_request(slider, 180, -1);
    /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
    gtk_range_set_update_policy(GTK_RANGE(slider), preferences.gtk_update_policy);
    gtk_box_pack_end(GTK_BOX(hbox), slider, FALSE, FALSE, 5); /* make slider not sizeable */
    gtk_widget_show(slider);
    gtk_widget_set_sensitive(slider, settable);  
  }

  if (xsane_range_callback)
  {
    g_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_range_callback, val);
  }

  gtk_widget_show(label);
  gtk_widget_show(hbox);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_range_new_with_pixmap(GdkWindow *window, GtkBox *parent, const char *xpm_d[], const char *desc,
                                 float min, float max, float quant, float page_step,
                                 int digits, double *val, GtkWidget **data, int option, void *xsane_range_callback, SANE_Int settable)
{
 GtkWidget *hbox;
 GtkWidget *slider = NULL;
 GtkWidget *spinbutton;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 GtkWidget *value_label;

  DBG(DBG_proc, "xsane_slider_new_with_pixmap\n");

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(parent, hbox, FALSE, FALSE, 2);

  pixmap = gdk_pixmap_create_from_xpm_d(window, &mask, xsane.bg_trans, (gchar **) xpm_d);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  *data = (GtkWidget *) gtk_adjustment_new(*val, min, max, quant, page_step, 0);

  /* value label */
  if (preferences.show_range_mode & 8)
  {
    value_label = gtk_label_new("");
    gtk_widget_set_size_request(value_label, 35, -1);
    gtk_box_pack_end(GTK_BOX(hbox), value_label, FALSE, FALSE, 1);
    
    g_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_range_display_value_right_callback, (void *) digits);
    gtk_object_set_data(GTK_OBJECT(*data), "value-label", value_label); 
    g_signal_emit_by_name(GTK_OBJECT(*data), "value_changed"); /* update value */
    gtk_widget_show(value_label);
    gtk_widget_set_sensitive(value_label, settable);  
  }

  /* spinbutton */
  if (preferences.show_range_mode & 4)
  {
    spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(*data), 0, digits);
    gtk_widget_set_size_request(spinbutton, 60, -1);
    xsane_back_gtk_set_tooltip(xsane.tooltips, spinbutton, desc);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton), FALSE);
    if (preferences.show_range_mode & 3) /* slider also visible */
    {
      gtk_box_pack_end(GTK_BOX(hbox), spinbutton, FALSE, FALSE, 5); /* make spinbutton not sizeable */
    }
    else /* slider not visible */
    {
      gtk_box_pack_end(GTK_BOX(hbox), spinbutton, TRUE, TRUE, 5); /* make spinbutton sizeable */
    }
    gtk_widget_show(spinbutton);
    gtk_widget_set_sensitive(spinbutton, settable);  
  }

  /* slider */
  if (preferences.show_range_mode & 3)
  {
    if (preferences.show_range_mode & 1) /* bit 0 (val 1) : scale */
    {
      slider = gtk_hscale_new(GTK_ADJUSTMENT(*data));
      gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
      gtk_scale_set_digits(GTK_SCALE(slider), digits);
    }
    else /* bit 1 (val 2) : scrollbar */
    {
      slider = gtk_hscrollbar_new(GTK_ADJUSTMENT(*data));
    }
    xsane_back_gtk_set_tooltip(xsane.tooltips, slider, desc);
    gtk_widget_set_size_request(slider, 170, -1);
    /* GTK_UPDATE_CONTINUOUS, GTK_UPDATE_DISCONTINUOUS, GTK_UPDATE_DELAYED */
    gtk_range_set_update_policy(GTK_RANGE(slider), preferences.gtk_update_policy);
    gtk_box_pack_end(GTK_BOX(hbox), slider, TRUE, TRUE, 5); /* make slider sizeable */
    gtk_widget_show(slider);
    gtk_widget_set_sensitive(slider, settable);  
  }

  if (xsane_range_callback)
  {
    g_signal_connect(*data, "value_changed", (GtkSignalFunc) xsane_range_callback, val);
  }

  gtk_widget_show(hbox);

  if (option)
  {
   DialogElement *elem;

    elem=xsane.element + option;
    elem->data    = (GtkObject *) *data;
    elem->widget  = slider;
  }
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_browse_filename_callback(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];
 int show_extra_widgets;

  DBG(DBG_proc, "xsane_browse_filename_callback\n");

  xsane_set_sensitivity(FALSE);

  if (preferences.filename) /* make sure a correct filename is defined */
  {
    strncpy(filename, preferences.filename, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
  }
  else /* no filename given, take standard filename */
  {
    strcpy(filename, OUT_FILENAME);
  }

  show_extra_widgets = XSANE_GET_FILENAME_SHOW_FILETYPE;
  if (xsane.enable_color_management)
  {
    show_extra_widgets |= XSANE_GET_FILENAME_SHOW_CMS_FUNCTION;
  }


  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_OUTPUT_FILENAME, xsane.device_text);

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, &preferences.filetype, &preferences.cms_function, XSANE_FILE_CHOOSER_ACTION_SELECT_SAVE, show_extra_widgets, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_IMAGES, XSANE_FILE_FILTER_IMAGES);
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */    

  if (preferences.filename)
  {
    free((void *) preferences.filename);
  }

  preferences.filename = strdup(filename);

  xsane_set_sensitivity(TRUE);

  xsane_back_gtk_filetype_menu_set_history(xsane.filetype_option_menu, preferences.filetype);

#ifdef HAVE_LIBLCMS
  if (xsane.enable_color_management)
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(xsane.cms_function_option_menu), preferences.cms_function);
  }
#endif

  /* correct length of filename counter if it is shorter than minimum length */
  xsane_update_counter_in_filename(&preferences.filename, FALSE, 0, preferences.filename_counter_len);

  xsane_set_filename(preferences.filename);

  xsane_define_maximum_output_size(); /* is necessary in postscript mode */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_set_filename(gchar *filename)
{
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.outputfilename_entry), (GtkSignalFunc) xsane_outputfilename_changed_callback, NULL);
  gtk_entry_set_text(GTK_ENTRY(xsane.outputfilename_entry), (char *) filename); /* update filename in entry */
  gtk_entry_set_position(GTK_ENTRY(xsane.outputfilename_entry), strlen(filename)); /* set cursor to right position of filename */

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.outputfilename_entry), (GtkSignalFunc) xsane_outputfilename_changed_callback, NULL);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_filename_counter_step_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_filename_counter_step_callback\n");
 
  preferences.filename_counter_step = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_filetype_callback(GtkWidget *widget, gpointer data)
{
 char *new_filetype = (char *) data;
 char buffer[PATH_MAX];
 char *filename;

  DBG(DBG_proc, "xsane_filetype_callback\n");

  filename = preferences.filename;

  if ((new_filetype) && (*new_filetype)) /* filetype exists and is not empty (by ext) */
  {
   char *extension;

    extension = strrchr(preferences.filename, '.');

    if ((extension) && (extension != preferences.filename))
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
    preferences.filename = strdup(buffer);
  }

  if (preferences.filetype)
  {
    free(preferences.filetype);
    preferences.filetype = NULL;
  }

  if (new_filetype)
  {
    preferences.filetype = strdup(new_filetype);
  }

  /* correct length of filename counter if it is shorter than minimum length */
  xsane_update_counter_in_filename(&preferences.filename, FALSE, 0, preferences.filename_counter_len);
  xsane_set_filename(preferences.filename);
  xsane_define_maximum_output_size(); /* is necessary in postscript mode */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_outputfilename_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_outputfilename_changed_callback\n");

  if (preferences.filename)
  {
    free((void *) preferences.filename);
  }
  preferences.filename = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_define_maximum_output_size(); /* is necessary in postscript mode */
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_outputfilename_new(GtkWidget *vbox)
{
 GtkWidget *hbox;
 GtkWidget *text;
 GtkWidget *button;
 GtkWidget *xsane_filename_counter_step_option_menu;
 GtkWidget *xsane_filename_counter_step_menu;
 GtkWidget *xsane_filename_counter_step_item;
 GtkWidget *xsane_label;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;
 gchar buf[200];
 int i,j;
 int select_item = 0;

  DBG(DBG_proc, "xsane_outputfilename_new\n");

  /* first line: disk icon, filename box */

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  button = xsane_button_new_with_pixmap(xsane.xsane_window->window, hbox, file_xpm, DESC_BROWSE_FILENAME,
                                        (GtkSignalFunc) xsane_browse_filename_callback, NULL);
  gtk_widget_add_accelerator(button, "clicked", xsane.accelerator_group, GDK_F, GDK_MOD1_MASK, DEF_GTK_ACCEL_LOCKED);

  text = gtk_entry_new();
  gtk_widget_set_size_request(text, 80, -1); /* set minimum size */
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FILENAME);
  gtk_entry_set_max_length(GTK_ENTRY(text), 255);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.filename);
  gtk_entry_set_position(GTK_ENTRY(text), strlen(preferences.filename)); /* set cursor to right position of filename */

  gtk_box_pack_end(GTK_BOX(hbox), text, TRUE, TRUE, 5);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_outputfilename_changed_callback, NULL);

  xsane.outputfilename_entry = text;

  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* second line: Step, Type */

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  /* filename counter step */

  pixmap = gdk_pixmap_create_from_xpm_d(xsane.xsane_window->window, &mask, xsane.bg_trans, (gchar **) step_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gtk_widget_show(pixmapwidget);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);
 
  xsane_filename_counter_step_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, xsane_filename_counter_step_option_menu, DESC_FILENAME_COUNTER_STEP);
  gtk_box_pack_start(GTK_BOX(hbox), xsane_filename_counter_step_option_menu, FALSE, FALSE, 5);
  gtk_widget_show(xsane_filename_counter_step_option_menu);
  gtk_widget_show(hbox);
 
  xsane_filename_counter_step_menu = gtk_menu_new();

  select_item = 0;
  j = -2;
  for (i=0; i < 5; i++)
  {
    snprintf(buf, sizeof(buf), "%+d", j);
    xsane_filename_counter_step_item = gtk_menu_item_new_with_label(buf);
    gtk_container_add(GTK_CONTAINER(xsane_filename_counter_step_menu), xsane_filename_counter_step_item);
    g_signal_connect(GTK_OBJECT(xsane_filename_counter_step_item), "activate", (GtkSignalFunc) xsane_filename_counter_step_callback, (void *) j);
    gtk_widget_show(xsane_filename_counter_step_item);
    if (preferences.filename_counter_step == j++)
    {
      select_item = i;
    }
  }
 
  gtk_option_menu_set_menu(GTK_OPTION_MENU(xsane_filename_counter_step_option_menu), xsane_filename_counter_step_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(xsane_filename_counter_step_option_menu), select_item);

  xsane.filetype_option_menu = xsane_back_gtk_filetype_menu_new(preferences.filetype, (GtkSignalFunc) xsane_filetype_callback);
  gtk_box_pack_end(GTK_BOX(hbox), xsane.filetype_option_menu, FALSE, FALSE, 5);
  gtk_widget_show(xsane.filetype_option_menu);

  xsane_label = gtk_label_new(TEXT_FILETYPE); /* opposite order because of box_pack_end */
  gtk_box_pack_end(GTK_BOX(hbox), xsane_label, FALSE, FALSE, 2);
  gtk_widget_show(xsane_label);

  gtk_widget_show(text);
  gtk_widget_show(hbox);
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

void xsane_vseparator_new(GtkWidget *xsane_parent, int dist)
{
 GtkWidget *xsane_vseparator;

  DBG(DBG_proc, "xsane_vseparator_new\n");

  xsane_vseparator = gtk_vseparator_new();
  gtk_box_pack_start(GTK_BOX(xsane_parent), xsane_vseparator, FALSE, FALSE, dist);
  gtk_widget_show(xsane_vseparator);

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
   float size = (float) xsane.param.bytes_per_line * (float) xsane.param.lines;
   int depth = xsane.param.depth;

    if ( (depth == 16) && (preferences.reduce_16bit_to_8bit) )
    {
      depth = 8;
      size /= 2;
    }

    unit = "B";

    if (xsane.param.format >= SANE_FRAME_RED && xsane.param.format <= SANE_FRAME_BLUE)
    {
      size *= 3.0;
      depth *= 3;
    }
    else if (xsane.param.format == SANE_FRAME_RGB)
    {
      depth *= 3;
    }
#ifdef SUPPORT_RGBA
    else if (xsane.param.format == SANE_FRAME_RGBA)
    {
      depth *= 4;
    }
#endif

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
    snprintf(buf, sizeof(buf), "%d*%d*%d (%1.1f %s)", xsane.param.pixels_per_line, xsane.param.lines, depth, size, unit);

    if (xsane.param.format == SANE_FRAME_GRAY)
    {
      xsane.xsane_channels = 1;
    }
#ifdef SUPPORT_RGBA
    else if (xsane.param.format == SANE_FRAME_RGBA)
    {
      xsane.xsane_channels = 4;
    }
#endif
    else /* RGB */
    {
      xsane.xsane_channels = 3;
    }
  }
  else 
  {
    snprintf(buf, sizeof(buf), TEXT_INVALID_PARAMS);
  }

  gtk_label_set(GTK_LABEL(xsane.info_label), buf);


  if (xsane.preview && xsane.preview->surface_unit == SANE_UNIT_MM)
  {
   double dx, dy;

    unit = xsane_back_gtk_unit_string(xsane.preview->surface_unit);

    dx = fabs(xsane.preview->selection.coordinate[2] - xsane.preview->selection.coordinate[0]) / preferences.length_unit;
    dy = fabs(xsane.preview->selection.coordinate[3] - xsane.preview->selection.coordinate[1]) / preferences.length_unit;

    snprintf(buf, sizeof(buf), "%1.2f %s x %1.2f %s", dx, unit, dy, unit);
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.progress_bar), buf);
  }

  xsane_update_histogram(TRUE /* update raw */);

  if (xsane.preview)
  {
    preview_display_valid(xsane.preview);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_define_output_filename(void)
{
  DBG(DBG_proc, "xsane_define_output_filename\n");

  if (xsane.output_filename)
  {
    free(xsane.output_filename);
    xsane.output_filename = 0;
  }

  if (!xsane.force_filename)
  {
    xsane.output_filename = strdup(preferences.filename);
  }
  else
  {
    xsane.output_filename = strdup(xsane.external_filename);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_identify_output_format(char *filename, char *filetype, char **ext)
{
 char *extension;
 int output_format=-1;

  DBG(DBG_proc, "xsane_identify_output_format\n");

  if ((filetype) && (*filetype) && (!xsane.force_filename))
  {
    extension = filetype+1; /* go to filetype, skip leading dot */
  }
  else
  {
    extension = strrchr(filename, '.');
    if (extension)
    {
      extension++; /* skip "." */
    }
  }

  output_format = XSANE_UNKNOWN;

  if (extension)
  {
    if ( (!strcasecmp(extension, "pnm")) || (!strcasecmp(extension, "ppm")) ||
         (!strcasecmp(extension, "pgm")) || (!strcasecmp(extension, "pbm")) )
    {
      if ((xsane.param.depth == 16) && (!preferences.reduce_16bit_to_8bit) )
      {
        output_format = XSANE_PNM16;
      }
      else
      {
        output_format = XSANE_PNM;
      }
    }
    else if ( (!strcasecmp(extension, "txt")) || (!strcasecmp(extension, "text")) )
    {
      output_format = XSANE_TEXT;
    }
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
    else if (!strcasecmp(extension, "png"))
    {
      output_format = XSANE_PNG;
    }
#endif
#endif
#ifdef HAVE_LIBJPEG
    else if ( (!strcasecmp(extension, "jpg")) || (!strcasecmp(extension, "jpeg")) )
    {
      output_format = XSANE_JPEG;
    }
#endif
    else if (!strcasecmp(extension, "ps"))
    {
      output_format = XSANE_PS;
    }
    else if (!strcasecmp(extension, "pdf"))
    {
      output_format = XSANE_PDF;
    }
#ifdef HAVE_LIBTIFF
    else if ( (!strcasecmp(extension, "tif")) || (!strcasecmp(extension, "tiff")) )
    {
      output_format = XSANE_TIFF;
    }
#endif
#ifdef SUPPORT_RGBA
    else if (!strcasecmp(extension, "rgba"))
    {
      output_format = XSANE_RGBA;
    }
#endif
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

 return output_format;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0
void xsane_change_working_directory(void)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_change_working_directory\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_CHANGE_WORKING_DIR, xsane.device_text);
  if (getcwd(filename, sizeof(filename)))
  {
/*    xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, TRUE, FALSE, TRUE, FALSE); */
    if (chdir(filename))
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s (%s).", ERR_CHANGE_WORKING_DIR, filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
      xsane_set_sensitivity(TRUE);
      return;
    }
    else
    {
      if (preferences.working_directory)
      {
        free(preferences.working_directory);
      }
      preferences.working_directory = strdup(filename);
    }
  }

  xsane_set_sensitivity(TRUE);
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static int eula_accept_flag;
static GtkWidget *eula_dialog = NULL;

static gboolean xsane_eula_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  eula_accept_flag = (int) data;

  DBG(DBG_proc ,"xsane_eula_delete_event(%d)\n", eula_accept_flag);
  eula_dialog = NULL;

 return FALSE; /* continue with original delete even routine */
}

/* -------------------------------------- */

static void xsane_eula_button_callback(GtkWidget *widget, gpointer data)
{
  eula_accept_flag = (int) data;

  DBG(DBG_proc ,"xsane_eula_button_callback(%d)\n", eula_accept_flag);

  gtk_widget_destroy(eula_dialog);
  eula_dialog = NULL;
}

/* -------------------------------------- */

int xsane_display_eula(int ask_for_accept)
/* returns FALSE if accepted, TRUE if not accepted */
{
 GtkWidget *vbox, *hbox, *button, *label, *frame;
 GtkAccelGroup *accelerator_group;
 char buf[1024];
 char filename[PATH_MAX];
 FILE *infile;

  DBG(DBG_proc, "xsane_display_eula(%d)\n", ask_for_accept);

  if (eula_dialog) /* make sure the dialog is only opend once */
  {
    return 0;
  }

  eula_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request(eula_dialog, 550, 580);
  gtk_window_set_position(GTK_WINDOW(eula_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(eula_dialog), TRUE);
  g_signal_connect(GTK_OBJECT(eula_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_eula_delete_event), (void *) -1); /* -1 = cancel */
  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_EULA);
  gtk_window_set_title(GTK_WINDOW(eula_dialog), buf);
  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(eula_dialog), accelerator_group);

#if 0
  xsane_set_window_icon(eula_dialog, 0);
#endif

  /* create a frame */
  frame = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(eula_dialog), frame);
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox); 
#if 1
  /* this is normally done directly after gtk_window_set_title() */
  /* but gtk crashes when we would do that and select a text with the mouse */
  xsane_set_window_icon(eula_dialog, 0);
#endif

  /* display XSane copyright message */
  snprintf(buf, sizeof(buf), "XSane %s %s\n"
                             "%s %s\n"
                             "\n"
                             "%s\n"
                             "%s %s\n"
                             "%s %s\n",
                             TEXT_VERSION, XSANE_VERSION,
                             XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT,
                             TEXT_EULA,
                             TEXT_HOMEPAGE, XSANE_HOMEPAGE,
                             TEXT_EMAIL_ADR, XSANE_EMAIL_ADR);

  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  /* add hbox with text and scrollbar to display the eula text */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0); 
  gtk_widget_show(hbox);

#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkWidget *scrolled_window, *text_view;
   GtkTextBuffer *text_buffer;

    /* create a scrolled window to get a vertical scrollbar */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(hbox), scrolled_window);
    gtk_widget_show(scrolled_window);
 
    /* create the gtk_text_view widget */
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_widget_show(text_view); 

    /* get the text_buffer widget and insert the text from file */
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-eula", 0, ".txt", XSANE_PATH_SYSTEM);
    infile = fopen(filename, "r");
 
    if (infile)
    {
      char buffer[4096];
      int nchars;
   
      while (!feof(infile))
      {
        nchars = fread(buffer, 1, 4096, infile);
        gtk_text_buffer_insert_at_cursor(text_buffer, buffer, nchars);
      }
 
      fclose(infile);
    } 
    else
    {
      DBG(DBG_error0, "ERROR: eula text not found. Looks like xsane is not installed correct.\n");
     return TRUE;
    }

  }
#else /* we do not have gtk_text_view, so we use gtk_text */
  {
   GtkWidget *text, *vscrollbar;

    /* Create the gtk_text widget */
    text = gtk_text_new(NULL, NULL);
    gtk_text_set_editable(GTK_TEXT(text), FALSE); /* text is not editable */
    gtk_text_set_word_wrap(GTK_TEXT(text), TRUE); /* wrap complete words */
    gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0); 
    gtk_widget_show(text);
 
    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0); 
    gtk_widget_show(vscrollbar);

    /* Freeze the text widget, ready for multiple updates */ 
    gtk_text_freeze(GTK_TEXT(text)); 

    /* Load the file text.c into the text window */
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-eula", 0, ".txt", XSANE_PATH_SYSTEM);
    infile = fopen(filename, "r");
 
    if (infile)
    {
      char buffer[4096];
      int nchars;
   
      while (!feof(infile))
      {
        nchars = fread(buffer, 1, 4096, infile);
        gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buffer, nchars);
      }
 
      fclose(infile);
    } 
    else
    {
      DBG(DBG_error0, "ERROR: eula text not found. Looks like xsane is not installed correct.\n");
     return TRUE;
    }

    /* Thaw the text widget, allowing the updates to become visible */
    gtk_text_thaw(GTK_TEXT(text)); 
  }
#endif



  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); 

  if (ask_for_accept) /* show accept + not accept buttons */
  {
    button = gtk_button_new_with_label(BUTTON_NOT_ACCEPT);
    gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_eula_button_callback, (void *) 1 /* not accept */);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);

    button = gtk_button_new_with_label(BUTTON_ACCEPT);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_eula_button_callback, (void *) 0 /* accept */);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    gtk_widget_show(button);
  }
  else /* show close button */
  {
#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
#else
    button = gtk_button_new_with_label(BUTTON_CLOSE);
#endif
    gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_eula_button_callback, (void *) 0 /* ok = accept */);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);
  }

  gtk_widget_show(hbox);
  gtk_widget_show(vbox);
  gtk_widget_show(eula_dialog);

  if (ask_for_accept == 0) /* do not ask for accept */
  {
    return 0;
  }
  
  eula_accept_flag = -255;

  while(eula_accept_flag == -255)
  {
    gtk_main_iteration(); /* allow gtk to react to user action */
  }

  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }
 
 return eula_accept_flag;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static GtkWidget *license_dialog = NULL;

static void xsane_close_license_dialog_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc ,"xsane_close_license_dialog_callback\n");

  gtk_widget_destroy(license_dialog);
  license_dialog = NULL;
}

/* ------------------------------------------------ */

static gboolean xsane_license_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  DBG(DBG_proc ,"xsane_license_dialog_delete_event\n");

  license_dialog = NULL;

 return FALSE; /* continue with original delete even routine */
}

/* ------------------------------------------------ */

void xsane_display_gpl(void)
{
 GtkWidget *vbox, *hbox, *button, *label, *frame;
 GtkAccelGroup *accelerator_group;
 char buf[1024];
 char filename[PATH_MAX];
 FILE *infile;

  if (license_dialog) /* make sure the dialog is only opend once */
  {
    return;
  }

  license_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request(license_dialog, 550, 580);
  gtk_window_set_position(GTK_WINDOW(license_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(license_dialog), TRUE);
  g_signal_connect(GTK_OBJECT(license_dialog), "delete_event", GTK_SIGNAL_FUNC(xsane_license_dialog_delete_event), NULL);
  snprintf(buf, sizeof(buf), "%s: %s", xsane.prog_name, WINDOW_GPL);
  gtk_window_set_title(GTK_WINDOW(license_dialog), buf);
  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(license_dialog), accelerator_group);

#if 0
  xsane_set_window_icon(license_dialog, 0);
#endif

  /* create a frame */
  frame = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(license_dialog), frame);
  gtk_widget_show(frame);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox); 
#if 1
  /* this is normally done directly after gtk_window_set_title() */
  /* but gtk crashes when we would do that and select a text with the mouse */
  xsane_set_window_icon(license_dialog, 0);
#endif

  /* display XSane copyright message */
  snprintf(buf, sizeof(buf), "XSane %s %s\n"
                             "%s %s\n"
                             "\n"
                             "%s\n"
                             "%s %s\n"
                             "%s %s\n",
                             TEXT_VERSION, XSANE_VERSION,
                             XSANE_COPYRIGHT_SIGN, XSANE_COPYRIGHT_TXT,
                             TEXT_GPL,
                             TEXT_HOMEPAGE, XSANE_HOMEPAGE,
                             TEXT_EMAIL_ADR, XSANE_EMAIL_ADR);

  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  /* add hbox with text and scrollbar to display the license text */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0); 
  gtk_widget_show(hbox);

#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkWidget *scrolled_window, *text_view;
   GtkTextBuffer *text_buffer;

    /* create a scrolled window to get a vertical scrollbar */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(hbox), scrolled_window);
    gtk_widget_show(scrolled_window);
 
    /* create the gtk_text_view widget */
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_widget_show(text_view); 

    /* get the text_buffer widget and insert the text from file */
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-gpl", 0, ".txt", XSANE_PATH_SYSTEM);
    infile = fopen(filename, "r");
 
    if (infile)
    {
      char buffer[4096];
      int nchars;
   
      while (!feof(infile))
      {
        nchars = fread(buffer, 1, 4096, infile);
        gtk_text_buffer_insert_at_cursor(text_buffer, buffer, nchars);
      }
 
      fclose(infile);
    } 
    else
    {
      DBG(DBG_error0, "ERROR: license text not found. Looks like xsane is not installed correct.\n");
     return;
    }

  }
#else /* we do not have gtk_text_view, so we use gtk_text */
  {
   GtkWidget *text, *vscrollbar;

    /* Create the gtk_text widget */
    text = gtk_text_new(NULL, NULL);
    gtk_text_set_editable(GTK_TEXT(text), FALSE); /* text is not editable */
    gtk_text_set_word_wrap(GTK_TEXT(text), TRUE); /* wrap complete words */
    gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0); 
    gtk_widget_show(text);
 
    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0); 
    gtk_widget_show(vscrollbar);

    /* Freeze the text widget, ready for multiple updates */ 
    gtk_text_freeze(GTK_TEXT(text)); 

    /* Load the file text.c into the text window */
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, "xsane-gpl", 0, ".txt", XSANE_PATH_SYSTEM);
    infile = fopen(filename, "r");
 
    if (infile)
    {
      char buffer[4096];
      int nchars;
   
      while (!feof(infile))
      {
        nchars = fread(buffer, 1, 4096, infile);
        gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buffer, nchars);
      }
 
      fclose(infile);
    } 
    else
    {
      DBG(DBG_error0, "ERROR: license text not found. Looks like xsane is not installed correct.\n");
     return;
    }

    /* Thaw the text widget, allowing the updates to become visible */
    gtk_text_thaw(GTK_TEXT(text)); 
  }
#endif



  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); 

#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
#else
  button = gtk_button_new_with_label(BUTTON_CLOSE);
#endif
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_close_license_dialog_callback, (void *) 0 /* ok = accept */);
  gtk_container_add(GTK_CONTAINER(hbox), button);
  gtk_widget_grab_default(button);
  gtk_widget_show(button);

  gtk_widget_show(hbox);
  gtk_widget_show(vbox);
  gtk_widget_show(license_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_window_get_position(GtkWidget *gtk_window, gint *x, gint *y)
{
#ifdef USE_GTK2_WINDOW_GET_POSITION
  gtk_window_get_position(GTK_WINDOW(gtk_window), x, y);
#else
  if (xsane.get_deskrelative_origin)
  {
    DBG(DBG_proc, "xsane_window_get_position(deskrelative)\n");
    gdk_window_get_deskrelative_origin(gtk_window->window, x, y);
  }
  else
  {
    DBG(DBG_proc, "xsane_window_get_position(root)\n");
    gdk_window_get_root_origin(gtk_window->window, x, y);
  }
#endif
}   

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_widget_test_uposition(GtkWidget *gtk_window)
{
#ifndef USE_GTK2_WINDOW_GET_POSITION
 gint x, y, x_orig, y_orig;

  DBG(DBG_proc, "xsane_widget_test_uposition\n");

  gtk_widget_realize(gtk_window);

  while (!GTK_WIDGET_REALIZED(gtk_window) || (gtk_events_pending()))
  {
    gtk_main_iteration();
  }

  xsane_window_get_position(gtk_window, &x, &y);
  xsane_window_get_position(gtk_window, &x, &y);

  DBG(DBG_info, "xsane_widget_test_uposition: original position = %d, %d\n", x, y);

  x_orig = x;
  y_orig = y;
  gtk_window_move(GTK_WINDOW(gtk_window), x, y);

  xsane_window_get_position(gtk_window, &x, &y);
  DBG(DBG_info, "xsane_widget_test_uposition: new position = %d, %d\n", x, y);

  if ( (x != x_orig) || (y != y_orig) )
  {
    DBG(DBG_proc, "xsane_widget_test_uposition: using deskrelative function\n");
    xsane.get_deskrelative_origin = 1;
  }
  else
  {
    DBG(DBG_proc, "xsane_widget_test_uposition: using root function\n");
  }
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_front_gtk_getname_button;
                                                                                                                                 
static void xsane_front_gtk_getname_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_front_gtk_getname_button_callback\n");
                                                                                                                                 
  xsane_front_gtk_getname_button = (int) data;
}
                                                                                                                                 
/* ---------------------------------------------------------------------------------------------------------------------- */
                                                                                                                                 
int xsane_front_gtk_getname_dialog(const char *dialog_title, const char *desc_text, char *oldname, char **newname)
{
 GtkWidget *getname_dialog;
 GtkWidget *text;
 GtkWidget *button;
 GtkWidget *vbox, *hbox;
 GtkAccelGroup *accelerator_group;
 char buf[TEXTBUFSIZE];
                                                                                                                                 
  DBG(DBG_proc, "xsane_getname_dialog, oldname = %s\n", oldname);
                                                                                                                                 
  getname_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  xsane_set_window_icon(getname_dialog, 0);
                                                                                                                                 
  /* set getname dialog */
  gtk_window_set_position(GTK_WINDOW(getname_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(getname_dialog), FALSE);
  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, dialog_title);
  gtk_window_set_title(GTK_WINDOW(getname_dialog), buf);
  g_signal_connect(GTK_OBJECT(getname_dialog), "delete_event", (GtkSignalFunc) xsane_front_gtk_getname_button_callback, (void *) -1);
  gtk_widget_show(getname_dialog);
                                                                                                                                 
  /* set the main vbox */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
  gtk_container_add(GTK_CONTAINER(getname_dialog), vbox);
  gtk_widget_show(vbox);
                                                                                                                                 
  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(vbox, 2);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);
                                                                                                                                 
  text = gtk_entry_new_with_max_length(64);
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, desc_text);
  gtk_entry_set_text(GTK_ENTRY(text), oldname);
  gtk_widget_set_size_request(text, 300, -1);
  gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
  gtk_widget_show(text);
                                                                                                                                 
  accelerator_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(getname_dialog), accelerator_group);
                                                                                                                                 
#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_front_gtk_getname_button_callback, (void *) -1);
  gtk_widget_add_accelerator(button, "clicked", accelerator_group, GDK_Escape, 0, DEF_GTK_ACCEL_LOCKED); /* ESC */
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
                                                                                                                                 
#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
  button = gtk_button_new_with_label(BUTTON_OK);
#endif
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_front_gtk_getname_button_callback, (void *) 1);
  GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
                                                                                                                                 
  xsane_front_gtk_getname_button = 0;
                                                                                                                                 
  while (xsane_front_gtk_getname_button == 0)
  {
    while (gtk_events_pending())
    {
      DBG(DBG_info, "xsane_getname_dialog: calling gtk_main_iteration\n");
      gtk_main_iteration();
    }
  }
                                                                                                                                 
  *newname = strdup(gtk_entry_get_text(GTK_ENTRY(text)));
                                                                                                                                 
  gtk_widget_destroy(getname_dialog);
                                                                                                                                 
  xsane_set_sensitivity(TRUE);
                                                                                                                                 
  if (xsane_front_gtk_getname_button == 1) /* OK button has been pressed */
  {
    DBG(DBG_info, "renaming %s to %s\n", oldname, *newname);
   return 0; /* OK */
  }
                                                                                                                                 
 return 1; /* Escape */
}
                                                                                                                                 
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_front_gtk_list_entries_swap(GtkWidget *list_item_1, GtkWidget *list_item_2)
{
 char *page1;
 char *page2;
 char *type1;
 char *type2;

  DBG(DBG_proc, "xsane_front_gtk_list_entries_swap\n");

  page1 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_1), "list_item_data");
  type1 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_1), "list_item_type");
  page2 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_2), "list_item_data");
  type2 = (char *) gtk_object_get_data(GTK_OBJECT(list_item_2), "list_item_type");

  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_1))->data), page2);
  gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item_2))->data), page1);
  gtk_object_set_data(GTK_OBJECT(list_item_1), "list_item_data", page2);
  gtk_object_set_data(GTK_OBJECT(list_item_1), "list_item_type", type2);
  gtk_object_set_data(GTK_OBJECT(list_item_2), "list_item_data", page1);
  gtk_object_set_data(GTK_OBJECT(list_item_2), "list_item_type", type1);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_front_gtk_add_process_to_list(pid_t pid)
{
 XsaneChildprocess *newprocess;
 
  DBG(DBG_proc, "xsane_front_gtk_add_process_to_list(%d)\n", pid);
 
  newprocess = malloc(sizeof(XsaneChildprocess));
  newprocess->pid = pid;
  newprocess->next = xsane.childprocess_list;
  xsane.childprocess_list = newprocess;
} 

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_front_gtk_option_defined(char *string)
{
  if (string)
  {
    while (*string == ' ') /* skip spaces */
    {
      string++;
    }
    if (*string != 0)
    {
      return 1;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
#ifdef XSANE_ACTIVATE_EMAIL
void xsane_front_gtk_email_project_update_lockfile_status()
{
 FILE *lockfile;
 char filename[PATH_MAX];

  snprintf(filename, sizeof(filename), "%s/lockfile", preferences.email_project);
  lockfile = fopen(filename, "wb");

  if (lockfile)
  {
    fprintf(lockfile, "%s\n", xsane.email_status); /* first line is status of mail */
    fprintf(lockfile, "%3d\n", (int) (xsane.email_progress_val * 100));
  }

  fclose(lockfile);
}
#endif
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_project_dialog_close()
{
  DBG(DBG_proc, "xsane_project_dialog_close\n");
                                                                                
  if (xsane.project_dialog == NULL)
  {
    return;
  }

  if (xsane.project_dialog)
  {
    xsane_window_get_position(xsane.project_dialog, &xsane.project_dialog_posx, &xsane.project_dialog_posy);
    gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
  }
                                                                                
  gtk_widget_destroy(xsane.project_dialog);
                                                                                
  xsane.project_dialog = NULL;
  xsane.project_list = NULL;
  xsane.project_progress_bar = NULL;
}

/* ---------------------------------------------------------------------------------------------------------------------- */
                                                                                                                  
                                                                                                                                 

