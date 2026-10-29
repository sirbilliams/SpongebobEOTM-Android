# Startup reconstruction

Retail addresses refer to sb2.exe (image base 0x400000). Engine addresses below are cross-referenced with private decomp/omt/exports.json. No game assets are embedded.

## Call chain

* WinMain/platform entry -> 0x40e580 (game entry / omt_main): CD check 0x40e470, single-instance guard, allocate app, sound check, application start, destruction. WinMain is 0x42a540 (mislabelled main by the decompiler: four WinMain arguments, stored into OMediaWinStoreStartInfo). It derives the install working directory from the module path and calls 0x40e580. SDLMain performs that data-root mapping using OMT_DATA / Android imported files.
* 0x4097d0 app constructor: Application(2,true), Listener, Periodical; defaults; register Canv/3DMa/Wave builders; cache=0; hide cursor; InitDisplay 0x40cc10; DirectX::install(7); sound-manager check.
* Initialization order: InitWorld 0x40d010 -> menu/inventory controllers 0x40d120 -> event/window controller 0x40cdb0 -> font 0x40d0b0 -> SetVideoMode(640,480,16,-sw) 0x40e2c0 -> settings 0x409ed0 -> Granny initialization 0x40d2d0 -> Bink sound 0x40c2a0 -> scene dispatch 0x40a230("mlegal") -> click dispatcher.
* 0x40a230 named dispatch -> movie switch 0x40bbf0 -> movie ctor 0x429af0 -> open 0x429b60 (movies/%s.bik, CD fallback). 0x429b40 closes any active movie.
* Periodical 0x40da40 -> 0x429c80: Wait, DoFrame, present 0x429ce0, compare FrameNum==Frames, Close or NextFrame. On completion -> 0x40bcc0.
* Startup completion transitions: mlegal -> mthq -> mawe -> mnick -> mspongebob -> slot0 probe 0x40e160 (app virtual +0x104), then mintro or the alternate c1l2s6p1 branch. The menu controller is 0x41bbb0, which lazily opens sb2.omt and invokes 0x41f6a0 (menu/menut canvases); it is not a named mainmenu scene. The current port deliberately stops at that menu handoff after the requested five-logo chain; save/intro/scene continuation is TODO, not claimed implemented.

## Port and lane A integration

The SDL application/window bridge currently uses OMTClasses Application/FullscreenWindow/MonitorMap/World/ViewPort/Layer to establish the existing GL context. This is a temporary port adapter, not an ABI cast between OMT 2.5 and 1.x worlds. Bink is game/bink/bink.cpp, FFmpeg-backed, with engine/sdl/OMediaSDLSound.cpp mixer callbacks. DirectDraw surface probing/copy is replaced by a GL RGBA texture.

Lane A: provide a complete application-compatible OMT1 Display + DoubleCanvas + screen-device lifecycle, world/viewport attach and movie presentation path, plus CanvasElement UI creation for 0x41f6a0, CCaption/CStringField font rendering and WorldClickDispatcher menu event routing. Headers for several of these already exist in this snapshot; their integration and virtual behavior are not yet verified by this lane. No engine/omt1 files are edited.

Settings replace HKCU Software/THQ/SpongeBob 2/Settings with a text file under SDL_GetPrefPath (override OMT_CONFIG). Music/Sound/Voice default 7, Save -1, Text/Shadows 1. Software mode suppresses shadows. CD/data root is the imported install folder. Win32 single-instance mapping is omitted for the SDL port. CrashLog remains installed.

## Direct OMT imports in the startup slice

Includes initialization/destruction, named dispatch and menu construction imports. Virtual calls require the class contracts above; this table is the named-import cross-reference, not a claim that every branch is implemented.

