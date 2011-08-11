/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>

#include "gusb-context.h"
#include "gusb-source.h"
#include "gusb-device.h"
#include "gusb-device-list.h"

static void
gusb_context_func (void)
{
	GUsbContext *ctx;
	GError *error = NULL;

	ctx = g_usb_context_new (&error);
	g_assert_no_error (error);
	g_assert (ctx != NULL);

	g_usb_context_set_debug (ctx, G_LOG_LEVEL_ERROR);

	g_object_unref (ctx);
}

static void
gusb_source_func (void)
{
	GUsbSource *source;
	GUsbContext *ctx;
	GError *error = NULL;

	ctx = g_usb_context_new (&error);
	g_assert_no_error (error);
	g_assert (ctx != NULL);

	source = g_usb_source_new (NULL, ctx, &error);
	g_assert_no_error (error);
	g_assert (ctx != NULL);

	/* TODO: test callback? */

	g_usb_source_destroy (source);
	g_object_unref (ctx);
}

static void
gusb_device_list_func (void)
{
	GUsbContext *ctx;
	GUsbDeviceList *list;
	GError *error = NULL;
	GPtrArray *array;
	guint old_number_of_devices;
	guint8 bus, address;
	GUsbDevice *device;

	ctx = g_usb_context_new (&error);
	g_assert_no_error (error);
	g_assert (ctx != NULL);

	g_usb_context_set_debug (ctx, G_LOG_LEVEL_ERROR);

	list = g_usb_device_list_new (ctx);
	g_assert (list != NULL);

	/* ensure we have an empty list */
	array = g_usb_device_list_get_devices (list);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* coldplug, and ensure we got some devices */
	g_usb_device_list_coldplug (list);
	array = g_usb_device_list_get_devices (list);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, >, 0);
	old_number_of_devices = array->len;
	g_ptr_array_unref (array);

	/* coldplug again, and ensure we did not duplicate devices */
	g_usb_device_list_coldplug (list);
	array = g_usb_device_list_get_devices (list);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, old_number_of_devices);
	device = G_USB_DEVICE (g_ptr_array_index (array, 0));
	bus = g_usb_device_get_bus (device);
	address = g_usb_device_get_address (device);
	g_ptr_array_unref (array);

	/* ensure we can search for the same device */
	device = g_usb_device_list_get_dev_by_bus_n_address (list,
							     bus,
							     address);
	g_assert (device != NULL);
	g_assert_cmpint (bus, ==, g_usb_device_get_bus (device));
	g_assert_cmpint (address, ==, g_usb_device_get_address (device));
	g_object_unref (device);

	g_object_unref (list);
	g_object_unref (ctx);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/gusb/context", gusb_context_func);
	g_test_add_func ("/gusb/source", gusb_source_func);
	g_test_add_func ("/gusb/device-list", gusb_device_list_func);

	return g_test_run ();
}

