
/* sane - Scanner Access Now Easy.
   Copyright (C) 1999 Oliver Rauch
   This file is part of the XSANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA. */

/* ---------------------------------------------------------------------------------------------------------------- */

#include "xsane.h"
#include "xsane-rc-io.h"
#include "xsane-front-gtk.h"
#include "xsane-gamma.h"

/* ---------------------------------------------------------------------------------------------------------------- */

#define BITS_PER_LONG   (8*sizeof(u_long))

#define SET(set, bit) ((set)[(bit)/BITS_PER_LONG] |= (1UL << (bit)%BITS_PER_LONG))
#define IS_SET(set, bit) (((set)[(bit)/BITS_PER_LONG] & (1UL << (bit)%BITS_PER_LONG)) != 0) 

/* ---------------------------------------------------------------------------------------------------------------- */

int xsane_device_preferences_load_values(int fd, SANE_Handle device)
{
 const SANE_Option_Descriptor *opt;
 SANE_Word *word_array;
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
 Wire w;

  offset = lseek(fd, 0, SEEK_CUR);
  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);
  keep_going = 0;

  sane_control_option(device, 0, SANE_ACTION_GET_VALUE, &num_options, 0);
  size = (num_options + BITS_PER_LONG - 1) / BITS_PER_LONG * sizeof(long);
  caused_reload = alloca(size);
  memset(caused_reload, 0, size);

  while (1)
  {
    xsane_rc_io_w_space(&w, 3);

    if (!w.status)
    {
      xsane_rc_io_w_string(&w, &name);
    }

    if (w.status)
    {
      if (keep_going)
      {
        lseek(fd, offset, SEEK_SET);
        xsane_rc_io_w_set_dir(&w, WIRE_DECODE);
        keep_going = 0;
        continue;
      }
      return 0;
    }

    status = SANE_STATUS_GOOD;
    info = 0;
    for (i = 1; (opt = sane_get_option_descriptor(device, i)); ++i)
    {
      if (!opt->name || strcmp(opt->name, name) != 0)
      {
        continue;
      }

      if (IS_SET(caused_reload, i))
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
            xsane_rc_io_w_word(&w, &word);
            status = sane_control_option(device, i, SANE_ACTION_SET_VALUE, &word, &info);
          }
          else
          {
            SANE_Int len;

            xsane_rc_io_w_array(&w, &len, (void **) &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
            status = sane_control_option(device, i, SANE_ACTION_SET_VALUE, word_array, &info);
            w.direction = WIRE_FREE;
            xsane_rc_io_w_array(&w, &len, (void **) &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
            w.direction = WIRE_DECODE;
          }
	  break;

        case SANE_TYPE_STRING:
          xsane_rc_io_w_string(&w, &str);
          buf = malloc(opt->size);
          if (!w.status) /* got a string ? */
          {
            strncpy(buf, str, opt->size);
            buf[opt->size - 1] = '\0';
            xsane_rc_io_w_free(&w, (WireCodecFunc) xsane_rc_io_w_string, &str);
            status = sane_control_option(device, i, SANE_ACTION_SET_VALUE, buf, &info);
          }
          break;

        case SANE_TYPE_BUTTON:
        case SANE_TYPE_GROUP:
          /* nothing to read for button and group */
          break;
      }
      break;
    }
    xsane_rc_io_w_free(&w, (WireCodecFunc) xsane_rc_io_w_string, &name);

    if (status == SANE_STATUS_GOOD && (info & SANE_INFO_RELOAD_OPTIONS))
    {
      SET(caused_reload, i);
      keep_going = 1;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */

int xsane_device_preferences_save_values(int fd, SANE_Handle device)
{
 const SANE_Option_Descriptor *opt;
 size_t word_array_size = 0;
 SANE_Word *word_array = 0;
 size_t str_size = 0;
 SANE_String str = 0;
 SANE_Word word;
 Wire w;
 int i;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  xsane_rc_io_w_init(&w);
  xsane_rc_io_w_set_dir(&w, WIRE_ENCODE);

  for (i = 0; (opt = sane_get_option_descriptor(device, i)); ++i)
  {
    if ((opt->cap & (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT)) != (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT) || !opt->name)
	/* if we can't query AND set the option, don't bother saving it */
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
          if (sane_control_option(device, i, SANE_ACTION_GET_VALUE, &word, 0) != SANE_STATUS_GOOD)
          {
            continue;
          }
          xsane_rc_io_w_string(&w, (SANE_String *) &opt->name);
          xsane_rc_io_w_word(&w, &word);
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
               w.status = ENOMEM;
               return 1;
             }
          }

          if (sane_control_option(device, i, SANE_ACTION_GET_VALUE, word_array, 0) != SANE_STATUS_GOOD)
          {
            continue;
          }

          xsane_rc_io_w_string(&w, (SANE_String *) &opt->name);
          xsane_rc_io_w_array(&w, &len, (void **) &word_array, (WireCodecFunc) xsane_rc_io_w_word, sizeof(SANE_Word));
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
            w.status = ENOMEM;
            return 1;
          }
        }

        if (sane_control_option(device, i, SANE_ACTION_GET_VALUE, str, 0) != SANE_STATUS_GOOD)
        {
          continue;
        }

        xsane_rc_io_w_string(&w, (SANE_String *) &opt->name);
        xsane_rc_io_w_string(&w, &str);
        break;

      case SANE_TYPE_BUTTON:
      case SANE_TYPE_GROUP:
        break;
    }
  }
  xsane_rc_io_w_set_dir(&w, WIRE_DECODE);

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
 FILE *file;
 char buf[256];
 char option[256];
 char *optionp;
 char *version = 0;
 int len;

 int main_posx   = XSANE_DIALOG_POS_X;
 int main_posy   = XSANE_DIALOG_POS_Y;
 int main_width  = XSANE_DIALOG_WIDTH;
 int main_height = XSANE_DIALOG_HEIGHT;

 int standard_options_posx   = XSANE_DIALOG_POS_X;
 int standard_options_posy   = XSANE_DIALOG_POS_Y2;

 int advanced_options_posx   = XSANE_DIALOG_POS_X2;
 int advanced_options_posy   = XSANE_DIALOG_POS_Y2;

 int histogram_posx = XSANE_DIALOG_POS_X2;
 int histogram_posy = XSANE_DIALOG_POS_Y;

 int preview_posx   = 0;
 int preview_posy   = 0;
 int preview_width  = 0;
 int preview_height = 0;

  file = fopen(filename, "r");
  if (file == 0) /* error ? */
  {
    return;
  }

  if (!feof(file))
  {
    fgets(option, sizeof(option), file); /* get first line */
    option[strlen(option)-1] = 0; /* remove cr */

    if (strcmp(option, "\"XSANE_DEVICE_RC\"") != 0) /* wrong file format ? */
    {
      char buf[256];

      snprintf(buf, sizeof(buf), "%s\n%s %s", ERR_LOAD_DEVICE_SETTINGS, filename, ERR_NO_DRC_FILE);
      xsane_back_gtk_error(buf, TRUE);
      return;
    }

    if (!feof(file))
    {
      fgets(option, sizeof(option), file); /* get version */
      option[strlen(option)-1] = 0; /* remove cr */
      len = strlen(option);
      if (len)
      {
        if (option[len-1] == 34)
        {
          option[len-1] = 0; /* remove " */
        }
      }
      optionp = option+1;

      if (strcmp(optionp, xsane.device_set_filename))
      {
        snprintf(buf, sizeof(buf), "%s \"%s\"\n"
                                   "%s \"%s\",\n"
                                   "%s \"%s\",\n"
                                   "%s",
                                   TEXT_FILE, filename,
                                   ERR_CREATED_FOR_DEVICE, optionp,
                                   ERR_USED_FOR_DEVICE, xsane.device_set_filename,
                                   ERR_MAY_CAUSE_PROBLEMS);
        if (xsane_back_gtk_decision(ERR_HEADER_WARNING, buf, ERR_BUTTON_OK, BUTTON_CANCEL, TRUE) == FALSE)
        { /* cancel */
          fclose(file);
          return; 
        }
      }
    }
  }

  while (!feof(file))
  {
    fgets(option, sizeof(option), file); /* get option name */
    option[strlen(option)-1] = 0; /* remove cr */
    if (strcmp(option, "\"xsane-version\"") == 0)
    {
      fgets(option, sizeof(option), file); /* get version */
      option[strlen(option)-1] = 0; /* remove cr */
      len = strlen(option);
      if (len)
      {
        if (option[len-1] == 34)
        {
          option[len-1] = 0; /* remove " */
        }
      }
      version = strdup(option+1);
    }
    else if (strcmp(option, "\"xsane-gamma\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma);
    }
    else if (strcmp(option, "\"xsane-gamma-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_red);
    }
    else if (strcmp(option, "\"xsane-gamma-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_green);
    }
    else if (strcmp(option, "\"xsane-gamma-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.gamma_blue);
    }
    else if (strcmp(option, "\"xsane-brightness\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness);
    }
    else if (strcmp(option, "\"xsane-brightness-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_red);
    }
    else if (strcmp(option, "\"xsane-brightness-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_green);
    }
    else if (strcmp(option, "\"xsane-brightness-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.brightness_blue);
    }
    else if (strcmp(option, "\"xsane-contrast\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast);
    }
    else if (strcmp(option, "\"xsane-contrast-red\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_red);
    }
    else if (strcmp(option, "\"xsane-contrast-green\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_green);
    }
    else if (strcmp(option, "\"xsane-contrast-blue\"") == 0)
    {
      fscanf(file, "%lf\n", &xsane.contrast_blue);
    }
    else if (strcmp(option, "\"xsane-enhancement-rgb-default\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.enhancement_rgb_default);
    }
    else if (strcmp(option, "\"xsane-negative\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.negative);
    }
    else if (strcmp(option, "\"xsane-show-preview\"") == 0)
    {
      fscanf(file, "%d\n", &xsane.show_preview);
    }
    else if (strcmp(option, "\"xsane-main-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &main_posx);
    }
    else if (strcmp(option, "\"xsane-main-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &main_posy);
    }
    else if (strcmp(option, "\"xsane-main-window-width\"") == 0)
    {
      fscanf(file, "%d\n", &main_width);
    }
    else if (strcmp(option, "\"xsane-main-window-height\"") == 0)
    {
      fscanf(file, "%d\n", &main_height);
    }
    else if (strcmp(option, "\"xsane-standard-options-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &standard_options_posx);
    }
    else if (strcmp(option, "\"xsane-standard-options-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &standard_options_posy);
    }
    else if (strcmp(option, "\"xsane-advanced-options-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &advanced_options_posx);
    }
    else if (strcmp(option, "\"xsane-advanced-options-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &advanced_options_posy);
    }
    else if (strcmp(option, "\"xsane-histogram-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &histogram_posx);
    }
    else if (strcmp(option, "\"xsane-histogram-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &histogram_posy);
    }
    else if (strcmp(option, "\"xsane-preview-window-x-position\"") == 0)
    {
      fscanf(file, "%d\n", &preview_posx);
    }
    else if (strcmp(option, "\"xsane-preview-window-y-position\"") == 0)
    {
      fscanf(file, "%d\n", &preview_posy);
    }
    else if (strcmp(option, "\"xsane-preview-window-width\"") == 0)
    {
      fscanf(file, "%d\n", &preview_width);
    }
    else if (strcmp(option, "\"xsane-preview-window-height\"") == 0)
    {
      fscanf(file, "%d\n", &preview_height);
    }
    else
    {
      fgets(option, sizeof(option), file); /* skip option */
    }
  }
  fclose(file);

#if 0
  if (version)
  {
    if (strcmp(version, XSANE_VERSION))
    {
      snprintf(buf, sizeof(buf), "File: \"%s\"\n"
                                 "has been saved with xsane-%s,\n"
                                 "this may cause problems!", filename, version);
      xsane_back_gtk_warning(buf, TRUE);
    }
    free(version);
  }
  else
  {
    snprintf(buf, sizeof(buf), "File: \"%s\"\n"
                               "has been saved with xsane before version 0.40,\n"
                               "this may cause problems!", filename);
    xsane_back_gtk_warning(buf, TRUE);
  }
#endif


  gtk_widget_set_uposition(xsane.shell, main_posx, main_posy);
  gtk_widget_set_uposition(xsane.standard_options_shell, standard_options_posx, standard_options_posy);
  gtk_widget_set_uposition(xsane.advanced_options_shell, advanced_options_posx, advanced_options_posy);
  gtk_widget_set_uposition(xsane.histogram_dialog, histogram_posx, histogram_posy);

  if (xsane.preview)
  {
    gtk_widget_set_uposition(xsane.preview->top, preview_posx, preview_posy);
  }

  gtk_window_set_default_size(GTK_WINDOW(xsane.shell), main_width, main_height);

  if (xsane.preview)
  {
    gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), preview_width, preview_height);
  }


  fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    return;
  }
  xsane_device_preferences_load_values(fd, dialog->dev);
  close(fd);

  if (dialog->well_known.dpi > 0)
  {
   const SANE_Option_Descriptor *opt;

    opt = sane_get_option_descriptor(dialog->dev, dialog->well_known.dpi);
  
    switch (opt->type)
    {
      case SANE_TYPE_INT:
      {
       SANE_Int dpi;
        sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
        xsane.resolution = dpi;
      }
      break;

      case SANE_TYPE_FIXED:
      {
       SANE_Fixed dpi;
        sane_control_option(dialog->dev, dialog->well_known.dpi, SANE_ACTION_GET_VALUE, &dpi, 0);
        xsane.resolution = (int) SANE_UNFIX(dpi);
      }
      break;
 
      default:
       fprintf(stderr, "xsane_pref_load_file: %s %d\n", ERR_UNKNOWN_TYPE, opt->type);
      return;
    }
  }

  xsane_refresh_dialog(dialog);
  xsane_enhancement_by_gamma();
}

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_restore(void)
{
  char filename[PATH_MAX];

  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  xsane_device_preferences_load_file(filename);
}
                  
/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_load(void)
{
  char filename[PATH_MAX];
  char windowname[256];

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", prog_name, WINDOW_LOAD_SETTINGS, device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename);
  xsane_device_preferences_load_file(filename);
  xsane_set_sensitivity(TRUE);
}        

/* ---------------------------------------------------------------------------------------------------------------------- */

void xsane_device_preferences_save(GtkWidget *widget, gpointer data)
{
 char filename[PATH_MAX];
 char windowname[256];
 int fd;
 FILE *file;
 int posx, posy, width, height;

  xsane_set_sensitivity(FALSE);

  sprintf(windowname, "%s %s %s", prog_name, WINDOW_SAVE_SETTINGS, device_text);
  xsane_back_gtk_make_path(sizeof(filename), filename, "xsane", 0, 0, xsane.device_set_filename, ".drc");
  xsane_back_gtk_get_filename(windowname, filename, sizeof(filename), filename);

  file = fopen(filename, "w");
  if (file == 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }

  fprintf(file, "\"XSANE_DEVICE_RC\"\n");
  fprintf(file, "\"%s\"\n", xsane.device_set_filename);
  fclose(file);

  fd = open(filename, O_WRONLY | O_APPEND , 0666);
  if (fd < 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }
  xsane_device_preferences_save_values(fd, dialog->dev);
  close(fd);


  file = fopen(filename, "a");
  if (file == 0)
  {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s %s.", ERR_FAILED_CREATE_FILE, strerror(errno));
    xsane_back_gtk_error(buf, TRUE);
    xsane_set_sensitivity(TRUE);
    return;
  }

  fprintf(file, "\"xsane-version\"\n");
  fprintf(file, "\"" XSANE_VERSION "\"\n");
  fprintf(file, "\"xsane-gamma\"\n");
  fprintf(file, "%f\n", xsane.gamma);
  fprintf(file, "\"xsane-gamma-red\"\n");
  fprintf(file, "%f\n", xsane.gamma_red);
  fprintf(file, "\"xsane-gamma-green\"\n");
  fprintf(file, "%f\n", xsane.gamma_green);
  fprintf(file, "\"xsane-gamma-blue\"\n");
  fprintf(file, "%f\n", xsane.gamma_blue);

  fprintf(file, "\"xsane-brightness\"\n");
  fprintf(file, "%f\n", xsane.brightness);
  fprintf(file, "\"xsane-brightness-red\"\n");
  fprintf(file, "%f\n", xsane.brightness_red);
  fprintf(file, "\"xsane-brightness-green\"\n");
  fprintf(file, "%f\n", xsane.brightness_green);
  fprintf(file, "\"xsane-brightness-blue\"\n");
  fprintf(file, "%f\n", xsane.brightness_blue);

  fprintf(file, "\"xsane-contrast\"\n");
  fprintf(file, "%f\n", xsane.contrast);
  fprintf(file, "\"xsane-contrast-red\"\n");
  fprintf(file, "%f\n", xsane.contrast_red);
  fprintf(file, "\"xsane-contrast-green\"\n");
  fprintf(file, "%f\n", xsane.contrast_green);
  fprintf(file, "\"xsane-contrast-blue\"\n");
  fprintf(file, "%f\n", xsane.contrast_blue);

  fprintf(file, "\"xsane-enhancement-rgb-default\"\n");
  fprintf(file, "%d\n", xsane.enhancement_rgb_default);

  fprintf(file, "\"xsane-negative\"\n");
  fprintf(file, "%d\n", xsane.negative);

  gdk_window_get_root_origin(xsane.shell->window, &posx, &posy);
  gdk_window_get_size(xsane.shell->window, &width, &height);
  fprintf(file, "\"xsane-main-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-main-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  fprintf(file, "\"xsane-main-window-width\"\n");
  fprintf(file, "%d\n", width);
  fprintf(file, "\"xsane-main-window-height\"\n");
  fprintf(file, "%d\n", height);
  gtk_widget_set_uposition(xsane.shell, posx, posy); /* set default geometry used when window is closed and opened again */
  gtk_window_set_default_size(GTK_WINDOW(xsane.shell), width, height);

  gdk_window_get_root_origin(xsane.standard_options_shell->window, &posx, &posy);
  fprintf(file, "\"xsane-standard-options-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-standard-options-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.standard_options_shell, posx, posy);

  gdk_window_get_root_origin(xsane.advanced_options_shell->window, &posx, &posy);
  fprintf(file, "\"xsane-advanced-options-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-advanced-options-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.advanced_options_shell, posx, posy);

  gdk_window_get_root_origin(xsane.histogram_dialog->window, &posx, &posy);
  fprintf(file, "\"xsane-histogram-window-x-position\"\n");
  fprintf(file, "%d\n", posx);
  fprintf(file, "\"xsane-histogram-window-y-position\"\n");
  fprintf(file, "%d\n", posy);
  gtk_widget_set_uposition(xsane.histogram_dialog, posx, posy);

  fprintf(file, "\"xsane-show-preview\"\n");
  fprintf(file, "%d\n", xsane.show_preview);

  if (xsane.preview)
  {
    gdk_window_get_root_origin(xsane.preview->top->window, &posx, &posy);
    gdk_window_get_size(xsane.preview->top->window, &width, &height);
    fprintf(file, "\"xsane-preview-window-x-position\"\n");
    fprintf(file, "%d\n", posx);
    fprintf(file, "\"xsane-preview-window-y-position\"\n");
    fprintf(file, "%d\n", posy);
    fprintf(file, "\"xsane-preview-window-width\"\n");
    fprintf(file, "%d\n", width);
    fprintf(file, "\"xsane-preview-window-height\"\n");
    fprintf(file, "%d\n", height);
    gtk_widget_set_uposition(xsane.preview->top, posx, posy);
    gtk_window_set_default_size(GTK_WINDOW(xsane.preview->top), width, height);
  }

  fclose(file);

  xsane_set_sensitivity(TRUE);
}

/* ---------------------------------------------------------------------------------------------------------------- */

