/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright 2021 Igalia S.L.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Christian Kellner <gicmo@gnome.org> 
 */

#include "config.h"

#include "soup-memory-input-stream.h"
#include <glib/gi18n-lib.h>

/**
 * SECTION:SoupMemoryInputStream
 * @short_description: Streaming input operations on memory chunks
 * @include: gio/gio.h
 * @see_also: #SoupMemoryOutputStream
 *
 * #SoupMemoryInputStream is a class for using arbitrary
 * memory chunks as input for GIO streaming input operations.
 *
 * It differs from #GMemoryInputStream in that it frees older chunks
 * after they have been read, returns #G_IO_ERROR_WOULDBLOCK at the end
 * of data until soup_memory_input_stream_complete() is called, and implements
 * g_pollable_input_stream_is_readable().
 */

struct _SoupMemoryInputStream {
        GInputStream parent_instance;
};

typedef struct {
        GSList *chunks;
        GPollableInputStream *parent_stream;
        gsize start_offset;
        gsize len;
        gsize pos;
        gboolean completed;
} SoupMemoryInputStreamPrivate;

static gssize soup_memory_input_stream_read (GInputStream *stream,
                                             void *buffer,
                                             gsize count,
                                             GCancellable *cancellable,
                                             GError **error);
static gssize soup_memory_input_stream_skip (GInputStream *stream,
                                             gsize count,
                                             GCancellable *cancellable,
                                             GError **error);
static gboolean soup_memory_input_stream_close (GInputStream *stream,
                                                GCancellable *cancellable,
                                                GError **error);
static void soup_memory_input_stream_skip_async (GInputStream *stream,
                                                 gsize count,
                                                 int io_priority,
                                                 GCancellable *cancellabl,
                                                 GAsyncReadyCallback callback,
                                                 gpointer datae);
static gssize soup_memory_input_stream_skip_finish (GInputStream *stream,
                                                    GAsyncResult *result,
                                                    GError **error);
static void soup_memory_input_stream_close_async (GInputStream *stream,
                                                  int io_priority,
                                                  GCancellable *cancellabl,
                                                  GAsyncReadyCallback callback,
                                                  gpointer data);
static gboolean soup_memory_input_stream_close_finish (GInputStream *stream,
                                                       GAsyncResult *result,
                                                       GError **error);

static void soup_memory_input_stream_seekable_iface_init (GSeekableIface *iface);
static goffset soup_memory_input_stream_tell (GSeekable *seekable);
static gboolean soup_memory_input_stream_can_seek (GSeekable *seekable);
static gboolean soup_memory_input_stream_seek (GSeekable *seekable,
                                               goffset offset,
                                               GSeekType type,
                                               GCancellable *cancellable,
                                               GError **error);
static gboolean soup_memory_input_stream_can_truncate (GSeekable *seekable);
static gboolean soup_memory_input_stream_truncate (GSeekable *seekable,
                                                   goffset offset,
                                                   GCancellable *cancellable,
                                                   GError **error);

static void soup_memory_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface);
static gboolean soup_memory_input_stream_is_readable (GPollableInputStream *stream);
static GSource *soup_memory_input_stream_create_source (GPollableInputStream *stream,
                                                        GCancellable *cancellable);
static gssize soup_memory_input_stream_read_nonblocking (GPollableInputStream *stream,
                                                         void *buffer,
                                                         gsize count,
                                                         GError **error);

static void soup_memory_input_stream_dispose (GObject *object);
static void soup_memory_input_stream_finalize (GObject *object);

G_DEFINE_TYPE_WITH_CODE (SoupMemoryInputStream, soup_memory_input_stream, G_TYPE_INPUT_STREAM,
                         G_ADD_PRIVATE (SoupMemoryInputStream)
                             G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
                                                    soup_memory_input_stream_seekable_iface_init);
                         G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM,
                                                soup_memory_input_stream_pollable_iface_init);)

