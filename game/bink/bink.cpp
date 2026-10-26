// Bink movie playback on FFmpeg: see bink.h. C++98 style, no Windows, no x86.
#include "bink.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
}
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace {

struct BinkState {
    AVFormatContext *fmt;
    AVCodecContext *vdec, *adec;
    int vstream, astream;
    AVFrame *frame, *aframe;             // frame: the last decoded picture
    AVPacket *pkt;
    std::vector<AVPacket *> vqueue;      // video packets read while looking for audio
    bool eof, haveFrame, paused;
    // clock
    uint32_t startTicks, pauseTicks;     // no-audio fallback (SDL_GetTicks)
    bool started;
    // audio: float samples, interleaved, consumed by the SDL callback
    SDL_AudioDeviceID dev;
    int channels, rate;
    std::vector<float> ring;
    size_t ringPos;                      // read position in ring (floats)
    uint64_t played;                     // sample frames handed to the device since start
};

int g_surfaceType = BINKSURFACE32;
bool g_soundSet = false;

void audioCallback(void *ud, Uint8 *stream, int len)
{
    BinkState *s = (BinkState *)ud;              // runs with the device locked
    float *out = (float *)stream;
    size_t want = (size_t)len / sizeof(float);
    size_t have = s->ring.size() - s->ringPos;
    size_t n = have < want ? have : want;
    if (n) memcpy(out, &s->ring[s->ringPos], n * sizeof(float));
    if (n < want) memset(out + n, 0, (want - n) * sizeof(float));   // silence, the clock keeps running
    s->ringPos += n;
    s->played += want / (size_t)s->channels;
    if (s->ringPos > 65536) {                    // drop the consumed part now and then
        s->ring.erase(s->ring.begin(), s->ring.begin() + (long)s->ringPos);
        s->ringPos = 0;
    }
}

double ringSeconds(BinkState *s)
{
    if (!s->dev) return 0;
    SDL_LockAudioDevice(s->dev);
    size_t n = s->ring.size() - s->ringPos;
    SDL_UnlockAudioDevice(s->dev);
    return (double)n / (double)(s->channels * s->rate);
}

void appendAudio(BinkState *s, AVFrame *f)
{
    int ch = s->channels, n = f->nb_samples;
    std::vector<float> tmp((size_t)n * ch);
    if (av_sample_fmt_is_planar((AVSampleFormat)f->format)) {
        for (int c = 0; c < ch; c++) {
            const float *src = (const float *)f->extended_data[c];
            for (int i = 0; i < n; i++) tmp[(size_t)i * ch + c] = src[i];
        }
    } else {
        memcpy(&tmp[0], f->data[0], tmp.size() * sizeof(float));
    }
    SDL_LockAudioDevice(s->dev);
    s->ring.insert(s->ring.end(), tmp.begin(), tmp.end());
    SDL_UnlockAudioDevice(s->dev);
}

// Reads the next packet: audio goes to the decoder and the ring, video to the queue.
bool pump(BinkState *s)
{
    if (s->eof) return false;
    int r = av_read_frame(s->fmt, s->pkt);
    if (r < 0) { s->eof = true; return false; }
    if (s->pkt->stream_index == s->vstream) {
        s->vqueue.push_back(av_packet_clone(s->pkt));
    } else if (s->pkt->stream_index == s->astream && s->adec && s->dev) {
        if (avcodec_send_packet(s->adec, s->pkt) >= 0)
            while (avcodec_receive_frame(s->adec, s->aframe) >= 0) appendAudio(s, s->aframe);
    }
    av_packet_unref(s->pkt);
    return true;
}

void fillAudio(BinkState *s, double seconds)
{
    while (!s->eof && (s->vqueue.empty() || (s->dev && ringSeconds(s) < seconds))) pump(s);
}

double clockMs(BinkState *s)
{
    if (s->dev) {
        SDL_LockAudioDevice(s->dev);
        uint64_t p = s->played;
        SDL_UnlockAudioDevice(s->dev);
        return 1000.0 * (double)p / (double)s->rate;
    }
    return s->paused ? (double)(s->pauseTicks - s->startTicks) : (double)(SDL_GetTicks() - s->startTicks);
}

void startClock(BinkState *s)
{
    if (s->started) return;
    s->started = true;
    fillAudio(s, 0.5);
    s->startTicks = SDL_GetTicks();
    if (s->dev && !s->paused) SDL_PauseAudioDevice(s->dev, 0);
}

std::string fixPath(const char *name)
{
    std::string p(name);
    for (size_t i = 0; i < p.size(); i++) if (p[i] == '\\') p[i] = '/';   // port: the game uses Windows paths
    return p;
}

void openAudio(BinkState *s, const AVCodec *acodec, AVStream *as)
{
    s->adec = avcodec_alloc_context3(acodec);
    avcodec_parameters_to_context(s->adec, as->codecpar);
    if (avcodec_open2(s->adec, acodec, NULL) < 0) { avcodec_free_context(&s->adec); return; }
    s->channels = s->adec->ch_layout.nb_channels;
    s->rate = s->adec->sample_rate;
    if (s->channels < 1 || s->channels > 2 || s->rate <= 0) { avcodec_free_context(&s->adec); return; }
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = s->rate; want.format = AUDIO_F32SYS; want.channels = (Uint8)s->channels;
    want.samples = 1024; want.callback = audioCallback; want.userdata = s;
    s->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);     // SDL converts to what the device wants
    // port: Bink opened DirectSound through BinkSetSoundSystem; we own a small SDL device per movie.
}

} // namespace

