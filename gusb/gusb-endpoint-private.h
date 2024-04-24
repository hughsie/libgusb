/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-endpoint.h>
#include <json-glib/json-glib.h>
#include <libusb.h>

G_BEGIN_DECLS

GUsbEndpoint *
_g_usb_endpoint_new(const struct libusb_endpoint_descriptor *endpoint);

gboolean
_g_usb_endpoint_load(GUsbEndpoint *self, JsonObject *json_object, GError **error);
gboolean
_g_usb_endpoint_save(GUsbEndpoint *self, JsonBuilder *json_builder, GError **error);

G_END_DECLS