enum {
        WANT_READ,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void
soup_memory_input_stream_class_init (SoupMemoryInputStreamClass *klass)
{
        GObjectClass *object_class;
        GInputStreamClass *istream_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = soup_memory_input_stream_finalize;
        object_class->dispose = soup_memory_input_stream_dispose;

        istream_class = G_INPUT_STREAM_CLASS (klass);
        istream_class->read_fn = soup_memory_input_stream_read;
        istream_class->skip = soup_memory_input_stream_skip;
        istream_class->close_fn = soup_memory_input_stream_close;

        istream_class->skip_async = soup_memory_input_stream_skip_async;
        istream_class->skip_finish = soup_memory_input_stream_skip_finish;
        istream_class->close_async = soup_memory_input_stream_close_async;
        istream_class->close_finish = soup_memory_input_stream_close_finish;

        signals[WANT_READ] =
                g_signal_new ("want-read",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              NULL,
                              G_TYPE_ERROR,
                              2, G_TYPE_CANCELLABLE, G_TYPE_BOOLEAN);
}

static void
soup_memory_input_stream_dispose (GObject *object)
{
        SoupMemoryInputStream *stream = SOUP_MEMORY_INPUT_STREAM (object);
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (stream);

        priv->completed = TRUE;

        G_OBJECT_CLASS (soup_memory_input_stream_parent_class)->dispose (object);
}

static void
soup_memory_input_stream_finalize (GObject *object)
{
        SoupMemoryInputStream *stream = SOUP_MEMORY_INPUT_STREAM (object);
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (stream);

        g_slist_free_full (priv->chunks, (GDestroyNotify)g_bytes_unref);
        g_clear_object (&priv->parent_stream);

        G_OBJECT_CLASS (soup_memory_input_stream_parent_class)->finalize (object);
}

static void
soup_memory_input_stream_seekable_iface_init (GSeekableIface *iface)
{
        iface->tell = soup_memory_input_stream_tell;
        iface->can_seek = soup_memory_input_stream_can_seek;
        iface->seek = soup_memory_input_stream_seek;
        iface->can_truncate = soup_memory_input_stream_can_truncate;
        iface->truncate_fn = soup_memory_input_stream_truncate;
}

static void
soup_memory_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
        iface->is_readable = soup_memory_input_stream_is_readable;
        iface->create_source = soup_memory_input_stream_create_source;
        iface->read_nonblocking = soup_memory_input_stream_read_nonblocking;
}

static void
soup_memory_input_stream_init (SoupMemoryInputStream *stream)
{
}

/**
 * soup_memory_input_stream_new:
 *
 * Creates a new empty #SoupMemoryInputStream. 
 *
 * Returns: a new #GInputStream
 */
GInputStream *
soup_memory_input_stream_new (GPollableInputStream *parent_stream)
{
        GInputStream *stream;
        SoupMemoryInputStreamPrivate *priv;

        stream = g_object_new (SOUP_TYPE_MEMORY_INPUT_STREAM, NULL);
        priv = soup_memory_input_stream_get_instance_private (SOUP_MEMORY_INPUT_STREAM (stream));
        if (parent_stream)
                priv->parent_stream = g_object_ref (parent_stream);

        return stream;
}

void
soup_memory_input_stream_add_bytes (SoupMemoryInputStream *stream,
                                    GBytes *bytes)
{
        SoupMemoryInputStreamPrivate *priv;

        g_return_if_fail (SOUP_IS_MEMORY_INPUT_STREAM (stream));
        g_return_if_fail (bytes != NULL);

        priv = soup_memory_input_stream_get_instance_private (stream);

        priv->chunks = g_slist_append (priv->chunks, g_bytes_ref (bytes));
        priv->len += g_bytes_get_size (bytes);
}

