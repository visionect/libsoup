/*
 * Copyright 2021 Igalia S.L.
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
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
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "test-utils.h"
#include "soup-memory-input-stream.h"

static void
do_large_data_test (void)
{
#define CHUNK_SIZE (gsize)1024 * 1024 * 512 // 512 MiB
#define TEST_SIZE CHUNK_SIZE * 20 // 10 GiB

        GInputStream *stream = soup_memory_input_stream_new (NULL);
        SoupMemoryInputStream *mem_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        gsize data_needed = TEST_SIZE;
        char *memory_chunk = g_new (char, CHUNK_SIZE); 
        char *trash_buffer = g_new (char, CHUNK_SIZE);

        // We can add unlimited data and as long as its read the data will
        // be freed, so this should work fine even though its reading GB of data

        while (data_needed > 0) {
                // Copy chunk
                GBytes *bytes = g_bytes_new (memory_chunk, CHUNK_SIZE);
                soup_memory_input_stream_add_bytes (mem_stream, bytes);
                g_bytes_unref (bytes);

                // This should free the copy
                gssize read = g_input_stream_read (stream, trash_buffer, CHUNK_SIZE, NULL, NULL);
                g_assert_cmpint (read, ==, CHUNK_SIZE);

                data_needed -= CHUNK_SIZE;
                
        }

        g_free (trash_buffer);
        g_free (memory_chunk);
        g_object_unref (stream);
}

static void
do_multiple_chunk_test (void)
{
        GInputStream *stream = soup_memory_input_stream_new (NULL);
        SoupMemoryInputStream *mem_stream = SOUP_MEMORY_INPUT_STREAM (stream);
        const char * const chunks[] = {
                "1234", "5678", "9012", "hell", "owor", "ld..",
        };

        for (guint i = 0; i < G_N_ELEMENTS (chunks); ++i) {
                GBytes *bytes = g_bytes_new_static (chunks[i], 4);
                g_assert (g_bytes_get_size (bytes) == 4);
                soup_memory_input_stream_add_bytes (mem_stream, bytes);
                g_bytes_unref (bytes);
        }

        // Do partial reads of chunks to ensure it always comes out as expected
        for (guint i = 0; i < G_N_ELEMENTS (chunks); ++i) {
                char buffer[5] = { 0 };
                gssize read = g_input_stream_read (stream, buffer, 2, NULL, NULL);
                g_assert_cmpint (read, ==, 2);
                read = g_input_stream_read (stream, buffer + 2, 2, NULL, NULL);
                g_assert_cmpint (read, ==, 2);
                g_assert_cmpstr (buffer, ==, chunks[i]);
        }

        g_object_unref (stream);
}

int
main (int argc, char **argv)
{
	int ret;

	test_init (argc, argv, NULL);

	g_test_add_func ("/memory_stream/large_data", do_large_data_test);
        g_test_add_func ("/memory_stream/multiple_chunks", do_multiple_chunk_test);

	ret = g_test_run ();

        test_cleanup ();

	return ret;
}
