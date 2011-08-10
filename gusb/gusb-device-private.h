/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
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

#ifndef __GUSB_DEVICE_PRIVATE_H__
#define __GUSB_DEVICE_PRIVATE_H__

#include <gusb/gusb-device.h>

G_BEGIN_DECLS

GUsbDevice	*_g_usb_device_new		(libusb_device	*device);

libusb_device	*_g_usb_device_get_device	(GUsbDevice	*device);

G_END_DECLS

#endif /* __GUSB_DEVICE_PRIVATE_H__ */
