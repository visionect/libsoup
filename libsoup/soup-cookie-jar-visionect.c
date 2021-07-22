/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-cookie-jar-visionect.c: visionect database-based cookie storage (additional hook functions for text)
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
 * Copyright (C) 2021 Visionect d.o.o.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <endian.h>

#include <libpq-fe.h>

#include "soup-cookie-jar-visionect.h"
#include "soup.h"

int
soup_cookie_jar_visionect_try_connect(SoupCookieJarTextPrivate *jar);

static void
migrate_and_load_old (SoupCookieJar *jar);

static int 
psql_write_cookie(PGconn *conn, char *uuid, SoupCookie *cookie);

static int
psql_delete_cookie (PGconn *conn, char *uuid, SoupCookie *cookie);

static int
psql_load_cookies (SoupCookieJar *jar);

static void vlog(char *fmt, ...) {
	va_list args;
	char *newFmt;
	char *ts = g_malloc(32);
	time_t t = time(NULL);

	strftime(ts, 32, "%Y-%m-%dT%H:%M:%S%z", gmtime(&t));
	newFmt = g_strdup_printf("%s %s", ts, fmt);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	g_free(newFmt);
	g_free(ts);
}

void
soup_cookie_jar_visionect_finalize (SoupCookieJarTextPrivate *priv)
{	
	if (priv->conn != NULL) {
		PQfinish(priv->conn);
		priv->conn = NULL;
	}
	if (priv->uuid != NULL) {
		g_free (priv->uuid);
	}
}


int
soup_cookie_jar_visionect_load (SoupCookieJar *jar)
{	
	SoupCookieJarTextPrivate *priv =
		SOUP_COOKIE_JAR_TEXT_GET_PRIVATE (jar);

	int len;
	char *s;

	priv->db_mode = false;
	priv->uuid = NULL;
	priv->conn = NULL;

	// strip away the prefix
	len = strlen(priv->filename) + 1 - strlen(VISO_DB_FILE_PREFIX);
	s = g_malloc(len);
	s[len-1] = 0;
	g_strlcpy(s, priv->filename + strlen(VISO_DB_FILE_PREFIX), len);
	g_free(priv->filename);
	priv->filename = s;

	// get the uuid from file name.. first try to remove the path
	s = g_strrstr(priv->filename, "/");
	if (s != NULL) {
		s++; // move one byte forward to also remove /
	} else {
		s = priv->filename; // no / in path just set the beginning to the start of string
	}

	// allocate space for the uuid (a bit more, but ist not a big amount)
	len = strlen(s) + 1;	
	priv->uuid = g_malloc(len);
	priv->uuid[len-1] = 0;
	g_strlcpy(priv->uuid, s, len);

	s = strstr(priv->uuid, "-cookies.txt");
	if (s == NULL) {
		vlog("skipping db mode, cookie file is named %s\n", priv->filename);
		return -1;
	}
	*s = 0; // remove suffix

	//we set db mode only if cookie name matches
	priv->db_mode = true;

	migrate_and_load_old(jar);

	if (soup_cookie_jar_visionect_try_connect(priv) != 0) {
		return -1;
	}
	psql_load_cookies(jar);

	return 0;
}

int
soup_cookie_jar_visionect_try_connect(SoupCookieJarTextPrivate *jar)
{
	char *psql_host;
	char *psql_user;
	char *psql_pass;
	char *psql_db;
	
	gboolean connect;

	char *q;
	PGresult *res;

	connect = (jar->conn == NULL);

	if (!connect) {
		if (PQstatus(jar->conn) != CONNECTION_OK) {
			// we could have used PQreset here, but that means we have to handle yet another case.. 
			// so we just drop everything and continue from the blank slate
			PQfinish(jar->conn);
			connect = true;
		} else {
			// verfy connection by running a simple sql
			q = "SELECT 1;";
			res = PQexec(jar->conn, q);

			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				PQfinish(jar->conn);
				connect = true;
			}

			PQclear(res);
		}
	}

	if (connect) {
		psql_host = getenv("DB2_1_PORT_5432_TCP_ADDR");
		psql_user = getenv("DB2_1_PORT_5432_TCP_USER");
		psql_pass = getenv("DB2_1_PORT_5432_TCP_PASS");
		psql_db   = getenv("DB2_1_PORT_5432_TCP_DB");

		if (psql_host == NULL || psql_user == NULL || psql_pass == NULL || psql_db == NULL) {
			vlog("You must set all listed env variables:\n" \
			"DB2_1_PORT_5432_TCP_ADDR, DB2_1_PORT_5432_TCP_USER,\n" \
			"DB2_1_PORT_5432_TCP_PASS, DB2_1_PORT_5432_TCP_DB\n");
		}

		q = g_strdup_printf("host=%s user=%s password=%s dbname=%s", psql_host, psql_user, psql_pass, psql_db);
		jar->conn = PQconnectdb(q);
		if (PQstatus(jar->conn) != CONNECTION_OK) {
			vlog("Connection to database failed: %s\n",
				PQerrorMessage(jar->conn));

			PQfinish(jar->conn);
			jar->conn = NULL;

			return -1;
		}

		free(q);
	}

	return 0;
}

