/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* soup-uri.c : utility functions to parse URLs */

/*
 * Copyright 1999-2003 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "soup-uri.h"
#include "soup.h"
#include "soup-misc.h"

static inline int
soup_scheme_default_port (const char *scheme)
{
        if (!g_ascii_strcasecmp (scheme, "http") ||
            !g_ascii_strcasecmp (scheme, "ws"))
		return 80;
	else if (!g_ascii_strcasecmp (scheme, "https") ||
                 !g_ascii_strcasecmp (scheme, "wss"))
		return 443;
	else if (!g_ascii_strcasecmp (scheme, "ftp"))
		return 21;
	else
		return -1;
}

static inline gboolean
parts_equal (const char *one, const char *two, gboolean insensitive)
{
	if (!one && !two)
		return TRUE;
	if (!one || !two)
		return FALSE;
	return insensitive ? !g_ascii_strcasecmp (one, two) : !strcmp (one, two);
}

static inline gboolean
path_equal (const char *one, const char *two)
{
        if (one[0] == '\0')
                one = "/";
        if (two[0] == '\0')
                two = "/";

	return !strcmp (one, two);
}

int
soup_uri_get_port_with_default (GUri *uri)
{
        int port = g_uri_get_port (uri);
        if (port != -1)
                return port;

        return soup_scheme_default_port (g_uri_get_scheme (uri));
}

/**
 * soup_uri_equal:
 * @uri1: a #GUri
 * @uri2: another #GUri
 *
 * Tests whether or not @uri1 and @uri2 are equal in all parts
 *
 * Return value: %TRUE or %FALSE
 **/
gboolean
soup_uri_equal (GUri *uri1, GUri *uri2)
{
     	g_return_val_if_fail (uri1 != NULL, FALSE);
	g_return_val_if_fail (uri2 != NULL, FALSE);

       	if (!parts_equal (g_uri_get_scheme (uri1), g_uri_get_scheme (uri2), TRUE)          ||
	    soup_uri_get_port_with_default (uri1) != soup_uri_get_port_with_default (uri2) ||
	    !parts_equal (g_uri_get_user (uri1), g_uri_get_user (uri2), FALSE)             ||
	    !parts_equal (g_uri_get_password (uri1), g_uri_get_password (uri2), FALSE)     ||
	    !parts_equal (g_uri_get_host (uri1), g_uri_get_host (uri2), TRUE)              ||
	    !path_equal (g_uri_get_path (uri1), g_uri_get_path (uri2))                     ||
	    !parts_equal (g_uri_get_query (uri1), g_uri_get_query (uri2), FALSE)           ||
	    !parts_equal (g_uri_get_fragment (uri1), g_uri_get_fragment (uri2), FALSE)) {
                return FALSE;
            }

        return TRUE;
}

/* This does the "Remove Dot Segments" algorithm from section 5.2.4 of
 * RFC 3986, except that @path is modified in place.
 *
 * See https://tools.ietf.org/html/rfc3986#section-5.2.4
 */
static void
remove_dot_segments (gchar *path)
{
  gchar *p, *q;

  if (!*path)
    return;

  /* Remove "./" where "." is a complete segment. */
  for (p = path + 1; *p; )
    {
      if (*(p - 1) == '/' &&
          *p == '.' && *(p + 1) == '/')
        memmove (p, p + 2, strlen (p + 2) + 1);
      else
        p++;
    }
  /* Remove "." at end. */
  if (p > path + 2 &&
      *(p - 1) == '.' && *(p - 2) == '/')
    *(p - 1) = '\0';

  /* Remove "<segment>/../" where <segment> != ".." */
  for (p = path + 1; *p; )
    {
      if (!strncmp (p, "../", 3))
        {
          p += 3;
          continue;
        }
      q = strchr (p + 1, '/');
      if (!q)
        break;
      if (strncmp (q, "/../", 4) != 0)
        {
          p = q + 1;
          continue;
        }
      memmove (p, q + 4, strlen (q + 4) + 1);
      p = path + 1;
    }
  /* Remove "<segment>/.." at end where <segment> != ".." */
  q = strrchr (path, '/');
  if (q && q != path && !strcmp (q, "/.."))
    {
      p = q - 1;
      while (p > path && *p != '/')
        p--;
      if (strncmp (p, "/../", 4) != 0)
        *(p + 1) = 0;
    }

  /* Remove extraneous initial "/.."s */
  while (!strncmp (path, "/../", 4))
    memmove (path, path + 3, strlen (path) - 2);
  if (!strcmp (path, "/.."))
    path[1] = '\0';
}

