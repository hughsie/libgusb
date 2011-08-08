/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __GUSB_CONTEXT_H__
#define __GUSB_CONTEXT_H__

#include <glib.h>

G_BEGIN_DECLS

#define GUSB_CONTEXT_ERROR			(g_usb_context_error_quark ())

typedef enum {
	GUSB_CONTEXT_ERROR_INTERNAL
} GUsbContextError;

typedef struct _GUsbContext GUsbContext;

GUsbContext	*g_usb_context_new		(GError		**error);
GQuark		 g_usb_context_error_quark	(void);
void		 g_usb_context_destroy		(GUsbContext	*context);
void		 g_usb_context_set_debug	(GUsbContext	*context,
						 guint		 flags);

G_END_DECLS

#endif /* __GUSB_CONTEXT_H__ */