static SoupCookie*
parse_line (SoupCookieJar *jar, char *line, time_t now)
{
	SoupCookie *cookie;

	cookie = parse_cookie (line, now);
	if (cookie) {
		soup_cookie_jar_add_cookie (jar, cookie);
	}

	return cookie;
}

static char 
*psql_get_old_cookies(PGconn *conn, char *uuid)
{
	char *q;
	char *val;
	unsigned char *val_ok;
	PGresult *res;
	size_t to_len;

	q = g_strdup_printf("SELECT data FROM filestorage WHERE name = '%s-cookies.txt' AND uuid = '%s' AND type = 'session';", uuid, uuid);
	res = PQexec(conn, q);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		vlog("Selecting old cookies for device %s failed: %s\n",
			uuid, PQresultErrorMessage(res));

		PQclear(res);
		g_free(q);
		return NULL;
	}

	// no rows.. either already migrated or no records
	if (PQntuples(res) == 0) {
		PQclear(res);
		g_free(q);
		return NULL;
	}

	val = PQgetvalue(res, 0, 0);
	val_ok = PQunescapeBytea((const unsigned char *)val, &to_len);
	PQclear(res);
	g_free(q);

	return (char *)val_ok;
}

static int 
psql_delete_old_cookies(PGconn *conn, char *uuid)
{
	char *q;
	PGresult *res;

	q = g_strdup_printf("DELETE FROM filestorage WHERE name = '%s-cookies.txt' AND uuid = '%s' AND type = 'session';", uuid, uuid);
	res = PQexec(conn, q);

	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		vlog("Deleting old cookies for device %s failed: %s\n",
			uuid, PQresultErrorMessage(res));

		PQclear(res);
		g_free(q);
		return -1;
	}

	PQclear(res);
	g_free(q);

	return 0;
}

static void
migrate_and_load_old (SoupCookieJar *jar)
{
	SoupCookieJarTextPrivate *priv =
		SOUP_COOKIE_JAR_TEXT_GET_PRIVATE (jar);

	char *contents = NULL, *line, *p;
	time_t now = time (NULL);
	SoupCookie *cookie;

	if (soup_cookie_jar_visionect_try_connect(priv) != 0) {
		return;
	}
	contents = psql_get_old_cookies(priv->conn, priv->uuid);

	// no old data.. we are done
	if (contents == NULL) {
		return;
	}

	line = contents;
	for (p = contents; *p; p++) {
		/* \r\n comes out as an extra empty line and gets ignored */
		if (*p == '\r' || *p == '\n') {
			*p = '\0';
			cookie = parse_line (jar, line, now);
			if (cookie) {
				psql_write_cookie(priv->conn, priv->uuid, cookie);
			}
			line = p + 1;
		}
	}
	cookie = parse_line (jar, line, now);
	if (cookie) {
		psql_write_cookie(priv->conn, priv->uuid, cookie);
	}

	g_free (contents);

	psql_delete_old_cookies(priv->conn, priv->uuid);
}

void
soup_cookie_jar_visionect_changed (SoupCookieJar *jar,
				  SoupCookie	*old_cookie,
				  SoupCookie	*new_cookie)
{
	SoupCookieJarTextPrivate *priv =
		SOUP_COOKIE_JAR_TEXT_GET_PRIVATE (jar);

	/* We can sort of ignore the semantics of the 'changed'
	 * signal here and simply delete the old cookie if present
	 * and write the new cookie if present. That will do the
	 * right thing for all 'added', 'deleted' and 'modified'
	 * meanings.
	 */
	if (old_cookie) {
		if (soup_cookie_jar_visionect_try_connect(priv) != 0) {
			return;
		}

		psql_delete_cookie (priv->conn, priv->uuid, old_cookie);
	}

	if (new_cookie) {
		if (new_cookie->expires) {
			if (soup_cookie_jar_visionect_try_connect(priv) != 0) {
				return;
			}

			psql_write_cookie(priv->conn, priv->uuid, new_cookie);
		}
	}
}

