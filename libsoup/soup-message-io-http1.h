/* soup-message-io-http1.h
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

#include "soup-message-io-backend.h"
#include "soup-message-io-data.h"

G_BEGIN_DECLS

#define SOUP_TYPE_MESSAGE_IO_HTTP1 (soup_message_io_http1_get_type())
G_DECLARE_FINAL_TYPE (SoupMessageIOHTTP1, soup_message_io_http1, SOUP, MESSAGE_IO_HTTP1, GObject)

SoupMessageIOHTTP1 *soup_message_io_http1_new (void);

G_END_DECLS