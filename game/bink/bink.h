// Bink movie playback for the SpongeBob port: the part of RAD's binkw32.dll API that sb2.exe uses
// (10 functions), on top of FFmpeg's Bink demuxer and Bink video / audio decoders (LGPL, dynamic).
// sb2.exe calls them from the movie player at 0x429b60-0x429d60 and the sound setup at 0x40c2a0.
// Plain C API with binkw32.dll's names and argument lists (stdcall dropped: not an x86 build).
#ifndef SB_BINK_H
#define SB_BINK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The public head of RAD's BINK struct. sb2.exe reads Frames (+8) and FrameNum (+0xc) only.
typedef struct BINK {
    uint32_t Width;          // +0x00 pixels
    uint32_t Height;         // +0x04
    uint32_t Frames;         // +0x08 number of frames
    uint32_t FrameNum;       // +0x0c current frame, 1 based: 1..Frames
    uint32_t LastFrameNum;   // +0x10 last frame decoded
    uint32_t FrameRate;      // +0x14 frames per FrameRateDiv seconds
    uint32_t FrameRateDiv;   // +0x18
    uint32_t ReadError;      // +0x1c
    void *priv;              // port: our decoder state (RAD's struct goes on from here)
} BINK, *HBINK;

// BinkCopyToBuffer surface types (low bits of the flags argument, same numbers as bink.h)
enum {
    BINKSURFACE8P = 0, BINKSURFACE24 = 1, BINKSURFACE24R = 2, BINKSURFACE32 = 3, BINKSURFACE32R = 4,
    BINKSURFACE32A = 5, BINKSURFACE32RA = 6, BINKSURFACE4444 = 7, BINKSURFACE5551 = 8,
    BINKSURFACE555 = 9, BINKSURFACE565 = 10, BINKSURFACE655 = 11, BINKSURFACE664 = 12
};

typedef int (*BINKSNDSYSOPEN)(uintptr_t param);

HBINK    BinkOpen(const char *name, uint32_t flags);
void     BinkClose(HBINK bink);
int      BinkWait(HBINK bink);                 // nonzero: the next frame isn't due yet
int      BinkDoFrame(HBINK bink);              // decodes the current frame
void     BinkNextFrame(HBINK bink);            // advances to the next frame
void     BinkPause(HBINK bink, int pause);
int      BinkCopyToBuffer(HBINK bink, void *dest, int destpitch, uint32_t destheight,
                          uint32_t destx, uint32_t desty, uint32_t flags);
int      BinkDDSurfaceType(void *surface);     // BINKSURFACE* for a surface, or -1
int      BinkOpenDirectSound(uintptr_t param); // sound system handed to BinkSetSoundSystem
int      BinkSetSoundSystem(BINKSNDSYSOPEN open, uintptr_t param);

// port: not in binkw32. The engine tells us the pixel format of the surface it lets
// BinkDDSurfaceType look at (default BINKSURFACE32 = bytes B,G,R,X).
void     BinkPortSetSurfaceType(int type);

#ifdef __cplusplus
}
#endif
#endif