extern "C" {

// 0x40c2a0 hands the OMT sound manager's DirectSound pointer here; ours has no use for it.
int BinkOpenDirectSound(uintptr_t param) { (void)param; return 1; }
int BinkSetSoundSystem(BINKSNDSYSOPEN open, uintptr_t param) { g_soundSet = true; return open ? open(param) : 0; }
void BinkPortSetSurfaceType(int type) { g_surfaceType = type; }

int BinkDDSurfaceType(void *surface)
{
    // port: no DirectDraw. A NULL surface fails (-1) like an unsupported one; sb2.exe treats -1 and 0 as
    // "Invalid Bink Surface".
    return surface ? g_surfaceType : -1;
}

HBINK BinkOpen(const char *name, uint32_t flags)
{
    (void)flags;                                   // sb2.exe passes 0
    std::string path = fixPath(name);
    AVFormatContext *fmt = NULL;
    if (avformat_open_input(&fmt, path.c_str(), NULL, NULL) < 0) return NULL;
    if (avformat_find_stream_info(fmt, NULL) < 0) { avformat_close_input(&fmt); return NULL; }

    BinkState *s = new BinkState();
    s->fmt = fmt; s->vstream = s->astream = -1;
    for (unsigned i = 0; i < fmt->nb_streams; i++) {
        AVMediaType t = fmt->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_VIDEO && s->vstream < 0) s->vstream = (int)i;
        if (t == AVMEDIA_TYPE_AUDIO && s->astream < 0) s->astream = (int)i;    // track 0
    }
    const AVCodec *vc = s->vstream >= 0 ? avcodec_find_decoder(fmt->streams[s->vstream]->codecpar->codec_id) : NULL;
    if (!vc) { avformat_close_input(&fmt); delete s; return NULL; }
    s->vdec = avcodec_alloc_context3(vc);
    avcodec_parameters_to_context(s->vdec, fmt->streams[s->vstream]->codecpar);
    if (avcodec_open2(s->vdec, vc, NULL) < 0) { avcodec_free_context(&s->vdec); avformat_close_input(&fmt); delete s; return NULL; }
    if (s->astream >= 0) {
        const AVCodec *ac = avcodec_find_decoder(fmt->streams[s->astream]->codecpar->codec_id);
        if (ac) openAudio(s, ac, fmt->streams[s->astream]);
    }
    s->frame = av_frame_alloc(); s->aframe = av_frame_alloc(); s->pkt = av_packet_alloc();

    HBINK b = (HBINK)calloc(1, sizeof(BINK));
    AVStream *vs = fmt->streams[s->vstream];
    AVRational fr = vs->avg_frame_rate.num ? vs->avg_frame_rate : vs->r_frame_rate;
    b->Width = (uint32_t)s->vdec->width; b->Height = (uint32_t)s->vdec->height;
    b->Frames = (uint32_t)(vs->nb_frames ? vs->nb_frames : vs->duration);   // Bink demuxer: frame count is the duration, in frames
    b->FrameNum = 1; b->LastFrameNum = 0;
    b->FrameRate = (uint32_t)fr.num; b->FrameRateDiv = (uint32_t)fr.den;
    b->priv = s;
    return b;
}

