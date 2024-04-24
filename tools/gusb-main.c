/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib.h>
#include <gusb/gusb.h>
#include <locale.h>
#include <string.h>

static void
gusb_log_ignore_cb(const gchar *log_domain,
		   GLogLevelFlags log_level,
		   const gchar *message,
		   gpointer user_data)
{
}

static void
gusb_log_handler_cb(const gchar *log_domain,
		    GLogLevelFlags log_level,
		    const gchar *message,
		    gpointer user_data)
{
	gchar str_time[255];
	time_t the_time;

	/* header always in green */
	time(&the_time);
	strftime(str_time, 254, "%H:%M:%S", localtime(&the_time));
	g_print("%c[%dmTI:%s\t", 0x1B, 32, str_time);

	/* critical is also in red */
	if (log_level == G_LOG_LEVEL_CRITICAL || log_level == G_LOG_LEVEL_WARNING ||
	    log_level == G_LOG_LEVEL_ERROR) {
		g_print("%c[%dm%s\n%c[%dm", 0x1B, 31, message, 0x1B, 0);
	} else {
		/* debug in blue */
		g_print("%c[%dm%s\n%c[%dm", 0x1B, 34, message, 0x1B, 0);
	}
}

typedef struct {
	GOptionContext *context;
	GUsbContext *usb_ctx;
	GPtrArray *cmd_array;
} GUsbCmdPrivate;

typedef gboolean (*GUsbCmdPrivateCb)(GUsbCmdPrivate *cmd, gchar **values, GError **error);

typedef struct {
	gchar *name;
	gchar *description;
	GUsbCmdPrivateCb callback;
} GUsbCmdItem;

static void
gusb_cmd_item_free(GUsbCmdItem *item)
{
	g_free(item->name);
	g_free(item->description);
	g_slice_free(GUsbCmdItem, item);
}

/*
 * gusb_sort_command_name_cb:
 */
static gint
gusb_sort_command_name_cb(GUsbCmdItem **item1, GUsbCmdItem **item2)
{
	return g_strcmp0((*item1)->name, (*item2)->name);
}

