/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-fixedtext.h

   Oliver Rauch <Oliver.Rauch@Wolfsburg.DE>
   Copyright (C) 1998-2000 Oliver Rauch
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

/* ------------------------------------------------------------------------ */

#ifndef XSANE_FIXEDTEXT_H
#define XSANE_FIXEDTEXT_H

#define XSANE_STRSTATUS(status)		_(sane_strstatus(status))
#define _BGT(text)			dgettext(xsane.backend, text)

#define MENU_ITEM_FILETYPE_JPEG		_(".jpeg")
#define MENU_ITEM_FILETYPE_PNG		_(".png")
#define MENU_ITEM_FILETYPE_PNM		_(".pnm")
#define MENU_ITEM_FILETYPE_PS		_(".ps")
#define MENU_ITEM_FILETYPE_RAW		_(".raw")
#define MENU_ITEM_FILETYPE_TIFF		_(".tiff")


#endif
