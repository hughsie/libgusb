/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>
#include <gusb/gusb-bos-descriptor.h>
#include <gusb/gusb-interface.h>
#include <gusb/gusb-util.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE  (g_usb_device_get_type())
#define G_USB_DEVICE_ERROR (g_usb_device_error_quark())

G_DECLARE_DERIVABLE_TYPE(GUsbDevice, g_usb_device, G_USB, DEVICE, GObject)

/**
 * GUsbDeviceDirection:
 *
 * The message direction.
 **/
typedef enum {
	G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST, /* IN */
	G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE  /* OUT */
} GUsbDeviceDirection;

/**
 * GUsbDeviceRequestType:
 *
 * The message request type.
 **/
typedef enum {
	G_USB_DEVICE_REQUEST_TYPE_STANDARD,
	G_USB_DEVICE_REQUEST_TYPE_CLASS,
	G_USB_DEVICE_REQUEST_TYPE_VENDOR,
	G_USB_DEVICE_REQUEST_TYPE_RESERVED
} GUsbDeviceRequestType;

/**
 * GUsbDeviceRecipient:
 *
 * The message recipient.
 **/
typedef enum {
	G_USB_DEVICE_RECIPIENT_DEVICE,
	G_USB_DEVICE_RECIPIENT_INTERFACE,
	G_USB_DEVICE_RECIPIENT_ENDPOINT,
	G_USB_DEVICE_RECIPIENT_OTHER
} GUsbDeviceRecipient;

/**
 * GUsbDeviceError:
 * @G_USB_DEVICE_ERROR_INTERNAL:		Internal error
 * @G_USB_DEVICE_ERROR_IO:			IO error
 * @G_USB_DEVICE_ERROR_TIMED_OUT:		Operation timed out
 * @G_USB_DEVICE_ERROR_NOT_SUPPORTED:		Operation not supported
 * @G_USB_DEVICE_ERROR_NO_DEVICE:		No device found
 * @G_USB_DEVICE_ERROR_NOT_OPEN:		Device is not open
 * @G_USB_DEVICE_ERROR_ALREADY_OPEN:		Device is already open
 * @G_USB_DEVICE_ERROR_CANCELLED:		Operation was cancelled
 * @G_USB_DEVICE_ERROR_FAILED:			Operation failed
 * @G_USB_DEVICE_ERROR_PERMISSION_DENIED:	Permission denied
 * @G_USB_DEVICE_ERROR_BUSY:			Device was busy
 *
 * The error code.
 **/
typedef enum {
	G_USB_DEVICE_ERROR_INTERNAL,
	G_USB_DEVICE_ERROR_IO,
	G_USB_DEVICE_ERROR_TIMED_OUT,
	G_USB_DEVICE_ERROR_NOT_SUPPORTED,
	G_USB_DEVICE_ERROR_NO_DEVICE,
	G_USB_DEVICE_ERROR_NOT_OPEN,
	G_USB_DEVICE_ERROR_ALREADY_OPEN,
	G_USB_DEVICE_ERROR_CANCELLED,
	G_USB_DEVICE_ERROR_FAILED,
	G_USB_DEVICE_ERROR_PERMISSION_DENIED,
	G_USB_DEVICE_ERROR_BUSY,
	/*< private >*/
	G_USB_DEVICE_ERROR_LAST
} GUsbDeviceError;

/**
 * GUsbDeviceClaimInterfaceFlags:
 *
 * Flags for the g_usb_device_claim_interface and
 * g_usb_device_release_interface methods flags parameters.
 **/
typedef enum {
	G_USB_DEVICE_CLAIM_INTERFACE_NONE = 0,
	G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER = 1 << 0,
} GUsbDeviceClaimInterfaceFlags;

/**
 * GUsbDeviceClassCode:
 *
 * The USB device class.
 **/
