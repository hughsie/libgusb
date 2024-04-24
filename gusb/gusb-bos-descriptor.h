/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define G_USB_TYPE_BOS_DESCRIPTOR (g_usb_bos_descriptor_get_type())
G_DECLARE_FINAL_TYPE(GUsbBosDescriptor, g_usb_bos_descriptor, G_USB, BOS_DESCRIPTOR, GObject)

guint8
g_usb_bos_descriptor_get_capability(GUsbBosDescriptor *self);
GBytes *
g_usb_bos_descriptor_get_extra(GUsbBosDescriptor *self);

G_END_DECLS
