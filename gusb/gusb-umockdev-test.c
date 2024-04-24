/*
 * libusb umockdev based tests
 *
 * Copyright (C) 2022 Benjamin Berg <bberg@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include <string.h>
#include <unistd.h>

#include "gusb.h"
#include "umockdev.h"

#define UNUSED_DATA __attribute__((unused)) gconstpointer unused_data

/* avoid leak reports inside assertions; leaking stuff on assertion failures does not matter in
 * tests */
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#pragma GCC diagnostic ignored "-Wanalyzer-file-leak"
#endif

typedef struct {
	UMockdevTestbed *testbed;
	GUsbContext *ctx;
} UMockdevTestbedFixture;

static void
test_fixture_setup(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	fixture->testbed = umockdev_testbed_new();
	g_assert(fixture->testbed != NULL);
}

static void
test_fixture_setup_empty(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	test_fixture_setup(fixture, NULL);
	fixture->ctx = g_usb_context_new(NULL);
}

static void
test_fixture_teardown(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	/* break context -> device -> context cycle */
	if (fixture->ctx)
		g_object_run_dispose(G_OBJECT(fixture->ctx));
	g_clear_object(&fixture->ctx);
	g_clear_object(&fixture->testbed);

	/* running the mainloop is needed to ensure everything is cleaned up */
	while (g_main_context_iteration(NULL, FALSE)) {
	}
}

static void
test_fixture_add_canon(UMockdevTestbedFixture *fixture)
{
	/* NOTE: there is no device file, so cannot be opened */

	/* NOTE: add_device would not create a file, needed for device emulation */
	/* XXX: racy, see https://github.com/martinpitt/umockdev/issues/173 */
	umockdev_testbed_add_from_string(
	    fixture->testbed,
	    "P: /devices/usb1\n"
	    "N: bus/usb/001/001\n"
	    "E: SUBSYSTEM=usb\n"
	    "E: DRIVER=usb\n"
	    "E: BUSNUM=001\n"
	    "E: DEVNUM=001\n"
	    "E: DEVNAME=/dev/bus/usb/001/001\n"
	    "E: DEVTYPE=usb_device\n"
	    "A: bConfigurationValue=1\\n\n"
	    "A: busnum=1\\n\n"
	    "A: devnum=1\\n\n"
	    "A: bConfigurationValue=1\\n\n"
	    "A: speed=480\\n\n"
	    /* descriptor from a Canon PowerShot SX200; VID 04a9 PID 31c0 */
	    "H: descriptors="
	    "1201000200000040a904c03102000102"
	    "030109022700010100c0010904000003"
	    "06010100070581020002000705020200"
	    "020007058303080009\n",
	    NULL);
}

static void
test_ctx_enumerate(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	g_autoptr(GPtrArray) devices = NULL;

	test_fixture_add_canon(fixture);

	g_usb_context_enumerate(fixture->ctx);

	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(devices->len, ==, 1);
}

static void
count_hotplug_event_cb(GUsbContext *context, GUsbDevice *device, gpointer user_data)
{
	int *counter = user_data;

	*counter += 1;
}

static void
test_ctx_hotplug(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	g_autoptr(GPtrArray) devices = NULL;
	gint events = 0;

	g_signal_connect(fixture->ctx, "device-added", G_CALLBACK(count_hotplug_event_cb), &events);

	g_usb_context_enumerate(fixture->ctx);

	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(devices->len, ==, 0);
	g_assert_cmpint(events, ==, 0);
	g_clear_pointer(&devices, g_ptr_array_unref);

	test_fixture_add_canon(fixture);
	/* ensure the event was processed by helper thread */
	g_usleep(G_USEC_PER_SEC / 2);

	/* still not returned (and no event fired) */
	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(devices->len, ==, 0);
	g_assert_cmpint(events, ==, 0);
	g_clear_pointer(&devices, g_ptr_array_unref);

	/* run mainloop, which causes the event to be processed */
	while (g_main_context_iteration(NULL, FALSE)) {
	}

	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(events, ==, 1);
	g_assert_cmpint(devices->len, ==, 1);
	g_clear_pointer(&devices, g_ptr_array_unref);
}

static void
test_ctx_hotplug_dispose(UMockdevTestbedFixture *fixture, UNUSED_DATA)
{
	g_autoptr(GPtrArray) devices = NULL;
	gint events = 0;

	g_signal_connect(fixture->ctx, "device-added", G_CALLBACK(count_hotplug_event_cb), &events);

	g_usb_context_enumerate(fixture->ctx);
	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(devices->len, ==, 0);
	g_assert_cmpint(events, ==, 0);
	g_clear_pointer(&devices, g_ptr_array_unref);

	test_fixture_add_canon(fixture);
	/* ensure the event was processed by helper thread */
	g_usleep(G_USEC_PER_SEC / 2);

	/* still not returned (and no event fired) */
	g_usb_context_enumerate(fixture->ctx);
	devices = g_usb_context_get_devices(fixture->ctx);
	g_assert_cmpint(devices->len, ==, 0);
	g_assert_cmpint(events, ==, 0);
	g_clear_pointer(&devices, g_ptr_array_unref);

	/* idle handler is pending, we dispose our context reference */
	g_object_run_dispose(G_OBJECT(fixture->ctx));

	/* run mainloop, which causes the event to be processed */
	while (g_main_context_iteration(NULL, FALSE)) {
	}

	/* but no signal is fired */
	g_assert_cmpint(events, ==, 0);

	g_clear_object(&fixture->ctx);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	g_test_add("/gusb/ctx/enumerate",
		   UMockdevTestbedFixture,
		   NULL,
		   test_fixture_setup_empty,
		   test_ctx_enumerate,
		   test_fixture_teardown);

	g_test_add("/gusb/ctx/hotplug",
		   UMockdevTestbedFixture,
		   NULL,
		   test_fixture_setup_empty,
		   test_ctx_hotplug,
		   test_fixture_teardown);

	g_test_add("/gusb/ctx/hotplug-dispose",
		   UMockdevTestbedFixture,
		   NULL,
		   test_fixture_setup_empty,
		   test_ctx_hotplug_dispose,
		   test_fixture_teardown);

	return g_test_run();
}