typedef enum {
	G_USB_DEVICE_CLASS_INTERFACE_DESC = 0x00,
	G_USB_DEVICE_CLASS_AUDIO = 0x01,
	G_USB_DEVICE_CLASS_COMMUNICATIONS = 0x02,
	G_USB_DEVICE_CLASS_HID = 0x03,
	G_USB_DEVICE_CLASS_PHYSICAL = 0x05,
	G_USB_DEVICE_CLASS_IMAGE = 0x06,
	G_USB_DEVICE_CLASS_PRINTER = 0x07,
	G_USB_DEVICE_CLASS_MASS_STORAGE = 0x08,
	G_USB_DEVICE_CLASS_HUB = 0x09,
	G_USB_DEVICE_CLASS_CDC_DATA = 0x0a,
	G_USB_DEVICE_CLASS_SMART_CARD = 0x0b,
	G_USB_DEVICE_CLASS_CONTENT_SECURITY = 0x0d,
	G_USB_DEVICE_CLASS_VIDEO = 0x0e,
	G_USB_DEVICE_CLASS_PERSONAL_HEALTHCARE = 0x0f,
	G_USB_DEVICE_CLASS_AUDIO_VIDEO = 0x10,
	G_USB_DEVICE_CLASS_BILLBOARD = 0x11,
	G_USB_DEVICE_CLASS_DIAGNOSTIC = 0xdc,
	G_USB_DEVICE_CLASS_WIRELESS_CONTROLLER = 0xe0,
	G_USB_DEVICE_CLASS_MISCELLANEOUS = 0xef,
	G_USB_DEVICE_CLASS_APPLICATION_SPECIFIC = 0xfe,
	G_USB_DEVICE_CLASS_VENDOR_SPECIFIC = 0xff
} GUsbDeviceClassCode;

/**
 * GUsbDeviceLangid:
 *
 * The USB language ID.
 **/
typedef enum {
	G_USB_DEVICE_LANGID_INVALID = 0x0000,
	G_USB_DEVICE_LANGID_ENGLISH_UNITED_STATES = 0x0409,
} GUsbDeviceLangid;

struct _GUsbDeviceClass {
	GObjectClass parent_class;
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[64];
};

GQuark
g_usb_device_error_quark(void);

const gchar *
g_usb_device_get_platform_id(GUsbDevice *self);
gboolean
g_usb_device_is_emulated(GUsbDevice *self);
GDateTime *
g_usb_device_get_created(GUsbDevice *self);
GUsbDevice *
g_usb_device_get_parent(GUsbDevice *self);
GPtrArray *
g_usb_device_get_children(GUsbDevice *self);

guint8
g_usb_device_get_bus(GUsbDevice *self);
guint8
g_usb_device_get_address(GUsbDevice *self);
guint8
g_usb_device_get_port_number(GUsbDevice *self);

guint16
g_usb_device_get_vid(GUsbDevice *self);
guint16
g_usb_device_get_pid(GUsbDevice *self);
guint16
g_usb_device_get_release(GUsbDevice *self);
guint16
g_usb_device_get_spec(GUsbDevice *self);
const gchar *
g_usb_device_get_vid_as_str(GUsbDevice *self);
const gchar *
g_usb_device_get_pid_as_str(GUsbDevice *self);
guint8
g_usb_device_get_device_class(GUsbDevice *self);
guint8
g_usb_device_get_device_subclass(GUsbDevice *self);
guint8
g_usb_device_get_device_protocol(GUsbDevice *self);

void
g_usb_device_add_tag(GUsbDevice *self, const gchar *tag);
void
g_usb_device_remove_tag(GUsbDevice *self, const gchar *tag);
gboolean
g_usb_device_has_tag(GUsbDevice *self, const gchar *tag);
GPtrArray *
g_usb_device_get_tags(GUsbDevice *self);

guint8
g_usb_device_get_configuration_index(GUsbDevice *self);
guint8
g_usb_device_get_manufacturer_index(GUsbDevice *self);
guint8
g_usb_device_get_product_index(GUsbDevice *self);
guint8
g_usb_device_get_serial_number_index(GUsbDevice *self);
guint8
g_usb_device_get_custom_index(GUsbDevice *self,
			      guint8 class_id,
			      guint8 subclass_id,
			      guint8 protocol_id,
			      GError **error);

GUsbInterface *
g_usb_device_get_interface(GUsbDevice *self,
			   guint8 class_id,
			   guint8 subclass_id,
			   guint8 protocol_id,
			   GError **error);
