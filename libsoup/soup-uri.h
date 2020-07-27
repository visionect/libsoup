/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Copyright 1999-2002 Ximian, Inc.
 */


#pragma once

#include "soup-types.h"

G_BEGIN_DECLS

SOUP_AVAILABLE_IN_2_4
GUri *soup_uri_parse_normalized (GUri *base, const char *uri_string, GError **error);

SOUP_AVAILABLE_IN_2_4
char       *soup_uri_get_path_and_query    (GUri       *uri);

SOUP_AVAILABLE_IN_2_4
GUri       *soup_uri_copy_host            (GUri       *uri);

SOUP_AVAILABLE_IN_2_4
guint       soup_uri_host_hash (gconstpointer key);

SOUP_AVAILABLE_IN_2_4
gboolean    soup_uri_host_equal (gconstpointer v1, gconstpointer v2);

SOUP_AVAILABLE_IN_2_4
gboolean soup_uri_equal (GUri *uri1, GUri *uri2);

SOUP_AVAILABLE_IN_2_4
gboolean soup_uri_is_http (GUri *uri, char **aliases);

SOUP_AVAILABLE_IN_2_4
gboolean soup_uri_is_https (GUri *uri, char **aliases);

SOUP_AVAILABLE_IN_2_4
gboolean soup_uri_uses_default_port (GUri *uri);

SOUP_AVAILABLE_IN_2_4
char  	   *soup_uri_normalize             (const char *part,
					    const char *unescape_extra);
SOUP_AVAILABLE_IN_2_4
GUri       *soup_uri_copy_with_query_from_form (GUri       *uri,
					    GHashTable *form);

SOUP_AVAILABLE_IN_2_4
GUri       *soup_uri_copy_with_query_from_fields (GUri       *uri,
					    const char *first_field,
					    ...) G_GNUC_NULL_TERMINATED;
SOUP_AVAILABLE_IN_2_28
gboolean soup_uri_valid_for_http (GUri *uri, GError **error);

SOUP_AVAILABLE_IN_2_28
GUri     *soup_uri_copy_with_credentials (GUri *uri, const char *username, const char *password);

SOUP_AVAILABLE_IN_2_28
gboolean  soup_uri_paths_equal (const char *path1, const char *path2, gssize len);

#define SOUP_HTTP_URI_FLAGS (G_URI_FLAGS_HAS_PASSWORD | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_ENCODED_FRAGMENT)

GUri *soup_normalize_uri (GUri *uri);

int   soup_uri_get_port_with_default (GUri *uri);

G_END_DECLS