| OMT export | DLL address | sb2 callers | source coverage |
|---|---|---|---|
| `??0OMedia2DWorld@@QAE@XZ` | 0x10004380 | 0x442010 | engine/omt1/World/OMedia2DWorld.h |
| `?db_builder@OMedia3DMaterial@@SAPAVOMediaDBObject@@XZ` | 0x1001ce20 | 0x4097d0 | engine/OMTClasses/Graphics/3D/OMedia3DMaterial.h |
| `??0OMediaApplication@@QAE@W4omt_InitAppHints@@_N@Z` | 0x10037270 | 0x4097d0 | engine/OMTClasses/Application/OMediaApplication.h |
| `??1OMediaApplication@@UAE@XZ` | 0x10037320 | 0x409ca0 | engine/OMTClasses/Application/OMediaApplication.h |
| `??0OMediaBroadcaster@@QAE@XZ` | 0x1003b080 | 0x442010 | engine/OMTClasses/Messages/OMediaBroadcaster.h |
| `?db_builder@OMediaCanvas@@SAPAVOMediaDBObject@@XZ` | 0x1003b610 | 0x4097d0 | engine/omt1/Graphics/OMediaCanvas.h, engine/OMTClasses/Graphics/Canvas/OMediaCanvas.h |
| `??0OMediaCanvasElement@@QAE@XZ` | 0x1003cea0 | 0x41f6a0 | engine/omt1/World/OMediaCanvasElement.h, engine/OMTClasses/World/Element/OMediaCanvasElement.h |
| `??0OMediaDXSurfaceWorld@@QAE@_N@Z` | 0x1005cd70 | 0x429b60 | engine/omt1/Display/OMediaDXSurfaceWorld.h |
| `??0OMediaDataBase@@QAE@PAVOMediaStream@@@Z` | 0x10053b80 | 0x40d340 | engine/OMTClasses/Stream/Database/OMediaDataBase.h |
| `?register_object@OMediaDataBase@@SAXKP6APAVOMediaDBObject@@XZJ@Z` | 0x10054570 | 0x4097d0 | engine/OMTClasses/Stream/Database/OMediaDataBase.h |
| `?set_cache_size@OMediaDataBase@@SAXK@Z` | 0x10054d30 | 0x4097d0 | engine/OMTClasses/Stream/Database/OMediaDataBase.h |
| `??0OMediaDisplay@@QAE@PAVOMediaSupervisor@@PAVOMediaCanvas@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z` | 0x1038f8a0 | 0x40cc10 | engine/omt1/Display/OMediaDisplay.h |
| `??0OMediaDoubleCanvas@@QAE@XZ` | 0x10056840 | 0x40cc10 | engine/omt1/Graphics/OMediaCanvas.h |
| `?event_manager@OMediaEventManager@@1PAV1@A` | 0x10420d84 | 0x40cdb0 | engine/OMTClasses/Input/OMediaEventManager.h |
| `??0OMediaFilePath@@QAE@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@KK@Z` | 0x103913f0 | 0x40d340, 0x40e160 | engine/OMTClasses/Stream/Core/OMediaFilePath.h |
| `??1OMediaFilePath@@UAE@XZ` | 0x103914b0 | 0x40d340, 0x40e160 | engine/OMTClasses/Stream/Core/OMediaFilePath.h |
| `??0OMediaFileStream@@QAE@XZ` | 0x10391d30 | 0x40d340, 0x40e160 | engine/OMTClasses/Stream/Core/OMediaFileStream.h |
| `??1OMediaFileStream@@UAE@XZ` | 0x10391db0 | 0x40e160 | engine/OMTClasses/Stream/Core/OMediaFileStream.h |
| `?setpath@OMediaFileStream@@UAEXPBVOMediaFilePath@@@Z` | 0x10392050 | 0x40e160 | engine/OMTClasses/Stream/Core/OMediaFileStream.h |
| `?input_manager@OMediaInputManager@@1V1@A` | 0x104211f8 | 0x41a850, 0x41bbb0 | engine/omt1/OMediaInputManager.h |
| `??0OMediaListener@@QAE@XZ` | 0x1006ae50 | 0x4097d0, 0x41a850 | engine/OMTClasses/Messages/OMediaListener.h |
| `??1OMediaListener@@UAE@XZ` | 0x1006aeb0 | 0x409ca0 | engine/OMTClasses/Messages/OMediaListener.h |
| `??0OMediaPeriodical@@QAE@XZ` | 0x10082ee0 | 0x4097d0 | engine/OMTClasses/Core/OMediaPeriodical.h |
| `??1OMediaPeriodical@@UAE@XZ` | 0x10082ff0 | 0x409ca0 | engine/OMTClasses/Core/OMediaPeriodical.h |
| `?getdefaultdevice@OMediaScreenDevice@@SAPAV1@XZ` | 0x103981f0 | 0x40e2c0 | engine/omt1/Display/OMediaDisplay.h |
| `?db_builder@OMediaSound@@SAPAVOMediaDBObject@@XZ` | 0x10086d00 | 0x4097d0 | engine/OMTClasses/Sound/OMediaSound.h |
| `?getdefaultmanager@OMediaSoundManager@@SAPAV1@XZ` | 0x10086ee0 | 0x4097d0, 0x40c2a0, 0x40e580 | engine/omt1/OMediaSoundManager.h |
| `??0OMediaViewPort@@QAE@PAVOMediaWorld@@@Z` | 0x1038ec00 | 0x40d1d0 | engine/omt1/World/OMediaViewPort.h, engine/OMTClasses/World/Element/OMediaViewPort.h |
| `?get_world_position_info@OMediaWorld@@QAEXJJAAVOMediaWorldPositionInfo@@@Z` | 0x1039a900 | 0x41bbb0 | engine/omt1/World/OMediaWorld.h, engine/OMTClasses/World/Core/OMediaWorld.h |
| `??0OMediaWorldClickDispatcher@@QAE@XZ` | 0x1039aa30 | 0x4097d0 | **missing** |
| `?omt_GetMilliSecsCounter@@YAKXZ` | 0x10395370 | 0x4097d0, 0x40da40, 0x41bbb0 | platform function, SDL_GetTicks clock adapter |
| `?build@OMedia3DRasterizerDesc@@QAEPAVOMedia3DAbstractRasterizer@@PAVOMediaGfxWorld@@K@Z` | 0x1001f380 | 0x40ceb0 | engine/omt1/3D/OMedia3DRasterizerDesc.h |
| `?commandLine@OMediaWinStoreStartInfo@@2PADA` | 0x10421254 | 0x4097d0, 0x42a540 | engine/OMTClasses/OS/MSWindows/Application/OMediaWinStoreStartInfo.h |
| `?prevInstance@OMediaWinStoreStartInfo@@2PAUHINSTANCE__@@A` | 0x1042124c | 0x42a540 | engine/OMTClasses/OS/MSWindows/Application/OMediaWinStoreStartInfo.h |
| `?showState@OMediaWinStoreStartInfo@@2HA` | 0x10421250 | 0x42a540 | engine/OMTClasses/OS/MSWindows/Application/OMediaWinStoreStartInfo.h |
| `?startinfo_stored@OMediaWinStoreStartInfo@@2_NA` | 0x10421248 | 0x42a540 | engine/OMTClasses/OS/MSWindows/Application/OMediaWinStoreStartInfo.h |
| `?theInstance@OMediaWinStoreStartInfo@@2PAUHINSTANCE__@@A` | 0x10421258 | 0x42a540 | engine/OMTClasses/OS/MSWindows/Application/OMediaWinStoreStartInfo.h |
| `?install@OMediaDirectX@@SAXG@Z` | 0x10056480 | 0x4097d0 | engine/omt1/Display/OMediaDisplay.h |
| `?hide@OMediaMouseCursor@@SAXXZ` | 0x103950d0 | 0x4097d0 | engine/OMTClasses/Core/OMediaMouseCursor.h |
| `?show@OMediaMouseCursor@@SAXXZ` | 0x103950f0 | 0x40e580 | engine/OMTClasses/Core/OMediaMouseCursor.h |
| `?msg_broadcaster@OMediaWinDefaultMsgHandler@@1VOMediaBroadcaster@@A` | 0x10420d60 | 0x40cdb0 | engine/omt1/OMediaWinDefaultMsgHandler.h |