static int 
psql_write_cookie(PGconn *conn, char *uuid, SoupCookie *cookie)
{
	const int PARAM_COUNT = 10;
	PGresult *res;
	const char *paramValues[PARAM_COUNT];
	char *rslt;
	gulong expires;

	if (cookie->expires) {
		expires = (gulong)soup_date_to_time_t (cookie->expires);
		rslt = g_strdup_printf("%ld", expires);
	} else {
		rslt = NULL;
	}

	paramValues[0] = uuid;
	paramValues[1] = cookie->domain;
	paramValues[2] = cookie->name;
	paramValues[3] = cookie->value;
	paramValues[4] = cookie->path;
	paramValues[5] = rslt;
	paramValues[6] = NULL;
	paramValues[7] = g_strdup_printf("%d", cookie->secure);
	paramValues[8] = g_strdup_printf("%d", cookie->http_only);
	paramValues[9] = "0";
	res = PQexecParams(conn, 
			"INSERT INTO cookies (uuid, host, name, value, path, expiry, last_accessed, is_secure, is_http_only, same_site) VALUES " \
			"($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) " \
			"ON CONFLICT(uuid, host, name) " \
			"DO UPDATE SET value = $4, path = $5, expiry = $6, last_accessed = $7, is_secure = $8, is_http_only = $9, same_site = $10;", 
 			PARAM_COUNT, NULL, paramValues, NULL, NULL, 1);

	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		vlog("Cookie insert for device %s failed: %s\n",
			uuid, PQresultErrorMessage(res));

		g_free((void *)paramValues[5]);
		g_free((void *)paramValues[7]);
		g_free((void *)paramValues[8]);
		PQclear(res);
		return -1;
	}

	g_free((void *)paramValues[5]);
	g_free((void *)paramValues[7]);
	g_free((void *)paramValues[8]);
	PQclear(res);

	return 0;
}

static int 
psql_delete_cookie (PGconn *conn, char *uuid, SoupCookie *cookie)
{
	const int PARAM_COUNT = 3;
	PGresult *res;
	const char *paramValues[PARAM_COUNT];

	paramValues[0] = uuid;
	paramValues[1] = cookie->domain;
	paramValues[2] = cookie->name;
	res = PQexecParams(conn, "DELETE FROM cookies WHERE uuid = $1 AND host = $2 AND name = $3;", PARAM_COUNT, NULL, paramValues, NULL, NULL, 1);

	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		vlog("Deleting cookie for device %s failed: %s\n",
			uuid, PQresultErrorMessage(res));

		PQclear(res);
		return -1;
	}

	PQclear(res);

	return 0;
}

static int 
psql_load_cookies(SoupCookieJar *jar)
{
	SoupCookieJarTextPrivate *priv =
		SOUP_COOKIE_JAR_TEXT_GET_PRIVATE (jar);

	const int PARAM_COUNT = 1;
	PGresult *res;
	const char *paramValues[PARAM_COUNT];
	int i;

	SoupCookie *cookie = NULL;
	char *host, *name, *value, *path;
	char *tmpStr;
	int  tmpInt;
	gulong expire_time;
	int max_age;
	gboolean secure, http_only;
	time_t now;

	paramValues[0] = priv->uuid;
	res = PQexecParams(priv->conn, 
		"SELECT host, name, value, path, expiry, last_accessed, is_secure, is_http_only, same_site FROM cookies WHERE uuid = $1;",
		PARAM_COUNT, NULL, paramValues, NULL, NULL, 1);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		vlog("Selecting cookies for device %s failed: %s\n",
			priv->uuid, PQresultErrorMessage(res));

		PQclear(res);
		return -1;
	}

	// no rows..
	if (PQntuples(res) == 0) {
		PQclear(res);
		return 0;
	}

	for (i = 0; i < PQntuples(res); i++) {
		host = PQgetvalue(res, i, 0);
		name = PQgetvalue(res, i, 1);
		value = PQgetvalue(res, i, 2);
		path = PQgetvalue(res, i, 3);

		tmpStr = PQgetvalue(res, i, 4);
		expire_time = 0;
		max_age = 0;
		if (tmpStr != NULL) {			
			expire_time = be64toh(*((gulong *) tmpStr));
			now = time (NULL);
			if (now >= expire_time) {
				continue;
			}
			max_age = (expire_time - now <= G_MAXINT ? expire_time - now : G_MAXINT);
		}
		// last_accessed
		tmpStr = PQgetvalue(res, i, 5);
		/*if (tmpStr != NULL) {
			last_accessed = ntoh64(*((gulong *) tmpStr));
		}*/
		tmpStr = PQgetvalue(res, i, 6);
		secure = FALSE;
		if (tmpStr != NULL) {
			tmpInt = be32toh(*((guint32 *) tmpStr));
			secure = (tmpInt != FALSE);
		}
		tmpStr = PQgetvalue(res, i, 7);
		http_only = FALSE;
		if (tmpStr != NULL) {
			tmpInt = be32toh(*((guint32 *) tmpStr));
			http_only = (tmpInt != FALSE);
		}
		// same_site
		tmpStr = PQgetvalue(res, i, 8);

		// build up cooke and add it to jar
		cookie = soup_cookie_new (name, value, host, path, max_age);
		if (secure) {
			soup_cookie_set_secure (cookie, TRUE);
		}
		if (http_only) {
			soup_cookie_set_http_only (cookie, TRUE);
		}

		soup_cookie_jar_add_cookie (jar, cookie);
	}
	PQclear(res);

	return 0;
}
