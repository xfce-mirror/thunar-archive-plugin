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

#ifndef __TAP_PROVIDER_H__
#define __TAP_PROVIDER_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _TapProviderClass TapProviderClass;
typedef struct _TapProvider      TapProvider;

#define TAP_TYPE_PROVIDER             (tap_provider_get_type ())
#define TAP_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TAP_TYPE_PROVIDER, TapProvider))
#define TAP_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TAP_TYPE_PROVIDER, TapProviderClass))
#define TAP_IS_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TAP_TYPE_PROVIDER))
#define TAP_IS_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TAP_TYPE_PROVIDER))
#define TAP_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TAP_TYPE_PROVIDER, TapProviderClass))

GType tap_provider_get_type      (void) G_GNUC_CONST G_GNUC_INTERNAL;
void  tap_provider_register_type (ThunarxProviderPlugin *plugin) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__TAP_PROVIDER_H__ */
