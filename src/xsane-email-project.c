/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-email-project.c

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
#include "xsane-preview.h"
#include "xsane-save.h"
#include "xsane-gamma.h"
#include "xsane-setup.h"
#include "xsane-scan.h"
#include "xsane-rc-io.h"
#include "xsane-device-preferences.h"
#include "xsane-preferences.h"
#include "xsane-icons.h"
#include "xsane-batch-scan.h"

#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
#include <png.h>
#include <zlib.h>
#endif
#endif

#include <sys/wait.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifdef XSANE_ACTIVATE_EMAIL

/* ---------------------------------------------------------------------------------------------------------------------- */

static guint xsane_email_send_timer = 0;

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_email_project_save(void);
void xsane_email_dialog(void);

static gint xsane_email_dialog_delete();
static void xsane_email_filetype_callback(GtkWidget *filetype_option_menu, char *filetype);
static void xsane_email_receiver_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_email_subject_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_email_project_browse_filename_callback(GtkWidget *widget, gpointer data);
static void xsane_email_project_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_email_html_mode_callback(GtkWidget *widget);
static void xsane_email_project_display_status(void);
static void xsane_email_project_load(void);
static void xsane_email_project_delete(void);
static void xsane_email_project_update_project_status();
static void xsane_email_project_create(void);
static void xsane_email_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_email_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_email_entry_rename_callback(GtkWidget *widget, gpointer list);
static void xsane_email_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_email_show_callback(GtkWidget *widget, gpointer data);
#if 0
static void xsane_email_edit_callback(GtkWidget *widget, gpointer data);
#endif
static void xsane_email_send_process(void);
static void xsane_email_send(void);


/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_email_dialog_delete()
{
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_filetype_callback(GtkWidget *filetype_option_menu, char *filetype)
{
  DBG(DBG_proc, "xsane_email_filetype_callback(%s)\n", filetype);

  if (preferences.email_filetype)
  {
    free(preferences.email_filetype);
  }
  preferences.email_filetype = strdup(filetype);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_email_dialog()
{
 GtkWidget *email_dialog, *email_scan_vbox, *email_project_vbox;
 GtkWidget *email_project_exists_hbox, *button;
 GtkWidget *hbox;
 GtkWidget *scrolled_window, *list;
 GtkWidget *pixmapwidget, *text;
 GtkWidget *attachment_frame, *text_frame;
 GtkWidget *label;
 GtkWidget *filetype_menu, *filetype_item;
 GtkWidget *filetype_option_menu;
 GdkPixmap *pixmap;
 GdkBitmap *mask;
 char buf[64];
 int filetype_nr;
 int select_item;

  DBG(DBG_proc, "xsane_email_dialog\n");

  if (xsane.project_dialog) 
  {
    return; /* window already is open */
  }

  /* GTK_WINDOW_TOPLEVEL looks better but does not place it nice*/
  email_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_EMAIL_PROJECT);
  gtk_window_set_title(GTK_WINDOW(email_dialog), buf);
  g_signal_connect(GTK_OBJECT(email_dialog), "delete_event", (GtkSignalFunc) xsane_email_dialog_delete, NULL);
  xsane_set_window_icon(email_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(email_dialog), xsane.accelerator_group); 

  /* set the main vbox */
  email_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(email_scan_vbox), 0);
  gtk_container_add(GTK_CONTAINER(email_dialog), email_scan_vbox);
  gtk_widget_show(email_scan_vbox);


  /* email project */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(email_scan_vbox), hbox, FALSE, FALSE, 1);

  button = xsane_button_new_with_pixmap(xsane.dialog->window, hbox, email_xpm, DESC_EMAIL_PROJECT_BROWSE,
	                                                      (GtkSignalFunc) xsane_email_project_browse_filename_callback, NULL);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_PROJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.email_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_email_project_changed_callback, NULL);

  xsane.project_entry = text;
  xsane.project_entry_box = hbox;

  gtk_widget_show(text);
  gtk_widget_show(hbox);

  email_project_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(email_scan_vbox), email_project_vbox, TRUE, TRUE, 0);
  gtk_widget_show(email_project_vbox);


  /* email receiver */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), hbox, FALSE, FALSE, 1);

  gtk_widget_realize(email_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(email_dialog->window, &mask, xsane.bg_trans, (gchar **) emailreceiver_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  text = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_RECEIVER);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_email_receiver_changed_callback, NULL);

  xsane.email_receiver_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* subject */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), hbox, FALSE, FALSE, 1);

  gtk_widget_realize(email_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(email_dialog->window, &mask, xsane.bg_trans, (gchar **) subject_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_SUBJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_email_subject_changed_callback, NULL);

  xsane.email_subject_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);


  /* email text frame */
  text_frame = gtk_frame_new(TEXT_EMAIL_TEXT);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), text_frame, TRUE, TRUE, 2);
  gtk_widget_show(text_frame);

  /* email text box */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_container_add(GTK_CONTAINER(text_frame), hbox);
  gtk_widget_show(hbox);

#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkWidget *scrolled_window, *text_view, *text_buffer;
 
    /* create a scrolled window to get a vertical scrollbar */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(hbox), scrolled_window);
    gtk_widget_show(scrolled_window);
 
    /* create the gtk_text_view widget */
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_widget_show(text_view);
 
    /* get the text_buffer widget and insert the text from file */
    text_buffer = (GtkWidget *) gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    xsane.email_text_widget = text_buffer;
  }
