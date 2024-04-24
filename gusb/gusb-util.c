/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libusb.h>

#include "gusb-util.h"

/**
 * g_usb_strerror:
 * @error_code: a libusb error code
 *
 * Converts the error code into a string
 *
 * Return value: String, or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
g_usb_strerror(gint error_code)
{
	return libusb_strerror(error_code);
}
