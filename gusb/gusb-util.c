/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <libusb.h>

#include "gusb-util.h"

const gchar *
g_usb_strerror (gint error_code)
{
	return libusb_strerror (error_code);
}
