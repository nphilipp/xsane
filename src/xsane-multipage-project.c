/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-multipage-project.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 2005-2010 Oliver Rauch
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


#include "stdio.h"
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

#ifdef HAVE_LIBTIFF
# include "tiffio.h"
#endif

#include <sys/wait.h>

/* ---------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------- */

/* forward declarations: */


void xsane_multipage_dialog(void);
void xsane_multipage_dialog_close(void);
void xsane_multipage_project_save(void);
static gint xsane_multipage_dialog_delete();
static void xsane_multipage_filetype_callback(GtkWidget *filetype_option_menu, char *filetype);
static void xsane_multipage_project_browse_filename_callback(GtkWidget *widget, gpointer data);
static void xsane_multipage_project_changed_callback(GtkWidget *widget, gpointer data);
static void xsane_multipage_project_load(void);
static void xsane_multipage_project_delete(void);
static void xsane_multipage_project_create(void);
static void xsane_multipage_entry_move_up_callback(GtkWidget *widget, gpointer list);
static void xsane_multipage_entry_move_down_callback(GtkWidget *widget, gpointer list);
static void xsane_multipage_entry_delete_callback(GtkWidget *widget, gpointer list);
static void xsane_multipage_show_callback(GtkWidget *widget, gpointer data);
static void xsane_multipage_edit_callback(GtkWidget *widget, gpointer data);
static void xsane_multipage_save_file(void);

/* ---------------------------------------------------------------------------------------------------------------------- */

static gint xsane_multipage_dialog_delete()
{
 return TRUE;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_filetype_callback(GtkWidget *filetype_option_menu, char *filetype)
{
  DBG(DBG_proc, "xsane_multipage_filetype_callback(%s)\n", filetype);

  if (preferences.multipage_filetype)
  {
    free(preferences.multipage_filetype);
  }
  preferences.multipage_filetype = strdup(filetype);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_multipage_dialog()
{
 GtkWidget *multipage_dialog, *multipage_scan_vbox, *multipage_project_vbox;
 GtkWidget *multipage_project_exists_hbox, *button;
 GtkWidget *hbox;
 GtkWidget *scrolled_window, *list;
 GtkWidget *text;
 GtkWidget *pages_frame;
 GtkWidget *label;
 GtkWidget *filetype_menu, *filetype_item;
 GtkWidget *filetype_option_menu;
 char buf[64];
 int filetype_nr;
 int select_item;

  DBG(DBG_proc, "xsane_multipage_dialog\n");

  if (xsane.project_dialog) 
  {
    return; /* window already is open */
  }

  /* GTK_WINDOW_TOPLEVEL looks better but does not place it nice*/
  multipage_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  snprintf(buf, sizeof(buf), "%s %s", xsane.prog_name, WINDOW_MULTIPAGE_PROJECT);
  gtk_window_set_title(GTK_WINDOW(multipage_dialog), buf);
  g_signal_connect(GTK_OBJECT(multipage_dialog), "delete_event", (GtkSignalFunc) xsane_multipage_dialog_delete, NULL);
  xsane_set_window_icon(multipage_dialog, 0);
  gtk_window_add_accel_group(GTK_WINDOW(multipage_dialog), xsane.accelerator_group); 

  /* set the main vbox */
  multipage_scan_vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(multipage_scan_vbox), 0);
  gtk_container_add(GTK_CONTAINER(multipage_dialog), multipage_scan_vbox);
  gtk_widget_show(multipage_scan_vbox);


  /* multipage project */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(multipage_scan_vbox), hbox, FALSE, FALSE, 1);

  button = xsane_button_new_with_pixmap(xsane.dialog->window, hbox, multipage_xpm, DESC_MULTIPAGE_PROJECT_BROWSE,
                                                    (GtkSignalFunc) xsane_multipage_project_browse_filename_callback, NULL);

  text = gtk_entry_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, text, DESC_MULTIPAGE_PROJECT);
  gtk_entry_set_max_length(GTK_ENTRY(text), 128);
  gtk_entry_set_text(GTK_ENTRY(text), (char *) preferences.multipage_project);
  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
  g_signal_connect(GTK_OBJECT(text), "changed", (GtkSignalFunc) xsane_multipage_project_changed_callback, NULL);

  xsane.project_entry = text;
  xsane.project_entry_box = hbox;

  gtk_widget_show(text);
  gtk_widget_show(hbox);

  multipage_project_vbox = gtk_vbox_new(/* homogeneous */ FALSE, 0);
  gtk_box_pack_start(GTK_BOX(multipage_scan_vbox), multipage_project_vbox, TRUE, TRUE, 0);
  gtk_widget_show(multipage_project_vbox);


  /* FILETYPE MENU */
  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(multipage_project_vbox), hbox, FALSE, FALSE, 1);
  gtk_widget_show(hbox);

  filetype_menu = gtk_menu_new();

  filetype_nr = -1;
  select_item = 0;


  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PDF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_multipage_filetype_callback, (void *) XSANE_FILETYPE_PDF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.multipage_filetype) && (!strcasecmp(preferences.multipage_filetype, XSANE_FILETYPE_PDF)) )
  {
    select_item = filetype_nr;
  }


  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_PS);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_multipage_filetype_callback, (void *) XSANE_FILETYPE_PS);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.multipage_filetype) && (!strcasecmp(preferences.multipage_filetype, XSANE_FILETYPE_PS)) )
  {
    select_item = filetype_nr;
  }


