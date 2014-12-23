/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
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
g_usb_source_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("g_usb_source_error");
	return quark;
}

/**
 * g_usb_source_set_callback:
 * @source: a #GUsbSource
 * @func: a function to call
 * @data: data to pass to @func
 * @notify: a #GDestroyNotify
 *
 * This function does nothing.
 *
 * Since: 0.1.0
 **/
void
g_usb_source_set_callback (GUsbSource     *source,
                           GSourceFunc     func,
                           gpointer        data,
                           GDestroyNotify  notify)
{
	g_source_set_callback ((GSource *)source, func, data, notify);
}
