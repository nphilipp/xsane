/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-scan.h

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

#include <sane/sane.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_scan_done(SANE_Status status);
extern void xsane_cancel(void);
extern gint xsane_scan_dialog(gpointer *data);

/* ---------------------------------------------------------------------------------------------------------------------- */
