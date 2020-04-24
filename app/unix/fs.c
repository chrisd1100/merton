#define _DARWIN_C_SOURCE // DT_DIR

#include "lib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static __thread char FS_PATH[PATH_MAX];
static __thread char FS_PROG_DIR[PATH_MAX];

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

const char *fs_prog_dir(void)
{
	int32_t n = readlink("/proc/self/exe", FS_PROG_DIR, PATH_MAX);

	if (n > 0 && n < PATH_MAX) {
		FS_PROG_DIR[n] = '\0';

		char *last_bs = NULL;
		char *ptr = FS_PROG_DIR;

		do {
			ptr = strchr(ptr, '/');

			if (ptr) {
				last_bs = ptr;
				ptr++;
			}
		} while (ptr);

		if (last_bs)
			last_bs[0] = '\0';

	} else {
		snprintf(FS_PROG_DIR, PATH_MAX, ".");
	}

	return FS_PROG_DIR;
}

const char *fs_path(const char *dir, const char *file)
{
	snprintf(FS_PATH, PATH_MAX, "%s/%s", dir, file);

	return FS_PATH;
}

static int32_t fs_file_compare(const void *p1, const void *p2)
{
	struct finfo *fi1 = (struct finfo *) p1;
	struct finfo *fi2 = (struct finfo *) p2;

	if (fi1->dir && !fi2->dir) {
		return -1;
	} else if (!fi1->dir && fi2->dir) {
		return 1;
	} else {
		int32_t r = strcasecmp(fi1->name, fi2->name);

		if (r != 0)
			return r;

		return -strcmp(fi1->name, fi2->name);
	}
}

uint32_t fs_list(const char *path, struct finfo **fi)
{
	*fi = NULL;
	uint32_t n = 0;
	struct dirent *ent = NULL;

	bool ok = false;
	DIR *dir = opendir(path);

	if (dir) {
		ent = readdir(dir);
		ok = ent;
	}

	while (ok) {
		char *name = ent->d_name;
		bool is_dir = ent->d_type == DT_DIR;

		if (is_dir || strstr(name, ".nes")) {
			*fi = realloc(*fi, (n + 1) * sizeof(struct finfo));

			size_t name_size = strlen(name) + 1;
			size_t path_size = name_size + strlen(path) + 1;
			(*fi)[n].name = malloc(name_size);
			(*fi)[n].path = malloc(path_size);

			snprintf((char *) (*fi)[n].name, name_size, "%s", name);
			snprintf((char *) (*fi)[n].path, path_size, "%s/%s", path, name);

			(*fi)[n].dir = is_dir;
			n++;
		}

		ok = (ent = readdir(dir)), ent ? 1 : 0;
		if (!ok) closedir(dir);
	}

	if (n > 0)
		qsort(*fi, n, sizeof(struct finfo), fs_file_compare);

	return n;
}

void fs_free_list(struct finfo **fi, uint32_t len)
{
	if (!fi || !*fi)
		return;

	for (uint32_t x = 0; x < len; x++) {
		free((char *) (*fi)[x].name);
		free((char *) (*fi)[x].path);
	}

	free(*fi);
	*fi = NULL;
}
