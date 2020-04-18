#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if defined(_WIN32)
	#include <direct.h>
	#include <windows.h>

	#define mkdir(dir, mode) _mkdir(dir)
	#define chdir(dir) _chdir(dir)
	#define getcwd _getcwd
	#define strcasecmp _stricmp
	#define FILENAME(ent) (ent)->cFileName
	#define ISDIR(ent) ((ent)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	#define CLOSEDIR(dir) FindClose(dir)
	#define READDIR(dir, ent) (FindNextFileA(dir, ent) ? 1 : 0)
	typedef HANDLE OS_DIR;
	typedef WIN32_FIND_DATAA OS_DIRENT;
#else
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/stat.h>
	#include <libgen.h>

	#define FILENAME(ent) (*(ent))->d_name
	#define ISDIR(ent) ((*(ent))->d_type == DT_DIR)
	#define CLOSEDIR(dir) closedir(dir)
	#define READDIR(dir, ent) (*(ent) = readdir(dir), *(ent) ? 1 : 0)
	typedef DIR * OS_DIR;
	typedef struct dirent * OS_DIRENT;
#endif

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

uint32_t fs_list(char *path, struct finfo **fi)
{
	*fi = NULL;
	uint32_t n = 0;
	OS_DIRENT ent;

	#if defined(_WIN32)
		size_t path_wildcard_len = strlen(path) + 3;
		char *path_wildcard = calloc(path_wildcard_len, 1);
		snprintf(path_wildcard, path_wildcard_len, "%s\\*", path);

		OS_DIR dir = FindFirstFileA(path_wildcard, &ent);
		bool ok = (dir != INVALID_HANDLE_VALUE);

		free(path_wildcard);
	#else
		bool ok = false;
		OS_DIR dir = opendir(path);

		if (dir) {
			ent = readdir(dir);
			ok = ent;
		}
	#endif

	while (ok) {
		char *name = FILENAME(&ent);
		bool is_dir = ISDIR(&ent);

		if (is_dir || strstr(name, ".nes")) {
			*fi = realloc(*fi, (n + 1) * sizeof(struct finfo));
			snprintf((*fi)[n].name, MAX_FILE_NAME, "%s", name);
			(*fi)[n].dir = is_dir;
			n++;
		}

		ok = READDIR(dir, &ent);
		if (!ok) CLOSEDIR(dir);
	}

	if (n > 0)
		qsort(*fi, n, sizeof(struct finfo), fs_file_compare);

	return n;
}
