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

/* libusb_strerror is awaiting merging upstream */
#define libusb_strerror(error) "unknown"

static void g_usb_context_finalize (GObject *object);

#define G_USB_CONTEXT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), G_USB_TYPE_CONTEXT, GUsbContextPrivate))

enum {
	PROP_0,
	PROP_LIBUSB_CONTEXT,
	PROP_DEBUG_LEVEL,
};

/**
 * GUsbContextPrivate:
 *
 * Private #GUsbContext data
 **/
struct _GUsbContextPrivate
{
	libusb_context		*context;
	int			 debug_level;
};

G_DEFINE_TYPE (GUsbContext, g_usb_context, G_TYPE_OBJECT)

/**
 * usb_context_get_property:
 **/
static void
g_usb_context_get_property (GObject		*object,
			    guint		 prop_id,
			    GValue		*value,
			    GParamSpec		*pspec)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	switch (prop_id) {
	case PROP_LIBUSB_CONTEXT:
		g_value_set_pointer (value, priv->context);
		break;
	case PROP_DEBUG_LEVEL:
		g_value_set_int (value, priv->debug_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * usb_context_set_property:
 **/
static void
g_usb_context_set_property (GObject		*object,
			   guint		 prop_id,
			   const GValue		*value,
			   GParamSpec		*pspec)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	switch (prop_id) {
	case PROP_LIBUSB_CONTEXT:
		priv->context = g_value_get_pointer (value);
		break;
	case PROP_DEBUG_LEVEL:
		priv->debug_level = g_value_get_int (value);
		libusb_set_debug (priv->context, priv->debug_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GObject *
g_usb_context_constructor (GType			 gtype,
			   guint			 n_properties,
			   GObjectConstructParam	*properties)
{
	GObject *obj;
	GUsbContext *context;
	GUsbContextPrivate *priv;

	{
		/* Always chain up to the parent constructor */
		GObjectClass *parent_class;
		parent_class = G_OBJECT_CLASS (g_usb_context_parent_class);
		obj = parent_class->constructor (gtype, n_properties,
						 properties);
	}

	context = G_USB_CONTEXT (obj);
	priv = context->priv;

	/*
	 * Yes you're reading this right the sole reason for this constructor
	 * is to check the context has been set (for now).
	 */
	if (!priv->context)
		g_error("constructed without a context");

	return obj;
}

/**
 * usb_context_class_init:
 **/
static void
g_usb_context_class_init (GUsbContextClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor	= g_usb_context_constructor;
	object_class->finalize		= g_usb_context_finalize;
	object_class->get_property	= g_usb_context_get_property;
	object_class->set_property	= g_usb_context_set_property;

	/**
	 * GUsbContext:libusb_context:
	 */
	pspec = g_param_spec_pointer ("libusb_context", NULL, NULL,
				      G_PARAM_CONSTRUCT_ONLY|
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LIBUSB_CONTEXT,
					 pspec);

	/**
	 * GUsbContext:debug_level:
	 */
	pspec = g_param_spec_int ("debug_level", NULL, NULL,
				  0, 3, 0,
				  G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEBUG_LEVEL,
					 pspec);

	g_type_class_add_private (klass, sizeof (GUsbContextPrivate));
}

/**
 * g_usb_context_init:
 **/
static void
g_usb_context_init (GUsbContext *context)
{
	context->priv = G_USB_CONTEXT_GET_PRIVATE (context);
}

/**
 * g_usb_context_finalize:
 **/
static void
g_usb_context_finalize (GObject *object)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	libusb_exit (priv->context);

	G_OBJECT_CLASS (g_usb_context_parent_class)->finalize (object);
}

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
	return context->priv->context;
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
	GUsbContextPrivate *priv = context->priv;

	if (flags & (G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO))
		priv->debug_level = 3;
	else if (flags & (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING))
		priv->debug_level = 2;
	else if (flags & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR))
		priv->debug_level = 1;
	else
		priv->debug_level = 0;

	libusb_set_debug (priv->context, priv->debug_level);
}

/**
 * g_usb_context_new:
 * @error: a #GError, or %NULL
 *
 * Creates a new context for accessing USB devices.
 *
 * Return value: a new %GUsbContext object or %NULL on error.
 *
 * Since: 0.0.1
 **/
GUsbContext *
g_usb_context_new (GError **error)
{
	gint rc;
	GObject *obj;
	libusb_context *context;

	rc = libusb_init (&context);
	if (rc < 0) {
		g_set_error (error,
			     G_USB_CONTEXT_ERROR,
			     G_USB_CONTEXT_ERROR_INTERNAL,
			     "failed to init libusb: %s [%i]",
			     libusb_strerror (rc), rc);
		return NULL;
	}

	obj = g_object_new (G_USB_TYPE_CONTEXT, "libusb_context", context,
			    NULL);
	return G_USB_CONTEXT (obj);
}