Coverage means declaration exists; it does not certify all OMT 2.5 semantics. Port adapters and unresolved game-controller construction must remain explicit TODO logs until connected.

## Virtual engine calls and remaining contracts

These are the named virtual contracts used by startup (including backend overrides), cross-referenced against the same exports catalog. Their call sites are 0x40e580 (start), 0x40d010 (framerate), 0x40e2c0 (setmode), 0x40d1d0 (viewport), 0x40cd10 (clear/page flip), 0x40c2a0 (channels), and 0x429b60/0x429ce0 (surface creation and pixel lock/copy). Shared base no-op methods are not substitutes for the concrete display surface implementation.

| Method | DLL address |
|---|---|
| `OMediaApplication::start` | 0x10037390 |
| `OMediaWorld::set_framerate_mode` | 0x1039a0a0 |
| `OMediaScreenDevice::setmode` | 0x103987d0 |
| `OMediaDXScreenDevice::setmode` | 0x1005ac30 |
| `OMediaViewPort::setbounds` | 0x1038ee70 |
| `OMediaViewPort::setcanvas` | 0x1038f190 |
| `OMediaDoubleCanvas::getdefaultdrawgfxworld` | 0x10057080 |
| `OMediaGfxWorld::create` | 0x10066b10 |
| `OMediaGfxWorld::fill` | 0x10067c80 |
| `OMediaGfxWorld::getheight` | 0x10392460 |
| `OMediaGfxWorld::getpixmaplowleveldata` | 0x103924a0 |
| `OMediaGfxWorld::getwidth` | 0x10392440 |
| `OMediaGfxWorld::page_flip` | 0x1005a390 |
| `OMediaGfxWorld::pixlock` | 0x10066be0 |
| `OMediaGfxWorld::pixunlock` | 0x10066c00 |
| `OMediaGfxWorld::support_page_flipping` | 0x10069a50 |
| `OMediaWindowPortWorld::fill` | 0x10399a60 |
| `OMediaWindowPortWorld::getheight` | 0x103999a0 |
| `OMediaWindowPortWorld::getpixmaplowleveldata` | 0x10399a10 |
| `OMediaWindowPortWorld::getwidth` | 0x10399940 |
| `OMediaWindowPortWorld::pixlock` | 0x10399730 |
| `OMediaWindowPortWorld::pixunlock` | 0x103997b0 |
| `OMediaSoundManager::get_max_channels` | 0x100017f0 |
| `OMediaSoundManager::set_nchannels` | 0x10086f10 |
| `OMediaDXSoundManager::get_max_channels` | 0x1005cc70 |

