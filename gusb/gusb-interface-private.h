/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-interface.h>
#include <json-glib/json-glib.h>
#include <libusb.h>

G_BEGIN_DECLS

GUsbInterface *
_g_usb_interface_new(const struct libusb_interface_descriptor *iface);

gboolean
_g_usb_interface_load(GUsbInterface *self, JsonObject *json_object, GError **error);
gboolean
_g_usb_interface_save(GUsbInterface *self, JsonBuilder *json_builder, GError **error);

G_END_DECLS
