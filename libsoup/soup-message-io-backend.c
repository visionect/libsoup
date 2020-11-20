/* soup-message-io-backend.c
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

#include "config.h"

#include "soup-message-io-backend.h"
#include "soup-message-private.h"

G_DEFINE_INTERFACE (SoupMessageIOBackend, soup_message_io_backend, G_TYPE_OBJECT)

void
soup_message_io_run (SoupMessage *msg,
                     gboolean     blocking)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                iface->run (msg, blocking);
        }
}

void
soup_message_io_finished (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                iface->finished (msg);
        }
}

void
soup_message_io_pause (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                iface->pause (msg);
        }
}

void
soup_message_io_unpause (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                iface->unpause (msg);
        }
}

void
soup_message_io_stolen (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                iface->stolen (msg);
        }
}

gboolean
soup_message_io_in_progress (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                return iface->in_progress (msg);
        }
        return FALSE;
}

gboolean
soup_message_io_is_paused (SoupMessage *msg)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        if (backend) {
                SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
                return iface->is_paused (msg);
        }
        return FALSE;
}

GInputStream *
soup_message_io_get_response_istream (SoupMessage *msg,
                                      GError **error)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
        return iface->get_response_istream (msg, error);
}

gboolean
soup_message_io_run_until_read (SoupMessage *msg,
                                GCancellable *cancellable,
                                GError **error)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
        return iface->run_until_read (msg, cancellable, error);
}

gboolean
soup_message_io_run_until_finish (SoupMessage *msg,
                                  gboolean blocking,
                                  GCancellable *cancellable,
                                  GError **error)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
        return iface->run_until_finish (msg, blocking, cancellable, error);
}

void
soup_message_io_run_until_read_async (SoupMessage        *msg,
                                      int                 io_priority,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data)
{
        SoupMessageIOBackend *backend = soup_message_get_io_data (msg);
        SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
        iface->run_until_read_async (msg, io_priority, cancellable, callback, user_data);
}

gboolean
soup_message_io_run_until_read_finish (SoupMessage        *msg,
                                       GAsyncResult       *result,
                                       GError            **error)
{
        /* For now just assume all implemenations use GTask the same way as
           it simplifies the lifecycle of the backend being destroyed before the
           task returns */
        return g_task_propagate_boolean (G_TASK (result), error);
}

void
soup_message_io_backend_send_item (SoupMessageIOBackend      *backend,
                                   SoupMessageQueueItem      *item,
                                   SoupMessageIOCompletionFn  completion_cb,
                                   gpointer                   user_data)
{
        SoupMessageIOBackendInterface *iface = SOUP_MESSAGE_IO_BACKEND_GET_IFACE (backend);
        iface->send_item (backend, item, completion_cb, user_data);
}


static void
soup_message_io_backend_default_init (SoupMessageIOBackendInterface *iface)
{
}