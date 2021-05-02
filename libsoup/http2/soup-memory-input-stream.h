
#pragma once

#include "soup-types.h"

#define SOUP_TYPE_MEMORY_INPUT_STREAM (soup_memory_input_stream_get_type ())
G_DECLARE_FINAL_TYPE (SoupMemoryInputStream, soup_memory_input_stream, SOUP, MEMORY_INPUT_STREAM, GInputStream)

GInputStream * soup_memory_input_stream_new        (GPollableInputStream *parent_stream);

void           soup_memory_input_stream_add_bytes (SoupMemoryInputStream *stream,
                                                   GBytes                *bytes);

void          soup_memory_input_stream_complete    (SoupMemoryInputStream *stream);

G_END_DECLS
