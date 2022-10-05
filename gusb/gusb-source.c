/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:gusb-source
 * @short_description: GSource integration for libusb
 *
 * This object can be used to integrate libusb into the GLib main loop.
 */

#include "config.h"

#include <libusb-1.0/libusb.h>
#include <stdlib.h>

#include "gusb-context-private.h"
#include "gusb-context.h"
#include "gusb-source-private.h"
#include "gusb-source.h"
#include "gusb-util.h"

/* the <poll.h> header is not available on all platforms */
#ifndef POLLIN
#define POLLIN	0x0001
#define POLLOUT 0x0004
#endif

/**
 * g_usb_source_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
GQuark
g_usb_source_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("g_usb_source_error");
	return quark;
}

struct _GUsbSource {
	GSource source;
	GSList *pollfds;
	libusb_context *ctx;
};

static void
g_usb_source_pollfd_add(GUsbSource *self, int fd, short events)
{
	GPollFD *pollfd = g_new0(GPollFD, 1);

	pollfd->fd = fd;
	pollfd->events = 0;
	pollfd->revents = 0;
	if (events & POLLIN)
		pollfd->events |= G_IO_IN;
	if (events & POLLOUT)
		pollfd->events |= G_IO_OUT;

	self->pollfds = g_slist_prepend(self->pollfds, pollfd);
	g_source_add_poll((GSource *)self, pollfd);
}

static void
g_usb_source_pollfd_added_cb(int fd, short events, void *user_data)
{
	GUsbSource *self = user_data;
	g_usb_source_pollfd_add(self, fd, events);
}

static void
g_usb_source_pollfd_removed_cb(int fd, void *user_data)
{
	GUsbSource *self = user_data;

	/* find the pollfd in the list */
	for (GSList *elem = self->pollfds; elem != NULL; elem = elem->next) {
		GPollFD *pollfd = elem->data;
		if (pollfd->fd == fd) {
			g_source_remove_poll((GSource *)self, pollfd);
			g_free(pollfd);
			self->pollfds = g_slist_delete_link(self->pollfds, elem);
			return;
		}
	}
	g_warning("couldn't find fd %d in list", fd);
}

static gboolean
g_usb_source_prepare(GSource *source, gint *timeout)
{
	GUsbSource *self = (GUsbSource *)source;

	/* before the poll */
	libusb_lock_events(self->ctx);

	/* release the lock at least once per minute */
	*timeout = 60000;
	return FALSE;
}

static gboolean
g_usb_source_check(GSource *source)
{
	GUsbSource *self = (GUsbSource *)source;

	for (GSList *elem = self->pollfds; elem != NULL; elem = elem->next) {
		GPollFD *pollfd = elem->data;
		if (pollfd->revents)
			return TRUE;
	}
	libusb_unlock_events(self->ctx);
	return FALSE;
}

static gboolean
g_usb_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	GUsbSource *self = (GUsbSource *)source;
	struct timeval tv = {0, 0};
	gint rc;

	rc = libusb_handle_events_locked(self->ctx, &tv);
	libusb_unlock_events(self->ctx);
	if (rc < 0)
		g_warning("failed to handle event: %s [%i]", g_usb_strerror(rc), rc);

	if (callback != NULL)
		callback(user_data);

	return G_SOURCE_CONTINUE;
}

static void
g_usb_source_finalize(GSource *source)
{
	GUsbSource *self = (GUsbSource *)source;
	g_slist_free(self->pollfds);
}

static GSourceFuncs usb_source_funcs = {g_usb_source_prepare,
					g_usb_source_check,
					g_usb_source_dispatch,
					g_usb_source_finalize};

GUsbSource *
_g_usb_source_new(GMainContext *main_ctx, GUsbContext *gusb_ctx)
{
	GUsbSource *self;
	const struct libusb_pollfd **pollfds;

	self = (GUsbSource *)g_source_new(&usb_source_funcs, sizeof(GUsbSource));
	self->pollfds = NULL;
	self->ctx = _g_usb_context_get_context(gusb_ctx);

	/* watch the fd's already created */
	pollfds = libusb_get_pollfds(self->ctx);
	for (guint i = 0; pollfds != NULL && pollfds[i] != NULL; i++)
		g_usb_source_pollfd_add(self, pollfds[i]->fd, pollfds[i]->events);
	free(pollfds);

	/* watch for PollFD changes */
	g_source_attach((GSource *)self, main_ctx);
	libusb_set_pollfd_notifiers(self->ctx,
				    g_usb_source_pollfd_added_cb,
				    g_usb_source_pollfd_removed_cb,
				    self);
	return self;
}

static void
g_usb_source_pollfd_remove_cb(gpointer data, gpointer user_data)
{
	GPollFD *pollfd = (GPollFD *)data;
	GSource *source = (GSource *)user_data;
	g_source_remove_poll(source, pollfd);
}

void
_g_usb_source_destroy(GUsbSource *self)
{
	libusb_set_pollfd_notifiers(self->ctx, NULL, NULL, NULL);
	g_slist_foreach(self->pollfds, g_usb_source_pollfd_remove_cb, self);
	g_slist_free_full(g_steal_pointer(&self->pollfds), g_free);
	g_source_destroy((GSource *)self);
}

/**
 * g_usb_source_set_callback:
 * @self: a #GUsbSource
 * @func: a function to call
 * @data: data to pass to @func
 * @notify: a #GDestroyNotify
 *
 * Set a callback to be called when the source is dispatched.
 *
 * Since: 0.1.0
 **/
void
g_usb_source_set_callback(GUsbSource *self, GSourceFunc func, gpointer data, GDestroyNotify notify)
{
	g_source_set_callback((GSource *)self, func, data, notify);
}