char *
soup_uri_get_path_and_query (GUri *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_uri_join_with_user (SOUP_HTTP_URI_FLAGS,
				     NULL, NULL, NULL, NULL, NULL, -1,
				     g_uri_get_path (uri),
				     g_uri_get_query (uri),
				     NULL);
}

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

/* length must be set (e.g. from strchr()) such that [part, part + length]
 * contains no nul bytes */
static char *
uri_normalized_copy (const char *part, int length,
		     const char *unescape_extra)
{
	unsigned char *s, *d, c;
	char *normalized = g_strndup (part, length);
	gboolean need_fixup = FALSE;

	if (!unescape_extra)
		unescape_extra = "";

	s = d = (unsigned char *)normalized;
	while (*s) {
		if (*s == '%') {
			if (s[1] == '\0' ||
			    s[2] == '\0' ||
			    !g_ascii_isxdigit (s[1]) ||
			    !g_ascii_isxdigit (s[2])) {
				*d++ = *s++;
				continue;
			}

			c = HEXCHAR (s);
			if (soup_char_is_uri_unreserved (c) ||
			    (c && strchr (unescape_extra, c))) {
				*d++ = c;
				s += 3;
			} else {
				/* We leave it unchanged. We used to uppercase percent-encoded
				 * triplets but we do not do it any more as RFC3986 Section 6.2.2.1
				 * says that they only SHOULD be case normalized.
				 */
				*d++ = *s++;
				*d++ = *s++;
				*d++ = *s++;
			}
		} else {
			if (!g_ascii_isgraph (*s) &&
			    !strchr (unescape_extra, *s))
				need_fixup = TRUE;
			*d++ = *s++;
		}
	}
	*d = '\0';

	if (need_fixup) {
		GString *fixed;

		fixed = g_string_new (NULL);
		s = (guchar *)normalized;
		while (*s) {
			if (g_ascii_isgraph (*s) ||
			    strchr (unescape_extra, *s))
				g_string_append_c (fixed, *s);
			else
				g_string_append_printf (fixed, "%%%02X", (int)*s);
			s++;
		}
		g_free (normalized);
		normalized = g_string_free (fixed, FALSE);
	}

	return normalized;
}

/**
 * soup_uri_normalize:
 * @part: a URI part
 * @unescape_extra: (allow-none): reserved characters to unescape (or %NULL)
 *
 * %<!-- -->-decodes any "unreserved" characters (or characters in
 * @unescape_extra) in @part, and %<!-- -->-encodes any non-ASCII
 * characters, spaces, and non-printing characters in @part.
 *
 * "Unreserved" characters are those that are not allowed to be used
 * for punctuation according to the URI spec. For example, letters are
 * unreserved, so soup_uri_normalize() will turn
 * <literal>http://example.com/foo/b%<!-- -->61r</literal> into
 * <literal>http://example.com/foo/bar</literal>, which is guaranteed
 * to mean the same thing. However, "/" is "reserved", so
 * <literal>http://example.com/foo%<!-- -->2Fbar</literal> would not
 * be changed, because it might mean something different to the
 * server.
 *
 * In the past, this would return %NULL if @part contained invalid
 * percent-encoding, but now it just ignores the problem (as
 * soup_uri_new() already did).
 *
 * Return value: the normalized URI part
 */
