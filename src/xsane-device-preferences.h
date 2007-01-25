/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-device-preferences.h

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

#ifndef xsane_device_preferences_h
#define xsane_device_preferences_h

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <sane/sane.h>
#include "xsane.h"

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_device_preferences_store(void);
extern void xsane_device_preferences_save(void);
extern void xsane_device_preferences_save_file(char *filename);
extern void xsane_device_preferences_restore(void);
extern void xsane_device_preferences_load(void);
extern void xsane_device_preferences_load_file(char *filename);

/* ---------------------------------------------------------------------------------------------------------------------- */

#endif