Rasterizer selection 0x40ceb0 also builds OMedia3DAbstractRasterizer and invokes virtual +0x14/+0x18 with 3. The class is still missing in this snapshot (only forward declarations exist). This blocks faithful software/accelerated scene rendering, though SDL GL movie presentation works. OMediaGfxWorld::support_page_flipping/page_flip and the concrete pixel-surface overrides are needed by 0x40cd10/0x429ce0. Database lookup, object linking/unlinking, canvas assignment and font draw calls in the menu are additional integration work, not implemented UI.

0x40d2d0 / 0x40d330 initialize/close Granny using the existing compatibility library and enable texture format 0xb. Database acquisition is 0x40d340, invoked lazily by 0x41bbb0, rather than 0x40d2d0.

The milestone currently implements movie startup and logs the menu handoff. It does not claim the retail saved-game probe, intro continuation, or interactive menu widgets are complete. Any key, click, touch or controller button skips a logo in the SDL adapter; quit closes the application. This input mapping is a port choice pending original event-controller integration.

## Verification

- RelWithDebInfo and Debug ASan/UBSan `spongebob` builds pass (C++98). The shell is already inside the Ubuntu build container; distrobox entry was unnecessary.
- Pinned FFmpeg sources are absent and network DNS is unavailable. Verification used an installed FFmpeg 7 SDK through a temporary `/tmp/c-ffmpeg` prefix (FFmpeg headers and libraries only, no changes committed). Configure with `cmake -S engine -B build -DFFMPEG_PREFIX=/tmp/c-ffmpeg -DCMAKE_BUILD_TYPE=RelWithDebInfo`, then `make -C build -j6 spongebob`.
- Full desktop run on display :83, 640x480, dummy audio: all 912 movie frames complete in legal/THQ/AWE/Nick/SpongeBob order and `0x41bbb0: main menu path reached` appears before the frame-930 quit. Test log `/tmp/c-final-run.log`.
- Full final implementation ASan/UBSan run, frame 930: exit 0, no address/undefined-behavior diagnostics (`/tmp/c-verified-asan.log`). Options: `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`. LeakSanitizer itself cannot execute in this traced sandbox (fatal ptrace diagnostic); leak freedom is not certified.
- That sanitizer run used SDL disk audio to verify actual mixed samples: 6,705,152 signed 16-bit samples, 6,599,823 nonzero, peak magnitude 32768, `/tmp/c-startup-audio.raw`. Dummy audio was verified separately on the full desktop run.
- Custom `OMT_CONFIG` reads Music=3/Sound=4/Voice=5/Save=2/Text=0; `-sw` changes Shadows=1 to 0. Safe mode is a request for the missing retail software rasterizer; movie output still uses the SDL GL adapter.
- Captured PNGs at global frames 50, 150, 260, 360, 600 in `/tmp/c-shots/` (legal, THQ, AWE, Nick, SpongeBob). Inspected THQ/AWE/Nick and an animated SpongeBob frame. No images or audio are added to the repository. The requested `~/Projects/sb-lanes/c-shots/` is read-only in this sandbox; `mkdir` failed, so the images remain under `/tmp`.
- Git staging/commit is blocked: `/var/home/bill/Projects/SpongebobEOTM-Android/.git/worktrees/c/index.lock` is on a read-only filesystem. Branch remains lane/c. `/tmp/c-startup.patch` contains the changes for application/commit outside this sandbox.

Reproduce the smoke run with `DISPLAY=:83 SDL_AUDIODRIVER=dummy LD_LIBRARY_PATH=/tmp/c-ffmpeg/lib OMT_DATA="$PWD/.game" OMT_WINDOW_SIZE=640x480 OMT_FRAMES=930 OMT_SHOTS=50,150,260,360,600 OMT_SCREENSHOT=/tmp/c-shots/startup.bmp ./build/spongebob`. The additional library path is specific to the temporary non-minimal SDK used here; the pinned minimal FFmpeg build needs only its normal prefix.