char *
soup_uri_normalize (const char *part, const char *unescape_extra)
{
	g_return_val_if_fail (part != NULL, NULL);

	return uri_normalized_copy (part, strlen (part), unescape_extra);
}


/**
 * soup_uri_uses_default_port:
 * @uri: a #GUri
 *
 * Tests if @uri uses the default port for its scheme. (Eg, 80 for
 * http.) (This only works for http, https and ftp; libsoup does not know
 * the default ports of other protocols.)
 *
 * Return value: %TRUE or %FALSE
 **/
gboolean
soup_uri_uses_default_port (GUri *uri)
{
        g_return_val_if_fail (uri != NULL, FALSE);

        if (g_uri_get_port (uri) == -1)
                return TRUE;

        if (g_uri_get_scheme (uri))
                return g_uri_get_port (uri) == soup_scheme_default_port (g_uri_get_scheme (uri));

        return FALSE;
}

static GUri *
soup_uri_copy_with_query (GUri *uri, const char *query)
{
        return g_uri_build_with_user (
                g_uri_get_flags (uri) | G_URI_FLAGS_ENCODED_QUERY,
                g_uri_get_scheme (uri),
                g_uri_get_user (uri),
                g_uri_get_password (uri),
                g_uri_get_auth_params (uri),
                g_uri_get_host (uri),
                g_uri_get_port (uri),
                g_uri_get_path (uri),
                query,
                g_uri_get_fragment (uri)
        );
}

/**
 * soup_uri_copy_with_query_from_form:
 * @uri: a #GUri
 * @form: (element-type utf8 utf8): a #GHashTable containing HTML form
 * information
 *
 * Sets @uri's query to the result of encoding @form according to the
 * HTML form rules. See soup_form_encode_hash() for more information.
 **/
GUri *
soup_uri_copy_with_query_from_form (GUri *uri, GHashTable *form)
{
	g_return_val_if_fail (uri != NULL, NULL);

        char *query = soup_form_encode_hash (form);
	GUri *new_uri = soup_uri_copy_with_query (uri, query);
        g_free (query);
	return new_uri;
}

/**
 * soup_uri_set_query_from_fields:
 * @uri: a #GUri
 * @first_field: name of the first form field to encode into query
 * @...: value of @first_field, followed by additional field names
 * and values, terminated by %NULL.
 *
 * Sets @uri's query to the result of encoding the given form fields
 * and values according to the * HTML form rules. See
 * soup_form_encode() for more information.
 **/
GUri *
soup_uri_copy_with_query_from_fields (GUri       *uri,
                                      const char *first_field,
                                      ...)
{
	va_list args;

	g_return_val_if_fail (uri != NULL, NULL);

	va_start (args, first_field);
	char *query = soup_form_encode_valist (first_field, args);
	va_end (args);

	GUri *new_uri = soup_uri_copy_with_query (uri, query);
        g_free (query);
	return new_uri;
}

/**
 * soup_uri_copy_host:
 * @uri: a #GUri
 *
 * Makes a copy of @uri, considering only the protocol, host, and port
 *
 * Return value: the new #GUri
 *
 * Since: 2.28
 **/
GUri *
soup_uri_copy_host (GUri *uri)
{
        g_return_val_if_fail (uri != NULL, NULL);

        return g_uri_build (g_uri_get_flags (uri),
                            g_uri_get_scheme (uri), NULL,
                            g_uri_get_host (uri),
                            g_uri_get_port (uri),
                            "/", NULL, NULL);
}

/**
 * soup_uri_host_hash:
 * @key: (type GUri): a #GUri with a non-%NULL @host member
 *
 * Hashes @key, considering only the scheme, host, and port.
 *
 * Return value: a hash
 *
 * Since: 2.28
 **/
guint
soup_uri_host_hash (gconstpointer key)
{
	GUri *uri = (GUri*)key;
        const char *host;

	g_return_val_if_fail (uri != NULL, 0);

        host = g_uri_get_host (uri);

	g_return_val_if_fail (host != NULL, 0);

	return soup_str_case_hash (g_uri_get_scheme (uri)) +
               g_uri_get_port (uri) +
	       soup_str_case_hash (host);
}

