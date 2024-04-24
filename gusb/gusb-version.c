/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gusb-version.h"

/**
 * g_usb_version_string:
 *
 * Gets the GUsb installed runtime version.
 *
 * Returns: a version number, e.g. "0.3.1"
 *
 * Since: 0.3.1
 **/
const gchar *
g_usb_version_string(void)
{
	return G_STRINGIFY(G_USB_MAJOR_VERSION) "." G_STRINGIFY(
	    G_USB_MINOR_VERSION) "." G_STRINGIFY(G_USB_MICRO_VERSION);
}
