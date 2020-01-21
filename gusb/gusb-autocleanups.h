/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Kalev Lember <klember@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gusb/gusb-context.h>
#include <gusb/gusb-device.h>
#include <gusb/gusb-device-list.h>

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbContext, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbDeviceList, g_object_unref)

#endif
