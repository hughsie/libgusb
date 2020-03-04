/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "gusb-abi-compat.h"
#include "gusb-version.h"

/* New in 0.3.1, but originally versioned as 0.1.0 */
/* https://github.com/hughsie/libgusb/commit/3bf1467c775ef889f136dd20e97fce61068e0189 */
_GUSB_COMPAT_ALIAS (g_usb_version_string, "0.3.1")

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
g_usb_version_string (void)
{
	return G_STRINGIFY(G_USB_MAJOR_VERSION) "."
		G_STRINGIFY(G_USB_MINOR_VERSION) "."
		G_STRINGIFY(G_USB_MICRO_VERSION);
}
