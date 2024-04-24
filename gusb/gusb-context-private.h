/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-context.h>
#include <libusb.h>

G_BEGIN_DECLS

libusb_context *
_g_usb_context_get_context(GUsbContext *self);

const gchar *
_g_usb_context_lookup_vendor(GUsbContext *self, guint16 vid, GError **error);
const gchar *
_g_usb_context_lookup_product(GUsbContext *self, guint16 vid, guint16 pid, GError **error);
gboolean
_g_usb_context_has_flag(GUsbContext *self, GUsbContextFlags flags);

G_END_DECLS
