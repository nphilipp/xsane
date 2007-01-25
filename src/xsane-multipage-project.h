/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-multipage-project.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 2005-2007 Oliver Rauch
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

#ifndef HAVE_XSANE_MULTIPAGE_PROJECT_H
#define HAVE_XSANE_MULTIPAGE_PROJECT_H


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
# ifdef HAVE_LIBZ
#  include <png.h>
#  include <zlib.h>
# endif
#endif

#include <sys/wait.h>

/* ---------------------------------------------------------------------------------------------------------------------- */


extern void xsane_multipage_dialog(void);
extern void xsane_multipage_project_save(void);

#endif
