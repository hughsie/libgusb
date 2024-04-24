/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define G_USB_SOURCE_ERROR (g_usb_source_error_quark())

typedef struct _GUsbSource GUsbSource;

/**
 * GUsbSourceError:
 *
 * The error code.
 **/
typedef enum { G_USB_SOURCE_ERROR_INTERNAL } GUsbSourceError;

G_DEPRECATED_FOR(g_usb_context_error_quark)
GQuark
g_usb_source_error_quark(void);

G_DEPRECATED
void
g_usb_source_set_callback(GUsbSource *self, GSourceFunc func, gpointer data, GDestroyNotify notify);

G_END_DECLS