#ifdef HAVE_LIBTIFF
  filetype_item = gtk_menu_item_new_with_label(MENU_ITEM_FILETYPE_TIFF);
  gtk_container_add(GTK_CONTAINER(filetype_menu), filetype_item);
  g_signal_connect(GTK_OBJECT(filetype_item), "activate", (GtkSignalFunc) xsane_multipage_filetype_callback, (void *) XSANE_FILETYPE_TIFF);
  gtk_widget_show(filetype_item);
  filetype_nr++;
  if ( (preferences.multipage_filetype) && (!strcasecmp(preferences.multipage_filetype, XSANE_FILETYPE_TIFF)) )
  {
    select_item = filetype_nr;
  }
#endif
                                                                                                              
  label = gtk_label_new(TEXT_MULTIPAGE_FILETYPE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  filetype_option_menu = gtk_option_menu_new();
  xsane_back_gtk_set_tooltip(xsane.tooltips, filetype_option_menu, DESC_MULTIPAGE_FILETYPE);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(filetype_option_menu), filetype_menu);
  if (select_item >= 0)
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(filetype_option_menu), select_item);
  }
  gtk_box_pack_end(GTK_BOX(hbox), filetype_option_menu, FALSE, FALSE, 2);
  gtk_widget_show(filetype_menu);
  gtk_widget_show(filetype_option_menu);


  /* pages frame */
  pages_frame = gtk_frame_new(TEXT_PAGES);
  gtk_box_pack_start(GTK_BOX(multipage_project_vbox), pages_frame, TRUE, TRUE, 2);
  gtk_widget_show(pages_frame);

  /* pages list */
  scrolled_window = gtk_scrolled_window_new(0, 0);
  gtk_widget_set_size_request(scrolled_window, 200, 100);
  gtk_container_add(GTK_CONTAINER(pages_frame), scrolled_window);
  gtk_widget_show(scrolled_window);

  list = gtk_list_new();
