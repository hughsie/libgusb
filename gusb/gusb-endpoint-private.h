/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GUSB_ENDPOINT_PRIVATE_H__
#define __GUSB_ENDPOINT_PRIVATE_H__

#include <libusb.h>

#include <gusb/gusb-endpoint.h>

G_BEGIN_DECLS

GUsbEndpoint	*_g_usb_endpoint_new		(const struct libusb_endpoint_descriptor	*endpoint);

G_END_DECLS

#endif /* __GUSB_ENDPOINT_PRIVATE_H__ */
