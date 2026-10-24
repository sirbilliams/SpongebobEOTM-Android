/*  Game installer from the CD image. See NegImport.h.

    ISO 9660: the primary volume descriptor is sector 16; directories are lists of records
    (length, extent LBA, size, flags, name "DATA1.CAB;1"). The cabinet is searched for in the
    root and in sub-folders (some images put the disc contents in a folder).                   */

#define _FILE_OFFSET_BITS 64
#include "NegImport.h"
#include "libunshield.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SECTOR 2048
#define GROUP_NAME "Program Executable Files"

typedef struct { unsigned int lba, size; } Extent;

static unsigned int le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

static int read_at(int fd, void *buf, size_t n, long long off)
{
	size_t got = 0;
	while (got < n)
	{
		ssize_t r = pread(fd, (char *)buf + got, n - got, (off_t)(off + (long long)got));
		if (r < 0 && errno == EINTR) continue;
		if (r <= 0) return -1;
		got += (size_t)r;
	}
	return 0;
}

int neg_is_iso(int fd)
{
	unsigned char b[8];
	return read_at(fd, b, 6, 16LL * SECTOR) == 0 && b[0] == 1 && memcmp(b + 1, "CD001", 5) == 0;
}

/* ISO name ("DATA1.CAB;1") equals `want` ("data1.cab"), ignoring case and the version. */
static int name_is(const unsigned char *name, int len, const char *want)
{
	int n = 0;
	while (n < len && name[n] != ';') n++;
	if (n > 0 && name[n - 1] == '.') n--;
	if ((int)strlen(want) != n) return 0;
	for (int i = 0; i < n; i++)
		if (tolower(name[i]) != tolower((unsigned char)want[i])) return 0;
	return 1;
}

/* Finds file `want` in directory `dir`, or (depth > 0) in its sub-folders. Also returns the
   directory holding it in `in_dir`, so its sibling can be looked up there. */
static int find_file(int fd, Extent dir, const char *want, int depth, Extent *out, Extent *in_dir)
{
	unsigned char *buf = malloc(dir.size);
	if (!buf || read_at(fd, buf, dir.size, (long long)dir.lba * SECTOR) != 0) { free(buf); return 0; }
	int found = 0;
	for (int pass = 0; pass < 2 && !found; pass++)			/* files first, then sub-folders */
	{
		unsigned int o = 0;
		while (o < dir.size && !found)
		{
			unsigned int len = buf[o];
			if (len == 0) { o = (o / SECTOR + 1) * SECTOR; continue; }	/* records don't cross sectors */
			if (o + len > dir.size || len < 34) break;
			const unsigned char *r = buf + o;
			Extent e = { le32(r + 2), le32(r + 10) };
			int is_dir = r[25] & 2, nlen = r[32];
			const unsigned char *name = r + 33;
			int dot = nlen == 1 && (name[0] == 0 || name[0] == 1);	/* "." and ".." */
			if (pass == 0 && !is_dir && name_is(name, nlen, want)) { *out = e; *in_dir = dir; found = 1; }
			else if (pass == 1 && is_dir && !dot && depth > 0)
				found = find_file(fd, e, want, depth - 1, out, in_dir);
			o += len;
		}
	}
	free(buf);
	return found;
}

static int copy_out(int fd, Extent e, const char *path, NegImportProgress progress, void *user,
					long long *done, long long total)
{
	FILE *f = fopen(path, "wb");
	if (!f) return -1;
	static unsigned char buf[1 << 20];
	long long off = (long long)e.lba * SECTOR, left = e.size;
	while (left > 0)
	{
		size_t n = left > (long long)sizeof buf ? sizeof buf : (size_t)left;
		if (read_at(fd, buf, n, off) != 0 || fwrite(buf, 1, n, f) != n) { fclose(f); return -1; }
		off += (long long)n; left -= (long long)n; *done += (long long)n;
		if (progress && progress(user, 0, *done, total)) { fclose(f); return -2; }
	}
	return fclose(f) == 0 ? 0 : -1;
}

static void mkdirs(const char *path)
{
	char p[1024];
	snprintf(p, sizeof p, "%s", path);
	for (char *s = p + 1; *s; s++)
		if (*s == '/') { *s = 0; mkdir(p, 0755); *s = '/'; }
	mkdir(p, 0755);
}

