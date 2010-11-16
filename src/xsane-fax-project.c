/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-fax-project.c

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
#include "xsane-multipage-project.h"
#include "xsane-fax-project.h"
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

/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */

void xsane_fax_dialog(void);
void xsane_fax_project_save(void);

static gint xsane_fax_dialog_delete();
static void xsane_fax_project_browse_filename_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_receiver_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_project_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_fine_mode_callback(GtkWidget *widget);
static void xsane_fax_project_update_project_status();
static void xsane_fax_project_load(void);
static void xsane_fax_project_delete(void);
static void xsane_fax_project_create(void);
static void xsane_fax_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_insert_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_fax_show_callback(GtkWidget *widget, gpointer data);
static void xsane_fax_send(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_fax_dialog_delete()
{
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_fax_dialog()
{
 GtkWidget *fax_dialog, *fax_scan_vbox, *fax_project_vbox, *hbox, *fax_project_exists_hbox, *button;
 GtkWidget *scrolled_window, *list;
 char buf[64];
 GtkWidget *text;
 GtkWidget *pages_frame;
 GtkWidget *pixmapwidget;
 GdkBitmap *mask;
 GdkPixmap *pixmap;

  DBG(DBG_proc, "xsane_fax_dialog\n");

  if (xsane.project_dialog) 
  {
    return; /* window already is open */
  }

  /* GTK_WINDOW_TOPLEVEL looks better but does not place it nice*/
  fax_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_FAX_PROJECT);
  gtk_window_set_title(GTK_WINDOW(fax_dialog), buf);
  g_signal_connect(GTK_OBJECT(fax_dialog), "delete_event", (GtkSignalFunc) xsane_fax_dialog_delete, NULL);
  xsane_set_window_icon(fax_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(fax_dialog), xsane.accelerator_group); 

  /* set the main vbox */
  fax_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(fax_scan_vbox), 0);
  gtk_container_add(GTK_CONTAINER(fax_dialog), fax_scan_vbox);
  gtk_widget_show(fax_scan_vbox);

  /* fax project */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 1);

  button = xsane_button_new_with_pixmap(xsane.dialog->window, hbox, fax_xpm, DESC_FAX_PROJECT_BROWSE,
	                                            (GtkSignalFunc) xsane_fax_project_browse_filename_callback, NULL);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXPROJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.fax_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_fax_project_changed_callback, NULL);

  xsane.project_entry = text;
  xsane.project_entry_box = hbox;

  gtk_widget_show(text);
  gtk_widget_show(hbox);

  fax_project_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), fax_project_vbox, TRUE, TRUE, 0);
  gtk_widget_show(fax_project_vbox);

  /* fax receiver */

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), hbox, FALSE, FALSE, 1);

  gtk_widget_realize(fax_dialog);

  pixmap = gdk_pixmap_create_from_xpm_d(fax_dialog->window, &mask, xsane.bg_trans, (gchar **) faxreceiver_xpm);
  pixmapwidget = gtk_image_new_from_pixmap(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), pixmapwidget, FALSE, FALSE, 2);
  gdk_drawable_unref(pixmap);
  gdk_drawable_unref(mask);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXRECEIVER);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_fax_receiver_changed_callback, NULL);

  xsane.fax_receiver_entry = text;

  gtk_widget_show(pixmapwidget);
  gtk_widget_show(text);
  gtk_widget_show(hbox);

  /* fine mode */
  button = gtk_check_button_new_with_label(RADIO_BUTTON_FINE_MODE);
  xsane_back_gtk_set_tooltip(xsane.tooltips, button, DESC_FAX_FINE_MODE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), preferences.fax_fine_mode);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), button, FALSE, FALSE, 2);
  gtk_widget_show(button);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_fine_mode_callback, NULL);


  /* pages frame */
  pages_frame = gtk_frame_new(TEXT_PAGES);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), pages_frame, TRUE, TRUE, 2);
  gtk_widget_show(pages_frame);

  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(pages_frame), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);

  gtk_widget_show(list);

  xsane.project_list = list;


  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(fax_project_vbox), hbox, FALSE, FALSE, 1);

  button = gtk_button_new_with_label(BUTTON_FILE_INSERT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_insert_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_SHOW);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_RENAME);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_PAGE_DELETE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(fax_dialog->window, hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_fax_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(fax_dialog->window, hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_fax_entry_move_down_callback, list);

  gtk_widget_show(hbox);

  xsane.project_box = fax_project_vbox;

  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(fax_project_vbox, 2);
  gtk_box_pack_end(GTK_BOX(fax_scan_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);     


  fax_project_exists_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), fax_project_exists_hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(BUTTON_SEND_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_send, NULL);
  gtk_box_pack_start(GTK_BOX(fax_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_project_delete, NULL);
  gtk_box_pack_start(GTK_BOX(fax_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(fax_project_exists_hbox);
  xsane.project_exists = fax_project_exists_hbox;

  button = gtk_button_new_with_label(BUTTON_CREATE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_project_create, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  xsane.project_not_exists = button;

  /* progress bar */
  xsane.project_progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(fax_scan_vbox), (GtkWidget *) xsane.project_progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.project_progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.project_progress_bar));


  xsane.project_dialog = fax_dialog;

  xsane_fax_project_load();

  gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
  gtk_widget_show(fax_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_load()
{
 FILE *projectfile;
 char page[TEXTBUFSIZE];
 char filename[PATH_MAX];
 GtkWidget *list_item;
 int i;
 int c;

  DBG(DBG_proc, "xsane_fax_project_load\n");

  if (xsane.fax_status)
  {
    free(xsane.fax_status);
    xsane.fax_status = NULL;
  }

  if (xsane.fax_receiver)
  {
    free(xsane.fax_receiver);
    xsane.fax_receiver = NULL;
  }

  g_signal_handlers_disconnect_by_func(GTK_OBJECT(xsane.fax_receiver_entry), GTK_SIGNAL_FUNC(xsane_fax_receiver_changed_callback), 0);
  gtk_list_remove_items(GTK_LIST(xsane.project_list), GTK_LIST(xsane.project_list)->children);

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    xsane.fax_status=strdup(TEXT_PROJECT_STATUS_NOT_CREATED);

    snprintf(filename, sizeof(filename), "%s/page-1.pnm", preferences.fax_project);
    xsane.fax_filename=strdup(filename);
    xsane_update_counter_in_filename(&xsane.fax_filename, FALSE, 0, preferences.filename_counter_len); /* correct counter len */

    xsane.fax_receiver=strdup("");
    gtk_entry_set_text(GTK_ENTRY(xsane.fax_receiver_entry), (char *) xsane.fax_receiver);

    gtk_widget_set_sensitive(xsane.project_box, FALSE);
    gtk_widget_hide(xsane.project_exists);
    gtk_widget_show(xsane.project_not_exists);
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
  }
  else
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is fax status */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;
    if (strchr(page, '@'))
    {
      *strchr(page, '@') = 0;
    }
    xsane.fax_status = strdup(page);

    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* second line is receiver phone number or address */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    xsane.fax_receiver=strdup(page);
    gtk_entry_set_text(GTK_ENTRY(xsane.fax_receiver_entry), (char *) xsane.fax_receiver);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* third line is next fax filename */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    snprintf(filename, sizeof(filename), "%s/%s", preferences.fax_project, page);
    xsane.fax_filename=strdup(filename);

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

      if (c > 1)
      {
       char *type;
       char *extension;

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

        list_item = gtk_list_item_new_with_label(page);
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(page));
        gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(type));
        gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
        gtk_widget_show(list_item);
      }
    }
    gtk_widget_set_sensitive(xsane.project_box, TRUE);
    gtk_widget_show(xsane.project_exists);
    gtk_widget_hide(xsane.project_not_exists);
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  g_signal_connect(GTK_OBJECT(xsane.fax_receiver_entry), "changed", (GtkSignalFunc) xsane_fax_receiver_changed_callback, NULL);

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_delete()
{
 char *page;
 char file[PATH_MAX];
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_fax_project_delete\n");

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s.pnm", preferences.fax_project, page);
    free(page);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/xsane-fax-list", preferences.fax_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.fax_project);
  rmdir(file);

  xsane_fax_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_update_project_status()
{
 FILE *projectfile;
 char filename[PATH_MAX];
 char buf[TEXTBUFSIZE];

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);
  projectfile = fopen(filename, "r+b"); /* r+ = read and write, position = start of file */

  snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.fax_status); /* fill 32 characters status line */
  fprintf(projectfile, "%s\n", buf); /* first line is status of mail */

  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_fax_project_save()
{
 FILE *projectfile;
 char *page;
 char *type;
 char filename[PATH_MAX];
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_fax_project_save\n");

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  mkdir(preferences.fax_project, 0777); /* make sure directory exists */

  snprintf(filename, sizeof(filename), "%s/xsane-fax-list", preferences.fax_project);

  if (xsane_create_secure_file(filename)) /* remove possibly existing symbolic links for security
*/
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, filename);
    xsane_back_gtk_error(buf, TRUE);
   return; /* error */
  }
  projectfile = fopen(filename, "wb"); /* write binary (b for win32) */

  if (!projectfile)
  {
    xsane_back_gtk_error(ERR_CREATE_FAX_PROJECT, TRUE);

   return;
  }

  if (xsane.fax_status)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.fax_status); /* fill 32 characters status line */
    fprintf(projectfile, "%s\n", buf); /* first line is status of mail */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }
  else
  {
    fprintf(projectfile, "                                \n"); /* no mail status */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }

  if (xsane.fax_receiver)
  {
    fprintf(projectfile, "%s\n", xsane.fax_receiver); /* first line is receiver phone number or address */
  }
  else
  {
    fprintf(projectfile, "\n");
  }

  if (xsane.fax_filename)
  {
    fprintf(projectfile, "%s\n", strrchr(xsane.fax_filename, '/')+1); /* second line is next fax filename */
  }
  else
  {
    fprintf(projectfile, "\n");
  }


  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = (char *) gtk_object_get_data(list_item, "list_item_data");
    type = (char *) gtk_object_get_data(list_item, "list_item_type");
    fprintf(projectfile, "%s%s\n", page, type);
    list = list->next;
  }
  fclose(projectfile);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_create()
{
  DBG(DBG_proc, "xsane_fax_project_create\n");

  if (strlen(preferences.fax_project))
  {
    if (xsane.fax_status)
    {
      free(xsane.fax_status);
    }
    xsane.fax_status = strdup(TEXT_PROJECT_STATUS_CREATED);
    xsane_fax_project_save();
    xsane_fax_project_load();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_receiver_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_fax_receiver_changed_callback\n");

  if (xsane.fax_status)
  {
    free(xsane.fax_status);
  }
  xsane.fax_status = strdup(TEXT_PROJECT_STATUS_CHANGED);

  if (xsane.fax_receiver)
  {
    free((void *) xsane.fax_receiver);
  }
  xsane.fax_receiver = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_fax_project_save();

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
}

/* ----------------------------------------------------------------------------------------------------------------- */

void xsane_fax_project_set_filename(gchar *filename)
{
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_fax_project_changed_callback, NULL);
  gtk_entry_set_text(GTK_ENTRY(xsane.project_entry), (char *) filename); /* update filename in entry */
  gtk_entry_set_position(GTK_ENTRY(xsane.project_entry), strlen(filename)); /* set cursor to right position of filename */

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_fax_project_changed_callback, NULL);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_browse_filename_callback(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_fax_project_browse_filename_callback\n");

  xsane_set_sensitivity(FALSE);

  if (preferences.fax_project) /* make sure a correct filename is defined */
  {
    strncpy(filename, preferences.fax_project, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
  }
  else /* no filename given, take standard filename */
  {
    strcpy(filename, OUT_FILENAME);
  }

  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_FAX_PROJECT_BROWSE, xsane.device_text);

  umask((mode_t) preferences.directory_umask); /* define new file permissions */
  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_PROJECT, FALSE, 0, 0))
  {

    if (preferences.fax_project)
    {
      free((void *) preferences.fax_project);
    }

    preferences.fax_project = strdup(filename);

    xsane_set_sensitivity(TRUE);
    xsane_fax_project_set_filename(filename);

    xsane_fax_project_load();
  }
  else
  {
    xsane_set_sensitivity(TRUE);
  }
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */

}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_project_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_fax_project_changed_callback\n");

  if (preferences.fax_project)
  {
    free((void *) preferences.fax_project);
  }
  preferences.fax_project = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_fax_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_fine_mode_callback(GtkWidget * widget)
{
  DBG(DBG_proc, "xsane_fax_fine_mode_callback\n");

  preferences.fax_fine_mode = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_move_up_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_fax_entry_move_up\n");

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
        xsane_fax_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_move_down_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_fax_entry_move_down\n");

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
        xsane_fax_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

int xsane_fax_entry_rename;

static void xsane_fax_entry_rename_button_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_fax_entry_rename\n");

  xsane_fax_entry_rename = (int) data;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_rename_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 GList *select;
 char *oldpage;
 char *newpage;
 char *type;
 char oldfile[PATH_MAX];
 char newfile[PATH_MAX];

  DBG(DBG_proc, "xsane_fax_entry_rename_callback\n");

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
    snprintf(filename, sizeof(filename), "%s %s", xsane.prog_name, WINDOW_FAX_RENAME);
    gtk_window_set_title(GTK_WINDOW(rename_dialog), filename);
    g_signal_connect(GTK_OBJECT(rename_dialog), "delete_event", (GtkSignalFunc) xsane_fax_entry_rename_button_callback,(void *) -1);
    gtk_widget_show(rename_dialog);

    text = gtk_entry_new();
    xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_FAXPAGENAME);
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
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) -1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);


