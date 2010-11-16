/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-device-preferences.c

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2010 Oliver Rauch
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

/* ---------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-rc-io.h"
#include "xsane-back-gtk.h"
#include "xsane-front-gtk.h"
#include "xsane-gamma.h"

/* ---------------------------------------------------------------------------------------------------------------- */

#define BITS_PER_LONG   (8*sizeof(u_long))

#define SET(set, bit) ((set)[(bit)/BITS_PER_LONG] |= (1UL << (bit)%BITS_PER_LONG))
#define IS_SET(set, bit) (((set)[(bit)/BITS_PER_LONG] & (1UL << (bit)%BITS_PER_LONG)) != 0) 

#define DPOFFSET(field)  ((char *) &((Xsane *) 0)->field - (char *) 0) 

/* ---------------------------------------------------------------------------------------------------------------- */

static struct
{
  SANE_String name;
  void (*codec) (Wire *w, void *p, long offset);
  long offset;
}
desc_xsane_device[] =
{
    {"xsane-main-window-x-position",             xsane_rc_pref_int, DPOFFSET(dialog_posx)},
    {"xsane-main-window-y-position",             xsane_rc_pref_int, DPOFFSET(dialog_posy)},

    {"xsane-main-window-width",                  xsane_rc_pref_int, DPOFFSET(dialog_width)},
    {"xsane-main-window-height",                 xsane_rc_pref_int, DPOFFSET(dialog_height)},

    {"xsane-project-window-x-position",          xsane_rc_pref_int, DPOFFSET(project_dialog_posx)},
    {"xsane-project-window-y-position",          xsane_rc_pref_int, DPOFFSET(project_dialog_posy)},

    {"xsane-standard-options-window-x-position", xsane_rc_pref_int, DPOFFSET(standard_options_dialog_posx)},
    {"xsane-standard-options-window-y-position", xsane_rc_pref_int, DPOFFSET(standard_options_dialog_posy)},

    {"xsane-advanced-options-window-x-position", xsane_rc_pref_int, DPOFFSET(advanced_options_dialog_posx)},
    {"xsane-advanced-options-window-y-position", xsane_rc_pref_int, DPOFFSET(advanced_options_dialog_posy)},

    {"xsane-histogram-window-x-position",        xsane_rc_pref_int, DPOFFSET(histogram_dialog_posx)},
    {"xsane-histogram-window-y-position",        xsane_rc_pref_int, DPOFFSET(histogram_dialog_posy)},

    {"xsane-gamma-window-x-position",            xsane_rc_pref_int, DPOFFSET(gamma_dialog_posx)},
    {"xsane-gamma-window-y-position",            xsane_rc_pref_int, DPOFFSET(gamma_dialog_posy)},

    {"xsane-batch-window-x-position",            xsane_rc_pref_int, DPOFFSET(batch_dialog_posx)},
    {"xsane-batch-window-y-position",            xsane_rc_pref_int, DPOFFSET(batch_dialog_posy)},

    {"xsane-preview-window-x-position",          xsane_rc_pref_int, DPOFFSET(preview_dialog_posx)},
    {"xsane-preview-window-y-position",          xsane_rc_pref_int, DPOFFSET(preview_dialog_posy)},
    {"xsane-preview-window-width",               xsane_rc_pref_int, DPOFFSET(preview_dialog_width)},
    {"xsane-preview-window-height",              xsane_rc_pref_int, DPOFFSET(preview_dialog_height)},

    {"xsane-gamma",                   xsane_rc_pref_double,       DPOFFSET(gamma)},
    {"xsane-gamma-red",               xsane_rc_pref_double,       DPOFFSET(gamma_red)},
    {"xsane-gamma-green",             xsane_rc_pref_double,       DPOFFSET(gamma_green)},
    {"xsane-gamma-blue",              xsane_rc_pref_double,       DPOFFSET(gamma_blue)},

    {"xsane-brightness",              xsane_rc_pref_double,       DPOFFSET(brightness)},
    {"xsane-brightness-red",          xsane_rc_pref_double,       DPOFFSET(brightness_red)},
    {"xsane-brightness-green",        xsane_rc_pref_double,       DPOFFSET(brightness_green)},
    {"xsane-brightness-blue",         xsane_rc_pref_double,       DPOFFSET(brightness_blue)},

    {"xsane-contrast",                xsane_rc_pref_double,       DPOFFSET(contrast)},
    {"xsane-contrast-red",            xsane_rc_pref_double,       DPOFFSET(contrast_red)},
    {"xsane-contrast-green",          xsane_rc_pref_double,       DPOFFSET(contrast_green)},
    {"xsane-contrast-blue",           xsane_rc_pref_double,       DPOFFSET(contrast_blue)},

    {"xsane-lineart-mode",            xsane_rc_pref_int,          DPOFFSET(lineart_mode)},
    {"xsane-threshold",               xsane_rc_pref_double,       DPOFFSET(threshold)},
    {"xsane-threshold-min",           xsane_rc_pref_double,       DPOFFSET(threshold_min)},
    {"xsane-threshold-max",           xsane_rc_pref_double,       DPOFFSET(threshold_max)},
    {"xsane-threshold-multiplier",    xsane_rc_pref_double,       DPOFFSET(threshold_mul)},
    {"xsane-threshold-offset",        xsane_rc_pref_double,       DPOFFSET(threshold_off)},
    {"xsane-grayscale-scanmode",      xsane_rc_pref_string,       DPOFFSET(grayscale_scanmode)},

    {"xsane-enhancement-rgb-default", xsane_rc_pref_int,          DPOFFSET(enhancement_rgb_default)},
    {"xsane-negative",                xsane_rc_pref_int,          DPOFFSET(negative)},
    {"xsane-show-preview",            xsane_rc_pref_int,          DPOFFSET(show_preview)},

    {"xsane-enable-color-management",		xsane_rc_pref_int,	DPOFFSET(enable_color_management)},
    {"xsane-scanner-default-color-icm-profile",	xsane_rc_pref_string,   DPOFFSET(scanner_default_color_icm_profile)},
    {"xsane-scanner-default-gray-icm-profile",	xsane_rc_pref_string,	DPOFFSET(scanner_default_gray_icm_profile)},
};

