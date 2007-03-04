/* xsane -- a graphical (X11, gtk) scanner-oriented SANE frontend

   xsane-save.h

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

#ifndef HAVE_XSANE_SAVE_H
#define HAVE_XSANE_SAVE_H

/* ---------------------------------------------------------------------------------------------------------------------- */

#include <xsane.h>
#ifdef HAVE_LIBTIFF
# include "tiffio.h"
#endif

/* ---------------------------------------------------------------------------------------------------------------------- */
                                                                                                                   
/* The pdf_xref struct holds byte offsets from the beginning of the PDF
 * file to each object of the PDF file -- used to build the xref table
 */
#define PDF_PAGES_MAX 1000
struct pdf_xref
{
  unsigned long obj[PDF_PAGES_MAX * 2 + 8];
  unsigned long xref; /* xref table */
  unsigned long slen; /* length of image stream */
  unsigned long slenp; /* position of image stream length */
};
                                                                                                                   
/* ---------------------------------------------------------------------------------------------------------------------- */


extern int xsane_create_secure_file(const char *filename);
extern void xsane_cancel_save();
extern void xsane_convert_text_to_filename(char **filename);
extern int xsane_get_filesize(char *filename);
extern void xsane_ensure_counter_in_filename(char **filename, int counter_len);
extern void xsane_update_counter_in_filename(char **filename, int skip, int step, int min_counter_len);
extern void xsane_increase_counter_in_filename(char *filename, int skip);
extern void xsane_read_pnm_header(FILE *file, Image_info *image_info);
extern void xsane_write_pnm_header(FILE *file, Image_info *image_info, int save_pnm16_as_ascii);
extern int xsane_copy_file(FILE *outfile, FILE *infile, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_copy_file_by_name(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save);
#ifdef HAVE_LIBLCMS
extern cmsHTRANSFORM xsane_create_cms_transform(Image_info *image_info, int cms_function, int cms_intent, int cms_bpc);
#endif
extern int xsane_save_grayscale_image_as_lineart(FILE *outfile, FILE *imagefile, Image_info *image_info, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_scaled_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float x_scale, float y_scale, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_despeckle_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int radius, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_blur_image(FILE *outfile, FILE *imagefile, Image_info *image_info, float radius, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_rotate_image(FILE *outfile, FILE *imagefile, Image_info *image_info, int rotation, GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_ps_create_document_header(FILE *outfile, int pages,
	                                         int paper_left_margin, int paper_bottom_margin,
                                                 int paper_width, int paper_height,
                                                 int paper_orientation, int flatedecode);
extern void xsane_save_ps_create_document_trailer(FILE *outfile, int pages);
extern int xsane_save_ps_page(FILE *outfile, int page,
                       FILE *imagefile, Image_info *image_info, float width, float height,
                       int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                       int flatedecode,
                       cmsHTRANSFORM hTransform, int apply_ICM_profile, int embed_CSA, char *CSA_profile, int intent,
                       GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_ps(FILE *outfile, FILE *imagefile, Image_info *image_info,
                         float width, float height,
                         int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
			 int flatedecode,
                         cmsHTRANSFORM hTransform, int apply_ICM_profile, int embed_CSA, char *CSA_profile,
                         int embed_CRD, char *CRD_profile, int blackpointcompensation, int intent,
                         GtkProgressBar *progress_bar, int *cancel_save);
extern void xsane_save_pdf_create_document_header(FILE *outfile, struct pdf_xref *xref, int pages, int flatedecode);
extern void xsane_save_pdf_create_document_trailer(FILE *outfile, struct pdf_xref *xref, int pages);
extern int xsane_save_pdf_page(FILE *outfile, struct pdf_xref *xref, int page,
                               FILE *imagefile, Image_info *image_info, float width, float height,
                               int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                               int flatedecode,
                               cmsHTRANSFORM hTransform, int embed__scanner_icm_profile, int icc_object,
                               GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_pdf(FILE *outfile, FILE *imagefile, Image_info *image_info,
                          float width, float height,
                          int paper_left_margin, int paper_bottom_margin, int paper_width, int paper_height, int paper_orientation,
                          int flatedecode,
                          cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
                          GtkProgressBar *progress_bar, int *cancel_save);
#ifdef HAVE_LIBJPEG
extern int xsane_save_jpeg(FILE *outfile, int quality, FILE *imagefile, Image_info *image_info, cmsHTRANSFORM hTransform,  int apply_ICM_profile, int cms_function, GtkProgressBar *progress_bar, int *cancel_save);
#endif
#ifdef HAVE_LIBTIFF
extern int xsane_save_tiff_page(TIFF *tiffile, int page, int pages, int quality, FILE *imagefile, Image_info *image_info, cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function,
	                         GtkProgressBar *progress_bar, int *cancel_save);
#endif
extern int xsane_save_png(FILE *outfile, int compression, FILE *imagefile, Image_info *image_info, cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_png_16(FILE *outfile, int compression, FILE *imagefile, Image_info *image_info, cmsHTRANSFORM hTransform, int apply_ICM_profile, int cms_function, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_pnm_16(FILE *outfile, FILE *imagefile, Image_info *image_info, cmsHTRANSFORM hTransform, int apply_ICM_profile, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as_lineart(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as_text(char *output_filename, char *input_filename, GtkProgressBar *progress_bar, int *cancel_save);
extern int xsane_save_image_as(char *output_filename, char *input_filename, int output_format, int apply_ICM_profile, int cms_function, int cms_intent, int cms_bpc, GtkProgressBar *progress_bar, int *cancel_save);
extern void null_print_func(gchar *msg);
extern int xsane_transfer_to_gimp(char *input_filename, int apply_ICM_profile, int cms_function, GtkProgressBar *progress_bar, int *cancel_save);
extern void write_base64(int fd_socket, FILE *infile);
extern void write_email_header(int fd_socket, char *from, char *reply_to, char *to, char *subject, char *boundary, int related);
extern void write_email_footer(int fd_socket, char *boundary);
extern void write_email_mime_ascii(int fd_socket, char *boundary);
extern void write_email_mime_html(int fd_socket, char *boundary);
extern void write_email_attach_image(int fd_socket, char *boundary, char *content_id, char *content_type, FILE *infile, char *filename);
extern void write_email_attach_file(int fd_socket, char *boundary, FILE *infile, char *filename);
extern int open_socket(char *server, int port);
extern int pop3_login(int fd_socket, char *user, char *passwd);
extern int write_smtp_header(int fd_socket, char *from, char *to, int auth_type, char *user, char *pass);
extern int write_smtp_footer(int fd_socket);

/* ---------------------------------------------------------------------------------------------------------------------- */
#endif
/* ---------------------------------------------------------------------------------------------------------------------- */
