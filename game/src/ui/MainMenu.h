#ifndef SB_MAIN_MENU_H
#define SB_MAIN_MENU_H
#include "OMediaListener.h"
#include "OMediaElement.h"
#include "OMediaCCaption.h"
#include "GameFont.h"
#include <vector>
class OMedia2DWorld;
class OMediaDataBase;
class OMediaCanvasElement;
class MenuHost {
public:
    virtual ~MenuHost() {}
    virtual bool menu_movie(const char *name)=0;
    virtual bool menu_new_game()=0;
    virtual void menu_settings_changed()=0;
    int music, sound, voice, save, text, shadows;
};
class MainMenu : public OMediaListener {
public:
    MainMenu(OMedia2DWorld *world, OMediaDataBase *art, GameFont *font,
             const RetailData &retail, MenuHost *host);
    virtual ~MainMenu();
    virtual void listen_to_message(omt_Message msg, void *param);
    void hover(int x,int y);
    void back();
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
    int page, confirm, selected, help_page;
    void rebuild();
    void clear();
    OMediaCanvasElement *image(const char *name,int x,int y,int z);
    void caption(const string &s,int x,int y,int id);
    void center(const string &s,int y,int id);
    void arrow(bool left,int y,int id);
    void activate(int id);
    bool hit(const Button &b,int x,int y);
};
#endif