static gssize
soup_memory_input_stream_read_real (GInputStream *stream,
                                    gboolean blocking,
                                    void *buffer,
                                    gsize read_count,
                                    GCancellable *cancellable,
                                    GError **error)
{
        SoupMemoryInputStream *memory_stream;
        SoupMemoryInputStreamPrivate *priv;
        GSList *l;
        GBytes *chunk;
        gsize len;
        gsize offset, start, rest, size;
        gsize count;

        memory_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        priv = soup_memory_input_stream_get_instance_private (memory_stream);

        /* We have a list of chunked bytes that we continually read from.
         * Once a chunk is fully read it is removed from our list and we
         * keep the offset of where the chunks start.
         */

        count = MIN (read_count, priv->len - priv->pos);

        offset = priv->start_offset;
        for (l = priv->chunks; l; l = l->next) {
                chunk = (GBytes *)l->data;
                len = g_bytes_get_size (chunk);

                if (offset + len > priv->pos)
                        break;

                offset += len;
        }

        priv->start_offset = offset;
        start = priv->pos - offset;
        rest = count;

        while (l && rest > 0) {
                GSList *next = l->next;

                const guint8 *chunk_data;
                chunk = (GBytes *)l->data;

                chunk_data = g_bytes_get_data (chunk, &len);

                size = MIN (rest, len - start);

                memcpy ((guint8 *)buffer + (count - rest), chunk_data + start, size);
                rest -= size;

                // Remove fully read chunk from list, note that we are always near the start of the list
                if (start + size == len) {
                        priv->start_offset += len;
                        priv->chunks = g_slist_delete_link (priv->chunks, l);
                        g_bytes_unref (chunk);
                }

                start = 0;
                l = next;
        }

        priv->pos += count;

        // We need to block until the read is completed.
        // So emit a signal saying we need more data.
        if (count == 0 && blocking && !priv->completed) {
                GError *read_error = NULL;
                g_signal_emit (memory_stream, signals[WANT_READ], 0,
                               cancellable,
                               TRUE,
                               &read_error);

                if (read_error) {
                        g_propagate_error (error, read_error);
                        return -1;
                }

                return soup_memory_input_stream_read_real (
                        stream, blocking, buffer, read_count, cancellable, error
                );
        }

        return count;
}

static gssize
soup_memory_input_stream_read (GInputStream *stream,
                               void *buffer,
                               gsize count,
                               GCancellable *cancellable,
                               GError **error)
{
        return soup_memory_input_stream_read_real (stream, TRUE, buffer, count, cancellable, error);
}

static gssize
soup_memory_input_stream_read_nonblocking (GPollableInputStream *stream,
                                           void *buffer,
                                           gsize count,
                                           GError **error)
{
        SoupMemoryInputStream *memory_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (memory_stream);
        GError *inner_error = NULL;

        gsize read = soup_memory_input_stream_read_real (G_INPUT_STREAM (stream), FALSE, buffer, count, NULL, &inner_error);

        if (read == 0 && !priv->completed && !inner_error) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK, "Operation would block");

                // Try requesting more reads from the io backend
                GError *inner_error = NULL;
                g_signal_emit (memory_stream, signals[WANT_READ], 0,
                               NULL, FALSE, &inner_error);

                // TODO: Do we care?
                g_clear_error (&inner_error);

                return -1;
        }

        if (inner_error)
                g_propagate_error (error, inner_error);

        return read;
}

void
soup_memory_input_stream_complete (SoupMemoryInputStream *stream)
{
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (stream);
        priv->completed = TRUE;
}

static gssize
soup_memory_input_stream_skip (GInputStream *stream,
                               gsize count,
                               GCancellable *cancellable,
                               GError **error)
{
        SoupMemoryInputStream *memory_stream;
        SoupMemoryInputStreamPrivate *priv;

        memory_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        priv = soup_memory_input_stream_get_instance_private (memory_stream);

        count = MIN (count, priv->len - priv->pos);
        priv->pos += count;

        return count;
}

static gboolean
soup_memory_input_stream_close (GInputStream *stream,
                                GCancellable *cancellable,
                                GError **error)
{
        return TRUE;
}

static void
soup_memory_input_stream_skip_async (GInputStream *stream,
                                     gsize count,
                                     int io_priority,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
        GTask *task;
        gssize nskipped;
        GError *error = NULL;

        nskipped = G_INPUT_STREAM_GET_CLASS (stream)->skip (stream, count, cancellable, &error);
        task = g_task_new (stream, cancellable, callback, user_data);
        g_task_set_source_tag (task, soup_memory_input_stream_skip_async);

        if (error)
                g_task_return_error (task, error);
        else
                g_task_return_int (task, nskipped);
        g_object_unref (task);
}

static gssize
soup_memory_input_stream_skip_finish (GInputStream *stream,
                                      GAsyncResult *result,
                                      GError **error)
{
        g_return_val_if_fail (g_task_is_valid (result, stream), -1);

        return g_task_propagate_int (G_TASK (result), error);
}

static void
soup_memory_input_stream_close_async (GInputStream *stream,
                                      int io_priority,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
        GTask *task;

        task = g_task_new (stream, cancellable, callback, user_data);
        g_task_set_source_tag (task, soup_memory_input_stream_close_async);
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
}

static gboolean
soup_memory_input_stream_close_finish (GInputStream *stream,
                                       GAsyncResult *result,
                                       GError **error)
{
        return TRUE;
}

