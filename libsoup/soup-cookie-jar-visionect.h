/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-cookie-jar-visionect.c: visionect database-based cookie storage (additional hook functions for text)
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
 * Copyright (C) 2021 Visionect d.o.o.
 */
#ifndef SOUP_COOKIE_JAR_VISIONECT_H
#define SOUP_COOKIE_JAR_VISIONECT_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <libpq-fe.h>
#include "soup.h"


#define VISO_DB_FILE_PREFIX "?db?"

typedef struct {
	char *filename;

	char     *uuid;
	gboolean db_mode;
	PGconn   *conn;	
} SoupCookieJarTextPrivate;

#define SOUP_COOKIE_JAR_TEXT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SOUP_TYPE_COOKIE_JAR_TEXT, SoupCookieJarTextPrivate))

void
soup_cookie_jar_visionect_finalize (SoupCookieJarTextPrivate *priv);

int
soup_cookie_jar_visionect_load (SoupCookieJar *jar);

void
soup_cookie_jar_visionect_changed (SoupCookieJar *jar,
				  SoupCookie    *old_cookie,
				  SoupCookie    *new_cookie);


// forward declarations for functions implemented in soup_cookie_jar_text
SoupCookie*
parse_cookie (char *line, time_t now);

#endif /* SOUP_COOKIE_JAR_VISIONECT_H */