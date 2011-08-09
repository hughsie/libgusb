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

/**
 * SECTION:gusb-context
 * @short_description: Per-thread instance integration for libusb
 *
 * This object is used to get a context that is thread safe.
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

/**
 * _g_usb_context_get_context:
 * @context: a #GUsbContext
 *
 * Gets the internal libusb_context.
 *
 * Return value: (transfer none): the libusb_context
 *
 * Since: 0.0.1
 **/
libusb_context *
_g_usb_context_get_context (GUsbContext *context)
{
	g_return_val_if_fail (context != NULL, NULL);
	return context->ctx;
}

/**
 * g_usb_context_set_debug:
 * @context: a #GUsbContext
 * @flags: a GLogLevelFlags such as %G_LOG_LEVEL_ERROR | %G_LOG_LEVEL_INFO, or 0
 *
 * Sets the debug flags which control what is logged to the console.
 *
 * Using %G_LOG_LEVEL_INFO will output to standard out, and everything
 * else logs to standard error.
 *
 * Since: 0.0.1
 **/
void
g_usb_context_set_debug (GUsbContext *context, GLogLevelFlags flags)
{
	guint level = 0;

	g_return_if_fail (context != NULL);
	g_return_if_fail (context->ctx != NULL);

	if ((flags & G_LOG_LEVEL_ERROR) > 0)
		level = 4;
	else if ((flags & G_LOG_LEVEL_WARNING) > 0)
		level = 3;
	else if ((flags & G_LOG_LEVEL_INFO) > 0)
		level = 2;
	else if ((flags & G_LOG_LEVEL_DEBUG) > 0)
		level = 1;

	libusb_set_debug (context->ctx, level);
}

/**
 * g_usb_context_new:
 * @error: a #GError, or %NULL
 *
 * Creates a new context for accessing USB devices.
 *
 * Return value: (transfer none): the %GUsbContext. Use g_usb_context_destroy() to unref.
 *
 * Since: 0.0.1
 **/
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
			     G_USB_CONTEXT_ERROR,
			     G_USB_CONTEXT_ERROR_INTERNAL,
			     "failed to init libusb: %s [%i]",
			     libusb_strerror (rc), rc);
	}
	return context;
}

/**
 * g_usb_context_destroy:
 * @context: a #GUsbContext
 *
 * Destroys a context.
 *
 * Since: 0.0.1
 **/
void
g_usb_context_destroy (GUsbContext *context)
{
	g_return_if_fail (context != NULL);
	g_return_if_fail (context->ctx != NULL);
	libusb_exit (context->ctx);
	context->ctx = NULL;
}