static goffset
soup_memory_input_stream_tell (GSeekable *seekable)
{
        SoupMemoryInputStream *memory_stream;
        SoupMemoryInputStreamPrivate *priv;

        memory_stream = SOUP_MEMORY_INPUT_STREAM (seekable);
        priv = soup_memory_input_stream_get_instance_private (memory_stream);

        return priv->pos;
}

static gboolean soup_memory_input_stream_can_seek (GSeekable *seekable)
{
        return FALSE;
}

static gboolean
soup_memory_input_stream_seek (GSeekable *seekable,
                               goffset offset,
                               GSeekType type,
                               GCancellable *cancellable,
                               GError **error)
{
        g_set_error_literal (error,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             _ ("Cannot seek SoupMemoryInputStream"));
        return FALSE;
}

static gboolean
soup_memory_input_stream_can_truncate (GSeekable *seekable)
{
        return FALSE;
}

static gboolean
soup_memory_input_stream_truncate (GSeekable *seekable,
                                   goffset offset,
                                   GCancellable *cancellable,
                                   GError **error)
{
        g_set_error_literal (error,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             _ ("Cannot truncate SoupMemoryInputStream"));
        return FALSE;
}

static gboolean
soup_memory_input_stream_is_readable (GPollableInputStream *stream)
{
        SoupMemoryInputStream *memory_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (memory_stream);

        return priv->pos < priv->len || priv->completed;
}

/* Custom GSource */

typedef struct {
	GSource source;
	SoupMemoryInputStream *stream;
} SoupMemoryStreamSource;

static gboolean
memory_stream_source_prepare (GSource *source,
                              gint    *timeout)
{
        SoupMemoryStreamSource *stream_source = (SoupMemoryStreamSource *)source;
        return soup_memory_input_stream_is_readable (G_POLLABLE_INPUT_STREAM (stream_source->stream));
}

static gboolean
memory_stream_source_dispatch (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
	GPollableSourceFunc func = (GPollableSourceFunc )callback;
	SoupMemoryStreamSource *memory_source = (SoupMemoryStreamSource *)source;

        if (!func)
                return FALSE;

	return (*func) (G_OBJECT (memory_source->stream), user_data);
}

static gboolean
memory_stream_source_closure_callback (GObject *pollable_stream,
				       gpointer data)
{
	GClosure *closure = data;
	GValue param = G_VALUE_INIT;
	GValue result_value = G_VALUE_INIT;
	gboolean result;

	g_value_init (&result_value, G_TYPE_BOOLEAN);

        g_assert (G_IS_POLLABLE_INPUT_STREAM (pollable_stream));
	g_value_init (&param, G_TYPE_POLLABLE_INPUT_STREAM);
	g_value_set_object (&param, pollable_stream);

	g_closure_invoke (closure, &result_value, 1, &param, NULL);

	result = g_value_get_boolean (&result_value);
	g_value_unset (&result_value);
	g_value_unset (&param);

	return result;
}

static void
memory_stream_source_finalize (GSource *source)
{
	SoupMemoryStreamSource *memory_source = (SoupMemoryStreamSource *)source;

	g_object_unref (memory_source->stream);
}

static GSourceFuncs source_funcs =
{
	memory_stream_source_prepare,
	NULL,
	memory_stream_source_dispatch,
	memory_stream_source_finalize,
	(GSourceFunc)memory_stream_source_closure_callback,
	NULL,
};

static GSource *
soup_memory_input_stream_create_source (GPollableInputStream *stream,
                                        GCancellable *cancellable)
{
        SoupMemoryInputStreamPrivate *priv = soup_memory_input_stream_get_instance_private (SOUP_MEMORY_INPUT_STREAM (stream));

        GSource *source = g_source_new (&source_funcs, sizeof (SoupMemoryStreamSource));
	g_source_set_name (source, "SoupMemoryStreamSource");

	SoupMemoryStreamSource *stream_source = (SoupMemoryStreamSource *)source;
        stream_source->stream = g_object_ref (SOUP_MEMORY_INPUT_STREAM (stream));

        GSource *child_source;
        if (priv->parent_stream)
                child_source = g_pollable_input_stream_create_source (priv->parent_stream, cancellable);
        else
                child_source = g_cancellable_source_new (cancellable);

        g_source_set_dummy_callback (child_source);
        g_source_add_child_source (source, child_source);
        g_source_unref (child_source);

        return source;
}
