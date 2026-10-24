/*  neg_import: install the game files from the game's CD image, as the Android app does.
        neg_import <game.iso> <dest-folder>                                                    */
#include "NegImport.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int progress(void *user, int stage, long long done, long long total)
{
	static int last = -1;
	int pct = total ? (int)(done * 100 / total) : 0;
	(void)user;
	if (pct != last || done == total) { last = pct; fprintf(stderr, "\r%s %3d%%", stage ? "unpacking" : "copying  ", pct); }
	if (done == total) fprintf(stderr, "\n");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) { fprintf(stderr, "usage: neg_import <game.iso> <dest-folder>\n"); return 2; }
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) { perror(argv[1]); return 1; }
	char tmp[1024], err[512];
	snprintf(tmp, sizeof tmp, "%s.import-tmp", argv[2]);
	int r = neg_import_iso(fd, argv[2], tmp, progress, NULL, err, sizeof err);
	close(fd);
	rmdir(tmp);
	if (r) { fprintf(stderr, "error: %s\n", err); return 1; }
	printf("installed into %s\n", argv[2]);
	return 0;
}
