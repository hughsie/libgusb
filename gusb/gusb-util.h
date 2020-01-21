/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __GUSB_UTIL_H__
#define __GUSB_UTIL_H__

#include <glib.h>

G_BEGIN_DECLS

const gchar* g_usb_strerror(gint error_code);

G_END_DECLS

#endif /* __GUSB_UTIL_H__ */