#else 
  {
    GtkWidget *vscrollbar;

    /* Create the GtkText widget */
    text = gtk_text_new(NULL, NULL);
    gtk_text_set_editable(GTK_TEXT(text), TRUE); /* text is editable */
    gtk_text_set_word_wrap(GTK_TEXT(text), TRUE); /* wrap complete words */
    gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0);
    gtk_widget_show(text);
    xsane.email_text_widget = text;
 
    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);
    gtk_widget_show(vscrollbar); 
  }
#endif


  /* html email */
  button = gtk_check_button_new_with_label(RADIO_BUTTON_HTML_EMAIL);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_HTML_EMAIL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), xsane.email_html_mode);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), button, FALSE, FALSE, 2);
  gtk_widget_show(button);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_html_mode_callback, NULL);
  xsane.email_html_mode_widget = button;

  /* FILETYPE MENU */
  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), hbox, FALSE, FALSE, 1);
  gtk_widget_show(hbox);

  filetype_menu = gtk_menu_new();

  filetype_nr = -1;
  select_item = 0;

#ifdef HAVE_LIBJPEG
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_JPEG);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_email_filetype_callback, (void *) XSANE_FILETYPE_JPEG);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.email_filetype) && (!strcasecmp(preferences.email_filetype, XSANE_FILETYPE_JPEG)) )
  {
    select_item = filetype_nr;
  }
#endif


  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PDF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_email_filetype_callback, (void *) XSANE_FILETYPE_PDF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.email_filetype) && (!strcasecmp(preferences.email_filetype, XSANE_FILETYPE_PDF)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PNG);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_email_filetype_callback, (void *) XSANE_FILETYPE_PNG);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.email_filetype) && (!strcasecmp(preferences.email_filetype, XSANE_FILETYPE_PNG)) )
  {
    select_item = filetype_nr;
  }
#endif
#endif

  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PS);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_email_filetype_callback, (void *) XSANE_FILETYPE_PS);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.email_filetype) && (!strcasecmp(preferences.email_filetype, XSANE_FILETYPE_PS)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBTIFF
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TIFF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_email_filetype_callback, (void *) XSANE_FILETYPE_TIFF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.email_filetype) && (!strcasecmp(preferences.email_filetype, XSANE_FILETYPE_TIFF)) )
  {
    select_item = filetype_nr;
  }
#endif
                                                                                                              
  label = gtk_label_new(TEXT_EMAIL_FILETYPE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  filetype_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, filetype_option_menu, DESC_EMAIL_FILETYPE);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(filetype_option_menu), filetype_menu);
  if (select_item >= 0)
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(filetype_option_menu), select_item);
  }
  gtk_box_pack_end(GTK_BOX(hbox), filetype_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(filetype_menu);
  gtk_widget_show(filetype_option_menu);


  /* attachment frame */
  attachment_frame = gtk_frame_new(TEXT_ATTACHMENTS);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), attachment_frame, FALSE, FALSE, 2);
  gtk_widget_show(attachment_frame);

  /* attachment list */
  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(attachment_frame), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
  gtk_widget_show(list);
  xsane.project_list = list;


  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(email_project_vbox), hbox, FALSE, FALSE, 1);

  button = gtk_button_new_with_label(BUTTON_IMAGE_SHOW);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