/**
 * soup_uri_host_equal:
 * @v1: (type GUri): a #GUri with a non-%NULL @host member
 * @v2: (type GUri): a #GUri with a non-%NULL @host member
 *
 * Compares @v1 and @v2, considering only the scheme, host, and port.
 *
 * Return value: whether or not the URIs are equal in scheme, host,
 * and port.
 *
 * Since: 2.28
 **/
gboolean
soup_uri_host_equal (gconstpointer v1, gconstpointer v2)
{
	GUri *one = (GUri*)v1;
	GUri *two = (GUri*)v2;
        const char *one_host, *two_host;
        int one_port, two_port;

	g_return_val_if_fail (one != NULL && two != NULL, one == two);

        one_host = g_uri_get_host (one);
        two_host = g_uri_get_host (two);

	g_return_val_if_fail (one_host != NULL && two_host != NULL, one_host == two_host);

        if (one == two)
                return TRUE;
	if (g_ascii_strcasecmp (g_uri_get_scheme (one), g_uri_get_scheme (two)) != 0)
		return FALSE;

        one_port = g_uri_get_port (one);
        two_port = g_uri_get_port (two);

        if (one_port == -1 && g_uri_get_scheme (one))
                one_port = soup_scheme_default_port (g_uri_get_scheme (one));
        if (two_port == -1 && g_uri_get_scheme (two))
                two_port = soup_scheme_default_port (g_uri_get_scheme (two));

	if (one_port != two_port)
		return FALSE;

        // QUESTION: Used to just be a string comparison?
	return soup_host_matches_host (one_host, two_host);
}

gboolean
soup_uri_is_https (GUri *uri, char **aliases)
{
	g_return_val_if_fail (uri != NULL, FALSE);

        const char *scheme = g_uri_get_scheme (uri);

        if (!g_ascii_strcasecmp (scheme, "https") ||
            !g_ascii_strcasecmp (scheme, "wss"))
            return TRUE;
	else if (!aliases)
		return FALSE;

	for (int i = 0; aliases[i]; i++) {
		if (!g_ascii_strcasecmp (scheme, aliases[i]))
			return TRUE;
	}

	return FALSE;
}

gboolean
soup_uri_is_http (GUri *uri, char **aliases)
{
	g_return_val_if_fail (uri != NULL, FALSE);

        const char *scheme = g_uri_get_scheme (uri);

        if (!g_ascii_strcasecmp (scheme, "http") ||
            !g_ascii_strcasecmp (scheme, "ws"))
            return TRUE;
	else if (!aliases)
		return FALSE;

	for (int i = 0; aliases[i]; i++) {
		if (!g_ascii_strcasecmp (scheme, aliases[i]))
			return TRUE;
	}

	return FALSE;
}

gboolean
soup_uri_valid_for_http (GUri *uri, GError **error)
{
        if (G_UNLIKELY (!uri)) {
                g_set_error_literal (error, SOUP_REQUEST_ERROR, SOUP_REQUEST_ERROR_BAD_URI, "URI is NULL");
                return FALSE;
        }

        const char *scheme = g_uri_get_scheme (uri);
        // QUESITON: Accept any scheme?
        if (G_UNLIKELY (!(!g_ascii_strcasecmp (scheme, "https") ||
                          !g_ascii_strcasecmp (scheme, "http")))) {
                g_set_error (error, SOUP_REQUEST_ERROR, SOUP_REQUEST_ERROR_BAD_URI, "URI has invalid scheme: %s", scheme);
                return FALSE;
        }

        const char *host = g_uri_get_host (uri);
        if (G_UNLIKELY (!host && !*host)) {
                g_set_error_literal (error, SOUP_REQUEST_ERROR, SOUP_REQUEST_ERROR_BAD_URI, "URI missing host");
                return FALSE;
        }

        return TRUE;
}