void BinkClose(HBINK b)
{
    if (!b) return;
    BinkState *s = (BinkState *)b->priv;
    if (s->dev) SDL_CloseAudioDevice(s->dev);       // joins the callback thread first
    for (size_t i = 0; i < s->vqueue.size(); i++) av_packet_free(&s->vqueue[i]);
    av_frame_free(&s->frame); av_frame_free(&s->aframe); av_packet_free(&s->pkt);
    avcodec_free_context(&s->vdec); avcodec_free_context(&s->adec);
    avformat_close_input(&s->fmt);
    delete s; free(b);
}

int BinkWait(HBINK b)
{
    BinkState *s = (BinkState *)b->priv;
    startClock(s);
    if (b->FrameNum > b->Frames) return 0;
    double due = 1000.0 * (double)(b->FrameNum - 1) * (double)b->FrameRateDiv / (double)b->FrameRate;
    return clockMs(s) < due ? 1 : 0;
}

int BinkDoFrame(HBINK b)
{
    BinkState *s = (BinkState *)b->priv;
    startClock(s);
    fillAudio(s, 0.5);                                // audio runs ahead of the picture
    if (s->vqueue.empty()) return 0;                  // end of file: the last picture stays
    AVPacket *p = s->vqueue.front();
    s->vqueue.erase(s->vqueue.begin());
    if (avcodec_send_packet(s->vdec, p) >= 0 && avcodec_receive_frame(s->vdec, s->frame) >= 0) s->haveFrame = true;
    else b->ReadError = 1;
    av_packet_free(&p);
    b->LastFrameNum = b->FrameNum;
    return 0;
}

void BinkNextFrame(HBINK b)
{
    if (b->FrameNum < b->Frames + 1) b->FrameNum++;
}

void BinkPause(HBINK b, int pause)
{
    BinkState *s = (BinkState *)b->priv;
    if ((pause != 0) == s->paused) return;
    s->paused = pause != 0;
    if (s->paused) s->pauseTicks = SDL_GetTicks();
    else s->startTicks += SDL_GetTicks() - s->pauseTicks;
    if (s->dev && s->started) SDL_PauseAudioDevice(s->dev, s->paused ? 1 : 0);
}

static inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void packPixel(uint8_t *d, int type, int r, int g, int bl, int a)
{
    switch (type) {
    case BINKSURFACE24:   d[0] = (uint8_t)bl; d[1] = (uint8_t)g; d[2] = (uint8_t)r; break;
    case BINKSURFACE24R:  d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)bl; break;
    case BINKSURFACE32:   d[0] = (uint8_t)bl; d[1] = (uint8_t)g; d[2] = (uint8_t)r; d[3] = 0; break;
    case BINKSURFACE32R:  d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)bl; d[3] = 0; break;
    case BINKSURFACE32A:  d[0] = (uint8_t)bl; d[1] = (uint8_t)g; d[2] = (uint8_t)r; d[3] = (uint8_t)a; break;
    case BINKSURFACE32RA: d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)bl; d[3] = (uint8_t)a; break;
    default: {
        unsigned v = 0;
        switch (type) {
        case BINKSURFACE4444: v = ((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (bl >> 4); break;
        case BINKSURFACE5551: v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((bl >> 3) << 1) | (a >> 7); break;
        case BINKSURFACE555:  v = ((r >> 3) << 10) | ((g >> 3) << 5) | (bl >> 3); break;
        case BINKSURFACE565:  v = ((r >> 3) << 11) | ((g >> 2) << 5) | (bl >> 3); break;
        case BINKSURFACE655:  v = ((r >> 2) << 10) | ((g >> 3) << 5) | (bl >> 3); break;
        case BINKSURFACE664:  v = ((r >> 2) << 10) | ((g >> 2) << 4) | (bl >> 4); break;
        }
        d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8);
    }
    }
}

