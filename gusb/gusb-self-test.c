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

	return g_test_run ();
}