GPtrArray *
g_usb_device_get_interfaces(GUsbDevice *self, GError **error);

GPtrArray *
g_usb_device_get_events(GUsbDevice *self);
void
g_usb_device_clear_events(GUsbDevice *self);

GPtrArray *
g_usb_device_get_hid_descriptors(GUsbDevice *self, GError **error);
GBytes *
g_usb_device_get_hid_descriptor_default(GUsbDevice *self, GError **error);

GPtrArray *
g_usb_device_get_bos_descriptors(GUsbDevice *self, GError **error);
GUsbBosDescriptor *
g_usb_device_get_bos_descriptor(GUsbDevice *self, guint8 capability, GError **error);

gboolean
g_usb_device_open(GUsbDevice *self, GError **error);
gboolean
g_usb_device_close(GUsbDevice *self, GError **error);

gboolean
g_usb_device_reset(GUsbDevice *self, GError **error);
void
g_usb_device_invalidate(GUsbDevice *self);

gint
g_usb_device_get_configuration(GUsbDevice *self, GError **error);
gboolean
g_usb_device_set_configuration(GUsbDevice *self, gint configuration, GError **error);

gboolean
g_usb_device_claim_interface(GUsbDevice *self,
			     gint iface,
			     GUsbDeviceClaimInterfaceFlags flags,
			     GError **error);
gboolean
g_usb_device_release_interface(GUsbDevice *self,
			       gint iface,
			       GUsbDeviceClaimInterfaceFlags flags,
			       GError **error);
gboolean
g_usb_device_set_interface_alt(GUsbDevice *self, gint iface, guint8 alt, GError **error);

gchar *
g_usb_device_get_string_descriptor(GUsbDevice *self, guint8 desc_index, GError **error);
GBytes *
g_usb_device_get_string_descriptor_bytes(GUsbDevice *self,
					 guint8 desc_index,
					 guint16 langid,
					 GError **error);
GBytes *
g_usb_device_get_string_descriptor_bytes_full(GUsbDevice *self,
					      guint8 desc_index,
					      guint16 langid,
					      gsize length,
					      GError **error);

/* sync -- TODO: use GCancellable and GUsbSource */
gboolean
g_usb_device_control_transfer(GUsbDevice *self,
			      GUsbDeviceDirection direction,
			      GUsbDeviceRequestType request_type,
			      GUsbDeviceRecipient recipient,
			      guint8 request,
			      guint16 value,
			      guint16 idx,
			      guint8 *data,
			      gsize length,
			      gsize *actual_length,
			      guint timeout,
			      GCancellable *cancellable,
			      GError **error);

gboolean
g_usb_device_bulk_transfer(GUsbDevice *self,
			   guint8 endpoint,
			   guint8 *data,
			   gsize length,
			   gsize *actual_length,
			   guint timeout,
			   GCancellable *cancellable,
			   GError **error);

gboolean
g_usb_device_interrupt_transfer(GUsbDevice *self,
				guint8 endpoint,
				guint8 *data,
				gsize length,
				gsize *actual_length,
				guint timeout,
				GCancellable *cancellable,
				GError **error);

/* async */

void
g_usb_device_control_transfer_async(GUsbDevice *self,
				    GUsbDeviceDirection direction,
				    GUsbDeviceRequestType request_type,
				    GUsbDeviceRecipient recipient,
				    guint8 request,
				    guint16 value,
				    guint16 idx,
				    guint8 *data,
				    gsize length,
				    guint timeout,
				    GCancellable *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer user_data);
gssize
g_usb_device_control_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error);

void
g_usb_device_bulk_transfer_async(GUsbDevice *self,
				 guint8 endpoint,
				 guint8 *data,
				 gsize length,
				 guint timeout,
				 GCancellable *cancellable,
				 GAsyncReadyCallback callback,
				 gpointer user_data);
gssize
g_usb_device_bulk_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error);

void
g_usb_device_interrupt_transfer_async(GUsbDevice *self,
				      guint8 endpoint,
				      guint8 *data,
				      gsize length,
				      guint timeout,
				      GCancellable *cancellable,
				      GAsyncReadyCallback callback,
				      gpointer user_data);
gssize
g_usb_device_interrupt_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error);

G_END_DECLS
