/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define G_USB_TYPE_INTERFACE (g_usb_interface_get_type())
G_DECLARE_FINAL_TYPE(GUsbInterface, g_usb_interface, G_USB, INTERFACE, GObject)

guint8
g_usb_interface_get_length(GUsbInterface *self);
guint8
g_usb_interface_get_kind(GUsbInterface *self);
guint8
g_usb_interface_get_number(GUsbInterface *self);
guint8
g_usb_interface_get_alternate(GUsbInterface *self);
guint8
g_usb_interface_get_class(GUsbInterface *self);
guint8
g_usb_interface_get_subclass(GUsbInterface *self);
guint8
g_usb_interface_get_protocol(GUsbInterface *self);
guint8
g_usb_interface_get_index(GUsbInterface *self);
GBytes *
g_usb_interface_get_extra(GUsbInterface *self);
GPtrArray *
g_usb_interface_get_endpoints(GUsbInterface *self);

G_END_DECLS
