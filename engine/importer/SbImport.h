/*  Installs the game files from the game's CD image (.iso): finds the InstallShield cabinet on
    the disc (data1.hdr / data1.cab), copies it out, and extracts the "Program Executable Files"
    group (sb2.exe, OMT.dll, *.omt, scenes/ ... ) into a folder - what the CD's setup.exe does -
    plus the files the game reads from the CD: Movies/*.bik into movies/, Sounds/help.omt into
    sounds/help.omt.

    Used by the Android app (through JNI, see SbImportJNI.c) and by the desktop tool sb_import. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Progress: stage 0 = copying the cabinet off the disc, 1 = extracting; done/total in bytes
   (stage 0) or files (stage 1). Return non-zero to cancel. */
typedef int (*SbImportProgress)(void *user, int stage, long long done, long long total);

/* Installs from the ISO open on `iso_fd` (read with pread, position not used) into `dest_dir`
   (created; existing files are overwritten). `tmp_dir` holds the copied cabinet meanwhile.
   Returns 0 on success, otherwise writes a message for the player into `err`. */
int sb_import_iso(int iso_fd, const char *dest_dir, const char *tmp_dir,
				   SbImportProgress progress, void *user, char *err, int err_size);

/* 1 if the file open on `fd` looks like an ISO 9660 image. */
int sb_is_iso(int fd);

#ifdef __cplusplus
}
#endif
