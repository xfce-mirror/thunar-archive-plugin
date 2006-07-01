/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __TAP_BACKEND_H__
#define __TAP_BACKEND_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

GPid tap_backend_create_archive (const gchar *folder,
                                 GList       *files,
                                 GtkWidget   *window,
                                 GError     **error) G_GNUC_INTERNAL;

GPid tap_backend_extract_here   (const gchar *folder,
                                 GList       *files,
                                 GtkWidget   *window,
                                 GError     **error) G_GNUC_INTERNAL;

GPid tap_backend_extract_to     (const gchar *folder,
                                 GList       *files,
                                 GtkWidget   *window,
                                 GError     **error) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__TAP_BACKEND_H__ */
