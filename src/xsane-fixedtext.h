/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-fixedtext.h

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

/* ------------------------------------------------------------------------ */

#ifndef XSANE_FIXEDTEXT_H
#define XSANE_FIXEDTEXT_H

#ifdef _WIN32
# define XSANE_FILETYPE_JPEG		".jpg"
# define XSANE_FILETYPE_TIFF		".tif"
#else
# define XSANE_FILETYPE_JPEG		".jpeg"
# define XSANE_FILETYPE_TIFF		".tiff"
#endif

#define XSANE_FILETYPE_BY_EXT		""
#define XSANE_FILETYPE_PNG		".png"
#define XSANE_FILETYPE_PNM		".pnm"
#define XSANE_FILETYPE_PS		".ps"
#define XSANE_FILETYPE_PDF		".pdf"
#define XSANE_FILETYPE_RGBA		".rgba"
#define XSANE_FILETYPE_TEXT		".txt"

#define MENU_ITEM_FILETYPE_JPEG		"JPEG"
#define MENU_ITEM_FILETYPE_TIFF		"TIFF"
#define MENU_ITEM_FILETYPE_PNG		"PNG"
#define MENU_ITEM_FILETYPE_PNM		"PNM"
#define MENU_ITEM_FILETYPE_PS		"PostScript"
#define MENU_ITEM_FILETYPE_PDF		"PDF"
#define MENU_ITEM_FILETYPE_RGBA		"RGBA"
#define MENU_ITEM_FILETYPE_TEXT		"TEXT"

#endif
