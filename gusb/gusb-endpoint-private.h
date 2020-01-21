/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __GUSB_ENDPOINT_PRIVATE_H__
#define __GUSB_ENDPOINT_PRIVATE_H__

#include <libusb.h>

#include <gusb/gusb-endpoint.h>

G_BEGIN_DECLS

GUsbEndpoint	*_g_usb_endpoint_new		(const struct libusb_endpoint_descriptor	*endpoint);

G_END_DECLS

#endif /* __GUSB_ENDPOINT_PRIVATE_H__ */
