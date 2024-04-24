/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gusb-context-private.h"

static void
gusb_device_func(void)
{
	GUsbDevice *device;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) array = NULL;
	g_autoptr(GUsbContext) ctx = NULL;

#ifdef __FreeBSD__
	g_test_skip("Root hubs on FreeBSD have vid and pid set to zero");
	return;
#endif

	ctx = g_usb_context_new(&error);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

	array = g_usb_context_get_devices(ctx);
	g_assert(array != NULL);
	g_assert_cmpint(array->len, >, 0);
	device = G_USB_DEVICE(g_ptr_array_index(array, 0));

	g_assert_cmpint(g_usb_device_get_vid(device), >, 0x0000);
	g_assert_cmpint(g_usb_device_get_pid(device), >, 0x0000);

	g_assert_false(g_usb_device_has_tag(device, "foobar"));
	g_usb_device_add_tag(device, "foobar");
	g_usb_device_add_tag(device, "foobar");
	g_assert_true(g_usb_device_has_tag(device, "foobar"));
	g_usb_device_remove_tag(device, "foobar");
	g_assert_false(g_usb_device_has_tag(device, "foobar"));
}

static void
gusb_context_lookup_func(void)
{
	GError *error = NULL;
	const gchar *tmp;
	g_autoptr(GUsbContext) ctx = NULL;

	ctx = g_usb_context_new(&error);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	tmp = _g_usb_context_lookup_vendor(ctx, 0x04d8, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "Microchip Technology, Inc.");
	tmp = _g_usb_context_lookup_product(ctx, 0x04d8, 0xf8da, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "Hughski Ltd. ColorHug");
}

static void
gusb_context_func(void)
{
	GPtrArray *array;
	guint old_number_of_devices;
	guint8 bus, address;
	GUsbDevice *device;
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbContext) ctx = NULL;

#ifdef __FreeBSD__
	g_test_skip("Root hubs on FreeBSD have vid and pid set to zero");
	return;
#endif

	ctx = g_usb_context_new(&error);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

	/* coldplug, and ensure we got some devices */
	array = g_usb_context_get_devices(ctx);
	g_assert(array != NULL);
	g_assert_cmpint(array->len, >, 0);
	old_number_of_devices = array->len;

	/* Print a list (also exercising various bits of g_usb_device) */
	g_print("\n");
	for (guint i = 0; i < array->len; i++) {
		g_autofree gchar *manufacturer = NULL;
		g_autofree gchar *product = NULL;

		device = G_USB_DEVICE(g_ptr_array_index(array, i));

		g_assert_cmpint(g_usb_device_get_vid(device), >, 0x0000);
		g_assert_cmpint(g_usb_device_get_pid(device), >, 0x0000);

		/* Needed for g_usb_device_get_string_descriptor below,
		   not error checked to allow running basic tests without
		   needing r/w rights on /dev/bus/usb nodes */
		g_usb_context_set_debug(ctx, 0);
		g_usb_device_open(device, NULL);
		g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

		/* We don't error check these as not all devices have these
		   (and the device_open may have failed). */
		manufacturer =
		    g_usb_device_get_string_descriptor(device,
						       g_usb_device_get_manufacturer_index(device),
						       NULL);
		product = g_usb_device_get_string_descriptor(device,
							     g_usb_device_get_product_index(device),
							     NULL);

		g_usb_device_close(device, NULL);

		g_print("Found %04x:%04x, %s %s\n",
			g_usb_device_get_vid(device),
			g_usb_device_get_pid(device),
			manufacturer ? manufacturer : "",
			product ? product : "");
	}
	g_ptr_array_unref(array);

	/* coldplug again, and ensure we did not duplicate devices */
	array = g_usb_context_get_devices(ctx);
	g_assert(array != NULL);
	g_assert_cmpint(array->len, ==, old_number_of_devices);
	device = G_USB_DEVICE(g_ptr_array_index(array, 0));
	bus = g_usb_device_get_bus(device);
	address = g_usb_device_get_address(device);
	g_ptr_array_unref(array);

	/* ensure we can search for the same device */
	device = g_usb_context_find_by_bus_address(ctx, bus, address, &error);
	g_assert_no_error(error);
	g_assert(device != NULL);
	g_assert_cmpint(bus, ==, g_usb_device_get_bus(device));
	g_assert_cmpint(address, ==, g_usb_device_get_address(device));
	g_object_unref(device);

	/* get a device that can't exist */
	device = g_usb_context_find_by_vid_pid(ctx, 0xffff, 0xffff, &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_NO_DEVICE);
	g_assert(device == NULL);
}

