/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
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

#include "gusb-version.h"

/**
 * g_usb_version_string:
 *
 * Gets the GUsb installed runtime version.
 *
 * Returns: a version numer, e.g. "0.3.1"
 *
 * Since: 0.3.1
 **/
const gchar *
g_usb_version_string (void)
{
	return G_STRINGIFY(G_USB_MAJOR_VERSION) "."
		G_STRINGIFY(G_USB_MINOR_VERSION) "."
		G_STRINGIFY(G_USB_MICRO_VERSION);
}
