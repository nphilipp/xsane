/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend
 
   xsane-gtk-1_x-compat.h
 
   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 2002-2007 Oliver Rauch
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
 
#ifndef XSANE_GTK_1_x_COMPAT_H
#define XSANE_GTK_1_x_COMPAT_H  

# define DEF_GTK_MENU_ACCEL_VISIBLE 0
 
# define g_signal_connect(instance, detailed_signal, c_handler, data) \
         gtk_signal_connect(instance, detailed_signal, c_handler, data)
# define g_signal_connect_after(instance, detailed_signal, c_handler, data) \
         gtk_signal_connect_after(instance, detailed_signal, c_handler, data)
# define g_signal_connect_swapped(instance, detailed_signal, c_handler, data) \
         gtk_signal_connect_object(instance, detailed_signal, c_handler, data)
# define g_signal_handlers_block_by_func(instance, func, data) \
         gtk_signal_handler_block_by_func(instance, func, data)
# define g_signal_handlers_unblock_by_func(instance, func, data) \
         gtk_signal_handler_unblock_by_func(instance, func, data)
# define g_signal_handlers_disconnect_by_func(instance, func, data) \
         gtk_signal_disconnect_by_func(instance, func, data)
# define g_signal_lookup(name, itype) \
         gtk_signal_lookup(name, itype)
# define g_signal_emit(instance, signal_id, detail) \
         gtk_signal_emit(instance, signal_id)
# define g_signal_emit_by_name(instance, detailed_signal) \
         gtk_signal_emit_by_name(instance, detailed_signal)
 
# define gtk_widget_set_size_request(widget, width, height) \
         gtk_widget_set_usize(widget, width, height)
 
# define gdk_drawable_get_size(widget, width, height) \
         gdk_window_get_size(widget, width, height)
# define gdk_drawable_get_colormap(widget) \
         gdk_window_get_colormap(widget)
# define gdk_cursor_unref(widget) \
         gdk_cursor_destroy(widget)
# define gdk_gc_unref(widget) \
         gdk_gc_destroy(widget)
/* the following change is not unique, but we only need gdk_pixmap_unref, so it is ok */
# define gdk_drawable_unref(widget) \
         gdk_pixmap_unref(widget)
# define gtk_window_move(widget, x, y) \
         gtk_widget_set_uposition((GtkWidget *) widget, x, y)
# define gtk_window_set_resizable(widget, resizable) \
         gtk_window_set_policy(widget, FALSE, resizable, FALSE)
# define gtk_image_new_from_pixmap(pixmap, mask) \
         gtk_pixmap_new(pixmap, mask)
# define gtk_progress_bar_set_ellipsize(pbar, mode) /* empty */
#endif 
