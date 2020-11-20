
/* soup-message-io-source.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soup-message-io-source.h"

static gboolean
message_io_source_prepare (GSource *source,
                           gint    *timeout)
{
        SoupMessageIOSource *message_io_source = (SoupMessageIOSource *)source;
	*timeout = -1;
	return message_io_source->check_func (source);
}

static gboolean
message_io_source_dispatch (GSource     *source,
                            GSourceFunc  callback,
                            gpointer     user_data)
{
	SoupMessageIOSourceFunc func = (SoupMessageIOSourceFunc)callback;
	SoupMessageIOSource *message_io_source = (SoupMessageIOSource *)source;

	return (*func) (message_io_source->msg, user_data);
}

static void
message_io_source_finalize (GSource *source)
{
	SoupMessageIOSource *message_io_source = (SoupMessageIOSource *)source;

	g_object_unref (message_io_source->msg);
}

static gboolean
message_io_source_closure_callback (SoupMessage *msg,
				    gpointer     data)
{
	GClosure *closure = data;
	GValue param = G_VALUE_INIT;
	GValue result_value = G_VALUE_INIT;
	gboolean result;

	g_value_init (&result_value, G_TYPE_BOOLEAN);

	g_value_init (&param, SOUP_TYPE_MESSAGE);
	g_value_set_object (&param, msg);

	g_closure_invoke (closure, &result_value, 1, &param, NULL);

	result = g_value_get_boolean (&result_value);
	g_value_unset (&result_value);
	g_value_unset (&param);

	return result;
}

static GSourceFuncs message_io_source_funcs =
{
	message_io_source_prepare,
	NULL,
	message_io_source_dispatch,
	message_io_source_finalize,
	(GSourceFunc)message_io_source_closure_callback,
	(GSourceDummyMarshal)g_cclosure_marshal_generic,
};

GSource *
soup_message_io_source_new (GSource *base_source,
                            GObject *msg,
                            gboolean paused,
                            gboolean (*check_func) (GSource*))
{
        GSource *source = g_source_new (&message_io_source_funcs, sizeof (SoupMessageIOSource));
	g_source_set_name (source, "SoupMessageIOSource");
	SoupMessageIOSource *message_io_source = (SoupMessageIOSource *)source;
	message_io_source->msg = g_object_ref (msg);
	message_io_source->paused = paused;
        message_io_source->check_func = check_func;

	if (base_source) {
		g_source_set_dummy_callback (base_source);
		g_source_add_child_source (source, base_source);
		g_source_unref (base_source);
	}

        return source;
}
