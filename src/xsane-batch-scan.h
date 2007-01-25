/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-batch-scan.h

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

#ifndef xsane_batch_scan_h
#define xsane_batch_scan_h

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <sane/sane.h>

/* ---------------------------------------------------------------------------------------------------------------------- */
typedef enum
{
  BATCH_MODE_OFF = 0,
  BATCH_MODE_LAST_SCAN,
  BATCH_MODE_LOOP
} BATCH_MODE_T;

/* ---------------------------------------------------------------------------------------------------------------------- */

typedef struct
{
  char *name;
  char *scanmode;
  double tl_x;
  double tl_y;
  double br_x;
  double br_y;
  SANE_Unit unit;
  int rotation;
  double resolution_x;
  double resolution_y;
  int bit_depth;
  double gamma;
  double gamma_red;
  double gamma_green;
  double gamma_blue;
  double contrast;
  double contrast_red;
  double contrast_green;
  double contrast_blue;
  double brightness;
  double brightness_red;
  double brightness_green;
  double brightness_blue;
  int enhancement_rgb_default;
  int negative;
  GtkWidget *label;
  GtkWidget *gtk_preview;
  int gtk_preview_size;
} Batch_Scan_Parameters;

/* ---------------------------------------------------------------------------------------------------------------------- */

extern void xsane_batch_scan_add();
extern void xsane_create_batch_scan_dialog(const char *devicetext);
extern void xsane_batch_scan_update_label_list(void);
extern void xsane_batch_scan_update_icon_list(void);
extern int xsane_batch_scan_load_list_from_file(char *filename);

#endif /* batch_scan_h */
