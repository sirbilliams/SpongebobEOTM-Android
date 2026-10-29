// port: the Asset Viewer (not in the original game): a model browser and a scene explorer. See docs/asset-viewer.md.
#pragma once
class OMediaDoubleCanvas;
class OMediaEventManager;
class GameFont;

namespace sbport {
// spec = the value of SB_ASSET_VIEW: "models", "scene" or "scene:<name>". Runs until the player leaves (Back, Esc, B, Android
// Back) or the window closes; the caller then ends the application (the Android launcher is back on top).
void AssetViewRun(const char *spec, OMediaDoubleCanvas *screen, OMediaEventManager *events, GameFont *font);
}
