/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#include <gusb/gusb-util.h>
#include <gusb/gusb-interface.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE		(g_usb_device_get_type ())
#define G_USB_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_DEVICE, GUsbDevice))
#define G_USB_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_DEVICE))
#define G_USB_DEVICE_ERROR		(g_usb_device_error_quark ())

typedef struct _GUsbDevicePrivate	GUsbDevicePrivate;
typedef struct _GUsbDevice		GUsbDevice;
typedef struct _GUsbDeviceClass		GUsbDeviceClass;

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
	G_USB_DEVICE_ERROR_LAST
} GUsbDeviceError;

/**
 * GUsbDeviceClaimInterfaceFlags:
 *
 * Flags for the g_usb_device_claim_interface and
 * g_usb_device_release_interface methods flags parameters.
 **/
typedef enum {
	G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER		= 1 << 0,
} GUsbDeviceClaimInterfaceFlags;

/**
 * GUsbDeviceClassCode:
 *
 * The USB device class.
 **/
typedef enum {
	G_USB_DEVICE_CLASS_INTERFACE_DESC		= 0x00,
	G_USB_DEVICE_CLASS_AUDIO			= 0x01,
	G_USB_DEVICE_CLASS_COMMUNICATIONS		= 0x02,
	G_USB_DEVICE_CLASS_HID				= 0x03,
	G_USB_DEVICE_CLASS_PHYSICAL			= 0x05,
	G_USB_DEVICE_CLASS_IMAGE			= 0x06,
	G_USB_DEVICE_CLASS_PRINTER			= 0x07,
	G_USB_DEVICE_CLASS_MASS_STORAGE			= 0x08,
	G_USB_DEVICE_CLASS_HUB				= 0x09,
	G_USB_DEVICE_CLASS_CDC_DATA			= 0x0a,
	G_USB_DEVICE_CLASS_SMART_CARD			= 0x0b,
	G_USB_DEVICE_CLASS_CONTENT_SECURITY		= 0x0d,
	G_USB_DEVICE_CLASS_VIDEO			= 0x0e,
	G_USB_DEVICE_CLASS_PERSONAL_HEALTHCARE		= 0x0f,
	G_USB_DEVICE_CLASS_AUDIO_VIDEO			= 0x10,
	G_USB_DEVICE_CLASS_BILLBOARD			= 0x11,
	G_USB_DEVICE_CLASS_DIAGNOSTIC			= 0xdc,
	G_USB_DEVICE_CLASS_WIRELESS_CONTROLLER		= 0xe0,
	G_USB_DEVICE_CLASS_MISCELLANEOUS		= 0xef,
	G_USB_DEVICE_CLASS_APPLICATION_SPECIFIC		= 0xfe,
	G_USB_DEVICE_CLASS_VENDOR_SPECIFIC		= 0xff
} GUsbDeviceClassCode;

struct _GUsbDevice
{
	 GObject			 parent;
	 GUsbDevicePrivate		*priv;
};

struct _GUsbDeviceClass
{
	GObjectClass			 parent_class;
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[64];
};

GType			 g_usb_device_get_type		(void);
GQuark			 g_usb_device_error_quark	(void);

const gchar		*g_usb_device_get_platform_id	(GUsbDevice	*device);
GUsbDevice		*g_usb_device_get_parent	(GUsbDevice	*device);
GPtrArray		*g_usb_device_get_children	(GUsbDevice	*device);

guint8			 g_usb_device_get_bus		(GUsbDevice	*device);
guint8			 g_usb_device_get_address	(GUsbDevice	*device);
guint8			 g_usb_device_get_port_number	(GUsbDevice	*device);

guint16			 g_usb_device_get_vid		(GUsbDevice	*device);
guint16			 g_usb_device_get_pid		(GUsbDevice	*device);
guint16			 g_usb_device_get_release	(GUsbDevice	*device);
guint16			 g_usb_device_get_spec		(GUsbDevice	*device);
const gchar		*g_usb_device_get_vid_as_str	(GUsbDevice	*device);
const gchar		*g_usb_device_get_pid_as_str	(GUsbDevice	*device);
guint8			 g_usb_device_get_device_class	(GUsbDevice	*device);
guint8			 g_usb_device_get_device_subclass	(GUsbDevice *device);
guint8			 g_usb_device_get_device_protocol	(GUsbDevice *device);