GUri *
soup_uri_copy_with_credentials (GUri *uri, const char *username, const char *password)
{
        g_return_val_if_fail (uri != NULL, NULL);

        return g_uri_build_with_user (
                g_uri_get_flags (uri) | G_URI_FLAGS_HAS_PASSWORD,
                g_uri_get_scheme (uri),
                username, password,
                g_uri_get_auth_params (uri),
                g_uri_get_host (uri),
                g_uri_get_port (uri),
                g_uri_get_path (uri),
                g_uri_get_query (uri),
                g_uri_get_fragment (uri)
        );
}

gboolean
soup_uri_paths_equal (const char *path1, const char *path2, gssize len)
{
        g_return_val_if_fail (path1 != NULL, path1 == path2);
        g_return_val_if_fail (path2 != NULL, path1 == path2);

        if (path1[0] == '\0')
                path1 = "/";
        if (path2[0] == '\0')
                path2 = "/";

        if (len == -1)
                return g_ascii_strcasecmp (path1, path2) == 0;
        else
                return g_ascii_strncasecmp (path1, path2, len) == 0;
}

static inline gboolean
is_string_normalized (const char *str)
{
        if (str == NULL)
                return TRUE;

        const char *s = str;
        while (*s) {
		if (*s == '%') {
                        /* Check for invalid escapes */
			if (s[1] == '\0' ||
			    s[2] == '\0' ||
			    !g_ascii_isxdigit (s[1]) ||
			    !g_ascii_isxdigit (s[2]))
                                return FALSE;
			else
                                s += 3;
		} else {
                        /* Check for invalid characters */
			if (!g_ascii_isgraph (*s))
                                return FALSE;
                        s++;
		}
        }

        return TRUE;
}

static inline gboolean
is_string_lower (const char *str)
{
        if (str == NULL)
                return TRUE;

        const char *s = str;
        while (*s) {
                if (!g_ascii_islower (*s))
                        return FALSE;
                s++;
        }

        return TRUE;
}

GUri *
soup_uri_parse_normalized (GUri *base, const char *uri_string, GError **error)
{
        char *scheme, *user, *password, *auth_params, *host, *path, *query, *fragment;
        int port;

        g_return_val_if_fail (uri_string != NULL, NULL);

        if (!g_uri_split_with_user  (uri_string, SOUP_HTTP_URI_FLAGS,
                                     &scheme, &user, &password, &auth_params,
                                     &host, &port,
                                     &path, &query, &fragment, error))
                return NULL;

        char *normalized_path, *normalized_query, *normalized_fragment;
        normalized_path = path ? soup_uri_normalize (path, FALSE) : NULL;
        normalized_query = query ? soup_uri_normalize (query, FALSE) : NULL;
        normalized_fragment = fragment ? soup_uri_normalize (fragment, FALSE) : NULL;
        remove_dot_segments (normalized_path);

        if (scheme && port == soup_scheme_default_port (scheme))
                port = -1;

        if (!is_string_lower (scheme)) {
                char *lower_scheme = g_ascii_strdown (scheme, -1); // TODO: Lower in-place?
                g_free (scheme);
                scheme = g_steal_pointer (&lower_scheme);
        }

        char *normalized_uri_string = g_uri_join_with_user (SOUP_HTTP_URI_FLAGS,
                                                            scheme, user, password, auth_params,
                                                            host, port, normalized_path,
                                                            normalized_query, normalized_fragment);

        g_free (scheme);
        g_free (user);
        g_free (password);
        g_free (auth_params);
        g_free (host);
        g_free (path);
        g_free (query);
        g_free (fragment);
        g_free (normalized_path);
        g_free (normalized_query);
        g_free (normalized_fragment);

        GUri *normalized_uri = g_uri_parse_relative (base, normalized_uri_string, SOUP_HTTP_URI_FLAGS, error);
        g_free (normalized_uri_string);
        return normalized_uri;
}

