/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-message-server-io.c: server-side request/response
 *
 * Copyright (C) 2000-2003, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>

#include "soup.h"
#include "soup-message-private.h"
#include "soup-misc.h"
#include "soup-socket-private.h"

static GUri *
parse_connect_authority (const char *req_path)
{
	GUri *uri;
	char *fake_uri;

	fake_uri = g_strdup_printf ("http://%s", req_path);
	uri = soup_uri_parse_normalized (NULL, fake_uri, NULL);
	g_free (fake_uri);

        if (!uri)
                return NULL;

        if (g_uri_get_user (uri) ||
            g_uri_get_password (uri) ||
            g_uri_get_query (uri) ||
            g_uri_get_fragment (uri) ||
            !g_uri_get_host (uri) ||
            g_uri_get_port (uri) <= 0 ||
            strcmp (g_uri_get_path (uri), "/") != 0) {
                g_uri_unref (uri);
                return NULL;
        }

	return uri;
}

static guint
parse_request_headers (SoupMessage *msg, char *headers, guint headers_len,
		       SoupEncoding *encoding, gpointer sock, GError **error)
{
	char *req_method, *req_path;
	SoupHTTPVersion version;
	const char *req_host;
	guint status;
        char *url;
	GUri *uri;

	status = soup_headers_parse_request (headers, headers_len,
					     msg->request_headers,
					     &req_method,
					     &req_path,
					     &version);
	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		if (status == SOUP_STATUS_MALFORMED) {
			g_set_error_literal (error, SOUP_REQUEST_ERROR,
					     SOUP_REQUEST_ERROR_PARSING,
					     _("Could not parse HTTP request"));
		}
		return status;
	}

	g_object_set (G_OBJECT (msg),
		      SOUP_MESSAGE_METHOD, req_method,
		      SOUP_MESSAGE_HTTP_VERSION, version,
		      NULL);
	g_free (req_method);

	/* Handle request body encoding */
	*encoding = soup_message_headers_get_encoding (msg->request_headers);
	if (*encoding == SOUP_ENCODING_UNRECOGNIZED) {
		if (soup_message_headers_get_list (msg->request_headers, "Transfer-Encoding"))
			return SOUP_STATUS_NOT_IMPLEMENTED;
		else
			return SOUP_STATUS_BAD_REQUEST;
	}

	/* Generate correct context for request */
	req_host = soup_message_headers_get_one (msg->request_headers, "Host");
	if (req_host && strchr (req_host, '/')) {
		g_free (req_path);
		return SOUP_STATUS_BAD_REQUEST;
	}

	if (!strcmp (req_path, "*") && req_host) {
		/* Eg, "OPTIONS * HTTP/1.1" */
		url = g_strdup_printf ("%s://%s/*",
				       soup_socket_is_ssl (sock) ? "https" : "http",
				       req_host);
		uri = g_uri_parse (url, SOUP_HTTP_URI_FLAGS, NULL);
		g_free (url);
	} else if (msg->method == SOUP_METHOD_CONNECT) {
		/* Authority */
		uri = parse_connect_authority (req_path);
	} else if (*req_path != '/') {
		/* Absolute URI */
		uri = g_uri_parse (req_path, SOUP_HTTP_URI_FLAGS, NULL);
	} else if (req_host) {
		url = g_strdup_printf ("%s://%s%s",
				       soup_socket_is_ssl (sock) ? "https" : "http",
				       req_host, req_path);
		uri = g_uri_parse (url, SOUP_HTTP_URI_FLAGS, NULL);
		g_free (url);
	} else if (soup_message_get_http_version (msg) == SOUP_HTTP_1_0) {
		/* No Host header, no AbsoluteUri */
		GInetSocketAddress *addr = soup_socket_get_local_address (sock);
                GInetAddress *inet_addr = g_inet_socket_address_get_address (addr);
                char *local_ip = g_inet_address_to_string (inet_addr);
                int port = g_inet_socket_address_get_port (addr);
                if (port == 0)
                        port = -1;

                uri = g_uri_build (SOUP_HTTP_URI_FLAGS, 
                                   soup_socket_is_ssl (sock) ? "https" : "http",
                                   NULL, local_ip, port, req_path, NULL, NULL);
		g_free (local_ip);
	} else
		uri = NULL;

	g_free (req_path);

	if (!uri || !g_uri_get_host (uri)) {
		if (uri)
			g_uri_unref (uri);
		return SOUP_STATUS_BAD_REQUEST;
	}

        GUri *normalized_uri = soup_normalize_uri (uri);
	soup_message_set_uri (msg, normalized_uri);
	g_uri_unref (uri);
        g_uri_unref (normalized_uri);

	return SOUP_STATUS_OK;
}

