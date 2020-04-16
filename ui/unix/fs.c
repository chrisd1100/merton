#include "lib.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <sys/stat.h>

static __thread char FS_PATH[PATH_MAX];

void *fs_read(const char *path, size_t *size)
{
	void *data = NULL;
	*size = 0;

	FILE *f = fopen(path, "rb");

	if (f) {
		if (fseek(f, 0, SEEK_END) == 0) {
			long offset = ftell(f);

			if (offset != -1L) {
				*size = offset;

				if (fseek(f, 0, SEEK_SET) == 0) {
					data = malloc(*size);

					if (fread(data, 1, *size, f) != *size) {
						free(data);
						data = NULL;
					}
				}
			}
		}

		fclose(f);
	}

	return data;
}

void fs_write(const char *path, const void *data, size_t size)
{
	FILE *f = fopen(path, "wb");

	if (f) {
		fwrite(data, size, 1, f);
		fclose(f);
	}
}

void fs_mkdir(const char *path)
{
	mkdir(path, 0755);
}

const char *fs_path(const char *dir, const char *file)
{
	snprintf(FS_PATH, PATH_MAX, "%s/%s", dir, file);

	return FS_PATH;
}
