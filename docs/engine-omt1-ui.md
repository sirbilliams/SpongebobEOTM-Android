# OMT 1.x UI layer (engine/omt1), lane a

Rebuilt from OMT.dll (addresses in the headers). Test: `engine/tests/omt1_ui_test.cpp`
(`omt1_ui_test <game folder>`; headless, `OMT_SCREENSHOT=x.bmp OMT_FRAMES=n`).

## Classes added in this pass
| class | where | notes |
|---|---|---|
| OMediaUndiv2DElement | World/ | base of the three below; `backdrop` (+0xc4) copies the canvas under it first |
| OMediaVPElement | World/ | `set_viewport(vp)`; draws that viewport into the world, clicks pass on to its world |
| OMediaBoxElement | World/ | `set_size, set_color/light/dark, set_filled, set_3d, set_sunken`; defaults as OMT.dll |
| OMediaCCaption | World/ | `set_font(OMediaTextFont*)`, `set_string`, `compute_size` (public) |
| OMediaCStringField | World/ | text field; the setters mirror the fields sb2.exe pokes at 0x414340 (font +0x134, margin +0x108, `set_transparent_idle` +0x144, `set_backdrop` +0xc4, `set_auto_size` +0x145, `set_max_width` +0x148, `recalc_size()` public, `set_dimensions` +0x104/+0x106, `set_filter(4)` +0x140) |
| OMediaWorldClickDispatcher | World/ | listens to the event manager's mouse messages; `get_pick_broadcaster()` (worlds listen: omsg_MouseTrack) and `get_click_broadcaster()` (game: omsg_ViewPortClicked / _Up with an OMediaWorldClickInfo) |
| OMediaEventManager (1.x) | Input/ | `new OMediaEventManager` sets `event_manager`; feed it SDL events with `handle_sdl_event(&e)` (mouse buttons, SDL_TEXTINPUT, the non-character keys, SDL_QUIT -> `quit_requested()`); `spend_time()` polls SDL itself |
| OMediaDrawPort | Graphics/ | the GDI drawing calls (pen, rects, ovals, lines, 3D rects) on a gfx world; `drawstring` is a TODO log (OMT.dll's menus use QuickText) |
| OMediaQuickText | Graphics/ | font-canvas text (vertical glyph table, optional proportional widths) |
| OMediaTextFont | Graphics/ | the interface CCaption/CStringField call (vt +0x18 draw_full, +0x1c draw, +0x20 get_text_length, height): **sb2.exe has its own font class (vtable 0x44dd78, built on Font.omt's glyph canvases) and passes it as the font; the game port implements OMediaTextFont for it** (see GlyphFont in omt1_ui_test) |

`OMediaWorldClickInfo` (World/OMediaElement.h) grew the dispatcher's fields (0x38 bytes in all).
OMediaWorld's destructor now deletes its elements itself (port: UBSan, they ask the world `is_destroying()`).

## Already there before this pass
OMedia2DWorld/Element/Sorter/Anim/AnimFrame, ViewPort, Canvas/DoubleCanvas, Display, ScreenDevice,
DXSurfaceWorld/DirectX::install (SDL+GL), mouse cursor (omt1/OMediaMouseCursor.cpp), sound manager and
input driver (lane b, built into `omt`, not `omt1`).

## Open
- OMediaWinDefaultMsgHandler / DXRtg classes are not in `omt1`; the SDL events reach the engine only
  through OMediaEventManager::handle_sdl_event.
- OMediaCStringField::isinbounds is unused by sb2.exe (the viewport transform was read from the x87 code).
- UBSan: OMTClasses/Utility/OMediaRLEPacker.cpp:81 loads an int from an odd address (shared core, benign on ARM64/x86).
