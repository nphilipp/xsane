/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-save.h

   Oliver Rauch <Oliver.Rauch@rauch-domain.de>
   Copyright (C) 1998-2004 Oliver Rauch
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

extern int xsane_create_secure_file(const char *filename);
extern void xsane_cancel_save();
extern void xsane_convert_text_to_filename(char **filename);
extern void xsane_ensure_counter_in_filename(char **filename, int counter_len);
extern void xsane_update_counter_in_filename(char **filename, int skip, int step, int min_counter_len);
extern void xsane_increase_counter_in_filename(char *filename, int skip);
extern void xsane_read_pnm_header(FILE *infile, Image_info *image_info);
extern void xsane_write_pnm_header(FILE *outfile, Image_info *image_info, int save_pnm16_as_ascii);
extern int xsane_save_grayscale_image_as_lineart(FILE *outfile, FILE *imagefile, Image_info *image_info, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_scaled_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float x_scale, float y_scale, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_despeckle_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int radius, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_blur_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float radius, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_rotate_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int rotation, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_ps(FILE *outfile, FILE *imagefile, Image_info *image_info,
                          float width, float height,
                          int paper_left_margin, int paper_bottom_margin, int paperwidth, int paperheight, int paper_orientation,
                          GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_jpeg(FILE *outfile, FILE *imagefile, Image_info *image_info, int quality, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_tiff(const char *outfilename, FILE *imagefile, Image_info *image_info, int quality, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_png(FILE *outfile, FILE *imagefile, Image_info *image_info, int compression, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_png_16(FILE *outfile, FILE *imagefile, Image_info *image_info, int compression, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_pnm_16(FILE *outfile, FILE *imagefile, Image_info *image_info, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as_lineart(char *input_filename, char *output_filename, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as_text(char *input_filename, char *output_filename, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as(char *input_filename, char *output_filename, int output_format, GtkProgressBar *progress_bar, int *cancel_save);
extern void null_print_func(gchar *msg);
extern int xsane_transfer_to_gimp(char *input_filename, GtkProgressBar *progress_bar, int *cancel_save);
extern void write_base64(int fd_socket, FILE *infile);
extern void write_mail_header(int fd_socket, char *from, char *reply_to, char *to, char *subject, char *boundary, int related);
extern void write_mail_footer(int fd_socket, char *boundary);
extern void write_mail_mime_ascii(int fd_socket, char *boundary);
extern void write_mail_mime_html(int fd_socket, char *boundary);
extern void write_mail_attach_image_png(int fd_socket, char *boundary, char *content_id, FILE *infile, char *filename);
extern void write_mail_attach_file(int fd_socket, char *boundary, FILE *infile, char *filename);
extern int open_socket(char *server, int port);
extern int pop3_login(int fd_socket, char *user, char *passwd);
extern int write_smtp_header(int fd_socket, char *from, char *to);
extern int write_smtp_footer(int fd_socket);
extern int xsane_copy_file(char *source_filename, char *destination_filename, GtkProgressBar *progress_bar, int *cancel_save);


/* ---------------------------------------------------------------------------------------------------------------------- */