#if 0
  /* before we enable the edit function we have to make sure that the rename function
     does also rename the image name of the opened viewer */
  button = gtk_button_new_with_label(BUTTON_IMAGE_EDIT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_edit_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
#endif

  button = gtk_button_new_with_label(BUTTON_IMAGE_RENAME);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_entry_rename_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_IMAGE_DELETE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(email_dialog->window, hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_email_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(email_dialog->window, hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_email_entry_move_down_callback, list);

  gtk_widget_show(hbox);

  xsane.project_box = email_project_vbox;


  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(email_project_vbox, 2);
  gtk_box_pack_end(GTK_BOX(email_scan_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);     


  email_project_exists_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), email_project_exists_hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(BUTTON_SEND_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_send, NULL);
  gtk_box_pack_start(GTK_BOX(email_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_project_delete, NULL);
  gtk_box_pack_start(GTK_BOX(email_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(email_project_exists_hbox);
  xsane.project_exists = email_project_exists_hbox;

  button = gtk_button_new_with_label(BUTTON_CREATE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_project_create, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  xsane.project_not_exists = button;

  /* progress bar */
  xsane.project_progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(email_scan_vbox), (GtkWidget *) xsane.project_progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.project_progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.project_progress_bar));


  xsane.project_dialog = email_dialog;

  xsane_email_project_load();

  gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
  gtk_widget_show(email_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_set_sensitive(int sensitive)
{
  gtk_widget_set_sensitive(xsane.project_box, sensitive);
  gtk_widget_set_sensitive(xsane.project_exists, sensitive);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_display_status()
{
 FILE *lockfile;
 char buf[TEXTBUFSIZE];
 char filename[PATH_MAX];
 int val;
 int i, c;
 int items_done;

  DBG(DBG_proc, "xsane_email_project_display_status\n");

  snprintf(filename, sizeof(filename), "%s/lockfile", preferences.email_project);
  lockfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if (lockfile)
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is email status */
    {
      c = fgetc(lockfile);
      buf[i++] = c;
    }
    buf[i-1] = 0;

    items_done = fscanf(lockfile, "%d\n", &val);

    fclose(lockfile);

    if ( (!strcmp(buf, TEXT_EMAIL_STATUS_SENDING)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SENT)) ||
         (!strcmp(buf, TEXT_PROJECT_STATUS_ERR_READ_PROJECT)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_POP3_CONNECTION_FAILED)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_POP3_LOGIN_FAILED)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_ASMTP_AUTH_FAILED)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SMTP_CONNECTION_FAILED)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SMTP_ERR_FROM)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SMTP_ERR_RCPT)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SMTP_ERR_DATA)) ||
         (!strcmp(buf, TEXT_EMAIL_STATUS_SENT)) )
    {
      if (strcmp(xsane.email_status, buf))
      {
        if (xsane.email_status)
        {
          free(xsane.email_status);
        }
        xsane.email_status = strdup(buf);
 
        if (xsane.project_progress_bar)
        {
          gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.email_status));
        }
      }

      xsane.email_progress_val = val / 100.0;
      if (xsane.project_progress_bar)
      {
        xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), xsane.email_progress_val);
      }

      DBG(DBG_info, "reading from lockfile: email_status %s, email_progress_val %1.3f\n" , xsane.email_status, xsane.email_progress_val);

      if (strcmp(xsane.email_status, TEXT_EMAIL_STATUS_SENDING)) /* not sending */
      {
        DBG(DBG_info, "removing %s\n", filename);
        remove(filename); /* remove lockfile */
 
        xsane.email_progress_val = 0.0;
 
        xsane_email_project_update_project_status();

        if (xsane.project_dialog)
        {
          xsane_email_project_load();

          xsane_email_project_set_sensitive(TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
        }
      }
    }
  }
  else
  {
    DBG(DBG_info, "no lockfile present\n");
    if (xsane.project_progress_bar)
    {
      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.email_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), xsane.email_progress_val);
    }
  
    while (gtk_events_pending())
    {
      gtk_main_iteration();
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_email_send_timer_callback(gpointer data)
{
  xsane_email_project_display_status();

  if (strcmp(xsane.email_status, TEXT_EMAIL_STATUS_SENDING)) /* not sending */
  {
    if (xsane_email_send_timer)
    {
      DBG(DBG_info, "disabling email send timer\n");
      xsane_email_send_timer = 0;
    }
  }

 return xsane_email_send_timer;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_load()
{
 FILE *projectfile;
 char page[TEXTBUFSIZE];
 char *type;
 char *extension;
 char buf[TEXTBUFSIZE];
 char filename[PATH_MAX];
 GtkWidget *list_item;
 int i;
 int c;

  DBG(DBG_proc, "xsane_email_project_load\n");

  if (xsane.email_status)
  {
    free(xsane.email_status);
    xsane.email_status = NULL;
  }

  if (xsane.email_receiver)
  {
    free(xsane.email_receiver);
    xsane.email_receiver = NULL;
  }

  if (xsane.email_filename)
  {
    free(xsane.email_filename);
    xsane.email_filename = NULL;
  }

  if (xsane.email_subject)
  {
    free(xsane.email_subject);
    xsane.email_subject = NULL;
  }

  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.email_receiver_entry), GTK_SIGNAL_FUNC(xsane_email_receiver_changed_callback), 0);
  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.email_subject_entry), GTK_SIGNAL_FUNC(xsane_email_subject_changed_callback), 0);
  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.email_html_mode_widget), GTK_SIGNAL_FUNC(xsane_email_html_mode_callback), 0);

#ifdef HAVE_GTK_TEXT_VIEW_H
  gtk_text_buffer_set_text(GTK_TEXT_BUFFER(xsane.email_text_widget), "", 0);
#else
  gtk_text_set_point(GTK_TEXT(xsane.email_text_widget), 0);
  gtk_text_forward_delete(GTK_TEXT(xsane.email_text_widget), gtk_text_get_length(GTK_TEXT(xsane.email_text_widget)));