/* ---------------------------------------------------------------------------------------------------------------- */

static int xsane_device_preferences_load_values(Wire *w, SANE_Handle device)
{
 const SANE_Option_Descriptor *opt;
 char *word_array;
 SANE_String name, str;
 u_long *caused_reload;
 SANE_Int num_options;
 SANE_Status status;
 int i, keep_going;
 SANE_Word word;
 SANE_Int info;
 off_t offset;
 size_t size;
 char *buf;

  DBG(DBG_proc, "xsane_device_preferences_load_values\n");

  lseek(w->io.fd, 1, SEEK_SET); /* rewind file */
  xsane_rc_io_w_flush(w);

  offset = lseek(w->io.fd, 0, SEEK_CUR); /* remeber file position */

  keep_going = 0;

  xsane_control_option(device, 0, SANE_ACTION_GET_VALUE, &num_options, 0);
  size = (num_options + BITS_PER_LONG - 1) / BITS_PER_LONG * sizeof(long);
  caused_reload = alloca(size);
  memset(caused_reload, 0, size);

  while (1)
  {
    xsane_rc_io_w_space(w, 3);
    if (!w->status)
    {
      xsane_rc_io_w_string(w, &name);
    }

    if (w->status == XSANE_EOF) /* eof */
    {
      if (keep_going) /* we had a reload otpions? */
      {
        lseek(w->io.fd, offset, SEEK_SET); /* rewind file to position of first run */
        xsane_rc_io_w_flush(w);
        keep_going = 0;
        continue;
      }

      /* no keep_gooing: we can exit, all options should be set correct */
      return 0;
    }
    else if (w->status) /* error: skip line */
    {
      w->status = 0;
      xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_string, &name); /* free string memory */
      xsane_rc_io_w_skip_newline(w); /* skip this line */
      continue;
    }

    status = SANE_STATUS_GOOD;
    info = 0;
    for (i = 1; (i < num_options) && (opt = xsane_get_option_descriptor(device, i)); ++i) /* search all options */
    {
      if (!opt->name || strcmp(opt->name, name) != 0) /* test if option names are equal */
      {
        continue; /* not equal, continue the search */
      }

      if (IS_SET(caused_reload, i)) 
      {
        continue; /* option already caused a reload: */
                  /* we expect that this option already is set correct */
                  /* otherwise we could get infinite loops */
      }

      /* name is correct and option did not force a reload: set option */
      switch (opt->type)
      {
        case SANE_TYPE_BOOL:
        case SANE_TYPE_INT:
        case SANE_TYPE_FIXED:
          if (opt->size == sizeof(SANE_Word))
          {
            xsane_rc_io_w_word(w, &word);
            status = xsane_control_option(device, i, SANE_ACTION_SET_VALUE, &word, &info);
          }
          else /* array */
          {
           SANE_Int len;

            xsane_rc_io_w_array(w, &len, &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
            status = xsane_control_option(device, i, SANE_ACTION_SET_VALUE, word_array, &info);
            w->direction = WIRE_FREE;
            xsane_rc_io_w_array(w, &len, &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
            w->direction = WIRE_DECODE;
          }
	  break;

        case SANE_TYPE_STRING:
          xsane_rc_io_w_string(w, &str);
          buf = malloc(opt->size);
          if (!w->status) /* got a string ? */
          {
            strncpy(buf, str, opt->size);
            buf[opt->size - 1] = '\0';
            xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_string, &str);
            status = xsane_control_option(device, i, SANE_ACTION_SET_VALUE, buf, &info);
          }
          break;

        case SANE_TYPE_BUTTON:
        case SANE_TYPE_GROUP:
          /* nothing to read for button and group */
          break;
      }
      break; /* option is set: do not continue search */
    }
    xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_string, &name); /* free string memory */

    if (status == SANE_STATUS_GOOD && (info & SANE_INFO_RELOAD_OPTIONS))
    {
      SET(caused_reload, i);
      keep_going = 1;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */

static int xsane_device_preferences_save_values(Wire *w, SANE_Handle device)
{
 const SANE_Option_Descriptor *opt;
 size_t word_array_size = 0;
 char *word_array = 0;
 size_t str_size = 0;
 SANE_String str = 0;
 SANE_Word word;
 int i;
 SANE_Int num_options;

  DBG(DBG_proc, "xsane_device_preferences_save_values\n");

  xsane_control_option(device, 0, SANE_ACTION_GET_VALUE, &num_options, 0);

  for (i = 0; (i < num_options) && (opt = xsane_get_option_descriptor(device, i)); ++i)
  {
    if ((opt->cap & (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT)) !=
        (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT) || !opt->name)
	/* if we can't query AND set the option, don't bother saving it */
    {
      continue;
    }

    if (!SANE_OPTION_IS_ACTIVE(opt->cap)) /* option is not active, don`t save it */
    {
      continue;
    }

    switch (opt->type)
    {
      case SANE_TYPE_BOOL:
      case SANE_TYPE_INT:
      case SANE_TYPE_FIXED:
        if (opt->size == sizeof(SANE_Word))
        {
          if (xsane_control_option(device, i, SANE_ACTION_GET_VALUE, &word, 0) != SANE_STATUS_GOOD)
          {
            continue;
          }
          xsane_rc_io_w_string(w, (SANE_String *) &opt->name);
          xsane_rc_io_w_word(w, &word);
        }
        else
        {
         SANE_Int len = opt->size / sizeof(SANE_Word);

          if (opt->size > word_array_size)
          {
             word_array_size = ((opt->size + 32*sizeof(SANE_Word)) & ~(32*sizeof(SANE_Word) - 1));
             if (word_array)
             {
               word_array = realloc(word_array, word_array_size);
             }
             else
             {
               word_array = malloc(word_array_size);
             }

             if (word_array == 0)
             {
               /* Malloc failed, so return an error. */
               w->status = ENOMEM;
               return 1;
             }
          }

          if (xsane_control_option(device, i, SANE_ACTION_GET_VALUE, word_array, 0) != SANE_STATUS_GOOD)
          {
            continue;
          }

          xsane_rc_io_w_string(w, (SANE_String *) &opt->name);
          xsane_rc_io_w_array(w, &len, &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
        }
        break;

      case SANE_TYPE_STRING:
        if (opt->size > str_size)
        {
          str_size = (opt->size + 1024) & ~1023;

          if (str)
          {
            str = realloc(str, str_size);
          }
          else
          {
            str = malloc(str_size);
          }

          if (str == 0)
          {
            /* Malloc failed, so return an error. */
            w->status = ENOMEM;
            return 1;
          }
        }

        if (xsane_control_option(device, i, SANE_ACTION_GET_VALUE, str, 0) != SANE_STATUS_GOOD)
        {
          continue;
        }

        xsane_rc_io_w_string(w, (SANE_String *) &opt->name);
        xsane_rc_io_w_string(w, &str);
        break;

      case SANE_TYPE_BUTTON:
      case SANE_TYPE_GROUP:
        break;
    }
  }

  if (word_array)
  {
    free(word_array);
  }
  if (str)
  {
    free(str);
  }

  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_load_file(char *filename)
{
 int fd;
 char buf[TEXTBUFSIZE];
#if 0
 char *version = 0;
#endif
 Wire w;
 SANE_String name;
 int i;

  DBG(DBG_proc, "xsane_device_preferences_load_file\n");

  /* set geometry and position to standard values */
  xsane.dialog_posx                  = XSANE_DIALOG_POS_X;
  xsane.dialog_posy                  = XSANE_DIALOG_POS_Y;
  xsane.dialog_width                 = XSANE_DIALOG_WIDTH;
  xsane.dialog_height                = XSANE_DIALOG_HEIGHT;

  xsane.project_dialog_posx          = XSANE_PROJECT_DIALOG_POS_X;
  xsane.project_dialog_posy          = XSANE_PROJECT_DIALOG_POS_Y;

  xsane.standard_options_dialog_posx = XSANE_STD_OPTIONS_DIALOG_POS_X;
  xsane.standard_options_dialog_posy = XSANE_STD_OPTIONS_DIALOG_POS_Y;

  xsane.advanced_options_dialog_posx = XSANE_ADV_OPTIONS_DIALOG_POS_X;
  xsane.advanced_options_dialog_posy = XSANE_ADV_OPTIONS_DIALOG_POS_Y;

  xsane.histogram_dialog_posx       = XSANE_HISTOGRAM_DIALOG_POS_X;
  xsane.histogram_dialog_posy       = XSANE_HISTOGRAM_DIALOG_POS_Y;

  xsane.gamma_dialog_posx           = XSANE_GAMMA_DIALOG_POS_X;
  xsane.gamma_dialog_posy           = XSANE_GAMMA_DIALOG_POS_Y;

  xsane.batch_dialog_posx           = XSANE_BATCH_DIALOG_POS_X;
  xsane.batch_dialog_posy           = XSANE_BATCH_DIALOG_POS_Y;

  xsane.preview_dialog_posx         = XSANE_PREVIEW_DIALOG_POS_X;
  xsane.preview_dialog_posy         = XSANE_PREVIEW_DIALOG_POS_Y;
  xsane.preview_dialog_width        = XSANE_PREVIEW_DIALOG_WIDTH;
  xsane.preview_dialog_height       = XSANE_PREVIEW_DIALOG_HEIGHT;

  xsane.resolution                  = 1.0;
  xsane.resolution_x                = 1.0;
  xsane.resolution_y                = 1.0;

  xsane.gamma                       = 1.0;
  xsane.gamma_red                   = 1.0;
  xsane.gamma_green                 = 1.0;
  xsane.gamma_blue                  = 1.0;

  xsane.brightness                  = 0.0;
  xsane.brightness_red              = 0.0;
  xsane.brightness_green            = 0.0;
  xsane.brightness_blue             = 0.0;

  xsane.contrast                    = 0.0;
  xsane.contrast_red                = 0.0;
  xsane.contrast_green              = 0.0;
  xsane.contrast_blue               = 0.0;

  xsane.lineart_mode                = 0;
  xsane.grayscale_scanmode          = 0; /* Empty String => keeps "grayscale" */
  xsane.threshold                   = 50.0;
  xsane.threshold_min               = 0.0;
  xsane.threshold_max               = 100.0;
  xsane.threshold_mul               = 1.0;
  xsane.threshold_off               = 0.0;

  xsane.enhancement_rgb_default     = 1;
  xsane.negative                    = 0;
  xsane.show_preview                = 1;

  xsane.enable_color_management     = 0;

  fd = open(filename, O_RDONLY); 

  if (fd >= 0)
  {
    /* prepare wire */
    w.io.fd = fd;
    w.io.read = read;
    w.io.write = write;
    xsane_rc_io_w_init(&w);
    xsane_rc_io_w_set_dir(&w, WIRE_DECODE);

    xsane_rc_io_w_space(&w, 3);
    if (!w.status)
    {
      xsane_rc_io_w_string(&w, &name); /* get string */
      if (!w.status)
      {
        if (strcmp(name, "XSANE_DEVICE_RC")) /* no real *.drc file */
        {
          w.status = -1; /* no *.drc file => error */
        }
      }
    }

    if (w.status)
    {
      char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s\n%s %s", ERR_LOAD_DEVICE_SETTINGS, filename, ERR_NO_DRC_FILE);
      xsane_back_gtk_error(buf, TRUE);
      close(fd);
      return;
    }

    xsane_rc_io_w_space(&w, 3);
    if (!w.status)
    {
      xsane_rc_io_w_string(&w, &name); /* get string */
      if (!w.status)
      {
        if (strcmp(name, xsane.device_set_filename))
        {
          snprintf(buf, sizeof(buf), "%s \"%s\"\n"
                                     "%s \"%s\",\n"
                                     "%s \"%s\",\n"
                                     "%s",
                                     TEXT_FILE, filename,
                                     ERR_CREATED_FOR_DEVICE, name,
                                     ERR_USED_FOR_DEVICE, xsane.device_set_filename,
                                     ERR_MAY_CAUSE_PROBLEMS);
          if (xsane_back_gtk_decision(ERR_HEADER_WARNING, (gchar **) error_xpm, buf, BUTTON_OK, BUTTON_CANCEL, TRUE) == FALSE)
          { /* cancel */
            close(fd);
            return; 
          }
        }
      }
    }

    if (w.status)
    {
      /* may be we should pop up a window here */
      close(fd);
      return;
    }
  
    while (1) /* read device dependant xsane options */
    {
      xsane_rc_io_w_space(&w, 3);
      if (w.status)
      {
        break;
      }

      xsane_rc_io_w_string(&w, &name);

      if (!w.status && name)
      {
        for (i = 0; i < NELEMS (desc_xsane_device); ++i)
        {
          if (strcmp(name, desc_xsane_device[i].name) == 0)
          {
            (*desc_xsane_device[i].codec) (&w, &xsane, desc_xsane_device[i].offset);
            break; /* leave for loop */
          }
        }
      }
      w.status = 0;
    }                 

    xsane_device_preferences_load_values(&w, xsane.dev); /* read device preferences */
    close(fd); 

    if (xsane.well_known.dpi > 0)
    {
     const SANE_Option_Descriptor *opt;

      opt = xsane_get_option_descriptor(xsane.dev, xsane.well_known.dpi);
  
      switch (opt->type)
      {
        case SANE_TYPE_INT:
        {
         SANE_Int dpi;
          xsane_control_option(xsane.dev, xsane.well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
          xsane.resolution = dpi;
        }
        break;

        case SANE_TYPE_FIXED:
        {
         SANE_Fixed dpi;
          xsane_control_option(xsane.dev, xsane.well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
          xsane.resolution = (int) SANE_UNFIX(dpi);
        }
        break;
 
        default:
         DBG(DBG_error, "xsane_pref_load_file: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
        return;
      }
    }
  }

  if (!xsane.scanner_default_color_icm_profile)
  {
    xsane.scanner_default_color_icm_profile = strdup("");
  }

  if (!xsane.scanner_default_gray_icm_profile)
  {
    xsane.scanner_default_gray_icm_profile = strdup("");
  }

  gtk_window_move(GTK_WINDOW(xsane.dialog), xsane.dialog_posx, xsane.dialog_posy);
  gtk_window_set_default_size(GTK_WINDOW(xsane.dialog), xsane.dialog_width, xsane.dialog_height);

  if (xsane.project_dialog)
  {
    gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
  }

  gtk_window_move(GTK_WINDOW(xsane.standard_options_dialog), xsane.standard_options_dialog_posx, xsane.standard_options_dialog_posy);
  gtk_window_move(GTK_WINDOW(xsane.advanced_options_dialog), xsane.advanced_options_dialog_posx, xsane.advanced_options_dialog_posy);
  gtk_window_move(GTK_WINDOW(xsane.histogram_dialog), xsane.histogram_dialog_posx, xsane.histogram_dialog_posy);
#if 0
  gtk_window_move(GTK_WINDOW(xsane.gamma_dialog), xsane.gamma_dialog_posx, xsane.gamma_dialog_posy);
#endif
  gtk_window_move(GTK_WINDOW(xsane.batch_scan_dialog), xsane.batch_dialog_posx, xsane.batch_dialog_posy);
  gtk_window_move(GTK_WINDOW(xsane.preview->top), xsane.preview_dialog_posx, xsane.preview_dialog_posy);
#ifdef HAVE_GTK2
  gtk_window_resize(GTK_WINDOW(xsane.preview->top), xsane.preview_dialog_width, xsane.preview_dialog_height);
#else
  gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), xsane.preview_dialog_width, xsane.preview_dialog_height);
#endif

#ifdef HAVE_LIBLCMS
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(xsane.enable_color_management_widget), xsane.enable_color_management);
#endif

  xsane_update_param(0);
  xsane_set_all_resolutions(); /* XXX test XXX */
  xsane_refresh_dialog();
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_restore(void)
{
 char filename[PATH_MAX];
 struct stat st; 

  DBG(DBG_proc, "xsane_device_preferences_restore\n");

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc", XSANE_PATH_LOCAL_SANE);

  if (stat(filename, &st) >= 0)
  {   
    xsane_device_preferences_load_file(filename);
  }
  else /* no local sane file, look for system file */
  {
    xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc", XSANE_PATH_SYSTEM);
    xsane_device_preferences_load_file(filename);
  }
}
                  
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_load(void)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_device_preferences_load\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_LOAD_SETTINGS, xsane.device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc", XSANE_PATH_LOCAL_SANE);

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_OPEN, FALSE, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_DRC, XSANE_FILE_FILTER_DRC))
  {
    xsane_device_preferences_load_file(filename);
  }
  xsane_set_sensitivity(TRUE);
  xsane_update_histogram(TRUE /* update raw */);
}        

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_RC_IO_W_STRINGCONST(wire, string)	{ SANE_String str=string; xsane_rc_io_w_string(wire, &str); }

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_save_file(char *filename)
{
 int fd;
 Wire w;
 int i;

  if (filename)
  {
    DBG(DBG_info, "Saving device preferences to file %s\n", filename);

    umask(XSANE_DEFAULT_UMASK); /* define new file permissions */
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
     char buf[TEXTBUFSIZE];

      snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
      xsane_back_gtk_error(buf, TRUE);
      xsane_set_sensitivity(TRUE);
      return;
    }

    /* prepare wire */
    w.io.fd = fd;
    w.io.read = read;
    w.io.write = write;
    xsane_rc_io_w_init(&w);
    xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

    XSANE_RC_IO_W_STRINGCONST(&w, "XSANE_DEVICE_RC");
    xsane_rc_io_w_string(&w, &xsane.device_set_filename);

    XSANE_RC_IO_W_STRINGCONST(&w, "xsane-version");
    XSANE_RC_IO_W_STRINGCONST(&w, XSANE_VERSION);

    /* make geometry and position values up to date */
    xsane_window_get_position(xsane.dialog, &xsane.dialog_posx, &xsane.dialog_posy);
    gdk_drawable_get_size(xsane.dialog->window, &xsane.dialog_width, &xsane.dialog_height);
#if 0 /* TO BE REMOVED */
    gtk_window_move(GTK_WINDOW(xsane.dialog), xsane.dialog_posx, xsane.dialog_posy); /* geometry used when window closed and opened again */
    gtk_window_set_default_size(GTK_WINDOW(xsane.dialog), xsane.dialog_width, xsane.dialog_height);
#endif

    if (xsane.project_dialog)
    {
      xsane_window_get_position(xsane.project_dialog, &xsane.project_dialog_posx, &xsane.project_dialog_posy);
#if 0 /* TO BE REMOVED */
      gtk_window_move(GTK_WINDOW(xsane.project_dialog), xsane.project_dialog_posx, xsane.project_dialog_posy);
#endif
    }

    if (preferences.show_standard_options)
    {
      xsane_window_get_position(xsane.standard_options_dialog, &xsane.standard_options_dialog_posx, &xsane.standard_options_dialog_posy);
#if 0 /* TO BE REMOVED */
      gtk_window_move(GTK_WINDOW(xsane.standard_options_dialog), xsane.standard_options_dialog_posx, xsane.standard_options_dialog_posy);
#endif
    }

    if (preferences.show_advanced_options)
    {
      xsane_window_get_position(xsane.advanced_options_dialog, &xsane.advanced_options_dialog_posx, &xsane.advanced_options_dialog_posy);
#if 0 /* TO BE REMOVED */
      gtk_window_move(GTK_WINDOW(xsane.advanced_options_dialog), xsane.advanced_options_dialog_posx, xsane.advanced_options_dialog_posy);
#endif
    }

    if (preferences.show_histogram)
    {
      xsane_window_get_position(xsane.histogram_dialog, &xsane.histogram_dialog_posx, &xsane.histogram_dialog_posy);
#if 0 /* TO BE REMOVED */
      gtk_window_move(GTK_WINDOW(xsane.histogram_dialog), xsane.histogram_dialog_posx, xsane.histogram_dialog_posy);
#endif
    }

#if 0
    xsane_window_get_position(xsane.gamma_dialog, &xsane.gamma_dialog_posx, &xsane.gamma_dialog_posy);
    gtk_window_move(GTK_WINDOW(xsane.gamma_dialog), xsane.gamma_dialog_posx, xsane.gamma_dialog_posy);
#endif

    if (preferences.show_batch_scan)
    {
      xsane_window_get_position(xsane.batch_scan_dialog, &xsane.batch_dialog_posx, &xsane.batch_dialog_posy);
#if 0 /* TO BE REMOVED */
      gtk_window_move(GTK_WINDOW(xsane.batch_scan_dialog), xsane.batch_dialog_posx, xsane.batch_dialog_posy);
#endif
    }

    if (xsane.preview)
    {
      xsane_window_get_position(xsane.preview->top, &xsane.preview_dialog_posx, &xsane.preview_dialog_posy);
      gdk_drawable_get_size(xsane.preview->top->window, &xsane.preview_dialog_width, &xsane.preview_dialog_height);
#if 0
      gtk_window_move(GTK_WINDOW(xsane.preview->top), xsane.preview_dialog_posx, xsane.preview_dialog_posy);
      gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), xsane.preview_dialog_width, xsane.preview_dialog_height);
#endif
    }

    xsane_device_preferences_save_values(&w, xsane.dev);

    for (i = 0; i < NELEMS(desc_xsane_device); ++i) /* save device preferences xsane values */
    {
      xsane_rc_io_w_string(&w, &desc_xsane_device[i].name);
      (*desc_xsane_device[i].codec) (&w, &xsane, desc_xsane_device[i].offset);
    }   

    xsane_rc_io_w_flush(&w);
    close(fd);
  }
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_store(void)
{
 char filename[PATH_MAX];

  DBG(DBG_proc, "xsane_device_preferences_store\n");

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc", XSANE_PATH_LOCAL_SANE);
  xsane_device_preferences_save_file(filename);
}
                  
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_save(void)
{
 char filename[PATH_MAX];
 char windowname[TEXTBUFSIZE];

  DBG(DBG_proc, "xsane_device_preferences_save\n");

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", xsane.prog_name, WINDOW_SAVE_SETTINGS, xsane.device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc", XSANE_PATH_LOCAL_SANE);

  if (!xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename, NULL, NULL, XSANE_FILE_CHOOSER_ACTION_SAVE, FALSE, XSANE_FILE_FILTER_ALL | XSANE_FILE_FILTER_DRC, XSANE_FILE_FILTER_DRC))
  {
    xsane_device_preferences_save_file(filename);
  }

  xsane_set_sensitivity(TRUE);
  xsane_update_histogram(TRUE /* update raw */);
}

/* ---------------------------------------------------------------------------------------------------------------- */