typedef enum {
        SOUP_NORMALIZE_FLAG_DEFAULT = 0,
        SOUP_NORMALISE_FLAG_PORT = (1 << 0),
} SoupNormalizeFlags;

static GUri *
soup_normalize_uri_internal (GUri *uri, SoupNormalizeFlags flags)
{
        const char *scheme, *path, *query, *fragment;
        int port;

        scheme = g_uri_get_scheme (uri);
        path = g_uri_get_path (uri);
        query = g_uri_get_query (uri);
        fragment = g_uri_get_fragment (uri);
        port = g_uri_get_port (uri);

        char *normalized_scheme = NULL, *normalized_path = NULL, *normalized_query = NULL, *normalized_fragment = NULL;
        int normalized_port = 0;

        if (!is_string_lower (scheme)) {
                normalized_scheme = g_ascii_strdown (scheme, -1);
                scheme = normalized_scheme;
        }

        /* If the path isn't escaped we always escape it */
        if (!(g_uri_get_flags (uri) & G_URI_FLAGS_ENCODED_PATH))
                normalized_path = g_uri_escape_string (path, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
        /* If it is escaped we ensure its valid */
        else if (!is_string_normalized (path))
                normalized_path = uri_normalized_copy (path, strlen (path), NULL);
        else if (path[0] == '\0' &&
                 (!g_strcmp0 (scheme, "http") || !g_strcmp0 (scheme, "https")))
                normalized_path = g_strdup ("/");

        /* Roughly guess if we need to remove dots */
        if (strstr (path, "/.")) {
                if (!normalized_path)
                        normalized_path = g_strdup (path);
                remove_dot_segments (normalized_path);
        }

        if (!(g_uri_get_flags (uri) & G_URI_FLAGS_ENCODED_QUERY))
                normalized_query = g_uri_escape_string (query, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
        else if (!is_string_normalized (query))
                normalized_query = uri_normalized_copy (query, strlen (query), NULL);

        if (!(g_uri_get_flags (uri) & G_URI_FLAGS_ENCODED_FRAGMENT))
                normalized_fragment = g_uri_escape_string (fragment, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
        else if (!is_string_normalized (fragment))
                normalized_fragment = uri_normalized_copy (fragment, strlen (fragment), NULL);

        if (flags & SOUP_NORMALISE_FLAG_PORT && scheme != NULL &&
            port != -1 && port == soup_scheme_default_port (normalized_scheme ? normalized_scheme : scheme))
                normalized_port = -1;

        if (normalized_scheme || normalized_path || normalized_query || normalized_fragment || normalized_port) {
                GUri *normalized_uri = g_uri_build_with_user (
                        g_uri_get_flags (uri) | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_ENCODED_FRAGMENT,
                        normalized_scheme ? normalized_scheme : scheme,
                        g_uri_get_user (uri),
                        g_uri_get_password (uri),
                        g_uri_get_auth_params (uri),
                        g_uri_get_host (uri),
                        normalized_port ? normalized_port : port,
                        normalized_path ? normalized_path : path,
                        normalized_query ? normalized_query : query,
                        normalized_fragment ? normalized_fragment : fragment
                );

                g_free (normalized_scheme);
                g_free (normalized_path);
                g_free (normalized_query);
                g_free (normalized_fragment);

                return normalized_uri;
        }

        return g_uri_ref (uri);
}

#if 0
GUri *
soup_normalize_uri_take (GUri *uri)
{
        g_return_val_if_fail (uri != NULL, NULL);

        GUri *new_uri = soup_normalize_uri_internal (uri, SOUP_NORMALIZE_FLAG_DEFAULT);
        g_uri_unref (uri);
        return new_uri;
}
#endif

GUri *
soup_normalize_uri (GUri *uri)
{
        g_return_val_if_fail (uri != NULL, NULL);

        return soup_normalize_uri_internal (uri, SOUP_NORMALIZE_FLAG_DEFAULT);
}