static int bytesPerPixel(int type)
{
    switch (type) {
    case BINKSURFACE24: case BINKSURFACE24R: return 3;
    case BINKSURFACE32: case BINKSURFACE32R: case BINKSURFACE32A: case BINKSURFACE32RA: return 4;
    case BINKSURFACE4444: case BINKSURFACE5551: case BINKSURFACE555: case BINKSURFACE565:
    case BINKSURFACE655: case BINKSURFACE664: return 2;
    }
    return 0;                                         // 8P (palettised) isn't used by the game
}

// Converts the current picture (YUV 4:2:0, BT.601, limited range unless the decoder says full) into dest.
int BinkCopyToBuffer(HBINK b, void *dest, int destpitch, uint32_t destheight,
                     uint32_t destx, uint32_t desty, uint32_t flags)
{
    BinkState *s = (BinkState *)b->priv;
    int type = (int)(flags & 0xffff), bpp = bytesPerPixel(type);
    if (!dest || !bpp || !s->haveFrame) return 0;
    const AVFrame *f = s->frame;
    bool full = f->color_range == AVCOL_RANGE_JPEG;
    bool gray = f->format == AV_PIX_FMT_GRAY8;
    bool alpha = f->format == AV_PIX_FMT_YUVA420P;
    int w = f->width, h = f->height;
    for (int y = 0; y < h; y++) {
        uint32_t dy = desty + (uint32_t)y;
        if (dy >= destheight) break;
        uint8_t *row = (uint8_t *)dest + (long)dy * destpitch + (long)destx * bpp;
        const uint8_t *Y = f->data[0] + (long)y * f->linesize[0];
        const uint8_t *U = gray ? NULL : f->data[1] + (long)(y >> 1) * f->linesize[1];
        const uint8_t *V = gray ? NULL : f->data[2] + (long)(y >> 1) * f->linesize[2];
        const uint8_t *A = alpha ? f->data[3] + (long)y * f->linesize[3] : NULL;
        for (int x = 0; x < w; x++, row += bpp) {
            int yy = Y[x], r, g, bl;
            if (gray) {
                r = g = bl = full ? yy : clamp8((yy - 16) * 298 >> 8);
            } else {
                int u = U[x >> 1] - 128, v = V[x >> 1] - 128;
                if (full) {
                    r = clamp8(yy + ((359 * v) >> 8));
                    g = clamp8(yy - ((88 * u + 183 * v) >> 8));
                    bl = clamp8(yy + ((454 * u) >> 8));
                } else {
                    // Bink's own tables and rounding (colour tables 0x10046880 and blitter 0x1004a7e0 in granny.dll,
                    // the same Bink code; see game/granny/granny_bink.cpp): Y' = sat(Y-16)*4*0x4a85 >> 16
                    int ys = yy - 16 < 0 ? 0 : yy - 16;
                    int y2 = ((ys << 2) * 0x4a85) >> 16;
                    r = clamp8(y2 + (((v) * 0xcc4b) / 32768));
                    g = clamp8(y2 + (int16_t)((((-u) * 0x3225) / 32768) + (((-v) * 0x680f) / 32768)));
                    bl = clamp8(y2 + (((u) * 0x10235) / 32768));
                }
            }
            packPixel(row, type, r, g, bl, A ? A[x] : 255);
        }
    }
    return 0;
}

} // extern "C"