static void
gusb_cmd_add(GPtrArray *array,
	     const gchar *name,
	     const gchar *description,
	     GUsbCmdPrivateCb callback)
{
	g_auto(GStrv) names = g_strsplit(name, ",", -1);
	for (guint i = 0; names[i] != NULL; i++) {
		GUsbCmdItem *item = g_slice_new0(GUsbCmdItem);
		item->name = g_strdup(names[i]);
		if (i == 0) {
			item->description = g_strdup(description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf("Alias to %s", names[0]);
		}
		item->callback = callback;
		g_ptr_array_add(array, item);
	}
}

static gchar *
gusb_cmd_get_descriptions(GPtrArray *array)
{
	guint len;
	guint max_len = 19;
	g_autoptr(GString) string = NULL;

	/* print each command */
	string = g_string_new("");
	for (guint i = 0; i < array->len; i++) {
		GUsbCmdItem *item = g_ptr_array_index(array, i);
		g_string_append(string, "  ");
		g_string_append(string, item->name);
		g_string_append(string, " ");
		len = strlen(item->name);
		for (guint j = len; j < max_len + 2; j++)
			g_string_append_c(string, ' ');
		g_string_append(string, item->description);
		g_string_append_c(string, '\n');
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size(string, string->len - 1);

	return g_string_free(g_steal_pointer(&string), FALSE);
}

static void
gusb_main_device_open(GUsbDevice *device)
{
	guint8 idx;
	g_autoptr(GError) error = NULL;

	/* open */
	if (!g_usb_device_open(device, &error)) {
		g_print("failed to open: %s\n", error->message);
		return;
	}

	/* print info we can only get whilst open */
	idx = g_usb_device_get_product_index(device);
	if (idx != 0x00) {
		g_autofree gchar *product = g_usb_device_get_string_descriptor(device, idx, &error);
		if (product == NULL) {
			g_print("failed to get string desc: %s\n", error->message);
			return;
		}
		g_print("product: %s\n", product);
	}
	if (!g_usb_device_close(device, &error)) {
		g_print("failed to close: %s\n", error->message);
		return;
	}
}

static void
gusb_device_list_added_cb(GUsbContext *context, GUsbDevice *device, gpointer user_data)
{
	g_print("device %s added %x:%x\n",
		g_usb_device_get_platform_id(device),
		g_usb_device_get_bus(device),
		g_usb_device_get_address(device));
	gusb_main_device_open(device);
}

static void
gusb_device_list_removed_cb(GUsbContext *context, GUsbDevice *device, gpointer user_data)
{
	g_print("device %s removed %x:%x\n",
		g_usb_device_get_platform_id(device),
		g_usb_device_get_bus(device),
		g_usb_device_get_address(device));
}

static gint
gusb_devices_sort_by_platform_id_cb(gconstpointer a, gconstpointer b)
{
	GUsbDevice *device_a = *((GUsbDevice **)a);
	GUsbDevice *device_b = *((GUsbDevice **)b);
	return g_strcmp0(g_usb_device_get_platform_id(device_a),
			 g_usb_device_get_platform_id(device_b));
}

static gboolean
gusb_cmd_show_cb(GNode *node, gpointer data)
{
	GUsbDevice *device = G_USB_DEVICE(node->data);
	const gchar *tmp;
	g_autofree gchar *product = NULL;
	g_autofree gchar *vendor = NULL;
	g_autoptr(GString) str = NULL;

	if (device == NULL) {
		g_print("Root Device\n");
		return FALSE;
	}

	/* indent */
	str = g_string_new("");
	for (GNode *n = node; n->data != NULL; n = n->parent)
		g_string_append(str, " ");

	/* add bus:address */
	g_string_append_printf(str,
			       "%02x:%02x [%04x:%04x]",
			       g_usb_device_get_bus(device),
			       g_usb_device_get_address(device),
			       g_usb_device_get_vid(device),
			       g_usb_device_get_pid(device));

	/* pad */
	for (guint i = str->len; i < 30; i++)
		g_string_append(str, " ");

	/* We don't error check these as not all devices have these
	   (and the device_open may have failed). */
	g_usb_device_open(device, NULL);
	vendor = g_usb_device_get_string_descriptor(device,
						    g_usb_device_get_manufacturer_index(device),
						    NULL);
	product = g_usb_device_get_string_descriptor(device,
						     g_usb_device_get_product_index(device),
						     NULL);

	/* lookup from usb.ids */
	if (vendor == NULL) {
		tmp = g_usb_device_get_vid_as_str(device);
		if (tmp != NULL)
			vendor = g_strdup(tmp);
	}

	if (product == NULL) {
		tmp = g_usb_device_get_pid_as_str(device);
		if (tmp != NULL)
			product = g_strdup(tmp);
	}

	/* a hub */
	if (g_usb_device_get_device_class(device) == 0x09 && product == NULL) {
		product = g_strdup("USB HUB");
	}

	/* fall back to the VID/PID */
	if (product == NULL)
		product = g_strdup("Unknown");

	if (vendor == NULL)
		vendor = g_strdup("Unknown");

	/* add bus:address */
	g_string_append_printf(str, "%s - %s", vendor, product);

	g_print("%s\n", str->str);
	g_usb_device_close(device, NULL);
	return FALSE;
}

static gboolean
gusb_cmd_show(GUsbCmdPrivate *priv, gchar **values, GError **error)
{
	g_autoptr(GNode) node = NULL;
	g_autoptr(GPtrArray) devices = NULL;

	/* sort */
	devices = g_usb_context_get_devices(priv->usb_ctx);
	g_ptr_array_sort(devices, gusb_devices_sort_by_platform_id_cb);

	/* make a tree of the devices */
	node = g_node_new(NULL);
	for (guint i = 0; i < devices->len; i++) {
		GNode *n;
		GUsbDevice *device = g_ptr_array_index(devices, i);
		GUsbDevice *parent = g_usb_device_get_parent(device);
		if (parent == NULL) {
			g_node_append_data(node, device);
			continue;
		}
		n = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, parent);
		if (n == NULL) {
			g_set_error(error,
				    1,
				    0,
				    "no parent node for %s",
				    g_usb_device_get_platform_id(device));
			return FALSE;
		}
		g_node_append_data(n, device);
	}

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, gusb_cmd_show_cb, priv);
	return TRUE;
}