/*  gtk_list_set_selection_mode(list, GTK_SELECTION_BROWSE); */

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
  gtk_widget_show(list);
  xsane.project_list = list;


  /* button box, active when project exists */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
  gtk_box_pack_start(GTK_BOX(multipage_project_vbox), hbox, FALSE, FALSE, 1);

  button = gtk_button_new_with_label(BUTTON_IMAGE_SHOW);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_show_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_IMAGE_EDIT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_edit_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_IMAGE_DELETE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_entry_delete_callback, list);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  xsane_button_new_with_pixmap(multipage_dialog->window, hbox, move_up_xpm,   0, (GtkSignalFunc) xsane_multipage_entry_move_up_callback,   list);
  xsane_button_new_with_pixmap(multipage_dialog->window, hbox, move_down_xpm, 0, (GtkSignalFunc) xsane_multipage_entry_move_down_callback, list);

  gtk_widget_show(hbox);

  xsane.project_box = multipage_project_vbox;

  /* set the main hbox */
  hbox = gtk_hbox_new(FALSE, 0);
  xsane_separator_new(multipage_scan_vbox, 2);
  gtk_box_pack_end(GTK_BOX(multipage_scan_vbox), hbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
  gtk_widget_show(hbox);     


  multipage_project_exists_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), multipage_project_exists_hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label(BUTTON_SAVE_MULTIPAGE);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_save_file, NULL);
  gtk_box_pack_start(GTK_BOX(multipage_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label(BUTTON_DELETE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_project_delete, NULL);
  gtk_box_pack_start(GTK_BOX(multipage_project_exists_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(multipage_project_exists_hbox);
  xsane.project_exists = multipage_project_exists_hbox;

  button = gtk_button_new_with_label(BUTTON_CREATE_PROJECT);
  g_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) xsane_multipage_project_create, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  xsane.project_not_exists = button;

  /* progress bar */
  xsane.project_progress_bar = (GtkProgressBar *) gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(multipage_scan_vbox), (GtkWidget *) xsane.project_progress_bar, FALSE, FALSE, 0);
  gtk_progress_set_show_text(GTK_PROGRESS(xsane.project_progress_bar), TRUE);
  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
  gtk_widget_show(GTK_WIDGET(xsane.project_progress_bar));


  xsane.project_dialog = multipage_dialog;

  xsane_multipage_project_load();

  gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
  gtk_widget_show(multipage_dialog);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_project_set_sensitive(int sensitive)
{
  gtk_widget_set_sensitive(xsane.project_box, sensitive);
  gtk_widget_set_sensitive(xsane.project_exists, sensitive);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_project_load()
{
 FILE *projectfile;
 char page[TEXTBUFSIZE];
 char filename[PATH_MAX];
 GtkWidget *list_item;
 int i;
 int c;

  DBG(DBG_proc, "xsane_multipage_project_load\n");

  if (xsane.multipage_status)
  {
    free(xsane.multipage_status);
    xsane.multipage_status = NULL;
  }

  if (xsane.multipage_filename)
  {
    free(xsane.multipage_filename);
    xsane.multipage_filename = NULL;
  }

  gtk_list_remove_items(GTK_LIST(xsane.project_list), GTK_LIST(xsane.project_list)->children);

  snprintf(filename, sizeof(filename), "%s/xsane-multipage-list", preferences.multipage_project);
  projectfile = fopen(filename, "rb"); /* read binary (b for win32) */

  if ((!projectfile) || (feof(projectfile)))
  {
    snprintf(filename, sizeof(filename), "%s/image-1.pnm", preferences.multipage_project);
    xsane.multipage_filename=strdup(filename);
    xsane_update_counter_in_filename(&xsane.multipage_filename, FALSE, 0, preferences.filename_counter_len); /* correct counter len */

    xsane.multipage_status=strdup(TEXT_PROJECT_STATUS_NOT_CREATED);

    gtk_widget_hide(xsane.project_exists);
    gtk_widget_show(xsane.project_not_exists);

    gtk_widget_set_sensitive(xsane.project_box, FALSE);
    gtk_widget_set_sensitive(xsane.project_exists, FALSE);
    /* do not change sensitivity of multipage_project_entry_box here !!! */
    gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
  }
  else
  {
    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* first line is multipage status */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;
    if (strchr(page, '@'))
    {
      *strchr(page, '@') = 0;
    }

    if (xsane.multipage_status)
    {
      free(xsane.multipage_status);
    }
    xsane.multipage_status = strdup(page);


    i=0;
    c=0;
    while ((i<255) && (c != 10) && (c != EOF)) /* second line is next multipage filename */
    {
      c = fgetc(projectfile);
      page[i++] = c;
    }
    page[i-1] = 0;

    snprintf(filename, sizeof(filename), "%s/%s", preferences.multipage_project, page);
    xsane.multipage_filename=strdup(filename);

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

    if (!strcmp(xsane.multipage_status, TEXT_PROJECT_STATUS_FILE_SAVING)) 
    {
      xsane_multipage_project_set_sensitive(FALSE);
      gtk_widget_set_sensitive(xsane.project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), FALSE); 
    }
    else
    {
      xsane_multipage_project_set_sensitive(TRUE);
      gtk_widget_set_sensitive(xsane.project_entry_box, TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(xsane.start_button), TRUE); 
    }

    gtk_widget_show(xsane.project_exists);
    gtk_widget_hide(xsane.project_not_exists);
  }

  if (projectfile)
  {
    fclose(projectfile);
  }

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_project_delete()
{
 char *page;
 char *type;
 char file[PATH_MAX];
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;

  DBG(DBG_proc, "xsane_multipage_project_delete\n");

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.multipage_project, page, type);
    free(page);
    free(type);
    remove(file);
    list = list->next;
  }
  snprintf(file, sizeof(file), "%s/xsane-multipage-list", preferences.multipage_project);
  remove(file);
  snprintf(file, sizeof(file), "%s", preferences.multipage_project);
  rmdir(file);

  xsane_multipage_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_multipage_project_save()
{
 FILE *projectfile;
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_multipage_project_save\n");

  umask((mode_t) preferences.directory_umask); /* define new file permissions */    
  mkdir(preferences.multipage_project, 0777); /* make sure directory exists */

  snprintf(filename, sizeof(filename), "%s/xsane-multipage-list", preferences.multipage_project);

  if (xsane_create_secure_file(filename)) /* remove possibly existing symbolic links for security */
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, filename);
    xsane_back_gtk_error(buf, TRUE);
   return; /* error */
  }

  projectfile = fopen(filename, "wb"); /* write binary (b for win32) */

  if (xsane.multipage_status)
  {
   char buf[TEXTBUFSIZE];

    snprintf(buf, 32, "%s@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", xsane.multipage_status); /* fill 32 characters status line */
    fprintf(projectfile, "%s\n", buf); /* first line is status of multipage */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }
  else
  {
    fprintf(projectfile, "                                \n"); /* no multipage status */
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), "");
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
  }

  if (xsane.multipage_filename)
  {
    fprintf(projectfile, "%s\n", strrchr(xsane.multipage_filename, '/')+1); /* third line is next multipage filename */
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

static void xsane_multipage_project_create()
{
  DBG(DBG_proc, "xsane_multipage_project_create\n");

  if (strlen(preferences.multipage_project))
  {
    if (xsane.multipage_status)
    {
      free(xsane.multipage_status);
    }
    xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CREATED);
    xsane_multipage_project_save();
    xsane_multipage_project_load();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_multipage_project_set_filename(gchar *filename)
{
  g_signal_handlers_block_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_multipage_project_changed_callback, NULL);
  gtk_entry_set_text(GTK_ENTRY(xsane.project_entry), (char *) filename); /* update filename in entry */
  gtk_entry_set_position(GTK_ENTRY(xsane.project_entry), strlen(filename)); /* set cursor to right position of filename */

  g_signal_handlers_unblock_by_func(GTK_OBJECT(xsane.project_entry), (GtkSignalFunc) xsane_multipage_project_changed_callback, NULL);
}

/* ----------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_project_browse_filename_callback(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_multipage_project_browse_filename_callback\n");

  xsane_set_sensitivity(FALSE);

  if (preferences.multipage_project) /* make sure a correct filename is defined */
  {
    strncpy(filename, preferences.multipage_project, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
  }
  else /* no filename given, take standard filename */
  {
    strcpy(filename, OUT_FILENAME);
  }

  snprintf(windowname, sizeof(windowname), "%s %s %s", xsane.prog_name, WINDOW_MULTIPAGE_PROJECT_BROWSE, xsane.device_text);

  umask((mode_t) preferences.directory_umask); /* define new file permissions */
  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SELECT_PROJECT, XSANE_GET_FILENAME_SHOW_NOTHING, 0, 0))
  {

    if (preferences.multipage_project)
    {
      free((void *) preferences.multipage_project);
    }

    preferences.multipage_project = strdup(filename);

    xsane_set_sensitivity(TRUE);
    xsane_multipage_project_set_filename(filename);

    xsane_multipage_project_load();
  }
  else
  {
    xsane_set_sensitivity(TRUE);
  }
  umask(XSANE_DEFAULT_UMASK); /* define new file permissions */

}
                                                  

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_project_changed_callback(GtkWidget *widget, gpointer data)
{
  DBG(DBG_proc, "xsane_multipage_project_changed_callback\n");

  if (preferences.multipage_project)
  {
    free((void *) preferences.multipage_project);
  }
  preferences.multipage_project = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  xsane_multipage_project_load();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_entry_move_up_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_multipage_entry_move_up\n");

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

        if (xsane.multipage_status)
        {
          free(xsane.multipage_status);
        }
        xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
        xsane_multipage_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_entry_move_down_callback(GtkWidget *widget, gpointer list)
{
 GList *select;
 GList *item = GTK_LIST(list)->children;
 GtkWidget *list_item_1;
 GtkWidget *list_item_2;
 int position;
 int newpos;

  DBG(DBG_proc, "xsane_multipage_entry_move_down\n");

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

        if (xsane.multipage_status)
        {
          free(xsane.multipage_status);
        }
        xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
        xsane_multipage_project_save();
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_entry_delete_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char file[PATH_MAX];

  DBG(DBG_proc, "xsane_multipage_entry_delete_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(file, sizeof(file), "%s/%s%s", preferences.multipage_project, page, type);
    free(page);
    free(type);
    remove(file);
    gtk_widget_destroy(GTK_WIDGET(list_item));

    if (xsane.multipage_status)
    {
      free(xsane.multipage_status);
    }
    xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
    xsane_multipage_project_save();
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_show_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_multipage_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.multipage_project, page, type);
    free(page);
    free(type);

    xsane_viewer_new(filename, NULL, FALSE, filename, VIEWER_NO_MODIFICATION, IMAGE_SAVED);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_edit_callback(GtkWidget *widget, gpointer list)
{
 GtkObject *list_item;
 GList *select;
 char *page;
 char *type;
 char filename[PATH_MAX];
 char outfilename[PATH_MAX];
 int cancel_save = 0;

  DBG(DBG_proc, "xsane_multipage_entry_show_callback\n");

  select = GTK_LIST(list)->selection;
  if (select)
  {
    list_item = GTK_OBJECT(select->data);
    page = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&page);
    snprintf(filename, sizeof(filename), "%s/%s%s", preferences.multipage_project, page, type);
    free(page);
    free(type);

    xsane_back_gtk_make_path(sizeof(outfilename), outfilename, 0, 0, "xsane-viewer-", xsane.dev_name, ".pnm", XSANE_PATH_TMP);
    xsane_copy_file_by_name(outfilename, filename, xsane.project_progress_bar, &cancel_save);

    xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_CHANGED);
    xsane_multipage_project_save();

    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

    xsane_viewer_new(outfilename, NULL, FALSE, filename, VIEWER_NO_NAME_MODIFICATION, IMAGE_SAVED);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

static void xsane_multipage_save_file()
{
 char *image;
 char *type;
 GList *list = (GList *) GTK_LIST(xsane.project_list)->children;
 GtkObject *list_item;
 char source_filename[PATH_MAX];
 char multipage_filename[PATH_MAX];
 int output_format;
 int cancel_save = 0;
 int page, pages = 0;
 FILE *outfile = NULL, *imagefile = NULL;
#ifdef HAVE_LIBTIFF
 TIFF *tiffile = NULL;
#endif
 Image_info image_info;
 long int source_size = 0;
 float imagewidth, imageheight;
 char buf[TEXTBUFSIZE];
 struct pdf_xref xref;
 int remove_lineart_file = FALSE;

  DBG(DBG_proc, "xsane_multipage_save_file\n");

  xsane_set_sensitivity(FALSE); /* do not allow changing xsane mode */
  xsane_multipage_project_set_sensitive(FALSE);

  while (gtk_events_pending())
  {
    DBG(DBG_info, "calling gtk_main_iteration\n");
    gtk_main_iteration();
  }

  if (xsane.multipage_status)
  {
    free(xsane.multipage_status);
  }
  xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVING);

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);


  snprintf(multipage_filename, sizeof(multipage_filename), "%s%s", preferences.multipage_project, preferences.multipage_filetype);
  output_format = xsane_identify_output_format(multipage_filename, NULL, NULL);

  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    image = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type  = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&image);
    snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.multipage_project, image, type);
    free(image);
    free(type);
    list = list->next;
    pages++;
    source_size += xsane_get_filesize(source_filename);
  }


  if ( (preferences.overwrite_warning) ) /* test if filename already used */
  {
   FILE *testfile;
                                                                                                                             
    testfile = fopen(multipage_filename, "rb"); /* read binary (b for win32) */
    if (testfile) /* filename used: skip */
    {
     char buf[TEXTBUFSIZE];
                                                                                                                             
      fclose(testfile);

      snprintf(buf, sizeof(buf), WARN_FILE_EXISTS, multipage_filename);
      if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) warning_xpm, buf, BUTTON_OVERWRITE, BUTTON_CANCEL, TRUE /* wait */) == FALSE)
      {
        xsane_set_sensitivity(TRUE);
        xsane_multipage_project_set_sensitive(TRUE);
        if (xsane.multipage_status)
        {
          free(xsane.multipage_status);
        }
        xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVING_ABORTED);

        gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
        xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
       return;
      }
    }
  }


  if (xsane_create_secure_file(multipage_filename)) /* remove possibly existing symbolic links for security */
  {
    snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, multipage_filename);
    xsane_back_gtk_error(buf, TRUE);
    xsane_multipage_project_set_sensitive(TRUE);
    xsane_set_sensitivity(TRUE); /* allow changing xsane mode */
   return;
  }

  DBG(DBG_info, "xsane_multipage_save_file: created %s\n", multipage_filename);


  if ((output_format == XSANE_PS) || (output_format == XSANE_PDF))
  {
    outfile = fopen(multipage_filename, "wb"); /* b = binary mode for win32 */
    if (!outfile)
    {
      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_CREATE_SECURE_FILE, multipage_filename);
      xsane_back_gtk_error(buf, TRUE);
      xsane_multipage_project_set_sensitive(TRUE);
      xsane_set_sensitivity(TRUE); /* allow changing xsane mode */

      if (xsane.multipage_status)
      {
        free(xsane.multipage_status);
      }
      xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVING_ERROR);

      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
     return;
    }

    if (output_format == XSANE_PS)
    {
      xsane_save_ps_create_document_header(outfile, pages, 0, 0, 72.0*9, 72.0*12, 0 /* portrait top left */, preferences.save_ps_flatedecoded);
    }
    else if (output_format == XSANE_PDF)
    {
      xsane_save_pdf_create_document_header(outfile, &xref, pages, preferences.save_pdf_flatedecoded);
    }
  }
