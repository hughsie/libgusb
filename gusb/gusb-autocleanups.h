/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Kalev Lember <klember@redhat.com>
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

#ifndef __GUSB_AUTOCLEANUPS_H__
#define __GUSB_AUTOCLEANUPS_H__

#include <gusb/gusb-context.h>
#include <gusb/gusb-device.h>
#include <gusb/gusb-device-list.h>

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbContext, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbDeviceList, g_object_unref)

#endif

#endif /* __GUSB_AUTOCLEANUPS_H__ */