int neg_import_iso(int iso_fd, const char *dest_dir, const char *tmp_dir,
				   NegImportProgress progress, void *user, char *err, int err_size)
{
	unsigned char pvd[SECTOR];
	unshield_set_log_level(0);						/* (it reports recoverable read retries on stderr) */
	if (read_at(iso_fd, pvd, SECTOR, 16LL * SECTOR) != 0 || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0)
	{
		snprintf(err, err_size, "That file isn't a CD image (.iso).");
		return -1;
	}
	Extent root = { le32(pvd + 156 + 2), le32(pvd + 156 + 10) }, hdr, cab, where;
	if (!find_file(iso_fd, root, "data1.hdr", 2, &hdr, &where) || !find_file(iso_fd, where, "data1.cab", 0, &cab, &where))
	{
		snprintf(err, err_size, "That CD image doesn't contain the game's installer files (data1.hdr / data1.cab). "
				 "Use the Jimmy Neutron vs. Jimmy Negatron PC CD.");
		return -1;
	}

	char hdr_path[1024], cab_path[1024];
	mkdirs(tmp_dir);
	snprintf(hdr_path, sizeof hdr_path, "%s/data1.hdr", tmp_dir);
	snprintf(cab_path, sizeof cab_path, "%s/data1.cab", tmp_dir);
	long long done = 0, total = (long long)hdr.size + cab.size;
	int r = copy_out(iso_fd, hdr, hdr_path, progress, user, &done, total);
	if (r == 0) r = copy_out(iso_fd, cab, cab_path, progress, user, &done, total);
	if (r != 0)
	{
		unlink(hdr_path); unlink(cab_path);
		snprintf(err, err_size, r == -2 ? "Cancelled." : "Couldn't copy the installer off the CD image "
				 "(is there enough free space? about 130 MB is needed while installing).");
		return -1;
	}

	int result = -1;
	Unshield *u = unshield_open(cab_path);
	UnshieldFileGroup *g = u ? unshield_file_group_find(u, GROUP_NAME) : NULL;
	if (!g)
		snprintf(err, err_size, "The installer on that CD image couldn't be read.");
	else
	{
		long long nfiles = (long long)g->last_file - g->first_file + 1, n = 0;
		int saved = 0, have_exe = 0;
		result = 0;
		for (unsigned int i = g->first_file; i <= g->last_file; i++, n++)
		{
			if (!unshield_file_is_valid(u, (int)i)) continue;
			const char *dir = unshield_directory_name(u, unshield_file_directory(u, (int)i));
			const char *name = unshield_file_name(u, (int)i);
			if (!name || strchr(name, '/') || strstr(name, "..") || (dir && strstr(dir, ".."))) continue;
			char sub[512], path[1024];
			snprintf(sub, sizeof sub, "%s", dir ? dir : "");
			for (char *s = sub; *s; s++) if (*s == '\\') *s = '/';
			if (sub[0])
			{
				snprintf(path, sizeof path, "%s/%s", dest_dir, sub);
				mkdirs(path);
				snprintf(path, sizeof path, "%s/%s/%s", dest_dir, sub, name);
			}
			else
			{
				mkdirs(dest_dir);
				snprintf(path, sizeof path, "%s/%s", dest_dir, name);
			}
			if (!unshield_file_save(u, (int)i, path) && !unshield_file_save_old(u, (int)i, path))
			{
				snprintf(err, err_size, "Couldn't unpack %s (is there enough free space? the game needs about 270 MB).", name);
				result = -1;
				break;
			}
			saved++;
			if (!sub[0] && strcasecmp(name, "Neutron2.exe") == 0) have_exe = 1;
			if (progress && progress(user, 1, n + 1, nfiles)) { snprintf(err, err_size, "Cancelled."); result = -1; break; }
		}
		if (result == 0 && (!saved || !have_exe))
		{
			snprintf(err, err_size, "The installer on that CD image doesn't have the game files.");
			result = -1;
		}
	}
	if (u) unshield_close(u);
	unlink(hdr_path);
	unlink(cab_path);
	return result;
}
