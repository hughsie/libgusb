/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-bos-descriptor.h>
#include <json-glib/json-glib.h>
#include <libusb.h>

G_BEGIN_DECLS

GUsbBosDescriptor *
_g_usb_bos_descriptor_new(const struct libusb_bos_dev_capability_descriptor *bos_cap);

gboolean
_g_usb_bos_descriptor_load(GUsbBosDescriptor *self, JsonObject *json_object, GError **error);
gboolean
_g_usb_bos_descriptor_save(GUsbBosDescriptor *self, JsonBuilder *json_builder, GError **error);

G_END_DECLS
