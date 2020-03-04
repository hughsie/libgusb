/*<private_header>*/
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright 2020 Simon McVittie
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#ifdef _GUSB_VERSIONED_SYMBOLS
#define _GUSB_COMPAT_ALIAS(sym, default_ver) \
  extern __typeof__(sym) _compat_ ## sym __attribute__((alias ("_default_" #sym))); \
  extern __typeof__(sym) _default_ ## sym; \
  __asm__(".symver _compat_" #sym "," #sym "@LIBGUSB_0.1.0"); \
  __asm__(".symver _default_" #sym "," #sym "@@LIBGUSB_" default_ver);

/* These are parsed by generate-version-script.py, do not reformat */
#define g_usb_context_get_main_context _default_g_usb_context_get_main_context
#define g_usb_context_set_main_context _default_g_usb_context_set_main_context
#define g_usb_context_wait_for_replug _default_g_usb_context_wait_for_replug
#define g_usb_device_get_interface _default_g_usb_device_get_interface
#define g_usb_device_get_interfaces _default_g_usb_device_get_interfaces
#define g_usb_device_get_release _default_g_usb_device_get_release
#define g_usb_device_set_interface_alt _default_g_usb_device_set_interface_alt
#define g_usb_interface_get_alternate _default_g_usb_interface_get_alternate
#define g_usb_interface_get_class _default_g_usb_interface_get_class
#define g_usb_interface_get_extra _default_g_usb_interface_get_extra
#define g_usb_interface_get_index _default_g_usb_interface_get_index
#define g_usb_interface_get_kind _default_g_usb_interface_get_kind
#define g_usb_interface_get_length _default_g_usb_interface_get_length
#define g_usb_interface_get_number _default_g_usb_interface_get_number
#define g_usb_interface_get_protocol _default_g_usb_interface_get_protocol
#define g_usb_interface_get_subclass _default_g_usb_interface_get_subclass
#define g_usb_interface_get_type _default_g_usb_interface_get_type
#define g_usb_endpoint_get_number _default_g_usb_endpoint_get_number
#define g_usb_endpoint_get_refresh _default_g_usb_endpoint_get_refresh
#define g_usb_endpoint_get_synch_address _default_g_usb_endpoint_get_synch_address
#define g_usb_endpoint_get_type _default_g_usb_endpoint_get_type
#define g_usb_version_string _default_g_usb_version_string

#else /* !_GUSB_VERSIONED_SYMBOLS */

#define _GUSB_COMPAT_ALIAS(sym, default_ver) /* nothing */

#endif /* !_GUSB_VERSIONED_SYMBOLS */
