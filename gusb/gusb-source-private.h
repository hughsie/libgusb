/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Debarshi Ray <debarshir@src.gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

GUsbSource *
_g_usb_source_new(GMainContext *main_ctx, GUsbContext *context);
void
_g_usb_source_destroy(GUsbSource *source);

G_END_DECLS
