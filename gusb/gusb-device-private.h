/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gusb/gusb-device.h>

G_BEGIN_DECLS

GUsbDevice *
_g_usb_device_new(GUsbContext *context, libusb_device *device, GError **error);
gboolean
_g_usb_device_load(GUsbDevice *self, JsonObject *json_object, GError **error);
gboolean
_g_usb_device_save(GUsbDevice *self, JsonBuilder *json_builder, GError **error);

libusb_device *
_g_usb_device_get_device(GUsbDevice *self);
gboolean
_g_usb_device_open_internal(GUsbDevice *self, GError **error);

G_END_DECLS
