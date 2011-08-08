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

#include "config.h"

#include <libusb-1.0/libusb.h>

#include "gusb-context.h"
#include "gusb-context-private.h"

/* libusb_strerror is in upstream */
#define libusb_strerror(error) "unknown"

struct _GUsbContext {
	libusb_context	*ctx;
};

/**
 * g_usb_context_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.0.1
 **/
GQuark
g_usb_context_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("g_usb_context_error");
	return quark;
}

libusb_context *
_g_usb_context_get_context (GUsbContext *context)
{
	g_return_val_if_fail (context != NULL, NULL);
	return context->ctx;
}

void
g_usb_context_set_debug (GUsbContext *context, guint flags)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (context->ctx != NULL);
	libusb_set_debug (context->ctx, flags);
}

GUsbContext *
g_usb_context_new (GError **error)
{
	gint rc;
	GUsbContext *context;

	context = g_new0 (GUsbContext, 1);
	rc = libusb_init (&context->ctx);
	if (rc < 0) {
		g_free (context);
		context = NULL;
		g_set_error (error,
			     GUSB_CONTEXT_ERROR,
			     GUSB_CONTEXT_ERROR_INTERNAL,
			     "failed to init libusb: %s [%i]",
			     libusb_strerror (rc), rc);
	}
	return context;
}

void
g_usb_context_destroy (GUsbContext *context)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (context->ctx != NULL);
	libusb_exit (context->ctx);
	context->ctx = NULL;
}