#ifdef HAVE_LIBTIFF
  else if (output_format == XSANE_TIFF)
  {
    tiffile = TIFFOpen(multipage_filename, "w");
    if (!tiffile)
    {
      snprintf(buf, sizeof(buf), "%s %s %s\n", ERR_DURING_SAVE, ERR_OPEN_FAILED, multipage_filename);
      xsane_back_gtk_error(buf, TRUE);
      xsane_multipage_project_set_sensitive(TRUE);
      xsane_set_sensitivity(TRUE); /* allow changing xsane mode */

      if (xsane.multipage_status)
      {
        free(xsane.multipage_status);
      }
      xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVING_ERROR);

      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
     return;
    }
  }
#endif
  else
  {
    snprintf(buf, sizeof(buf), "%s unsupported multipage fileformat\n", ERR_DURING_SAVE);
    xsane_back_gtk_error(buf, TRUE);
    xsane_multipage_project_set_sensitive(TRUE);
    xsane_set_sensitivity(TRUE); /* allow changing xsane mode */

    if (xsane.multipage_status)
    {
      free(xsane.multipage_status);
    }
    xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVING_ERROR);

    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
    xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
   return;
  }


  list = (GList *) GTK_LIST(xsane.project_list)->children;

  page = 1;
  while (list)
  {
    list_item = GTK_OBJECT(list->data);
    image = strdup((char *) gtk_object_get_data(list_item, "list_item_data"));
    type  = strdup((char *) gtk_object_get_data(list_item, "list_item_type"));
    xsane_convert_text_to_filename(&image);
    snprintf(source_filename, sizeof(source_filename), "%s/%s%s", preferences.multipage_project, image, type);

    imagefile = fopen(source_filename, "rb"); /* read binary (b for win32) */
    if (!imagefile)
    {
      DBG(DBG_error, "could not read imagefile %s\n", source_filename);
     return;
    }

    xsane_read_pnm_header(imagefile, &image_info);

    /* reduce lineart images to lineart before conversion */
    if (image_info.reduce_to_lineart)
    {
     char lineart_filename[PATH_MAX];

      DBG(DBG_info, "original image is a lineart => reduce to lineart\n");
      fclose(imagefile);
      xsane_back_gtk_make_path(sizeof(lineart_filename), lineart_filename, 0, 0, "xsane-conversion-", xsane.dev_name, ".pbm", XSANE_PATH_TMP);

      snprintf(buf, sizeof(buf), "%s", PROGRESS_PACKING_DATA);
                                                                                                                         
      gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), buf);
      xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);
                                                                                                                         
      while (gtk_events_pending())
      {
        gtk_main_iteration();
      }
                                                                                                                         
      xsane_save_image_as_lineart(lineart_filename, source_filename, xsane.project_progress_bar, &cancel_save);
                                                                                                                         
      strncpy(source_filename, lineart_filename, sizeof(source_filename));
      remove_lineart_file = TRUE;
                                                                                                                         
      imagefile = fopen(source_filename, "rb"); /* read binary (b for win32) */
      if (imagefile == 0)
      {
       char buf[TEXTBUFSIZE];
        snprintf(buf, sizeof(buf), "%s `%s': %s", ERR_OPEN_FAILED, source_filename, strerror(errno));
        xsane_back_gtk_error(buf, TRUE);
                                                                                                                          
       return;
      }
                                                                                                                         
      xsane_read_pnm_header(imagefile, &image_info);
    }


    snprintf(buf, sizeof(buf), "%s %s %d/%d", _(xsane.multipage_status), PROGRESS_PAGE, page, pages);
    gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), buf);


    if (output_format == XSANE_PS)
    {
      imagewidth  = 72.0 * image_info.image_width/image_info.resolution_x; /* width in 1/72 inch */
      imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

      xsane_save_ps_page(outfile, page,
                         imagefile, &image_info, imagewidth, imageheight,
                         0, 0, imagewidth, imageheight, 0 /* portrait top left */,
                         preferences.save_ps_flatedecoded,
                         NULL /* hTransform */, 0 /* embed_scanner_icm_profile */, 0 /* embed CSA */, NULL, /* CSA profile */ 0 /* intent */,
                         xsane.project_progress_bar, &cancel_save);
    }
    else if (output_format == XSANE_PDF)
    {
      imagewidth  = 72.0 * image_info.image_width/image_info.resolution_x; /* width in 1/72 inch */
      imageheight = 72.0 * image_info.image_height/image_info.resolution_y; /* height in 1/72 inch */

      xsane_save_pdf_page(outfile, &xref, page,
                         imagefile, &image_info, imagewidth, imageheight,
                         0, 0, imagewidth, imageheight, 0 /* portrait top left */,
                         preferences.save_pdf_flatedecoded,
                         NULL /* hTransform */, 0 /* embed_scanner_icm_profile */, 0 /* icc_object */,
                         xsane.project_progress_bar, &cancel_save);
    }
