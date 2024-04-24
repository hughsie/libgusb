/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-source
 * @short_description: GSource integration for libusb
 *
 * This object used to integrate libusb into the GLib main loop before we used
 * a thread. It's now pretty much unused.
 */

#include "config.h"

#include "gusb-source.h"

/**
 * g_usb_source_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
GQuark
g_usb_source_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("g_usb_source_error");
	return quark;
}

/**
 * g_usb_source_set_callback:
 * @self: a #GUsbSource
 * @func: a function to call
 * @data: data to pass to @func
 * @notify: a #GDestroyNotify
 *
 * This function does nothing.
 *
 * Since: 0.1.0
 **/
void
g_usb_source_set_callback(GUsbSource *self, GSourceFunc func, gpointer data, GDestroyNotify notify)
{
	g_source_set_callback((GSource *)self, func, data, notify);
}
