// bink_play: desktop tool that plays a .bik in an SDL window with sound, through the same compat API
// sb2.exe's movie player uses (BinkWait / DoFrame / CopyToBuffer / NextFrame, like 0x429c80).
//   bink_play movie.bik
// Env: OMT_SCREENSHOT=out.bmp  save the frame number OMT_FRAMES (default 1, 1 based) and exit
//      OMT_FRAMES=n            (see above)       SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw: capture audio
//      BINK_NOWINDOW=1         no window (headless / pacing test)
#include "bink.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: bink_play movie.bik\n"); return 2; }
    const char *shot = getenv("OMT_SCREENSHOT");
    const char *fr = getenv("OMT_FRAMES");
    unsigned shotFrame = fr ? (unsigned)atoi(fr) : 1;
    bool nowin = getenv("BINK_NOWINDOW") != NULL;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }

    BinkSetSoundSystem(BinkOpenDirectSound, 0);
    BinkPortSetSurfaceType(BINKSURFACE32);
    HBINK b = BinkOpen(argv[1], 0);
    if (!b) { fprintf(stderr, "Could not open Bink file\n"); return 1; }
    printf("%ux%u, %u frames, %u/%u fps\n", b->Width, b->Height, b->Frames, b->FrameRate, b->FrameRateDiv);

    SDL_Window *win = NULL; SDL_Renderer *ren = NULL; SDL_Texture *tex = NULL;
    if (!nowin) {
        win = SDL_CreateWindow("bink_play", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)b->Width, (int)b->Height, 0);
        ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
        tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)b->Width, (int)b->Height) : NULL;
    }
    std::vector<uint8_t> buf(b->Width * b->Height * 4);
    uint32_t t0 = SDL_GetTicks(), shown = 0;
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) quit = true;
        if (BinkWait(b)) { SDL_Delay(1); continue; }
        BinkDoFrame(b);
        BinkCopyToBuffer(b, &buf[0], (int)b->Width * 4, b->Height, 0, 0, BINKSURFACE32);
        shown = b->FrameNum;
        if (tex) { SDL_UpdateTexture(tex, NULL, &buf[0], (int)b->Width * 4); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren); }
        if (shot && shown == shotFrame) {
            SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormatFrom(&buf[0], (int)b->Width, (int)b->Height, 32, (int)b->Width * 4, SDL_PIXELFORMAT_ARGB8888);
            SDL_SaveBMP(sf, shot); SDL_FreeSurface(sf);
            printf("saved frame %u to %s\n", shown, shot);
            break;
        }
        if (b->FrameNum == b->Frames) break;      // like 0x429c80: close after the last frame is shown
        BinkNextFrame(b);
    }
    printf("%u frames shown in %u ms\n", shown, SDL_GetTicks() - t0);
    BinkClose(b);
    SDL_Quit();
    return 0;
}
