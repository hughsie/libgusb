/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GUSB_CONTEXT_H__
#define __GUSB_CONTEXT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define G_USB_TYPE_CONTEXT		(g_usb_context_get_type ())
#define G_USB_CONTEXT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_CONTEXT, GUsbContext))
#define G_USB_IS_CONTEXT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_CONTEXT))
#define G_USB_CONTEXT_ERROR		(g_usb_context_error_quark ())

typedef struct _GUsbContextPrivate	GUsbContextPrivate;
typedef struct _GUsbContext		GUsbContext;
typedef struct _GUsbContextClass	GUsbContextClass;

struct _GUsbContext
{
	 GObject			 parent;
	 GUsbContextPrivate		*priv;
};

struct _GUsbContextClass
{
	GObjectClass			 parent_class;
};

typedef enum {
	G_USB_CONTEXT_ERROR_INTERNAL
} GUsbContextError;

GType		 g_usb_context_get_type		(void);
GQuark		 g_usb_context_error_quark	(void);

GUsbContext	*g_usb_context_new		(GError		**error);

void		 g_usb_context_set_debug	(GUsbContext	*context,
						 GLogLevelFlags	 flags);

G_END_DECLS

#endif /* __GUSB_CONTEXT_H__ */
