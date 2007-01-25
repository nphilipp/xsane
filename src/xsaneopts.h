/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsaneopts.h

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

#include <sane/saneopts.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef XSANEOPTS_H
#define XSANEOPTS_H

/* ---------------------------------------------------------------------------------------------------------------------- */

#ifndef SANE_NAME_BATCH_SCAN_START
# define SANE_NAME_BATCH_SCAN_START	"batch-scan-start"
#endif

#ifndef SANE_NAME_BATCH_SCAN_LOOP
# define SANE_NAME_BATCH_SCAN_LOOP	"batch-scan-loop"
#endif

#ifndef SANE_NAME_BATCH_SCAN_END
# define SANE_NAME_BATCH_SCAN_END	"batch-scan-end"
#endif

#ifndef SANE_NAME_BATCH_SCAN_NEXT_TL_Y
# define SANE_NAME_BATCH_SCAN_NEXT_TL_Y	"batch-scan-next-tl-y"
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */

#endif /* XSANEOPTS_H */
