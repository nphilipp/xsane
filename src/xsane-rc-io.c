/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-rc-io.c

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

/* ---------------------------------------------------------------------------------------------------------------- */

#ifdef _AIX
# include <lalloca.h>	/* MUST come first for AIX! */
#endif
#include <sane/config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sane/sane.h>
#include <sane/config.h>
#include <ctype.h>
#include <unistd.h>

#ifdef HAVE_LIBC_H
# include <libc.h>	/* NeXTStep/OpenStep */
#endif

#include <sane/sane.h>
#include "xsane-rc-io.h"

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_space(Wire *w, size_t howmuch)
{
  size_t nbytes, left_over;
  int fd = w->io.fd;
  ssize_t nread, nwritten;

  if (w->buffer.curr + howmuch > w->buffer.end)
  {
    switch (w->direction)
    {
      case WIRE_ENCODE:
        nbytes = w->buffer.curr - w->buffer.start;
        w->buffer.curr = w->buffer.start;
        while (nbytes > 0)
        {
          nwritten = (*w->io.write) (fd, w->buffer.curr, nbytes);
          if (nwritten < 0)
          {
            w->status = errno;
            return;
          }
          w->buffer.curr += nwritten;
          nbytes -= nwritten;
        }

        w->buffer.curr = w->buffer.start;
        w->buffer.end  = w->buffer.start + w->buffer.size;
        break;

      case WIRE_DECODE:
        left_over = w->buffer.end - w->buffer.curr;
        if (left_over)
        {
          memcpy(w->buffer.start, w->buffer.curr, left_over);
        }
        w->buffer.curr = w->buffer.start;
        w->buffer.end  = w->buffer.start + left_over;

        do
        {
          nread = (*w->io.read) (fd, w->buffer.end, w->buffer.size - left_over);
          if (nread <= 0)
          {
            if (nread == 0)
            {
/*              errno = EINVAL; */
              errno = XSANE_EOF;
            }
            w->status = errno;
            return;
          }
          left_over += nread;
          w->buffer.end += nread;
        } while (left_over < howmuch);
        break;

      case WIRE_FREE:
        break;
    }
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_void(Wire *w)
{
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_array(Wire *w, SANE_Word *len_ptr, void **v, WireCodecFunc w_element, size_t element_size)
{
 SANE_Word len;
 char *val;
 int i;

  if (w->direction == WIRE_FREE)
  {
    free(*v);
    return;
  }

  if (w->direction == WIRE_ENCODE)
  {
    len = *len_ptr;
  }

  xsane_rc_io_w_word(w, &len);

  if (w->direction == WIRE_DECODE)
  {
    *len_ptr = len;
    if (len)
    {
      *v = malloc(len * element_size);

      if (*v == 0)
      {
        /* Malloc failed, so return an error. */
        w->status = ENOMEM;
        return;
      }
    }
    else
    {
      *v = 0;
    }
  }

  val = *v;

  for (i = 0; i < len; ++i)
  {
    (*w_element) (w, val);
    val += element_size;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_ptr(Wire *w, void **v, WireCodecFunc w_value, size_t value_size)
{
 SANE_Word is_null;

  if (w->direction == WIRE_FREE)
  {
    if (*v)
    {
      free(*v);
    }
    return;
  }

  if (w->direction == WIRE_ENCODE)
  {
    is_null = (*v == 0);
  }

  xsane_rc_io_w_word(w, &is_null);

  if (!is_null)
  {
    if (w->direction == WIRE_DECODE)
    {
      *v = malloc(value_size);

      if (*v == 0)
      {
        /* Malloc failed, so return an error. */
        w->status = ENOMEM;
        return;
      }
    }
    (*w_value) (w, *v);
  }
  else if (w->direction == WIRE_DECODE)
  {
    *v = 0;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_status(Wire *w, SANE_Status *v)
{
  SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
  {
    *v = word;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_bool(Wire *w, SANE_Bool *v)
{
 SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
  {
    *v = word;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_constraint_type(Wire *w, SANE_Constraint_Type *v)
{
 SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_value_type(Wire *w, SANE_Value_Type *v)
{
 SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_unit(Wire *w, SANE_Unit *v)
{
  SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
  {
    *v = word;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_action(Wire *w, SANE_Action *v)
{
 SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
  {
    *v = word;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_frame(Wire *w, SANE_Frame *v)
{
 SANE_Word word = *v;

  xsane_rc_io_w_word(w, &word);
  if (w->direction == WIRE_DECODE)
  {
    *v = word;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_range(Wire *w, SANE_Range *v)
{
  xsane_rc_io_w_word(w, &v->min);
  xsane_rc_io_w_word(w, &v->max);
  xsane_rc_io_w_word(w, &v->quant);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_device(Wire *w, SANE_Device *v)
{
  xsane_rc_io_w_string(w, (SANE_String *) &v->name);
  xsane_rc_io_w_string(w, (SANE_String *) &v->vendor);
  xsane_rc_io_w_string(w, (SANE_String *) &v->model);
  xsane_rc_io_w_string(w, (SANE_String *) &v->type);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_device_ptr(Wire *w, SANE_Device **v)
{
  xsane_rc_io_w_ptr(w, (void **) v, (WireCodecFunc) xsane_rc_io_w_device, sizeof (**v));
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_option_descriptor(Wire *w, SANE_Option_Descriptor *v)
{
 SANE_Word len;

  xsane_rc_io_w_string(w, (SANE_String *) &v->name);
  xsane_rc_io_w_string(w, (SANE_String *) &v->title);
  xsane_rc_io_w_string(w, (SANE_String *) &v->desc);
  xsane_rc_io_w_value_type(w, &v->type);
  xsane_rc_io_w_unit(w, &v->unit);
  xsane_rc_io_w_word(w, &v->size);
  xsane_rc_io_w_word(w, &v->cap);
  xsane_rc_io_w_constraint_type(w, &v->constraint_type);

  switch (v->constraint_type)
  {
    case SANE_CONSTRAINT_NONE:
      break;

    case SANE_CONSTRAINT_RANGE:
      xsane_rc_io_w_ptr(w, (void **) &v->constraint.range, (WireCodecFunc) xsane_rc_io_w_range, sizeof (SANE_Range));
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      if (w->direction == WIRE_ENCODE)
	len = v->constraint.word_list[0] + 1;
      xsane_rc_io_w_array(w, &len, (void **) &v->constraint.word_list, w->codec.w_word, sizeof(SANE_Word));
      break;

    case SANE_CONSTRAINT_STRING_LIST:
      if (w->direction == WIRE_ENCODE)
	{
	  for (len = 0; v->constraint.string_list[len]; ++len);
	  ++len;	/* send NULL string, too */
	}
      xsane_rc_io_w_array(w, &len, (void **) &v->constraint.string_list, w->codec.w_string, sizeof(SANE_String));
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_option_descriptor_ptr(Wire *w, SANE_Option_Descriptor **v)
{
  xsane_rc_io_w_ptr(w, (void **) v, (WireCodecFunc) xsane_rc_io_w_option_descriptor, sizeof (**v));
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_parameters(Wire *w, SANE_Parameters *v)
{
  xsane_rc_io_w_frame(w, &v->format);
  xsane_rc_io_w_bool(w, &v->last_frame);
  xsane_rc_io_w_word(w, &v->bytes_per_line);
  xsane_rc_io_w_word(w, &v->pixels_per_line);
  xsane_rc_io_w_word(w, &v->lines);
  xsane_rc_io_w_word(w, &v->depth);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_flush(Wire *w)
{
  w->status = 0;

  if (w->direction == WIRE_ENCODE)
  {
    xsane_rc_io_w_space(w, w->buffer.size + 1);
  }
  else if (w->direction == WIRE_DECODE)
  {
    w->buffer.curr = w->buffer.end = w->buffer.start;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_set_dir(Wire *w, WireDirection dir)
{
  xsane_rc_io_w_flush(w);
  w->direction = dir;
  xsane_rc_io_w_flush(w);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_call(Wire *w, SANE_Word procnum, WireCodecFunc w_arg, void *arg, WireCodecFunc w_reply, void *reply)
{
  w->status = 0;
  xsane_rc_io_w_set_dir(w, WIRE_ENCODE);

  xsane_rc_io_w_word(w, &procnum);
  (*w_arg) (w, arg);

  if (w->status == 0)
  {
    xsane_rc_io_w_set_dir(w, WIRE_DECODE);
    (*w_reply) (w, reply);
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_reply(Wire *w, WireCodecFunc w_reply, void *reply)
{
  w->status = 0;
  xsane_rc_io_w_set_dir(w, WIRE_ENCODE);
  (*w_reply) (w, reply);
  xsane_rc_io_w_flush(w);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_free(Wire *w, WireCodecFunc w_reply, void *reply)
{
  WireDirection saved_dir = w->direction;

  w->direction = WIRE_FREE;
  (*w_reply) (w, reply);
  w->direction = saved_dir;
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_init(Wire *w)
{
  w->status = 0;
  w->direction = WIRE_ENCODE;
  w->buffer.size = 8192;
  w->buffer.start = malloc(w->buffer.size);

  if (w->buffer.start == 0) /* Malloc failed, so return an error. */
  {
    w->status = ENOMEM;
  }

  w->buffer.curr = w->buffer.start;
  w->buffer.end = w->buffer.start + w->buffer.size;
}

/* ---------------------------------------------------------------------------------------------------------------- */

static const char *hexdigit = "0123456789abcdef";

/* ---------------------------------------------------------------------------------------------------------------- */

static void xsane_rc_io_skip_ws(Wire *w)
{
  while (1)
  {
    xsane_rc_io_w_space(w, 1);

    if (w->status != 0)
    {
      return;
    }

    if (!isspace(*w->buffer.curr))
    {
      return;
    }

    ++w->buffer.curr;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_skip_newline(Wire *w)
{
  while (*w->buffer.curr != 10)
  {
    xsane_rc_io_w_space(w, 1);

    if (w->status != 0)
    {
      return;
    }
    ++w->buffer.curr;
  }
  ++w->buffer.curr;
}

/* ---------------------------------------------------------------------------------------------------------------- */

static unsigned xsane_rc_io_get_digit(Wire *w)
{
 unsigned digit;

  xsane_rc_io_w_space(w, 1);
  digit = tolower(*w->buffer.curr++) - '0';

  if (digit > 9)
  {
    digit -= 'a' - ('9' + 1);
  }

  if (digit > 0xf)
  {
    w->status = EINVAL;
    return 0;
  }
  return digit;
}

/* ---------------------------------------------------------------------------------------------------------------- */

static SANE_Byte xsane_rc_io_get_byte(Wire *w)
{
  return xsane_rc_io_get_digit(w) << 4 | xsane_rc_io_get_digit(w);
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_byte(Wire *w, SANE_Byte *v)
{
 SANE_Byte *b = v;

  switch (w->direction)
  {
    case WIRE_ENCODE:
      xsane_rc_io_w_space(w, 3);
      *w->buffer.curr++ = hexdigit[(*b >> 4) & 0x0f];
      *w->buffer.curr++ = hexdigit[(*b >> 0) & 0x0f];
      *w->buffer.curr++ = '\n';
      break;

    case WIRE_DECODE:
      xsane_rc_io_skip_ws(w);
      *b = xsane_rc_io_get_byte(w);
      break;

    case WIRE_FREE:
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_char(Wire *w, SANE_Char *v)
{
 SANE_Char *c = v;

  switch (w->direction)
  {
    case WIRE_ENCODE:
      xsane_rc_io_w_space(w, 5);
      *w->buffer.curr++ = '\'';

      if (*c == '\'' || *c == '\\')
      {
	*w->buffer.curr++ = '\\';
      }

      *w->buffer.curr++ = *c;
      *w->buffer.curr++ = '\'';
      *w->buffer.curr++ = '\n';
      break;

    case WIRE_DECODE:
      xsane_rc_io_w_space(w, 4);
      if (*w->buffer.curr++ != '\'')
      {
        w->status = EINVAL;
        return;
      }
      *c = *w->buffer.curr++;

      if (*c == '\\')
      {
        xsane_rc_io_w_space(w, 2);
        *c = *w->buffer.curr++;
      }

      if (*w->buffer.curr++ != '\'')
      {
        w->status = EINVAL;
        return;
      }
      break;

    case WIRE_FREE:
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_string(Wire *w, SANE_String *s)
{
 size_t len, alloced_len;
 char * str, ch;
 int done;

  switch (w->direction)
  {
    case WIRE_ENCODE:
    if (*s)
    {
      xsane_rc_io_w_space(w, 1);
      *w->buffer.curr++ = '"';
      str = *s;
      while ((ch = *str++))
      {
        xsane_rc_io_w_space(w, 2);
        if (ch == '"' || ch == '\\')
        {
          *w->buffer.curr++ = '\\';
        }
        *w->buffer.curr++ = ch;
      }
      *w->buffer.curr++ = '"';
    }
    else
    {
      xsane_rc_io_w_space(w, 5);
      *w->buffer.curr++ = '(';
      *w->buffer.curr++ = 'n';
      *w->buffer.curr++ = 'i';
      *w->buffer.curr++ = 'l';
      *w->buffer.curr++ = ')';
    }

    xsane_rc_io_w_space(w, 1);
    *w->buffer.curr++ = '\n';
    break;

    case WIRE_DECODE:
      xsane_rc_io_skip_ws(w);
      xsane_rc_io_w_space(w, 1);

      if (w->status != 0)
      {
        *s = 0; /* make sure pointer does not point to an invalid address */
        return;
      }

      ch = *w->buffer.curr++;
      if (ch == '"')
      {
        alloced_len = len = 0;
        str = 0;
        done = 0;

        do
        {
          xsane_rc_io_w_space(w, 1);

          if (w->status != 0)
          {
            return;
          }

          ch = *w->buffer.curr++;
          if (ch == '"')
          {
            done = 1;
          }

          if (ch == '\\')
          {
            xsane_rc_io_w_space(w, 1);
            ch = *w->buffer.curr++;
          }

          if (len >= alloced_len)
          {
            alloced_len += 1024;
            if (!str)
            {
              str = malloc(alloced_len);
            }
            else
            {
              str = realloc(str, alloced_len);
            }

            if (str == 0)
            {
              /* Malloc failed, so return an error. */
              w->status = ENOMEM;
              return;
            }
          }
          str[len++] = ch;
        }
        while(!done);

        str[len - 1] = '\0';
        *s = realloc(str, len);

        if (*s == 0)
        {
         /* Malloc failed, so return an error. */
          w->status = ENOMEM;
          return;
        }
      }
      else /* string does not begin with a " */
      {
        w->status = EINVAL;
        *s = 0; /* make sure pointer does not point to an invalid address */
        return;
      }
      break;

    case WIRE_FREE:
      if (*s)
      {
	free(*s);
      }
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_io_w_word(Wire *w, SANE_Word *v)
{
 SANE_Word val, *word = v;
 int i, is_negative = 0;
 char buf[16];

  switch (w->direction)
  {
    case WIRE_ENCODE:
      val = *word;
      i = sizeof(buf) - 1;

      if (val < 0)
      {
        is_negative = 1;
        val = -val;
      }

      do
      {
        buf[i--] = '0' + (val % 10);
        val /= 10;
      }
      while (val);

      if (is_negative)
      {
	buf[i--] = '-';
      }

      xsane_rc_io_w_space(w, sizeof(buf) - i);
      memcpy(w->buffer.curr, buf + i + 1, sizeof(buf) - i - 1);
      w->buffer.curr += sizeof(buf) - i - 1;
      *w->buffer.curr++ = '\n';
      break;

    case WIRE_DECODE:
      xsane_rc_io_skip_ws(w);
      val = 0;
      xsane_rc_io_w_space(w, 1);
      if (*w->buffer.curr == '-')
      {
        is_negative = 1;
        ++w->buffer.curr;
      }

      while (1)
      {
        xsane_rc_io_w_space(w, 1);

        if (w->status != 0)
        {
          return;
        }

        if (!isdigit (*w->buffer.curr))
        {
          break;
        }

        val = 10*val + (*w->buffer.curr++ - '0');
      }
      *word = is_negative ? -val : val;
      break;

    case WIRE_FREE:
      break;
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

#define PFIELD(p,offset,type)	(*((type *)(((char *)(p)) + (offset))))

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_pref_string(Wire *w, void *p, long offset)
{
  SANE_String string;

  if (w->direction == WIRE_ENCODE)
  {
    string = PFIELD(p, offset, char *);
  }

  xsane_rc_io_w_string(w, &string);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
     const char **field;

      field = &PFIELD(p, offset, const char *);
      if (*field)
      {
        free((char *) *field);
      }
      *field = string ? strdup (string) : 0;
    }
    xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_string, &string);
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_pref_double(Wire *w, void *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
  {
    word = SANE_FIX(PFIELD (p, offset, double));
  }

  xsane_rc_io_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
      PFIELD(p, offset, double) = SANE_UNFIX (word);
    }
    xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_word, &word);
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */

void xsane_rc_pref_int(Wire *w, void *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
  {
    word = PFIELD(p, offset, int);
  }

  xsane_rc_io_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
  {
    if (w->status == 0)
    {
      PFIELD(p, offset, int) = word;
    }
    xsane_rc_io_w_free(w, (WireCodecFunc) xsane_rc_io_w_word, &word);
  }
}

/* ---------------------------------------------------------------------------------------------------------------- */