static void
gusb_device_huey_func(void)
{
	gboolean ret;
	GCancellable *cancellable = NULL;
	const gchar request[8] = {0x0e, 'G', 'r', 'M', 'b', 'k', 'e', 'd'};
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbContext) ctx = NULL;
	g_autoptr(GUsbDevice) device = NULL;

	ctx = g_usb_context_new(&error);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

	/* coldplug, and get the huey */
	device = g_usb_context_find_by_vid_pid(ctx, 0x0971, 0x2005, &error);
	if (device == NULL && error->domain == G_USB_DEVICE_ERROR &&
	    error->code == G_USB_DEVICE_ERROR_NO_DEVICE) {
		g_print("No device detected!\n");
		return;
	}
	g_assert_no_error(error);
	g_assert(device != NULL);

	/* close not opened */
	ret = g_usb_device_close(device, &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_NOT_OPEN);
	g_assert(!ret);
	g_clear_error(&error);

	/* open */
	ret = g_usb_device_open(device, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* open opened */
	ret = g_usb_device_open(device, &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_ALREADY_OPEN);
	g_assert(!ret);
	g_clear_error(&error);

	/* claim interface 0 */
	ret = g_usb_device_claim_interface(device,
					   0x00,
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* do a request (unlock) */
	ret = g_usb_device_control_transfer(device,
					    G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					    G_USB_DEVICE_REQUEST_TYPE_CLASS,
					    G_USB_DEVICE_RECIPIENT_INTERFACE,
					    0x09,   /* request */
					    0x0200, /* value */
					    0,	    /* index */
					    (guint8 *)request,
					    8,
					    NULL,
					    2000,
					    cancellable,
					    &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* do a request we know is going to fail */
	ret = g_usb_device_control_transfer(device,
					    G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					    G_USB_DEVICE_REQUEST_TYPE_CLASS,
					    G_USB_DEVICE_RECIPIENT_INTERFACE,
					    0x09,   /* request */
					    0x0200, /* value */
					    0,	    /* index */
					    (guint8 *)request,
					    8,
					    NULL,
					    2000,
					    cancellable,
					    &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_NOT_SUPPORTED);
	g_assert(!ret);
	g_clear_error(&error);

	/* release interface 0 */
	ret = g_usb_device_release_interface(device,
					     0x00,
					     G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					     &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* close */
	ret = g_usb_device_close(device, &error);
	g_assert_no_error(error);
	g_assert(ret);
}

typedef struct {
	guint8 *buffer;
	guint buffer_len;
	GMainLoop *loop;
} GUsbDeviceAsyncHelper;

static void
g_usb_device_print_data(const gchar *title, const guchar *data, gsize length)
{
	if (g_strcmp0(title, "request") == 0)
		g_print("%c[%dm", 0x1B, 31);
	if (g_strcmp0(title, "reply") == 0)
		g_print("%c[%dm", 0x1B, 34);
	g_print("%s\t", title);

	for (guint i = 0; i < length; i++) {
		g_print("%02x [%c]\t", data[i], g_ascii_isprint(data[i]) ? data[i] : '?');
	}
	g_print("%c[%dm\n", 0x1B, 0);
}

static void
g_usb_test_button_pressed_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	gssize rc;
	GUsbDeviceAsyncHelper *helper = (GUsbDeviceAsyncHelper *)user_data;
	g_autoptr(GError) error = NULL;

	rc = g_usb_device_interrupt_transfer_finish(G_USB_DEVICE(source_object), res, &error);
	if (rc < 0) {
		g_error("%s", error->message);
		return;
	}

	g_usb_device_print_data("button press", helper->buffer, helper->buffer_len);
	g_main_loop_quit(helper->loop);
	g_free(helper->buffer);
}

static void
gusb_device_munki_func(void)
{
	gboolean ret;
	GCancellable *cancellable = NULL;
	guint8 request[24];
	GUsbDeviceAsyncHelper *helper;
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbContext) ctx = NULL;
	g_autoptr(GUsbDevice) device = NULL;

	ctx = g_usb_context_new(&error);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

	/* coldplug, and get the ColorMunki */
	device = g_usb_context_find_by_vid_pid(ctx, 0x0971, 0x2007, &error);
	if (device == NULL && error->domain == G_USB_DEVICE_ERROR &&
	    error->code == G_USB_DEVICE_ERROR_NO_DEVICE) {
		g_print("No device detected!\n");
		return;
	}
	g_assert_no_error(error);
	g_assert(device != NULL);

	/* close not opened */
	ret = g_usb_device_close(device, &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_NOT_OPEN);
	g_assert(!ret);
	g_clear_error(&error);

	/* open */
	ret = g_usb_device_open(device, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* open opened */
	ret = g_usb_device_open(device, &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_ALREADY_OPEN);
	g_assert(!ret);
	g_clear_error(&error);

	/* claim interface 0 */
	ret = g_usb_device_claim_interface(device,
					   0x00,
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* do a request (get chip id) */
	ret = g_usb_device_control_transfer(device,
					    G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					    G_USB_DEVICE_REQUEST_TYPE_VENDOR,
					    G_USB_DEVICE_RECIPIENT_DEVICE,
					    0x86,   /* request */
					    0x0000, /* value */
					    0,	    /* index */
					    (guint8 *)request,
					    24,
					    NULL,
					    2000,
					    cancellable,
					    &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* do a request we know is going to fail */
	ret = g_usb_device_control_transfer(device,
					    G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					    G_USB_DEVICE_REQUEST_TYPE_CLASS,
					    G_USB_DEVICE_RECIPIENT_INTERFACE,
					    0x00,   /* request */
					    0x0200, /* value */
					    0,	    /* index */
					    (guint8 *)request,
					    8,
					    NULL,
					    100,
					    cancellable,
					    &error);
	g_assert_error(error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_TIMED_OUT);
	g_assert(!ret);
	g_clear_error(&error);

	/* do async read of button event */
	helper = g_slice_new0(GUsbDeviceAsyncHelper);
	helper->buffer_len = 8;
	helper->buffer = g_new0(guint8, helper->buffer_len);
	helper->loop = g_main_loop_new(NULL, FALSE);
	g_usb_device_interrupt_transfer_async(device,
					      0x83,
					      helper->buffer,
					      8,
					      30 * 1000,
					      cancellable, /* TODO; use GCancellable */
					      g_usb_test_button_pressed_cb,
					      helper);
	g_main_loop_run(helper->loop);
	g_main_loop_unref(helper->loop);

	/* release interface 0 */
	ret = g_usb_device_release_interface(device,
					     0x00,
					     G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					     &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* close */
	ret = g_usb_device_close(device, &error);
	g_assert_no_error(error);
	g_assert(ret);
}

static void
_context_signal_count_cb(GUsbContext *ctx, GUsbDevice *usb_device, gpointer user_data)
{
	guint *cnt = (guint *)user_data;
	(*cnt)++;
}

static gboolean
_g_usb_context_load_json(GUsbContext *ctx, const gchar *json, GError **error)
{
	JsonObject *json_obj;
	g_autoptr(JsonParser) parser = json_parser_new();

	if (!json_parser_load_from_data(parser, json, -1, error))
		return FALSE;
	json_obj = json_node_get_object(json_parser_get_root(parser));
	return g_usb_context_load_with_tag(ctx, json_obj, "emulation", error);
}

static void
gusb_device_json_func(void)
{
	gboolean ret;
	guint8 idx;
	guint added_cnt = 0;
	guint changed_cnt = 0;
	guint removed_cnt = 0;
	g_autoptr(GUsbDevice) device = NULL;
	g_autoptr(GUsbDevice) device2 = NULL;
	g_autoptr(GUsbDevice) device3 = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(GUsbContext) ctx = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *json =
	    "{"
	    "  \"UsbDevices\" : ["
	    "    {"
	    "      \"PlatformId\" : \"usb:AA:AA:06\","
	    "      \"Created\" : \"2023-02-01T16:35:03.302027Z\","
	    "      \"Tags\" : ["
	    "        \"emulation\""
	    "      ],"
	    "      \"IdVendor\" : 10047,"
	    "      \"IdProduct\" : 4100,"
	    "      \"Device\" : 2,"
	    "      \"USB\" : 512,"
	    "      \"Manufacturer\" : 1,"
	    "      \"UsbInterfaces\" : ["
	    "        {"
	    "          \"Length\" : 9,"
	    "          \"DescriptorType\" : 4,"
	    "          \"InterfaceNumber\" : 1,"
	    "          \"InterfaceClass\" : 255,"
	    "          \"InterfaceSubClass\" : 70,"
	    "          \"InterfaceProtocol\" : 87,"
	    "          \"Interface\" : 3"
	    "        },"
	    "        {"
	    "          \"Length\" : 9,"
	    "          \"DescriptorType\" : 4,"
	    "          \"InterfaceNumber\" : 2,"
	    "          \"InterfaceClass\" : 255,"
	    "          \"InterfaceSubClass\" : 71,"
	    "          \"InterfaceProtocol\" : 85,"
	    "          \"Interface\" : 4"
	    "        },"
	    "        {"
	    "          \"Length\" : 9,"
	    "          \"DescriptorType\" : 4,"
	    "          \"InterfaceClass\" : 3,"
	    "          \"UsbEndpoints\" : ["
	    "            {"
	    "              \"DescriptorType\" : 5,"
	    "              \"EndpointAddress\" : 129,"
	    "              \"Interval\" : 1,"
	    "              \"MaxPacketSize\" : 64"
	    "            },"
	    "            {"
	    "              \"DescriptorType\" : 5,"
	    "              \"EndpointAddress\" : 1,"
	    "              \"Interval\" : 1,"
	    "              \"MaxPacketSize\" : 64"
	    "            }"
	    "          ],"
	    "          \"ExtraData\" : \"CSERAQABIh0A\""
	    "        }"
	    "      ],"
	    "      \"UsbEvents\" : ["
	    "        {"
	    "          \"Id\" : "
	    "\"GetCustomIndex:ClassId=0xff,SubclassId=0x46,ProtocolId=0x57\","
	    "          \"Data\" : \"Aw==\""
	    "        },"
	    "        {"
	    "          \"Id\" : "
	    "\"GetCustomIndex:ClassId=0xff,SubclassId=0x46,ProtocolId=0x57\","
	    "          \"Data\" : \"Aw==\""
	    "        },"
	    "        {"
	    "          \"Id\" : \"GetStringDescriptor:DescIndex=0x03\","
	    "          \"Data\" : "
	    "\"Mi4wLjcAAAAR3HzMiH8AAAAAAAAAAAAA8HBIAQAAAACQ6AsW/n8AADeVQAAAAAAAsOgLFv5/"
	    "AABNkkAAVwAAAEboCxb/"
	    "fwAAkAZLAQAAAAAAZkAAAAAAAwAAAAAAAAAAsPpJAQAAAAAwBksBAAAAAJAZTwEAAAAAAFZZJp63C7g=\""
	    "        }"
	    "      ]"
	    "    }"
	    "  ]"
	    "}";
	const gchar *json_new = "{"
				"  \"UsbDevices\" : ["
				"    {"
				"      \"PlatformId\" : \"usb:FF:FF:06\","
				"      \"Created\" : \"2099-02-01T16:35:03.302027Z\","
				"      \"Tags\" : ["
				"        \"emulation\""
				"      ],"
				"      \"IdVendor\" : 10047,"
				"      \"IdProduct\" : 4101,"
				"      \"Device\" : 2,"
				"      \"USB\" : 512,"
				"      \"Manufacturer\" : 1"
				"    }"
				"  ]"
				"}";
	ctx = g_usb_context_new(&error);
	g_usb_context_set_flags(ctx, G_USB_CONTEXT_FLAGS_DEBUG);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	/* watch events */
	g_usb_context_enumerate(ctx);
	g_signal_connect(G_USB_CONTEXT(ctx),
			 "device-added",
			 G_CALLBACK(_context_signal_count_cb),
			 &added_cnt);
	g_signal_connect(G_USB_CONTEXT(ctx),
			 "device-removed",
			 G_CALLBACK(_context_signal_count_cb),
			 &removed_cnt);
	g_signal_connect(G_USB_CONTEXT(ctx),
			 "device-changed",
			 G_CALLBACK(_context_signal_count_cb),
			 &changed_cnt);

	/* parse */
	ret = _g_usb_context_load_json(ctx, json, &error);
	g_assert_no_error(error);
	g_assert(ret);
	g_assert_cmpint(added_cnt, ==, 1);
	g_assert_cmpint(removed_cnt, ==, 0);
	g_assert_cmpint(changed_cnt, ==, 0);

	/* get vendor data */
	device = g_usb_context_find_by_vid_pid(ctx, 0x273f, 0x1004, &error);
	g_assert_no_error(error);
	g_assert(device != NULL);
	g_assert_true(g_usb_device_has_tag(device, "emulation"));
	idx = g_usb_device_get_custom_index(device,
					    G_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
					    'F',
					    'W',
					    &error);
	g_assert_no_error(error);
	g_assert_cmpint(idx, ==, 3);

	/* in-order, repeat */
	idx = g_usb_device_get_custom_index(device,
					    G_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
					    'F',
					    'W',
					    &error);
	g_assert_no_error(error);
	g_assert_cmpint(idx, ==, 3);

	/* out-of-order */
	idx = g_usb_device_get_custom_index(device,
					    G_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
					    'F',
					    'W',
					    &error);
	g_assert_no_error(error);
	g_assert_cmpint(idx, ==, 3);

	/* get the firmware version */
	tmp = g_usb_device_get_string_descriptor(device, idx, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "2.0.7");

	/* load the same data */
	ret = _g_usb_context_load_json(ctx, json, &error);
	g_assert_no_error(error);
	g_assert(ret);
	g_assert_cmpint(added_cnt, ==, 1);
	g_assert_cmpint(removed_cnt, ==, 0);
	g_assert_cmpint(changed_cnt, ==, 1);
	device2 = g_usb_context_find_by_vid_pid(ctx, 0x273f, 0x1004, &error);
	g_assert_no_error(error);
	g_assert(device2 != NULL);
	g_assert_true(g_usb_device_has_tag(device2, "emulation"));

	/* load a different device */
	ret = _g_usb_context_load_json(ctx, json_new, &error);
	g_assert_no_error(error);
	g_assert(ret);
	g_assert_cmpint(added_cnt, ==, 2);
	g_assert_cmpint(changed_cnt, ==, 1);
	g_assert_cmpint(removed_cnt, ==, 1);
	device3 = g_usb_context_find_by_vid_pid(ctx, 0x273f, 0x1005, &error);
	g_assert_no_error(error);
	g_assert(device3 != NULL);
	g_assert_true(g_usb_device_has_tag(device3, "emulation"));
}

static void
gusb_device_ch2_func(void)
{
	gboolean ret;
	guint8 idx;
	g_autofree gchar *data = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbContext) ctx = NULL;
	g_autoptr(GUsbDevice) device = NULL;
	g_autoptr(JsonBuilder) json_builder = json_builder_new();
	g_autoptr(JsonGenerator) json_generator = NULL;
	g_autoptr(JsonNode) json_root = NULL;

	ctx = g_usb_context_new(&error);
	g_usb_context_set_flags(ctx, G_USB_CONTEXT_FLAGS_SAVE_EVENTS);
	g_assert_no_error(error);
	g_assert(ctx != NULL);

	g_usb_context_set_debug(ctx, G_LOG_LEVEL_ERROR);

	/* coldplug, and get the ColorHug */
	device = g_usb_context_find_by_vid_pid(ctx, 0x273f, 0x1004, &error);
	if (device == NULL && error->domain == G_USB_DEVICE_ERROR &&
	    error->code == G_USB_DEVICE_ERROR_NO_DEVICE) {
		g_print("No device detected!\n");
		return;
	}
	g_assert_no_error(error);
	g_assert(device != NULL);

	/* open */
	ret = g_usb_device_open(device, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* get vendor data */
	idx = g_usb_device_get_custom_index(device,
					    G_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
					    'F',
					    'W',
					    &error);
	g_assert_no_error(error);
	g_assert_cmpint(idx, ==, 3);

	/* get the firmware version */
	tmp = g_usb_device_get_string_descriptor(device, idx, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "2.0.7");

	/* close */
	ret = g_usb_device_close(device, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* save context */
	ret = g_usb_context_save(ctx, json_builder, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* export as a string */
	json_root = json_builder_get_root(json_builder);
	json_generator = json_generator_new();
	json_generator_set_pretty(json_generator, TRUE);
	json_generator_set_root(json_generator, json_root);
	data = json_generator_to_data(json_generator, NULL);
	g_assert_nonnull(data);
	g_print("%s\n", data);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask(NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func("/gusb/context", gusb_context_func);
	g_test_add_func("/gusb/context{lookup}", gusb_context_lookup_func);
	g_test_add_func("/gusb/device", gusb_device_func);
	g_test_add_func("/gusb/device[huey]", gusb_device_huey_func);
	g_test_add_func("/gusb/device[munki]", gusb_device_munki_func);
	g_test_add_func("/gusb/device[colorhug2]", gusb_device_ch2_func);
	g_test_add_func("/gusb/device[json]", gusb_device_json_func);

	return g_test_run();
}