#ifdef HAVE_LIBTIFF
    else if (output_format == XSANE_TIFF)
    {
     cmsHTRANSFORM hTransform = NULL;

#ifdef HAVE_LIBLCMS
      if ( (preferences.cms_function != XSANE_CMS_FUNCTION_EMBED_SCANNER_ICM_PROFILE)  && xsane.enable_color_management )
      {
        hTransform = xsane_create_cms_transform(&image_info, preferences.cms_function, preferences.cms_intent, preferences.cms_bpc);
      }
#endif

      xsane_save_tiff_page(tiffile, page, pages, preferences.jpeg_quality, imagefile, &image_info,
                           hTransform, xsane.enable_color_management, preferences.cms_function, 
                           xsane.project_progress_bar, &cancel_save);
#ifdef HAVE_LIBLCMS
      if (hTransform != NULL)
      {
        cmsDeleteTransform(hTransform);
      }
#endif
    }
#endif

    if (remove_lineart_file)
    {
      remove(source_filename); /* remove lineart pbm file  */
    }

    free(image);
    free(type);
    list = list->next;

    page++;
  }

  if (output_format == XSANE_PS)
  {
    xsane_save_ps_create_document_trailer(outfile, 0 /* we defined pages at beginning */);
                                                                                                                                
    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];
                                                                                                                                
      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      cancel_save = 1;
    } 

    fclose(outfile);
  }
  else if (output_format == XSANE_PDF)
  {
    xsane_save_pdf_create_document_trailer(outfile, &xref, pages);
                                                                                                                                
    if (ferror(outfile))
    {
     char buf[TEXTBUFSIZE];
                                                                                                                                
      snprintf(buf, sizeof(buf), "%s %s", ERR_DURING_SAVE, strerror(errno));
      DBG(DBG_error, "%s\n", buf);
      xsane_back_gtk_decision(ERR_HEADER_ERROR, (gchar **) error_xpm, buf, BUTTON_OK, NULL, TRUE /* wait */);
      cancel_save = 1;
    }

    fclose(outfile);
  }
#ifdef HAVE_LIBTIFF
  else if (output_format == XSANE_TIFF)
  {
    TIFFClose(tiffile);
  }
#endif

  if (xsane.multipage_status)
  {
    free(xsane.multipage_status);
    xsane.multipage_status = NULL;
  }

  if (cancel_save)
  {
    xsane.multipage_status = strdup(ERR_DURING_SAVE);
  }
  else
  {
    xsane.multipage_status = strdup(TEXT_PROJECT_STATUS_FILE_SAVED);
  }
  xsane_multipage_project_save();

  gtk_progress_set_format_string(GTK_PROGRESS(xsane.project_progress_bar), _(xsane.multipage_status));
  xsane_progress_bar_set_fraction(GTK_PROGRESS_BAR(xsane.project_progress_bar), 0.0);

  xsane_multipage_project_set_sensitive(TRUE);
  xsane_set_sensitivity(TRUE); /* allow changing xsane mode */
}


