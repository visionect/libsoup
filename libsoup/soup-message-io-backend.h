/* soup-message-io-backend.h
 *
 * Copyright 2020 Igalia S.L.
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "soup-types.h"
#include "soup-message-headers.h"
#include "soup-filter-input-stream.h"

G_BEGIN_DECLS

#define SOUP_TYPE_MESSAGE_IO_BACKEND (soup_message_io_backend_get_type ())

G_DECLARE_INTERFACE (SoupMessageIOBackend, soup_message_io_backend, SOUP, MESSAGE_IO_BACKEND, GObject)

typedef gboolean (*SoupMessageSourceFunc) (SoupMessage *, gpointer);

typedef enum {
	SOUP_MESSAGE_IO_COMPLETE,
        SOUP_MESSAGE_IO_INTERRUPTED,
        SOUP_MESSAGE_IO_STOLEN
} SoupMessageIOCompletion;

typedef void (*SoupMessageIOCompletionFn) (GObject                *msg,
					   SoupMessageIOCompletion completion,
					   gpointer                user_data);

struct _SoupMessageIOBackendInterface
{
        GTypeInterface parent;

        void (*finished) (SoupMessage *);
        void (*cleanup) (SoupMessage *);
        void (*pause) (SoupMessage *);
        void (*unpause) (SoupMessage *);
        void (*stolen) (SoupMessage *);
        gboolean (*is_paused) (SoupMessage *);
        gboolean (*in_progress) (SoupMessage *);
        GSource * (*get_source) (SoupMessage *, GCancellable *, SoupMessageSourceFunc, gpointer);
        GInputStream * (*get_response_istream) (SoupMessage *, GError **);
        void (*run) (SoupMessage *, gboolean);
        gboolean (*run_until_read) (SoupMessage *, GCancellable *, GError **);
        gboolean (*run_until_finish) (SoupMessage *, gboolean, GCancellable *, GError **);
        void (*run_until_read_async) (SoupMessage *, int, GCancellable *, GAsyncReadyCallback, gpointer);
        void (*send_item) (SoupMessageIOBackend *, SoupMessageQueueItem *, SoupMessageIOCompletionFn, gpointer);
};

void       soup_message_io_finished    (SoupMessage *msg);
void       soup_message_io_pause       (SoupMessage *msg);
void       soup_message_io_unpause     (SoupMessage *msg);
void       soup_message_io_stolen      (SoupMessage *msg);
gboolean   soup_message_io_in_progress (SoupMessage *msg);
gboolean   soup_message_io_is_paused   (SoupMessage *msg);

void       soup_message_io_run         (SoupMessage *msg,
					gboolean     blocking);

gboolean soup_message_io_run_until_read        (SoupMessage        *msg,
                                                GCancellable       *cancellable,
                                                GError            **error);
void     soup_message_io_run_until_read_async  (SoupMessage        *msg,
						int                 io_priority,
                                                GCancellable       *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer            user_data);
gboolean soup_message_io_run_until_read_finish (SoupMessage        *msg,
                                                GAsyncResult       *result,
                                                GError            **error);

gboolean soup_message_io_run_until_finish      (SoupMessage        *msg,
                                                gboolean            blocking,
                                                GCancellable       *cancellable,
                                                GError            **error);

GInputStream *soup_message_io_get_response_istream (SoupMessage  *msg,
						    GError      **error);

void soup_message_io_backend_send_item (SoupMessageIOBackend      *backend,
                                        SoupMessageQueueItem      *item,
                                        SoupMessageIOCompletionFn  completion_cb,
                                        gpointer                   user_data);

G_END_DECLS