#endif
  gtk_list_remove_items(GTK_LIST(xsane.project_list), GTK_LIST(xsane.project_list)->children);

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.email_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    snprintf(filename, sizeof(filename), "%s/image-1.pnm", preferences.email_project);
    xsane.email_filename=strdup(filename);
    xsane_update_counter_in_filename(&xsane.email_filename, FALSE, 0, preferences.filename_counter_len); /* correct counter len */

    xsane.email_status=strdup(TEXT_PROJECT_STATUS_NOT_CREATED);
    xsane.email_progress_val = 0.0;

    xsane.email_receiver=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.email_receiver_entry), (char *) xsane.email_receiver);

    xsane.email_subject=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.email_subject_entry), (char *) xsane.email_subject);

    gtk_widget_hide(xsane.project_exists);
    gtk_widget_show(xsane.project_not_exists);

    gtk_widget_set_sensitive(xsane.project_box, FALSE);
    gtk_widget_set_sensitive(xsane.project_exists, FALSE);
    /* do not change sensitivity of email_project_entry_box here !!! */
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 

    xsane.email_project_save = 0;
  }
  else
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is email status */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;
    if (strchr(page, '@'))
    {
      *strchr(page, '@') = 0;
    }

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(page);
    xsane.email_progress_val = 0.0;


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* second line is email address */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.email_receiver=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.email_receiver_entry), (char *) xsane.email_receiver);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* third line is next email filename */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    snprintf(filename, sizeof(filename), "%s/%s", preferences.email_project, page);
    xsane.email_filename=strdup(filename);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* fourth line is subject */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.email_subject=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.email_subject_entry), (char *) xsane.email_subject);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* fifth line is html/ascii */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    if (!strcasecmp("html", page))
    {
      xsane.email_html_mode = 1;
    }
    else
    {
      xsane.email_html_mode = 0;
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xsane.email_html_mode_widget), xsane.email_html_mode);


    while (!feof(projectfile))
    {
      i=0;
      c=0;

      while ((i<255) && (c != 10) && (c != EOF))
      {
        c = fgetc(projectfile);
        page[i++] = c;
      }
      page[i-1]=0;

      if (!strcmp("mailtext:", page))
      {
        break; /* emailtext follows */
      }

      extension = strrchr(page, '.');
      if (extension)
      {
        type = strdup(extension);
        *extension = 0;
      }
      else
      {
        type = strdup("");
      }

      if (c > 1)
      {
        list_item = gtk_list_item_new_with_label(page);
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
        gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
        gtk_widget_show(list_item);
      }
    }

    while (!feof(projectfile))
    {
      i = fread(buf, 1, sizeof(buf), projectfile);
#ifdef HAVE_GTK_TEXT_VIEW_H
      gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(xsane.email_text_widget), buf, i);
#else
      gtk_text_insert(GTK_TEXT(xsane.email_text_widget), NULL, NULL, NULL, buf, i);
#endif
    }

    if (!strcmp(xsane.email_status, TEXT_EMAIL_STATUS_SENDING)) /* email project is locked (sending) */
    {
      xsane_email_project_set_sensitive(FALSE);
      gtk_widget_set_sensitive(xsane.project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 

      if (xsane_email_send_timer == 0)
      {
        xsane_email_send_timer = gtk_timeout_add(100, (GtkFunction) xsane_email_send_timer_callback, NULL);
        DBG(DBG_info, "enabling email send timer (%d)\n", xsane_email_send_timer);
      }
    }
    else
    {
      xsane_email_project_set_sensitive(TRUE);
      gtk_widget_set_sensitive(xsane.project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
    }

    gtk_widget_show(xsane.project_exists);
    gtk_widget_hide(xsane.project_not_exists);

    xsane.email_project_save = 1;
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.email_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), xsane.email_progress_val);

  xsane_email_project_display_status();

  g_signal_connect(GTK_OBJECT(xsane.email_html_mode_widget), "clicked", (GtkSignalFunc) xsane_email_html_mode_callback, NULL);
  g_signal_connect(GTK_OBJECT(xsane.email_receiver_entry), "changed", (GtkSignalFunc) xsane_email_receiver_changed_callback, NULL);
  g_signal_connect(GTK_OBJECT(xsane.email_subject_entry), "changed", (GtkSignalFunc) xsane_email_subject_changed_callback, NULL);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_delete()
{
 char *page;
 char *type;
 char file[PATH_MAX];
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_email_project_delete\n");

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.email_project, page, type);
    free(page);
    free(type);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/xsane-mail-list", preferences.email_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.email_project);
  rmdir(file);

  xsane_email_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_update_project_status()
{
 FILE *projectfile;
 char filename[PATH_MAX];
 char buf[TEXTBUFSIZE];

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.email_project);
  projectfile = fopen(filename, "r+b"); /* r+ = read and write, position = start of file */

  snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.email_status); /* fill 32 characters status line */
  fprintf(projectfile, "%s\n", buf); /* first line is status of email */

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_email_project_save()
{
 FILE *projectfile;
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;
 char *page;
 char *type;
 gchar *email_text;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_email_project_save\n");

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  mkdir(preferences.email_project, 0777); /* make sure directory exists */

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.email_project);

  if (xsane_create_secure_file(filename)) /* remove possibly existing symbolic links for security */
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, filename);
    xsane_back_gtk_error(buf, TRUE);
   return; /* error */
  }

  projectfile = fopen(filename, "wb"); /* write binary (b for win32) */

  if (xsane.email_status)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.email_status); /* fill 32 characters status line */
    fprintf(projectfile, "%s\n", buf); /* first line is status of email */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.email_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }
  else
  {
    fprintf(projectfile, "                                \n"); /* no email status */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }

  if (xsane.email_receiver)
  {
    fprintf(projectfile, "%s\n", xsane.email_receiver); /* second line is receiver phone number or address */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.email_filename)
  {
    fprintf(projectfile, "%s\n", strrchr(xsane.email_filename, '/')+1); /* third line is next email filename */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.email_subject)
  {
    fprintf(projectfile, "%s\n", xsane.email_subject); /* fourth line is subject */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.email_html_mode) /* fith line is mode html/ascii */
  {
    fprintf(projectfile, "html\n");
  }
  else
  {
    fprintf(projectfile, "ascii\n");
  }


  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = (char *) gtk_object_get_data(list_item, "list_item_data");
    type = (char *) gtk_object_get_data(list_item, "list_item_type");
    fprintf(projectfile, "%s%s\n", page, type);
    list = list->next;
  }

  /* save email text */
  fprintf(projectfile, "mailtext:\n");
