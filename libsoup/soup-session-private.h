/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000-2003, Ximian, Inc.
 */

#ifndef __SOUP_SESSION_PRIVATE_H__
#define __SOUP_SESSION_PRIVATE_H__ 1

#include "soup-session.h"

G_BEGIN_DECLS

GUri    *soup_session_get_message_proxy_uri (SoupSession *session,
					     SoupMessage *msg);
void     soup_session_requeue_message       (SoupSession *session,
					     SoupMessage *msg);

G_END_DECLS

#endif /* __SOUP_SESSION_PRIVATE_H__ */