static gboolean
gusb_cmd_watch(GUsbCmdPrivate *priv, gchar **values, GError **error)
{
	g_autoptr(GMainLoop) loop = NULL;
	g_autoptr(GPtrArray) devices = NULL;

	devices = g_usb_context_get_devices(priv->usb_ctx);
	for (guint i = 0; i < devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(devices, i);
		g_print("device %s already present %x:%x\n",
			g_usb_device_get_platform_id(device),
			g_usb_device_get_bus(device),
			g_usb_device_get_address(device));
		gusb_main_device_open(device);
	}

	loop = g_main_loop_new(NULL, FALSE);
	g_signal_connect(priv->usb_ctx,
			 "device-added",
			 G_CALLBACK(gusb_device_list_added_cb),
			 priv);
	g_signal_connect(priv->usb_ctx,
			 "device-removed",
			 G_CALLBACK(gusb_device_list_removed_cb),
			 priv);
	g_main_loop_run(loop);
	return TRUE;
}

static gboolean
gusb_cmd_replug(GUsbCmdPrivate *priv, gchar **values, GError **error)
{
	guint16 vid, pid;
	g_autoptr(GUsbDevice) device = NULL;
	g_autoptr(GUsbDevice) device_new = NULL;

	/* check args */
	if (g_strv_length(values) != 2) {
		g_set_error_literal(error, 1, 0, "no VID:PID specified");
		return FALSE;
	}

	/* get vid:pid */
	vid = g_ascii_strtoull(values[0], NULL, 16);
	pid = g_ascii_strtoull(values[1], NULL, 16);
	device = g_usb_context_find_by_vid_pid(priv->usb_ctx, vid, pid, error);
	if (device == NULL)
		return FALSE;

	/* watch for debugging */
	g_signal_connect(priv->usb_ctx,
			 "device-added",
			 G_CALLBACK(gusb_device_list_added_cb),
			 priv);
	g_signal_connect(priv->usb_ctx,
			 "device-removed",
			 G_CALLBACK(gusb_device_list_removed_cb),
			 priv);

	/* wait for replug */
	device_new = g_usb_context_wait_for_replug(priv->usb_ctx, device, 5000, error);
	return device_new != NULL;
}

static gboolean
gusb_cmd_load(GUsbCmdPrivate *priv, gchar **values, GError **error)
{
	/* check args */
	if (g_strv_length(values) == 0) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_ARGUMENT,
				    "no filename specified");
		return FALSE;
	}

	for (guint i = 0; values[i] != NULL; i++) {
		JsonObject *json_obj;
		JsonNode *json_node;
		g_autoptr(JsonParser) parser = json_parser_new();

		/* parse */
		if (!json_parser_load_from_file(parser, values[i], error))
			return FALSE;

		/* sanity check */
		json_node = json_parser_get_root(parser);
		if (!JSON_NODE_HOLDS_OBJECT(json_node)) {
			g_set_error_literal(error,
					    G_IO_ERROR,
					    G_IO_ERROR_INVALID_DATA,
					    "not a JSON object");
			return FALSE;
		}

		/* not supplied */
		json_obj = json_node_get_object(json_node);
		if (!g_usb_context_load(priv->usb_ctx, json_obj, error))
			return FALSE;
	}

	/* success */
	return gusb_cmd_show(priv, NULL, error);
}

static gboolean
gusb_cmd_save(GUsbCmdPrivate *priv, gchar **values, GError **error)
{
	g_autofree gchar *data = NULL;
	g_autoptr(JsonBuilder) json_builder = json_builder_new();
	g_autoptr(JsonGenerator) json_generator = NULL;
	g_autoptr(JsonNode) json_root = NULL;

	if (!g_usb_context_save(priv->usb_ctx, json_builder, error))
		return FALSE;

	/* export as a string */
	json_root = json_builder_get_root(json_builder);
	json_generator = json_generator_new();
	json_generator_set_pretty(json_generator, TRUE);
	json_generator_set_root(json_generator, json_root);
	data = json_generator_to_data(json_generator, NULL);
	if (data == NULL) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "Failed to convert to JSON string");
		return FALSE;
	}

	/* save to file */
	if (g_strv_length(values) == 1)
		return g_file_set_contents(values[0], data, -1, error);

	/* just print */
	g_print("%s\n", data);
	return TRUE;
}

