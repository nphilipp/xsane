/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-fax-project.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2007 Oliver Rauch
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

#ifndef HAVE_XSANE_FAX_PROJECT_H
#define HAVE_XSANE_FAX_PROJECT_H

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

extern void xsane_fax_dialog(void);
extern void xsane_fax_project_save(void);

#if 0
static gint xsane_fax_dialog_delete();
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
#endif

#endif
