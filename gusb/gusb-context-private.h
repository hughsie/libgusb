/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <libusb.h>

#include <gusb/gusb-context.h>

G_BEGIN_DECLS

libusb_context	*_g_usb_context_get_context	(GUsbContext	*context);

const gchar	*_g_usb_context_lookup_vendor	(GUsbContext	*context,
						 guint16	 vid,
						 GError		**error);
const gchar	*_g_usb_context_lookup_product	(GUsbContext	*context,
						 guint16	 vid,
						 guint16	 pid,
						 GError		**error);

G_END_DECLS
