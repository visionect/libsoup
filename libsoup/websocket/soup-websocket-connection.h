/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-websocket-connection.h: This file was originally part of Cockpit.
 *
 * Copyright 2013, 2014 Red Hat, Inc.
 *
 * Cockpit is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Cockpit is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "soup-types.h"
#include "soup-websocket.h"

G_BEGIN_DECLS

#define SOUP_TYPE_WEBSOCKET_CONNECTION (soup_websocket_connection_get_type ())
SOUP_AVAILABLE_IN_2_50
G_DECLARE_DERIVABLE_TYPE (SoupWebsocketConnection, soup_websocket_connection, SOUP, WEBSOCKET_CONNECTION, GObject)

struct _SoupWebsocketConnectionClass {
	GObjectClass parent;

	/* signals */
	void      (* message)     (SoupWebsocketConnection *self,
				   SoupWebsocketDataType type,
				   GBytes *message);

	void      (* error)       (SoupWebsocketConnection *self,
				   GError *error);

	void      (* closing)     (SoupWebsocketConnection *self);

	void      (* closed)      (SoupWebsocketConnection *self);

	void      (* pong)        (SoupWebsocketConnection *self,
				   GBytes *message);
};

SOUP_AVAILABLE_IN_2_50
SoupWebsocketConnection *soup_websocket_connection_new (GIOStream                    *stream,
							SoupURI                      *uri,
							SoupWebsocketConnectionType   type,
							const char                   *origin,
							const char                   *protocol);
SOUP_AVAILABLE_IN_2_68
SoupWebsocketConnection *soup_websocket_connection_new_with_extensions (GIOStream                    *stream,
                                                                        SoupURI                      *uri,
                                                                        SoupWebsocketConnectionType   type,
                                                                        const char                   *origin,
                                                                        const char                   *protocol,
                                                                        GList                        *extensions);

SOUP_AVAILABLE_IN_2_50
GIOStream *         soup_websocket_connection_get_io_stream  (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
SoupWebsocketConnectionType soup_websocket_connection_get_connection_type (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
SoupURI *           soup_websocket_connection_get_uri        (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
const char *        soup_websocket_connection_get_origin     (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
const char *        soup_websocket_connection_get_protocol   (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_68
GList *             soup_websocket_connection_get_extensions (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
SoupWebsocketState  soup_websocket_connection_get_state      (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
gushort             soup_websocket_connection_get_close_code (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
const char *        soup_websocket_connection_get_close_data (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_50
void                soup_websocket_connection_send_text      (SoupWebsocketConnection *self,
							      const char *text);
SOUP_AVAILABLE_IN_2_50
void                soup_websocket_connection_send_binary    (SoupWebsocketConnection *self,
							      gconstpointer data,
							      gsize length);
SOUP_AVAILABLE_IN_2_68
void                soup_websocket_connection_send_message   (SoupWebsocketConnection *self,
							      SoupWebsocketDataType type,
							      GBytes *message);

SOUP_AVAILABLE_IN_2_50
void                soup_websocket_connection_close          (SoupWebsocketConnection *self,
							      gushort code,
							      const char *data);

SOUP_AVAILABLE_IN_2_56
guint64             soup_websocket_connection_get_max_incoming_payload_size (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_56
void                soup_websocket_connection_set_max_incoming_payload_size (SoupWebsocketConnection *self,
                                                                             guint64                  max_incoming_payload_size);

SOUP_AVAILABLE_IN_2_58
guint               soup_websocket_connection_get_keepalive_interval (SoupWebsocketConnection *self);

SOUP_AVAILABLE_IN_2_58
void                soup_websocket_connection_set_keepalive_interval (SoupWebsocketConnection *self,
                                                                      guint                    interval);

G_END_DECLS
