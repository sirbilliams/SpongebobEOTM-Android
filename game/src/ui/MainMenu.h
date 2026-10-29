#ifndef SB_MAIN_MENU_H
#define SB_MAIN_MENU_H
#include "OMediaListener.h"
#include "OMediaElement.h"
#include "OMediaCCaption.h"
#include "GameFont.h"
#include <vector>
#include <string>
class OMedia2DWorld;
class OMediaDataBase;
class MenuCaption;
class OMediaCanvasElement;
class MenuHost {
public:
    virtual ~MenuHost() {}
    virtual bool menu_movie(const char *name)=0;
    virtual bool menu_new_game()=0;
    virtual void menu_settings_changed()=0;
    virtual void menu_preview(int kind) { (void)kind; }          // 0x4256b0: a sample of the Sound (kind 2) / Voice (kind 3) volume just set
    int music, sound, voice, save, text, shadows;
    // ---- save games (lane b, game/src/game/SaveGame.cpp); defaults: no game behind the menu ----
    virtual bool menu_game_active() { return false; }               // a game is paused behind the menu: Save Game works, Resume returns to it
    virtual bool menu_can_resume() { return false; }                // 0x4248b0: Resume Game is enabled (game in progress, or a last-used slot)
    virtual void menu_resume() {}                                   // 0x4248b0 / 0x41c1c0: leave the menu, back to the game
    virtual bool menu_save(int slot, const char *name) { (void)slot; (void)name; return false; }   // 0x428740 -> 0x41db50
    virtual bool menu_load(int slot) { (void)slot; return false; }                                 // 0x429120 (game vt+0xd8)
};
class MainMenu : public OMediaListener {
public:
    MainMenu(OMedia2DWorld *world, OMediaDataBase *art, GameFont *font,
             const RetailData &retail, MenuHost *host);
    virtual ~MainMenu();
    virtual void listen_to_message(omt_Message msg, void *param);
    void hover(int x,int y);
    void back();
    bool atTop() const { return page==-1 && confirm==-1; }
    void key(int sym);                    // (also the Help / Options keys: 0x425aa0 key handler)
    void padButton(int button);           // port: SDL controller button (d-pad / A / B) -> keys                    // SDL keycode: the Save / Load Game screens (0x425aa0 key handler)
    void text(const char *utf8);          // SDL_TEXTINPUT: the name typed on the Save Game screen
    bool quit;
private:
    struct Button { int id,x,y,w,h; OMediaCCaption *caption; };
    OMedia2DWorld *world;
    OMediaDataBase *art;
    GameFont *font;
    const RetailData &data;
    MenuHost *host;
    std::vector<OMediaElement*> elements;
    std::vector<OMediaCanvas*> controller_art;
    std::vector<Button> buttons;
    int page, confirm, selected, help_page, focus;   // focus: the Options row of 0x425aa0 (+8): 0 Music 1 Sound 2 Voice 3 Text 4 Shadows 5 Ok
    void adjust(int dir);
    // save / load screens (0x427ef0 / 0x428bb0 / 0x4295d0): +0xa0 slot, +0x80 name
    int slot;                                  // -1: none
    std::string slotName;
    std::vector<unsigned char> slotThumb;
    std::vector<OMediaCanvas*> owned;          // thumbnail canvases of this page
    void selectSlot(int s);
    void saveAction();                         // 0x428740
    void loadAction();                         // 0x429120
    void stepSlot(int dir);                    // 0x4291b0 / 0x4293c0
    void rebuild();
    void clear();
    OMediaCanvasElement *image(const char *name,int x,int y,int z);
    MenuCaption *caption(const string &s,int x,int y,int id);
    MenuCaption *center(const string &s,int y,int id);
    void arrow(bool left,int y,int id);
    void activate(int id);
    bool hit(const Button &b,int x,int y);
};
#endif
