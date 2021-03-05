#!/usr/bin/env python3

import sys

input_file = sys.argv[1]
output_file = sys.argv[2]
current_version = sys.argv[3]
major, minor, micro = (int(v) for v in current_version.split('.'))

def is_stable(version):
    return not version % 2

with open(input_file) as i:
    template = i.read()

versions = []

if major == 2:
    # Specific to 3.x dev releases
    versions.append((3, 0))
else:
    minor_max = minor if minor != 0 and is_stable(minor) else minor + 1
    for i in range(0, minor_max):
        if is_stable(i):
            versions.append((3, i))

version_macros = ''
version_attributes = ''

for version in versions:
    version_macros += '''/**
 * SOUP_VERSION_{major_version}_{minor_version}:
 *
 * A macro that evaluates to the {major_version}.{minor_version} version of libsoup, in a format
 * that can be used by the C pre-processor.
 *
 * Since: {major_version}.{minor_version}
 */
#define SOUP_VERSION_{major_version}_{minor_version} (G_ENCODE_VERSION ({major_version}, {minor_version}))

'''.format(major_version=version[0], minor_version=version[1])

    version_attributes += '''#if SOUP_VERSION_MIN_REQUIRED >= SOUP_VERSION_{major_version}_{minor_version}
# define SOUP_DEPRECATED_IN_{major_version}_{minor_version}                G_DEPRECATED
# define SOUP_DEPRECATED_IN_{major_version}_{minor_version}_FOR(f)         G_DEPRECATED_FOR(f)
#else
# define SOUP_DEPRECATED_IN_{major_version}_{minor_version}
# define SOUP_DEPRECATED_IN_{major_version}_{minor_version}_FOR(f)
#endif

#if SOUP_VERSION_MAX_ALLOWED < SOUP_VERSION_{major_version}_{minor_version}
# define SOUP_AVAILABLE_IN_{major_version}_{minor_version}                 G_UNAVAILABLE({major_version}, {minor_version}) _SOUP_EXTERN
#else
# define SOUP_AVAILABLE_IN_{major_version}_{minor_version}                 _SOUP_EXTERN
#endif
'''.format(major_version=version[0], minor_version=version[1])

header = template.format(version_macros=version_macros,
                         version_attributes=version_attributes,
                         major_version=major,
                         minor_version=minor,
                         micro_version=micro)

with open(output_file, 'w+') as o:
    o.write(header)