static gboolean
gusb_cmd_run(GUsbCmdPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	g_autoptr(GString) string = g_string_new(NULL);

	/* find command */
	for (guint i = 0; i < priv->cmd_array->len; i++) {
		GUsbCmdItem *item = g_ptr_array_index(priv->cmd_array, i);
		if (g_strcmp0(item->name, command) == 0) {
			return item->callback(priv, values, error);
		}
	}

	/* TRANSLATORS: error message */
	g_string_append_printf(string, "%s\n", "Command not found, valid commands are:");
	for (guint i = 0; i < priv->cmd_array->len; i++) {
		GUsbCmdItem *item = g_ptr_array_index(priv->cmd_array, i);
		g_string_append_printf(string, " * %s\n", item->name);
	}
	g_set_error_literal(error, 1, 0, string->str);
	return FALSE;
}

static void
gusb_cmd_private_free(GUsbCmdPrivate *priv)
{
	if (priv->cmd_array != NULL)
		g_ptr_array_unref(priv->cmd_array);
	if (priv->usb_ctx != NULL)
		g_object_unref(priv->usb_ctx);
	g_option_context_free(priv->context);
	g_slice_free(GUsbCmdPrivate, priv);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbCmdPrivate, gusb_cmd_private_free)

int
main(int argc, char *argv[])
{
	GUsbContextFlags context_flags = G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES;
	gboolean verbose = FALSE;
	gboolean save_events = FALSE;
	g_autofree gchar *cmd_descriptions = NULL;
	g_autofree gchar *options_help = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbCmdPrivate) priv = NULL;

	const GOptionEntry options[] = {{"verbose",
					 'v',
					 0,
					 G_OPTION_ARG_NONE,
					 &verbose,
					 "Show extra debugging information",
					 NULL},
					 {"events",
					 '\0',
					 0,
					 G_OPTION_ARG_NONE,
					 &save_events,
					 "Save USB events",
					 NULL},
					{NULL}};

	setlocale(LC_ALL, "");

	/* create helper object */
	priv = g_slice_new0(GUsbCmdPrivate);
	priv->context = g_option_context_new("GUSB Console Program");
	g_option_context_add_main_entries(priv->context, options, NULL);
	if (!g_option_context_parse(priv->context, &argc, &argv, &error)) {
		g_printerr("Failed to parse arguments: %s\n", error->message);
		return 2;
	}

	/* verbose? */
	if (verbose) {
		g_log_set_fatal_mask(NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
		g_log_set_handler("GUsb",
				  G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_DEBUG |
				      G_LOG_LEVEL_WARNING,
				  gusb_log_handler_cb,
				  NULL);
	} else {
		/* hide all debugging */
		g_log_set_fatal_mask(NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
		g_log_set_handler("GUsb", G_LOG_LEVEL_DEBUG, gusb_log_ignore_cb, NULL);
	}

	/* GUsbContext */
	priv->usb_ctx = g_usb_context_new(NULL);
	if (save_events)
		context_flags |= G_USB_CONTEXT_FLAGS_SAVE_EVENTS;
	if (verbose)
		context_flags |= G_USB_CONTEXT_FLAGS_DEBUG;
	g_usb_context_set_flags(priv->usb_ctx, context_flags);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func((GDestroyNotify)gusb_cmd_item_free);
	gusb_cmd_add(priv->cmd_array, "show", "Show currently connected devices", gusb_cmd_show);
	gusb_cmd_add(priv->cmd_array, "watch", "Watch devices as they come and go", gusb_cmd_watch);
	gusb_cmd_add(priv->cmd_array, "replug", "Watch a device as it reconnects", gusb_cmd_replug);
	gusb_cmd_add(priv->cmd_array, "load", "Load a set of devices from JSON", gusb_cmd_load);
	gusb_cmd_add(priv->cmd_array, "save", "Save a set of devices to JSON", gusb_cmd_save);

	/* sort by command name */
	g_ptr_array_sort(priv->cmd_array, (GCompareFunc)gusb_sort_command_name_cb);

	/* get a list of the commands */
	cmd_descriptions = gusb_cmd_get_descriptions(priv->cmd_array);
	g_option_context_set_summary(priv->context, cmd_descriptions);

	/* nothing specified */
	if (argc < 2) {
		options_help = g_option_context_get_help(priv->context, TRUE, NULL);
		g_print("%s", options_help);
		return 1;
	}

	/* run the specified command */
	if (!gusb_cmd_run(priv, argv[1], (gchar **)&argv[2], &error)) {
		g_print("%s\n", error->message);
		return 1;
	}

	/* success */
	return 0;
}