static void
handle_partial_get (SoupMessage *msg)
{
	SoupRange *ranges;
	int nranges;
	GBytes *full_response;
	guint status;

	/* Make sure the message is set up right for us to return a
	 * partial response; it has to be a GET, the status must be
	 * 200 OK (and in particular, NOT already 206 Partial
	 * Content), and the SoupServer must have already filled in
	 * the response body
	 */
	if (msg->method != SOUP_METHOD_GET ||
	    msg->status_code != SOUP_STATUS_OK ||
	    soup_message_headers_get_encoding (msg->response_headers) !=
	    SOUP_ENCODING_CONTENT_LENGTH ||
	    msg->response_body->length == 0 ||
	    !soup_message_body_get_accumulate (msg->response_body))
		return;

	/* Oh, and there has to have been a valid Range header on the
	 * request, of course.
	 */
	status = soup_message_headers_get_ranges_internal (msg->request_headers,
							   msg->response_body->length,
							   TRUE,
							   &ranges, &nranges);
	if (status == SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE) {
		soup_message_set_status (msg, status);
		soup_message_body_truncate (msg->response_body);
		return;
	} else if (status != SOUP_STATUS_PARTIAL_CONTENT)
		return;

	full_response = soup_message_body_flatten (msg->response_body);
	if (!full_response) {
		soup_message_headers_free_ranges (msg->request_headers, ranges);
		return;
	}

	soup_message_set_status (msg, SOUP_STATUS_PARTIAL_CONTENT);
	soup_message_body_truncate (msg->response_body);

	if (nranges == 1) {
		GBytes *range_buf;

		/* Single range, so just set Content-Range and fix the body. */

		soup_message_headers_set_content_range (msg->response_headers,
							ranges[0].start,
							ranges[0].end,
							g_bytes_get_size (full_response));
		range_buf = g_bytes_new_from_bytes (full_response,
						    ranges[0].start,
						    ranges[0].end - ranges[0].start + 1);
		soup_message_body_append_bytes (msg->response_body, range_buf);
		g_bytes_unref (range_buf);
	} else {
		SoupMultipart *multipart;
		SoupMessageHeaders *part_headers;
		GBytes *part_body;
		GBytes *body = NULL;
		const char *content_type;
		int i;

		/* Multiple ranges, so build a multipart/byteranges response
		 * to replace msg->response_body with.
		 */

		multipart = soup_multipart_new ("multipart/byteranges");
		content_type = soup_message_headers_get_one (msg->response_headers,
							     "Content-Type");
		for (i = 0; i < nranges; i++) {
			part_headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_MULTIPART);
			if (content_type) {
				soup_message_headers_append (part_headers,
							     "Content-Type",
							     content_type);
			}
			soup_message_headers_set_content_range (part_headers,
								ranges[i].start,
								ranges[i].end,
								g_bytes_get_size (full_response));
			part_body = g_bytes_new_from_bytes (full_response,
							    ranges[i].start,
							    ranges[i].end - ranges[i].start + 1);
			soup_multipart_append_part (multipart, part_headers,
						    part_body);
			soup_message_headers_free (part_headers);
			g_bytes_unref (part_body);
		}

		soup_multipart_to_message (multipart, msg->response_headers, &body);
		soup_message_body_append_bytes (msg->response_body, body);
		g_bytes_unref (body);
		soup_multipart_free (multipart);
	}

	g_bytes_unref (full_response);
	soup_message_headers_free_ranges (msg->request_headers, ranges);
}

static void
get_response_headers (SoupMessage *msg, GString *headers,
		      SoupEncoding *encoding, gpointer user_data)
{
	SoupEncoding claimed_encoding;
	SoupMessageHeadersIter iter;
	const char *name, *value;

	if (msg->status_code == 0)
		soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

	handle_partial_get (msg);

	g_string_append_printf (headers, "HTTP/1.%c %d %s\r\n",
				soup_message_get_http_version (msg) == SOUP_HTTP_1_0 ? '0' : '1',
				msg->status_code, msg->reason_phrase);

	claimed_encoding = soup_message_headers_get_encoding (msg->response_headers);
	if ((msg->method == SOUP_METHOD_HEAD ||
	     msg->status_code  == SOUP_STATUS_NO_CONTENT ||
	     msg->status_code  == SOUP_STATUS_NOT_MODIFIED ||
	     SOUP_STATUS_IS_INFORMATIONAL (msg->status_code)) ||
	    (msg->method == SOUP_METHOD_CONNECT &&
	     SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)))
		*encoding = SOUP_ENCODING_NONE;
	else
		*encoding = claimed_encoding;

	if (claimed_encoding == SOUP_ENCODING_CONTENT_LENGTH &&
	    !soup_message_headers_get_content_length (msg->response_headers)) {
		soup_message_headers_set_content_length (msg->response_headers,
							 msg->response_body->length);
	}

	soup_message_headers_iter_init (&iter, msg->response_headers);
	while (soup_message_headers_iter_next (&iter, &name, &value))
		g_string_append_printf (headers, "%s: %s\r\n", name, value);
	g_string_append (headers, "\r\n");
}

void
soup_message_read_request (SoupMessage               *msg,
			   SoupSocket                *sock,
			   SoupMessageCompletionFn    completion_cb,
			   gpointer                   user_data)
{
	GMainContext *async_context;
	GIOStream *iostream;

	async_context = g_main_context_ref_thread_default ();
	iostream = soup_socket_get_iostream (sock);

	soup_message_io_server (msg, iostream, async_context,
				get_response_headers,
				parse_request_headers,
				sock,
				completion_cb, user_data);
	g_main_context_unref (async_context);
}
