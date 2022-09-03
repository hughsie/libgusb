/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <libusb.h>

#include <gusb/gusb-bos-descriptor.h>

G_BEGIN_DECLS

GUsbBosDescriptor	*_g_usb_bos_descriptor_new	(const struct libusb_bos_dev_capability_descriptor	*bos_cap);

G_END_DECLS