guint8			 g_usb_device_get_manufacturer_index	(GUsbDevice *device);
guint8			 g_usb_device_get_product_index		(GUsbDevice *device);
guint8			 g_usb_device_get_serial_number_index	(GUsbDevice *device);
guint8			 g_usb_device_get_custom_index	(GUsbDevice	*device,
							 guint8		 class_id,
							 guint8		 subclass_id,
							 guint8		 protocol_id,
							 GError		**error);

GUsbInterface		*g_usb_device_get_interface	(GUsbDevice	*device,
							 guint8		 class_id,
							 guint8		 subclass_id,
							 guint8		 protocol_id,
							 GError		**error);
GPtrArray		*g_usb_device_get_interfaces	(GUsbDevice	*device,
							 GError		**error);

gboolean		 g_usb_device_open		(GUsbDevice	*device,
							 GError		**error);
gboolean		 g_usb_device_close		(GUsbDevice	*device,
							 GError		**error);

gboolean		 g_usb_device_reset		(GUsbDevice	*device,
							 GError		**error);

gint			 g_usb_device_get_configuration (GUsbDevice	*device,
							 GError		**error);
gboolean		 g_usb_device_set_configuration (GUsbDevice	*device,
							 gint		 configuration,
							 GError		**error);

gboolean		 g_usb_device_claim_interface	(GUsbDevice	*device,
							 gint		 interface,
							 GUsbDeviceClaimInterfaceFlags flags,
							 GError		**error);
gboolean		 g_usb_device_release_interface	(GUsbDevice	*device,
							 gint		 interface,
							 GUsbDeviceClaimInterfaceFlags flags,
							 GError		**error);
gboolean		 g_usb_device_set_interface_alt	(GUsbDevice	*device,
							 gint		 interface,
							 guint8		 alt,
							 GError		**error);

gchar			*g_usb_device_get_string_descriptor (GUsbDevice *device,
							 guint8		 desc_index,
							 GError		**error);

/* sync -- TODO: use GCancellable and GUsbSource */
gboolean		 g_usb_device_control_transfer	(GUsbDevice	*device,
							 GUsbDeviceDirection direction,
							 GUsbDeviceRequestType request_type,
							 GUsbDeviceRecipient recipient,
							 guint8		 request,
							 guint16	 value,
							 guint16	 idx,
							 guint8		*data,
							 gsize		 length,
							 gsize		*actual_length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GError		**error);

gboolean		 g_usb_device_bulk_transfer	(GUsbDevice	*device,
							 guint8		 endpoint,
							 guint8		*data,
							 gsize		 length,
							 gsize		*actual_length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GError		**error);

gboolean		 g_usb_device_interrupt_transfer (GUsbDevice	*device,
							 guint8		 endpoint,
							 guint8		*data,
							 gsize		 length,
							 gsize		*actual_length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GError		**error);

/* async */

void		 g_usb_device_control_transfer_async	(GUsbDevice	*device,
							 GUsbDeviceDirection direction,
							 GUsbDeviceRequestType request_type,
							 GUsbDeviceRecipient recipient,
							 guint8		 request,
							 guint16	 value,
							 guint16	 idx,
							 guint8		*data,
							 gsize		 length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gssize		 g_usb_device_control_transfer_finish	(GUsbDevice	*device,
							 GAsyncResult	*res,
							 GError		**error);

void		 g_usb_device_bulk_transfer_async	(GUsbDevice	*device,
							 guint8		 endpoint,
							 guint8		*data,
							 gsize		 length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gssize		 g_usb_device_bulk_transfer_finish	(GUsbDevice	*device,
							 GAsyncResult	*res,
							 GError		**error);

void		 g_usb_device_interrupt_transfer_async	(GUsbDevice	*device,
							 guint8		 endpoint,
							 guint8		*data,
							 gsize		 length,
							 guint		 timeout,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gssize		 g_usb_device_interrupt_transfer_finish	(GUsbDevice	*device,
							 GAsyncResult	*res,
							 GError		**error);

G_END_DECLS
