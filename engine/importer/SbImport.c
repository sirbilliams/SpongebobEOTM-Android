/*  Game installer from the CD image. See SbImport.h.

    ISO 9660: the volume descriptors start at sector 16 (the Joliet one, if the disc has it, keeps
    the installer's upper / lower case); directories are lists of records (length, extent LBA,
    size, flags, name "DATA1.CAB;1"). Installing = what the disc's setup does (the cabinet's
    "Program Executable Files" group, into the folder) plus what it leaves on the CD: Movies/*.bik
    into movies/ and Sounds/help.omt into sounds/help.omt.                                   */

#define _FILE_OFFSET_BITS 64
#include "SbImport.h"
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
#define MAX_MOVIES 64

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

int sb_is_iso(int fd)
{
	unsigned char b[8];
	return read_at(fd, b, 6, 16LL * SECTOR) == 0 && b[0] == 1 && memcmp(b + 1, "CD001", 5) == 0;
}

/* A directory record's name as plain text, without the ";1" version. Joliet names are UTF-16BE
   (this disc's Joliet tree keeps the case the installer's file names have: mAWE.bik; the plain
   ISO tree is upper case); characters beyond Latin-1 never occur in this disc's names. */
static int rec_name(const unsigned char *r, int joliet, char *out, int out_size)
{
	int nlen = r[32], n = 0;
	const unsigned char *name = r + 33;
	if (joliet) { for (int i = 0; i + 1 < nlen && n < out_size - 1; i += 2) out[n++] = name[i] ? '?' : (char)name[i + 1]; }
	else { for (int i = 0; i < nlen && n < out_size - 1; i++) out[n++] = (char)name[i]; }
	out[n] = 0;
	char *semi = strchr(out, ';');
	if (semi) { *semi = 0; n = (int)(semi - out); }
	if (n > 0 && out[n - 1] == '.') out[--n] = 0;
	return n;
}

typedef struct
{
	Extent e;
	int is_dir;
	char name[256];
} Entry;

/* Reads the entries of directory `dir` (not "." / ".."); returns how many (at most `max`), or -1. */
static int list_dir(int fd, Extent dir, int joliet, Entry *out, int max)
{
	unsigned char *buf = malloc(dir.size ? dir.size : 1);
	int count = 0;
	if (!buf || read_at(fd, buf, dir.size, (long long)dir.lba * SECTOR) != 0) { free(buf); return -1; }
	unsigned int o = 0;
	while (o < dir.size && count < max)
	{
		unsigned int len = buf[o];
		if (len == 0) { o = (o / SECTOR + 1) * SECTOR; continue; }	/* records don't cross sectors */
		if (o + len > dir.size || len < 34) break;
		const unsigned char *r = buf + o;
		o += len;
		if (r[32] == 1 && (r[33] == 0 || r[33] == 1)) continue;		/* "." and ".." */
		out[count].e.lba = le32(r + 2);
		out[count].e.size = le32(r + 10);
		out[count].is_dir = (r[25] & 2) != 0;
		rec_name(r, joliet, out[count].name, sizeof out[count].name);
		count++;
	}
	free(buf);
	return count;
}

#define MAX_ENTRIES 512
static Entry g_entries[MAX_ENTRIES];			/* (the importer runs on one thread at a time) */

/* Finds `want` (case-insensitive) in directory `dir`; returns 1 and its extent. */
static int find_entry(int fd, Extent dir, int joliet, const char *want, int want_dir, Extent *out)
{
	int n = list_dir(fd, dir, joliet, g_entries, MAX_ENTRIES);
	for (int i = 0; i < n; i++)
		if (g_entries[i].is_dir == want_dir && strcasecmp(g_entries[i].name, want) == 0) { *out = g_entries[i].e; return 1; }
	return 0;
}

