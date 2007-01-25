/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-rc-io.h

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

/* ---------------------------------------------------------------------------------------------------------------- */

#ifndef xsane_rc_io_h
#define xsane_rc_io_h

#include <sys/types.h>

/* ---------------------------------------------------------------------------------------------------------------------- */

#define XSANE_EOF -1   

/* ---------------------------------------------------------------------------------------------------------------- */

typedef enum
  {
    WIRE_ENCODE = 0,
    WIRE_DECODE,
    WIRE_FREE
  }
WireDirection;

/* ---------------------------------------------------------------------------------------------------------------- */

struct Wire;

/* ---------------------------------------------------------------------------------------------------------------- */

typedef void (*WireCodecFunc) (struct Wire *w, void *val_ptr);
typedef ssize_t (*WireReadFunc) (int fd, void * buf, size_t len);
typedef ssize_t (*WireWriteFunc) (int fd, const void * buf, size_t len);

/* ---------------------------------------------------------------------------------------------------------------- */

typedef struct Wire
  {
    int version;		/* protocol version in use */
    WireDirection direction;
    int status;
    struct
      {
	WireCodecFunc w_byte;
	WireCodecFunc w_char;
	WireCodecFunc w_word;
	WireCodecFunc w_string;
      }
    codec;
    struct
      {
	size_t size;
	char *curr;
	char *start;
	char *end;
      }
    buffer;
    struct
      {
	int fd;
	WireReadFunc read;
	WireWriteFunc write;
      }
    io;
  }
Wire;

/* ---------------------------------------------------------------------------------------------------------------- */

extern void xsane_rc_io_w_init(Wire *w);
extern void xsane_rc_io_w_exit(Wire *w);
extern void xsane_rc_io_w_space(Wire *w, size_t howmuch);
extern void xsane_rc_io_w_skip_newline(Wire *w);
extern void xsane_rc_io_w_void(Wire *w);
extern void xsane_rc_io_w_byte(Wire *w, SANE_Byte *v);
extern void xsane_rc_io_w_char(Wire *w, SANE_Char *v);
extern void xsane_rc_io_w_word(Wire *w, SANE_Word *v);
extern void xsane_rc_io_w_string(Wire *w, SANE_String *v);
extern void xsane_rc_io_w_status(Wire *w, SANE_Status *v);
extern void xsane_rc_io_w_constraint_type(Wire *w, SANE_Constraint_Type *v);
extern void xsane_rc_io_w_value_type(Wire *w, SANE_Value_Type *v);
extern void xsane_rc_io_w_unit(Wire *w, SANE_Unit *v);
extern void xsane_rc_io_w_action(Wire *w, SANE_Action *v);
extern void xsane_rc_io_w_frame(Wire *w, SANE_Frame *v);
extern void xsane_rc_io_w_range(Wire *w, SANE_Range *v);
extern void xsane_rc_io_w_range_ptr(Wire *w, SANE_Range **v);
extern void xsane_rc_io_w_device(Wire *w, SANE_Device *v);
extern void xsane_rc_io_w_device_ptr(Wire *w, SANE_Device **v);
extern void xsane_rc_io_w_option_descriptor(Wire *w, SANE_Option_Descriptor *v);
extern void xsane_rc_io_w_option_descriptor_ptr(Wire *w, SANE_Option_Descriptor **v);
extern void xsane_rc_io_w_parameters(Wire *w, SANE_Parameters *v);
extern void xsane_rc_io_w_array(Wire *w, SANE_Word *len, char **v, WireCodecFunc w_element, size_t element_size);
extern void xsane_rc_io_w_flush(Wire *w);
extern void xsane_rc_io_w_set_dir(Wire *w, WireDirection dir);
extern void xsane_rc_io_w_call(Wire *w, SANE_Word proc_num, WireCodecFunc w_arg, void *arg, WireCodecFunc w_reply, void *reply);
extern void xsane_rc_io_w_reply(Wire *w, WireCodecFunc w_reply, void *reply);
extern void xsane_rc_io_w_free(Wire *w, WireCodecFunc w_reply, void *reply);

extern void xsane_rc_pref_string(Wire *w, void *p, long offset);
extern void xsane_rc_pref_double(Wire *w, void *p, long offset);
extern void xsane_rc_pref_int(Wire *w, void *p, long offset);

/* ---------------------------------------------------------------------------------------------------------------- */

#endif /* xsane_rc_io_wire_h */
