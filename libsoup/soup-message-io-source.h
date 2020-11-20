
#pragma once

#include "soup.h"

typedef struct {
	GSource source;
	GObject *msg;
        gboolean (*check_func) (GSource*);
	gboolean paused;
} SoupMessageIOSource;

typedef gboolean (*SoupMessageIOSourceFunc) (GObject     *msg,
					     gpointer     user_data);

GSource *soup_message_io_source_new (GSource     *base_source,
                                     GObject     *msg,
                                     gboolean     paused,
                                     gboolean   (*check_func) (GSource*));