#ifdef HAVE_GTK2
    button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
    button = gtk_button_new_with_label(BUTTON_OK);
#endif
    g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_fax_entry_rename_button_callback, (void *) 1);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_show(button);


    xsane_fax_entry_rename = 0;

    while (xsane_fax_entry_rename == 0)
    {
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    newpage = strdup(gtk_entry_get_text(GTK_ENTRY(text)));

    if (xsane_fax_entry_rename == 1)
    {
      gtk_label_set(GTK_LABEL(gtk_container_children(GTK_CONTAINER(list_item))->data), newpage);
      gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(newpage));

      xsane_convert_text_to_filename(&oldpage);
      xsane_convert_text_to_filename(&newpage);
      snprintf(oldfile, sizeof(oldfile), "%s/%s%s", preferences.fax_project, oldpage, type);
      snprintf(newfile, sizeof(newfile), "%s/%s%s", preferences.fax_project, newpage, type);

      rename(oldfile, newfile);

      xsane_fax_project_save();
    }

    free(oldpage);
    free(newpage);
    free(type);

    gtk_widget_destroy(rename_dialog);

    xsane_set_sensitivity(TRUE);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_insert_callback(GtkWidget *widget, gpointer list)
{
 GtkWidget *list_item;
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_fax_entry_insert_callback\n");

  xsane_set_sensitivity(FALSE);

  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_FAX_INSERT, preferences.fax_project);
  filename[0] = 0;

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_OPEN, FALSE, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_IMAGES, XSANE_FILE_FILTER_IMAGES)) /* filename is selected */
  {
   FILE *sourcefile;
 
    sourcefile = fopen(filename, "rb"); /* read binary (b for win32) */
    if (sourcefile) /* file exists */
    {
     char buf[1024];

      fgets(buf, sizeof(buf), sourcefile);

      if (!strncmp("%!PS", buf, 4))
      {
       FILE *destfile;
       char destpath[PATH_MAX];
       char *destfilename;
       char *destfiletype;
       char *extension;

        destfilename = strdup(strrchr(filename, '/')+1);
        extension = strrchr(destfilename, '.');
        if (extension)
        {
          destfiletype = strdup(extension);
          *extension = 0;
        }
        else
        {
          destfiletype = strdup("");
        }
        
        snprintf(destpath, sizeof(destpath), "%s/%s%s", preferences.fax_project, destfilename, destfiletype);
        /* copy file to project directory */
        if (xsane_create_secure_file(destpath)) /* remove possibly existing symbolic links for security
*/
        {
          fclose(sourcefile);
          snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, destpath);
          xsane_back_gtk_error(buf, TRUE);
         return; /* error */
        }

        destfile = fopen(destpath, "wb"); /* write binary (b for win32) */

        if (destfile) /* file is created */
        {
          fprintf(destfile, "%s\n", buf);

          while (!feof(sourcefile))
          {
            fgets(buf, sizeof(buf), sourcefile);
            fprintf(destfile, "%s", buf);
          }

          fclose(destfile);


          /* add filename to fax page list */
          list_item = gtk_list_item_new_with_label(destfilename);
          gtk_object_set_data(GTK_OBJECT(list_item), "list_item_data", strdup(destfilename));
          gtk_object_set_data(GTK_OBJECT(list_item), "list_item_type", strdup(destfiletype));
          gtk_container_add(GTK_CONTAINER(xsane.project_list), list_item);
          gtk_widget_show(list_item);

          xsane_update_counter_in_filename(&xsane.fax_filename, TRUE, 1, preferences.filename_counter_len);
          xsane_fax_project_save();
          free(destfilename);
        }
        else /* file could not be created */
        {
          snprintf(buf, sizeof(buf), "%s %s", ERR_OPEN_FAILED, filename);
          xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
        }
      }
      else
      {
        snprintf(buf, sizeof(buf), ERR_FILE_NOT_POSTSCRIPT, filename);
        xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      }

      fclose(sourcefile);
    } 
    else
    {
     char buf[TEXTBUFSIZE];
      snprintf(buf, sizeof(buf), ERR_FILE_NOT_EXISTS, filename);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
    }
  }

  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */    

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_fax_entry_delete_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.fax_project, page, type);
    free(page);
    free(type);
    remove(filename);
    gtk_widget_destroy(GTK_WIDGET(list_item));
    xsane_fax_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_fax_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.fax_project, page, type);

    if (!strncmp(type, ".pnm", 4))
    {
      /* when we do not allow any modification then we can work with the original file */
      /* so we do not have to copy the image into a dummy file here! */

      xsane_viewer_new(filename, NULL, FALSE, filename, VIEWER_NO_MODIFICATION, IMAGE_SAVED);
    }
    else if (!strncmp(type, ".ps", 3))
    {
     char *arg[100];
     int argnr;
     pid_t pid;

      argnr = xsane_parse_options(preferences.fax_viewer, arg);
      arg[argnr++] = filename;
      arg[argnr] = 0;

      pid = fork();

      if (pid == 0) /* new process */
      {
       FILE *ipc_file = NULL;

        if (xsane.ipc_pipefd[0])
        {
          close(xsane.ipc_pipefd[0]); /* close reading end of pipe */
          ipc_file = fdopen(xsane.ipc_pipefd[1], "w");
        }

        DBG(DBG_info, "trying to change user id fo new subprocess:\n");
        DBG(DBG_info, "old effective uid = %d\n", (int) geteuid());
        setuid(getuid());
        DBG(DBG_info, "new effective uid = %d\n", (int) geteuid());

        execvp(arg[0], arg); /* does not return if successfully */
        DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_FAX_VIEWER, preferences.fax_viewer);

        /* send error message via IPC pipe to parent process */
        if (ipc_file)
        {
          fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_FAX_VIEWER, preferences.fax_viewer, strerror(errno));
          fflush(ipc_file); /* make sure message is displayed */
          fclose(ipc_file);
        }

        _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
      }
      else /* parent process */
      {
        xsane_front_gtk_add_process_to_list(pid); /* add pid to child process list */
      }
    }

    free(page);
    free(type);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static int xsane_fax_convert_pnm_to_ps(char *source_filename, char *fax_filename)
{
 FILE *outfile;
 FILE *infile;
 Image_info image_info;
 char buf[TEXTBUFSIZE];
 int cancel_save;

  /* open progressbar */
  snprintf(buf, sizeof(buf), "%s - %s", PROGRESS_CONVERTING_DATA, source_filename);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), buf);
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  infile = fopen(source_filename, "rb"); /* read binary (b for win32) */
  if (infile != 0)
  {
    xsane_read_pnm_header(infile, &image_info);

    umask((mode_t) preferences.image_umask); /* define image file permissions */   
    outfile = fopen(fax_filename, "wb"); /* b = binary mode for win32 */
    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */   
    if (outfile != 0)
    {
     float imagewidth, imageheight;

      imagewidth  = 72.0 * image_info.image_width /image_info.resolution_x; /* width in 1/72 inch */
      imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

      DBG(DBG_info, "imagewidth  = %f 1/72 inch\n", imagewidth);
      DBG(DBG_info, "imageheight = %f 1/72 inch\n", imageheight);

      xsane_save_ps(outfile, infile,
                    &image_info,
                    imagewidth, imageheight,
                    preferences.fax_leftoffset   * 72.0/MM_PER_INCH, /* paper_left_margin */
                    preferences.fax_bottomoffset * 72.0/MM_PER_INCH, /* paper_bottom_margin */
                    preferences.fax_width  * 72.0/MM_PER_INCH, /* paper_width */
                    preferences.fax_height * 72.0/MM_PER_INCH, /* paper_height */
                    0 /* portrait top left */,
                    preferences.fax_ps_flatedecoded, /* use ps level 3 zlib compression */
                    NULL, /* hTransform */
		    0 /* do not apply ICM profile */,
		    0, NULL, /* no CSA */
		    0, NULL, 0, /* no CRD */
		    0, /* intent */
                    xsane.project_progress_bar,
                    &cancel_save);
      fclose(outfile);
    }
    else
    {
     char buf[TEXTBUFSIZE];

      DBG(DBG_info, "open of faxfile `%s'failed : %s\n", fax_filename, strerror(errno));

      snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, fax_filename, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
    }

    fclose(infile);
  }
  else
  {
   char buf[TEXTBUFSIZE];

    DBG(DBG_info, "open of faxfile `%s'failed : %s\n", source_filename, strerror(errno));

    snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, source_filename, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

 return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_fax_send()
{
 char *page;
 char *type;
 char *fax_type=".ps";
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;
 pid_t pid;
 char *arg[1000];
 char buf[TEXTBUFSIZE];
 char source_filename[PATH_MAX];
 char fax_filename[PATH_MAX];
 int argnr = 0;
 int i;

  DBG(DBG_proc, "xsane_fax_send\n");

  if (list)
  {
    if (!xsane_front_gtk_option_defined(xsane.fax_receiver))
    {
      snprintf(buf, sizeof(buf), "%s\n", ERR_SENDFAX_RECEIVER_MISSING);
      xsane_back_gtk_error(buf, TRUE);
      return;
    }

    xsane_set_sensitivity(FALSE);
    /* gtk_widget_set_sensitive(xsane.project_dialog, FALSE); */

    argnr = xsane_parse_options(preferences.fax_command, arg);

    if (preferences.fax_fine_mode) /* fine mode */
    {
      if (xsane_front_gtk_option_defined(preferences.fax_fine_option))
      {
        arg[argnr++] = strdup(preferences.fax_fine_option);
      }
    }
    else /* normal mode */
    {
      if (xsane_front_gtk_option_defined(preferences.fax_normal_option))
      {
        arg[argnr++] = strdup(preferences.fax_normal_option);
      }
    }

    if (xsane_front_gtk_option_defined(preferences.fax_receiver_option))
    {
      arg[argnr++] = strdup(preferences.fax_receiver_option);
    }
    arg[argnr++] = strdup(xsane.fax_receiver);

    if (xsane_front_gtk_option_defined(preferences.fax_postscript_option))
    {
      arg[argnr++] = strdup(preferences.fax_postscript_option);
    }

    while ((list) && (argnr<999))	/* add pages to options */
    {
      list_item = GTK_OBJECT(list->data);
      page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
      type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
      xsane_convert_text_to_filename(&page);
      snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.fax_project, page, type);
      snprintf(fax_filename, sizeof(fax_filename), "%s/%s-fax%s", preferences.fax_project, page, fax_type);
      if (xsane_create_secure_file(fax_filename)) /* remove possibly existing symbolic links for security */
      {
       char buf[TEXTBUFSIZE];

        snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, fax_filename);
        xsane_back_gtk_error(buf, TRUE);
       return; /* error */
      }

      if (!strncmp(type, ".pnm", 4))
      {
        DBG(DBG_info, "converting %s to %s\n", source_filename, fax_filename);
        xsane_fax_convert_pnm_to_ps(source_filename, fax_filename);
      }
      else if (!strncmp(type, ".ps", 3))
      {
       int cancel_save = 0;
        xsane_copy_file_by_name(fax_filename, source_filename, xsane.project_progress_bar, &cancel_save);
      }
      arg[argnr++] = strdup(fax_filename);
      list = list->next;
      free(page);
      free(type);
    }

    arg[argnr] = 0;

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

      execvp(arg[0], arg); /* does not return if successfully */
      DBG(DBG_error, "%s %s\n", ERR_FAILED_EXEC_FAX_CMD, preferences.fax_command);

      /* send error message via IPC pipe to parent process */
      if (ipc_file)
      {
        fprintf(ipc_file, "%s %s:\n%s", ERR_FAILED_EXEC_FAX_CMD, preferences.fax_command, strerror(errno));
        fflush(ipc_file); /* make sure message is displayed */
        fclose(ipc_file);
      }

      _exit(0); /* do not use exit() here! otherwise gtk gets in trouble */
    }
    else /* parent process */
    {
      xsane_front_gtk_add_process_to_list(pid); /* add pid to child process list */
    }

    for (i=0; i<argnr; i++)
    {
      free(arg[i]);
    }

    if (xsane.fax_status)
    {
      free(xsane.fax_status);
    }
    xsane.fax_status = strdup(TEXT_FAX_STATUS_QUEUEING_FAX);
    xsane_fax_project_update_project_status();
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

    while (pid)
    {
     int status = 0;
     pid_t pid_status = waitpid(pid, &status, WNOHANG);
  
      if ( (pid_status < 0 ) || (pid == pid_status) )
      {
        pid = 0; /* ok, child process has terminated */
      }

      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
    }

    /* delete created fax files */
    list = (GList *) GTK_LIST(xsane.project_list)->children;
    while (list)
    {
      list_item = GTK_OBJECT(list->data);
      page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
      xsane_convert_text_to_filename(&page);
      snprintf(fax_filename, sizeof(fax_filename), "%s/%s-fax%s", preferences.fax_project, page, fax_type);
      free(page);

      DBG(DBG_info, "removing %s\n", fax_filename);
      remove(fax_filename);

      list = list->next;
    }

    xsane.fax_status = strdup(TEXT_FAX_STATUS_FAX_QUEUED);
    xsane_fax_project_update_project_status();
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.fax_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

    xsane_set_sensitivity(TRUE);

    /* gtk_widget_set_sensitive(xsane.project_dialog, TRUE); */
  }

  DBG(DBG_info, "xsane_fax_send: done\n");
}

/* ---------------------------------------------------------------------------------------------------------------------- */