#ifdef HAVE_GTK_TEXT_VIEW_H
  {
   GtkTextIter start, end;

    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(xsane.email_text_widget), &start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(xsane.email_text_widget), &end);
    email_text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(xsane.email_text_widget), &start, &end, FALSE);
  }
#else
  email_text = gtk_editable_get_chars(GTK_EDITABLE(xsane.email_text_widget), 0, -1);
#endif
  fprintf(projectfile, "%s", email_text);

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_create()
{
  DBG(DBG_proc, "xsane_email_project_create\n");

  if (strlen(preferences.email_project))
  {
    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_PROJECT_STATUS_CREATED);
    xsane_email_project_save();
    xsane_email_project_load();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_receiver_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_email_receiver_changed_callback\n");

  if (xsane.email_receiver)
  {
    free((void *) xsane.email_receiver);
  }
  xsane.email_receiver = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (xsane.email_status)
  {
    free(xsane.email_status);
  }
  xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
  xsane.email_project_save = 1;
  xsane_email_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_subject_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_email_subject_changed_callback\n");

  if (xsane.email_subject)
  {
    free((void *) xsane.email_subject);
  }
  xsane.email_subject = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (xsane.email_status)
  {
    free(xsane.email_status);
  }
  xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
  xsane.email_project_save = 1;
  xsane_email_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_email_project_set_filename(gchar *filename)
{
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_email_project_changed_callback, NULL);
  gtk_entry_set_text(GTK_ENTRY(xsane.project_entry), (char *) filename); /* update filename in entry */
  gtk_entry_set_position(GTK_ENTRY(xsane.project_entry), strlen(filename)); /* set cursor to right position of filename */

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_email_project_changed_callback, NULL);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_browse_filename_callback(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_email_project_browse_filename_callback\n");

  xsane_set_sensitivity(FALSE);

  if (preferences.email_project) /* make sure a correct filename is defined */
  {
    strncpy(filename, preferences.email_project, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
  }
  else /* no filename given, take standard filename */
  {
    strcpy(filename, OUT_FILENAME);
  }

  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_EMAIL_PROJECT_BROWSE, xsane.device_text);

  umask((mode_t) preferences.directory_umask); /* define new file permissions */
  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_PROJECT, XSANE_GET_FILENAME_SHOW_NOTHING, 0, 0))
  {

    if (preferences.email_project)
    {
      free((void *) preferences.email_project);
    }

    preferences.email_project = strdup(filename);

    xsane_set_sensitivity(TRUE);
    xsane_email_project_set_filename(filename);

    xsane_email_project_load();
  }
  else
  {
    xsane_set_sensitivity(TRUE);
  }
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_project_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_email_project_changed_callback\n");

  if (xsane.email_project_save)
  {
    xsane.email_project_save = 0;
    xsane_email_project_save();
  }

  if (preferences.email_project)
  {
    free((void *) preferences.email_project);
  }
  preferences.email_project = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_email_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_html_mode_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_email_html_mode_callback\n");

  xsane.email_html_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);

  /* we can save it because this routine is only called when the project already exists */
  if (xsane.email_status)
  {
    free(xsane.email_status);
  }
  xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
  xsane.email_project_save = 1;
  xsane_email_project_display_status();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_entry_move_up_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_email_entry_move_up\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item_1 = select->data;

    position = gtk_list_child_position(GTK_LIST(list), list_item_1);
    position--; /* move up */
    newpos = position;

    if (position >= 0)
    {
      while (position>0)
      {
        item = item->next;
        position--;
      }

      list_item_2 = item->data;
      if (list_item_2)
      {
        xsane_front_gtk_list_entries_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);

        if (xsane.email_status)
        {
          free(xsane.email_status);
        }
        xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
        xsane_email_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_entry_move_down_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_email_entry_move_down\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item_1 = select->data;

    position = gtk_list_child_position(GTK_LIST(list), list_item_1);
    position++; /* move down */
    newpos = position;

    while ((position>0) && (item))
    {
      item = item->next;
      position--;
    }

    if (item)
    {
      list_item_2 = item->data;
      if (list_item_2)
      {
        xsane_front_gtk_list_entries_swap(list_item_1, list_item_2);
        gtk_list_select_item(GTK_LIST(list), newpos);

        if (xsane.email_status)
        {
          free(xsane.email_status);
        }
        xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
        xsane_email_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_email_entry_rename;

static void xsane_email_entry_rename_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_email_entry_rename\n");

  xsane_email_entry_rename = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_entry_rename_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 GList *select;
 char *oldpage;
 char *newpage;
 char *type;
 char oldfile[PATH_MAX];
 char newfile[PATH_MAX];

  DBG(DBG_proc, "xsane_email_entry_rename_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
   GtkWidget *rename_dialog;
   GtkWidget *text;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   char filename[PATH_MAX]; 

    list_item = select->data;
    oldpage = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_data"));
    type    = strdup((char *) gtk_object_get_data(GTK_OBJECT(list_item), "list_item_type"));

    xsane_set_sensitivity(FALSE);

    rename_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    xsane_set_window_icon(rename_dialog, 0);

    /* set the main vbox */
    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(rename_dialog), vbox);
    gtk_widget_show(vbox);

    /* set the main hbox */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
    gtk_widget_show(hbox); 

    gtk_window_set_position(GTK_WINDOW(rename_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(rename_dialog), FALSE);
    snprintf(filename, sizeof(filename), "%s %s", xsane.prog_name, WINDOW_EMAIL_RENAME);
    gtk_window_set_title(GTK_WINDOW(rename_dialog), filename);
    g_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_email_entry_rename_button_callback, (void *) -1);
    gtk_widget_show(rename_dialog);

    text = gtk_entry_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_EMAIL_IMAGENAME);
    gtk_entry_set_max_length(GTK_ENTRY(text), 64);
    gtk_entry_set_text(GTK_ENTRY(text), oldpage);
    gtk_widget_set_size_request(text, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 4);
    gtk_widget_show(text);


#ifdef HAVE_GTK2
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
  button = gtk_button_new_with_label(BUTTON_CANCEL);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_entry_rename_button_callback,(void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);

#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
    button = gtk_button_new_with_label(BUTTON_OK);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_email_entry_rename_button_callback, (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);


    xsane_email_entry_rename = 0;

    while (xsane_email_entry_rename == 0)
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    newpage = strdup(gtk_entry_get_text(GTK_ENTRY(text)));

    if (xsane_email_entry_rename == 1)
    {
      gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item))->data), newpage);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(newpage));

      xsane_convert_text_to_filename(&oldpage);
      xsane_convert_text_to_filename(&newpage);
      snprintf(oldfile, sizeof(oldfile), "%s/%s%s", preferences.email_project, oldpage, type);
      snprintf(newfile, sizeof(newfile), "%s/%s%s", preferences.email_project, newpage, type);

      rename(oldfile, newfile);

      if (xsane.email_status)
      {
        free(xsane.email_status);
      }
      xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
      xsane_email_project_save();
    }

    free(oldpage);
    free(newpage);

    gtk_widget_destroy(rename_dialog);

    xsane_set_sensitivity(TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char file[PATH_MAX];

  DBG(DBG_proc, "xsane_email_entry_delete_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.email_project, page, type);
    free(page);
    free(type);
    remove(file);
    gtk_widget_destroy(GTK_WIDGET(list_item));

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
    xsane_email_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_email_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.email_project, page, type);
    free(page);
    free(type);

    xsane_viewer_new(filename, NULL, FALSE, filename, VIEWER_NO_MODIFICATION, IMAGE_SAVED);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

#if 0
static void xsane_email_edit_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];
 char outfilename[PATH_MAX];
 Image_info image_info;
 int cancel_save = 0;

  DBG(DBG_proc, "xsane_email_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.email_project, page, type);
    free(page);
    free(type);

    xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".pnm", XSANE_PATH_TMP);
    xsane_copy_file_by_name(outfilename, filename, xsane.multipage_progress_bar, &cancel_save);

    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

    xsane_viewer_new(outfilename, NULL, FALSE, filename, VIEWER_NO_NAME_MODIFICATION, IMAGE_SAVED);
  }
}
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_create_email(int fd)
{
 FILE *attachment_file;
 FILE *projectfile;
 char *boundary="-----partseparator";
 char *image_filename;
 char *email_text = NULL;
 char *email_text_pos = NULL;
 char **attachment_filename = NULL;
 char *mime_type = NULL;
 char buf[TEXTBUFSIZE];
 char filename[PATH_MAX];
 char content_id[TEXTBUFSIZE];
 char image[TEXTBUFSIZE];
 int i, j;
 int c;
 int attachments = 0;
 int use_attachment = 0;
 int email_text_size = 0;
 int display_images_inline = FALSE;
 size_t bytes_written;

  DBG(DBG_proc, "xsane_create_email\n");

  snprintf(filename, sizeof(filename), "%s/xsane-mail-list", preferences.email_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    DBG(DBG_error, "could not open email project file %s\n", filename);

    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_PROJECT_STATUS_ERR_READ_PROJECT);
    xsane.email_progress_val = 0.0;
    xsane_front_gtk_email_project_update_lockfile_status();

   return;
  }

  for (i=0; i<5; i++) /* skip 5 lines */
  {
    j=0;
    c=0;
    while ((j<255) && (c != 10) && (c != EOF)) /* first line is email status */
    {
      c = fgetc(projectfile);
      j++;
    }
  }

  if (!strcmp(preferences.email_filetype, XSANE_FILETYPE_PNG))
  {
    mime_type = "image/png";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.email_filetype, XSANE_FILETYPE_JPEG))
  {
    mime_type = "image/jpeg";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.email_filetype, XSANE_FILETYPE_TIFF))
  {
    mime_type = "image/tiff";
    display_images_inline = TRUE;
  }
  else if (!strcmp(preferences.email_filetype, XSANE_FILETYPE_PDF))
  {
    mime_type = "doc/pdf";
    display_images_inline = FALSE;
  }
  else if (!strcmp(preferences.email_filetype, XSANE_FILETYPE_PS))
  {
    mime_type = "doc/postscript";
    display_images_inline = FALSE;
  }
  else
  {
    mime_type = "doc/unknown";
    display_images_inline = FALSE;
  }

  DBG(DBG_info, "reading list of attachments:\n");
  /* read list of attachments */
  while (!feof(projectfile))
  {
    /* read next attachment line */
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF))
    {
      c = fgetc(projectfile);
      image[i++] = c;
    }
    image[i-1]=0;

    if (strcmp("mailtext:", image) && (c > 1))
    {
     char imagename[PATH_MAX];
     char *filename;
     char *extension;

      DBG(DBG_info, " - %s\n", image);

      extension = strrchr(image, '.');
      if (extension)
      {
        *extension = 0;
      }

      snprintf(imagename, sizeof(imagename), "%s%s", image, preferences.email_filetype);
      filename=strdup(imagename);
      xsane_convert_text_to_filename(&filename);
      attachment_filename = realloc(attachment_filename, (attachments+1)*sizeof(void *));
      attachment_filename[attachments++] = strdup(filename);
      free(filename);
    }
    else
    {
      break;
    }
  }

  /* read email text */
  while (!feof(projectfile))
  {
    email_text = realloc(email_text, email_text_size+1025); /* increase email_text by 1KB */
    email_text_size += fread(email_text+email_text_size, 1, 1024, projectfile); /* read next KB */
  }
  DBG(DBG_info, "%d bytes emailtext read\n", email_text_size);

  *(email_text + email_text_size) = 0; /* set end of text marker */
  email_text_pos = email_text;

  if (xsane.email_html_mode) /* create html email */
  {
    DBG(DBG_info, "sending email in html format\n");

    write_email_header(fd, preferences.email_from, preferences.email_reply_to, xsane.email_receiver, xsane.email_subject, boundary, 1 /* related */);
    write_email_mime_html(fd, boundary);

    DBG(DBG_info, "sending email text\n");
    while (*email_text_pos != 0)
    {
      if (!strncasecmp("<image>", email_text_pos, 7)) /* insert image */
      {
        email_text_pos += 6; /* <image> is 7 characters, 6 additional ones */

        if (use_attachment < attachments)
        {
          image_filename = attachment_filename[use_attachment++];
          DBG(DBG_info, "inserting image cid for %s\n", image_filename);
          snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */

          /* doc files like ps and pdf can not be displayed inline in html email */
          if (display_images_inline)
          {
            snprintf(buf, sizeof(buf), "<p><img SRC=\"cid:%s\">\r\n", content_id);
          }
          bytes_written = write(fd, buf, strlen(buf));
        }
        else /* more images selected than available */
        {
        }
      }
      else if (*email_text_pos == 10) /* new line */
      {
        snprintf(buf, sizeof(buf), "<br>\r\n");
        bytes_written = write(fd, buf, strlen(buf));
      }
      else
      {
        bytes_written = write(fd, email_text_pos, 1);
      }
      email_text_pos++;
    }

    while (use_attachment < attachments) /* append not already referenced images */
    {
      image_filename = attachment_filename[use_attachment++];
      DBG(DBG_info, "appending image cid for %s\n", image_filename);
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */

      /* doc files like ps and pdf can not be displayed inline in html email */
      if (display_images_inline)
      {
        snprintf(buf, sizeof(buf), "<p><img SRC=\"cid:%s\">\r\n", content_id);
      }
      bytes_written = write(fd, buf, strlen(buf));
    }

    snprintf(buf, sizeof(buf), "</html>\r\n");
    bytes_written = write(fd, buf, strlen(buf));


    for (i=0; i<attachments; i++)
    {
      image_filename = attachment_filename[i];
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */
      snprintf(filename, sizeof(filename), "%s/mail-%s", preferences.email_project, image_filename);
      attachment_file = fopen(filename, "rb"); /* read, b=binary for win32 */

      if (attachment_file)
      {
        DBG(DBG_info, "attaching file \"%s\" as \"%s\" with type %s\n", filename, image_filename, preferences.email_filetype);
        write_email_attach_image(fd, boundary, content_id, mime_type, attachment_file, image_filename);

        remove(filename);
      }
      else /* could not open attachment file */
      {
        DBG(DBG_error, "could not open attachment file \"%s\"\n", filename);
      }

      free(attachment_filename[i]);
    }
    free(attachment_filename);

    write_email_footer(fd, boundary);
  }
  else /* ascii email */
  {
    DBG(DBG_info, "sending email in ascii format\n");

    write_email_header(fd, preferences.email_from, preferences.email_reply_to, xsane.email_receiver, xsane.email_subject, boundary, 0 /* not related */);
    write_email_mime_ascii(fd, boundary);

    bytes_written = write(fd, email_text, strlen(email_text));
    bytes_written = write(fd, "\r\n\r\n", 4);

    for (i=0; i<attachments; i++)
    {
      image_filename = strdup(attachment_filename[i]);
      snprintf(content_id, sizeof(content_id), "%s", image_filename); /* content_id */
      snprintf(filename, sizeof(filename), "%s/mail-%s", preferences.email_project, image_filename);
      attachment_file = fopen(filename, "rb"); /* read, b=binary for win32 */

      if (attachment_file)
      {
        DBG(DBG_info, "attaching file \"%s\" as \"%s\" with type %s\n", filename, image_filename, preferences.email_filetype);
        write_email_attach_image(fd, boundary, content_id, mime_type, attachment_file, image_filename);

        remove(filename);
      }
      else /* could not open attachment file */
      {
        DBG(DBG_error, "could not oppen attachment png file \"%s\"\n", filename);
      }

      free(image_filename);
      free(attachment_filename[i]);
    }
    free(attachment_filename);

    write_email_footer(fd, boundary);
  }

  free(email_text);

  if (projectfile)
  {
    fclose(projectfile);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_send_process()
{
 int fd_socket;
 int status;
 char *password;
 int i;

  DBG(DBG_proc, "xsane_email_send_process\n");

  password = strdup(preferences.email_auth_pass);

  for (i=0; i<strlen(password); i++)
  {
    password[i] ^= 0x53;
  } 

  /* pop3 authentication */
  if (preferences.email_authentication == EMAIL_AUTH_POP3)
  {
    fd_socket = open_socket(preferences.email_pop3_server, preferences.email_pop3_port);

    if (fd_socket < 0) /* could not open socket */
    {
      if (xsane.email_status)
      {
        free(xsane.email_status);
      }
      xsane.email_status = strdup(TEXT_EMAIL_STATUS_POP3_CONNECTION_FAILED);
      xsane.email_progress_val = 0.0;
      xsane_front_gtk_email_project_update_lockfile_status();

      free(password);

     return;
    }

    status = pop3_login(fd_socket, preferences.email_auth_user, password);

    close(fd_socket);

    if (status == -1)
    {
      if (xsane.email_status)
      {
        free(xsane.email_status);
      }
      xsane.email_status = strdup(TEXT_EMAIL_STATUS_POP3_LOGIN_FAILED);
      xsane.email_progress_val = 0.0;
      xsane_front_gtk_email_project_update_lockfile_status();

      free(password);

     return;
    }

    DBG(DBG_info, "POP3 authentication done\n");
  }



  /* smtp email */
  fd_socket = open_socket(preferences.email_smtp_server, preferences.email_smtp_port);

  if (fd_socket < 0) /* could not open socket */
  {
    if (xsane.email_status)
    {
      free(xsane.email_status);
    }
    xsane.email_status = strdup(TEXT_EMAIL_STATUS_SMTP_CONNECTION_FAILED);
    xsane.email_progress_val = 0.0;
    xsane_front_gtk_email_project_update_lockfile_status();

    free(password);

   return;
  }


  status = write_smtp_header(fd_socket, preferences.email_from, xsane.email_receiver,
                             preferences.email_authentication, preferences.email_auth_user, password);
  if (status == -1)
  {
   return;
  }


  xsane_create_email(fd_socket); /* create email and write to socket */

  write_smtp_footer(fd_socket);

  close(fd_socket);

  if (xsane.email_status)
  {
    free(xsane.email_status);
  }
  xsane.email_status = strdup(TEXT_EMAIL_STATUS_SENT);
  xsane.email_progress_val = 1.0;
  xsane_front_gtk_email_project_update_lockfile_status();

  free(password);

  _exit(0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_email_send()
{
 pid_t pid;
 char *image;
 char *type;
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;
 char source_filename[PATH_MAX];
 char email_filename[PATH_MAX];
 int output_format;
 int cancel_save = 0;

  DBG(DBG_proc, "xsane_email_send\n");

  xsane_set_sensitivity(FALSE); /* do not allow changing xsane mode */

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  if (xsane.email_project_save)
  {
    xsane.email_project_save = 0;
    xsane_email_project_save();
  }

  xsane.email_progress_size  = 0;
  xsane.email_progress_bytes = 0;

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    image = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type  = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&image);
    snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.email_project, image, type);
    snprintf(email_filename, sizeof(email_filename), "%s/mail-%s%s", preferences.email_project, image, preferences.email_filetype);
    free(image);
    free(type);
    DBG(DBG_info, "converting %s to %s\n", source_filename, email_filename);
    output_format = xsane_identify_output_format(email_filename, NULL, NULL);
    xsane_save_image_as(email_filename, source_filename, output_format, xsane.enable_color_management, preferences.cms_function, preferences.cms_intent, preferences.cms_bpc, xsane.project_progress_bar, &cancel_save);
    list = list->next;
    xsane.email_progress_size += xsane_get_filesize(email_filename);
  }


  if (xsane.email_status)
  {
    free(xsane.email_status);
  }
  xsane.email_status = strdup(TEXT_EMAIL_STATUS_SENDING);
  xsane.email_progress_val = 0.0;
  xsane_email_project_display_status(); /* display status before creating lockfile! */
  xsane_front_gtk_email_project_update_lockfile_status(); /* create lockfile and update status */

  pid = fork();
 
  if (pid == 0) /* new process */
  {
   FILE *ipc_file = NULL;

    if (xsane.ipc_pipefd[0])
    {
      close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
      ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
    }

    DBG(DBG_info, "trying to change user id for new subprocess:\n");
    DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
    setuid(getuid());
    DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

    xsane_email_send_process();

    _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
  }
  else /* parent process */
  {
    xsane_front_gtk_add_process_to_list(pid); /* add pid to child process list */
  }

  xsane_email_send_timer = gtk_timeout_add(100, (GtkFunction) xsane_email_send_timer_callback, NULL);
  DBG(DBG_info, "enabling email send timer (%d)\n", xsane_email_send_timer);

  xsane_set_sensitivity(TRUE); /* allow changing xsane mode */
#if 0
  gtk_widget_set_sensitive(xsane.project_entry_box, TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
  gtk_widget_set_sensitive(xsane.project_box, FALSE);
#endif
  xsane_email_project_set_sensitive(FALSE);
}

#endif
