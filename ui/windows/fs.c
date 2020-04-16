#include "lib.h"

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>

static __declspec(thread) char FS_PATH[MAX_PATH];

void *fs_read(const char *path, size_t *size)
{
	void *data = NULL;
	*size = 0;

	FILE *f = NULL;
	errno_t e = fopen_s(&f, path, "rb");

	if (e == 0) {
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
	FILE *f = NULL;
	errno_t e = fopen_s(&f, path, "wb");

	if (e == 0) {
		fwrite(data, size, 1, f);
		fclose(f);
	}
}

void fs_mkdir(const char *path)
{
	CreateDirectoryA(path, NULL);
}

const char *fs_path(const char *dir, const char *file)
{
	snprintf(FS_PATH, MAX_PATH, "%s\\%s", dir, file);

	return FS_PATH;
}