static int copy_out(int fd, Extent e, const char *path, SbImportProgress progress, void *user,
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

int sb_import_iso(int iso_fd, const char *dest_dir, const char *tmp_dir,
				  SbImportProgress progress, void *user, char *err, int err_size)
{
	static const char wrong_disc[] = "That CD image isn't the SpongeBob SquarePants: Employee of the Month PC CD "
		"(its installer files, Movies folder or Sounds/help.omt are missing).";
	unsigned char vd[SECTOR];
	unshield_set_log_level(0);						/* (it reports recoverable read retries on stderr) */
	if (!sb_is_iso(iso_fd))
	{
		snprintf(err, err_size, "That file isn't a CD image (.iso).");
		return -1;
	}
	/* volume descriptors: the Joliet supplementary one (type 2, escape %/@ %/C %/E) wins over the primary one */
	Extent root = { 0, 0 };
	int joliet = 0, have_root = 0;
	for (int sector = 16; sector < 32; sector++)
	{
		if (read_at(iso_fd, vd, SECTOR, (long long)sector * SECTOR) != 0 || memcmp(vd + 1, "CD001", 5) != 0 || vd[0] == 255) break;
		int is_joliet = vd[0] == 2 && vd[88] == '%' && vd[89] == '/' && (vd[90] == '@' || vd[90] == 'C' || vd[90] == 'E');
		if ((vd[0] == 1 && !have_root) || is_joliet)
		{
			root.lba = le32(vd + 156 + 2); root.size = le32(vd + 156 + 10);
			joliet = is_joliet; have_root = 1;
		}
	}
	Extent hdr, cab, movies_dir, sounds_dir, help;
	if (!have_root || !find_entry(iso_fd, root, joliet, "data1.hdr", 0, &hdr) || !find_entry(iso_fd, root, joliet, "data1.cab", 0, &cab)
		|| !find_entry(iso_fd, root, joliet, "Movies", 1, &movies_dir) || !find_entry(iso_fd, root, joliet, "Sounds", 1, &sounds_dir)
		|| !find_entry(iso_fd, sounds_dir, joliet, "help.omt", 0, &help))
	{
		snprintf(err, err_size, "%s", wrong_disc);
		return -1;
	}
	Entry movies[MAX_MOVIES];
	int nmovies = list_dir(iso_fd, movies_dir, joliet, movies, MAX_MOVIES), nbik = 0;
	for (int i = 0; i < nmovies; i++)
	{
		size_t l = strlen(movies[i].name);
		if (!movies[i].is_dir && l > 4 && strcasecmp(movies[i].name + l - 4, ".bik") == 0 && strchr(movies[i].name, '/') == NULL)
			movies[nbik++] = movies[i];
	}
	if (nbik == 0)
	{
		snprintf(err, err_size, "%s", wrong_disc);
		return -1;
	}

	char path[1024], hdr_path[1024], cab_path[1024];
	long long done = 0, total = (long long)hdr.size + cab.size + help.size;
	for (int i = 0; i < nbik; i++) total += movies[i].e.size;
	int r = 0;
	snprintf(path, sizeof path, "%s/movies", dest_dir);
	mkdirs(path);
	for (int i = 0; i < nbik && r == 0; i++)
	{
		snprintf(path, sizeof path, "%s/movies/%s", dest_dir, movies[i].name);
		r = copy_out(iso_fd, movies[i].e, path, progress, user, &done, total);
	}
	if (r == 0)
	{
		snprintf(path, sizeof path, "%s/sounds", dest_dir);
		mkdirs(path);
		snprintf(path, sizeof path, "%s/sounds/help.omt", dest_dir);
		r = copy_out(iso_fd, help, path, progress, user, &done, total);
	}
	mkdirs(tmp_dir);
	snprintf(hdr_path, sizeof hdr_path, "%s/data1.hdr", tmp_dir);
	snprintf(cab_path, sizeof cab_path, "%s/data1.cab", tmp_dir);
	if (r == 0) r = copy_out(iso_fd, hdr, hdr_path, progress, user, &done, total);
	if (r == 0) r = copy_out(iso_fd, cab, cab_path, progress, user, &done, total);
	if (r != 0)
	{
		unlink(hdr_path); unlink(cab_path);
		snprintf(err, err_size, r == -2 ? "Cancelled." : "Couldn't copy the files off the CD image "
				 "(is there enough free space? about 1.4 GB is needed while installing).");
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
			char sub[512];
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
				snprintf(err, err_size, "Couldn't unpack %s (is there enough free space? the game needs about 1 GB).", name);
				result = -1;
				break;
			}
			saved++;
			if (!sub[0] && strcasecmp(name, "sb2.exe") == 0) have_exe = 1;
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
