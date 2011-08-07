/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:usb-source
 * @short_description: GSource integration for libusb
 *
 * This object can be used to integrate libusb into the GLib main loop.
 */

#include "config.h"

#include <libusb-1.0/libusb.h>
#include <poll.h>
#include <stdlib.h>

#include "gusb-source.h"

/* libusb_strerror is in upstream */
#define libusb_strerror(error) "unknown"

struct _GUsbSource {
	GSource		 source;
	GSList		*pollfds;
	libusb_context	*ctx;
};

static void
g_usb_source_pollfd_add (GUsbSource *source, int fd, short events)
{
	GPollFD *pollfd = g_slice_new(GPollFD);
	pollfd->fd = fd;
	pollfd->events = 0;
	pollfd->revents = 0;
	if (events & POLLIN)
		pollfd->events |= G_IO_IN;
	if (events & POLLOUT)
		pollfd->events |= G_IO_OUT;

	source->pollfds = g_slist_prepend(source->pollfds, pollfd);
	g_source_add_poll((GSource *)source, pollfd);

	g_debug ("added pollfd %i, events %hi", fd, events);
}

static void
g_usb_source_pollfd_added_cb (int fd, short events, void *user_data)
{
	GUsbSource *source = user_data;
	g_usb_source_pollfd_add (source, fd, events);
}

static void
g_usb_source_pollfd_remove (GUsbSource *source, int fd)
{
	GPollFD *pollfd;
	GSList *elem = source->pollfds;

	g_debug("remove pollfd %i", fd);

	/* nothing to see here, move along */
	if (elem == NULL) {
		g_warning("cannot remove from list as list is empty?");
		return;
	}

	/* find the pollfd in the list */
	do {
		pollfd = elem->data;
		if (pollfd->fd != fd)
			continue;

		g_source_remove_poll((GSource *)source, pollfd);
		g_slice_free(GPollFD, pollfd);
		source->pollfds = g_slist_delete_link(source->pollfds, elem);
		return;
	} while ((elem = g_slist_next(elem)));
	g_warning ("couldn't find fd %d in list", fd);
}

static void
g_usb_source_pollfd_removed_cb(int fd, void *user_data)
{
	GUsbSource *source = user_data;

	g_usb_source_pollfd_remove (source, fd);
}

static void
g_usb_source_pollfd_remove_all (GUsbSource *source)
{
	GPollFD *pollfd;
	GSList *curr, *next;

	next = source->pollfds;
	while (next) {
		curr = next;
		next = g_slist_next(curr);
		pollfd = curr->data;
		g_source_remove_poll((GSource *)source, pollfd);
		g_slice_free (GPollFD, pollfd);
		source->pollfds = g_slist_delete_link(source->pollfds, curr);
	}
}

/**
 * g_usb_source_prepare:
 *
 * Called before all the file descriptors are polled.
 * As we are a file descriptor source, the prepare function returns FALSE.
 * It sets the returned timeout to -1 to indicate that it doesn't mind
 * how long the poll() call blocks.
 *
 * No, we're not going to support FreeBSD.
 **/
static gboolean
g_usb_source_prepare (GSource *source, gint *timeout)
{
	*timeout = -1;
	return FALSE;
}

/**
 * g_usb_source_check:
 *
 * In the check function, it tests the results of the poll() call to see
 * if the required condition has been met, and returns TRUE if so.
 **/
static gboolean
g_usb_source_check (GSource *source)
{
	GUsbSource *usb_source = (GUsbSource *)source;
	GPollFD *pollfd;
	GSList *elem = usb_source->pollfds;

	/* no fds */
	if (elem == NULL)
		return FALSE;

	/* check each pollfd */
	do {
		pollfd = elem->data;
		if (pollfd->revents)
			return TRUE;
	} while ((elem = g_slist_next(elem)));

	return FALSE;
}

static gboolean
g_usb_source_dispatch (GSource *source,
		       GSourceFunc callback,
		       gpointer user_data)
{
	GUsbSource *usb_source = (GUsbSource *)source;
	struct timeval tv = { 0, 0 };
	gint rc;

	rc = libusb_handle_events_timeout (usb_source->ctx, &tv);
	if (rc < 0) {
		g_warning ("failed to handle event: %s [%i]",
			   libusb_strerror (rc), rc);
	}

	if (callback)
		callback(user_data);

	return TRUE;
}

static void
g_usb_source_finalize (GSource *source)
{
	GUsbSource *usb_source = (GUsbSource *)source;
	g_slist_free (usb_source->pollfds);
}

static GSourceFuncs usb_source_funcs = {
	g_usb_source_prepare,
	g_usb_source_check,
	g_usb_source_dispatch,
	g_usb_source_finalize,
	NULL, NULL
};

GUsbSource *
g_usb_source_new (GMainContext *main_ctx, void *libusb_ctx)
{
	guint i;
	const struct libusb_pollfd **pollfds;
	GUsbSource *gusb_source;

	gusb_source = (GUsbSource *)g_source_new (&usb_source_funcs,
						  sizeof(GUsbSource));
	gusb_source->pollfds = NULL;
	gusb_source->ctx = libusb_ctx;

	g_source_attach ((GSource *)gusb_source, main_ctx);

	/* watch the fd's already created */
	pollfds = libusb_get_pollfds (gusb_source->ctx);
	if (pollfds == NULL)
		g_error("libusb_get_pollfds failed to allocate memory");
	for (i=0; pollfds[i] != NULL; i++)
		g_usb_source_pollfd_add (gusb_source,
					 pollfds[i]->fd,
					 pollfds[i]->events);
	free (pollfds);

	/* watch for PollFD changes */
	libusb_set_pollfd_notifiers (gusb_source->ctx,
				     g_usb_source_pollfd_added_cb,
				     g_usb_source_pollfd_removed_cb,
				     gusb_source);
	return gusb_source;
}

void
g_usb_source_destroy (GUsbSource *source)
{
	libusb_set_pollfd_notifiers (source->ctx, NULL, NULL, NULL);
	g_usb_source_pollfd_remove_all (source);
	g_source_destroy ((GSource *)source);
}

void
g_usb_source_set_callback (GUsbSource *source,
			   GSourceFunc func,
			   gpointer data,
			   GDestroyNotify notify)
{
	g_source_set_callback ((GSource *)source, func, data, notify);
}
