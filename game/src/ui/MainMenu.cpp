#include "MainMenu.h"
#include "OMedia2DWorld.h"
#include "OMediaCanvasElement.h"
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include "OMedia2DAnim.h"
#include "OMediaBoxElement.h"
#include "OMediaDefMessages.h"
#include <SDL.h>
#include <stdio.h>
#include <sys/stat.h>

// The retail font draws bank 0 or 1 through pooled glyph elements. port: each caption
// selects its bank at draw time so normal and highlighted strings can share the font.
class MenuCaption : public OMediaCCaption {
public:
    GameFont *gamefont; bool highlighted;
    MenuCaption(GameFont *f) : gamefont(f), highlighted(false) { set_font(f); }
    virtual void draw(OMediaRect *src,long x,long y,OMediaCanvas *dest,OMediaRefreshList *rl) {
        gamefont->set_bank(highlighted?1:0);
        OMediaCCaption::draw(src,x,y,dest,rl);
        gamefont->set_bank(0);
    }
};
MainMenu::MainMenu(OMedia2DWorld *w,OMediaDataBase *a,GameFont *f,const RetailData &d,MenuHost *h)
 : quit(false),world(w),art(a),font(f),data(d),host(h),page(-1),confirm(-1),selected(0),help_page(1) {
    SDL_Log("0x41bbb0 -> 0x41f6a0 -> 0x41c1c0 -> 0x4242b0: sb2.omt menu dialog");
    // 0x41f6a0 creates these toolbar elements for the controller before the dialog.
    // The modal dialog draws its own bamboo4 panel instead of this toolbar.
    const char *names[]={"menu","menut"};
    for(int i=0;i<2;++i)
        if(art->object_exists(OMediaCanvas::db_type,string(names[i])))
            controller_art.push_back((OMediaCanvas*)art->get_object(OMediaCanvas::db_type,string(names[i])));
    rebuild();
}
MainMenu::~MainMenu() {
    clear();
    for(size_t i=0;i<controller_art.size();++i) controller_art[i]->db_unlock();
}
void MainMenu::clear() {
    buttons.clear();
    for(size_t i=0;i<elements.size();++i) delete elements[i];
    elements.clear();
}
OMediaCanvasElement *MainMenu::image(const char *name,int x,int y,int z) {
    OMediaCanvas *c=art->object_exists(OMediaCanvas::db_type,string(name))?
        (OMediaCanvas*)art->get_object(OMediaCanvas::db_type,string(name)):NULL;
    if(!c) { SDL_Log("menu: missing canvas %s",name); return NULL; }
    OMediaCanvasElement *e=new OMediaCanvasElement(c);
    if(x==-1) x=(640-c->getwidth())/2;
    if(y==-1) y=(480-c->getheight())/2;
    e->link(world); e->place((float)x,(float)y,(float)z); e->set_clickable(false);
    elements.push_back(e); c->db_unlock();
    return e;
}
void MainMenu::caption(const string &s,int x,int y,int id) {
    MenuCaption *c=new MenuCaption(font); c->set_string(s);
    c->link(world); c->place((float)x,(float)y,10300);
    // port: rectangular text hit regions match 0x426e00, including blank spaces.
    c->set_clickable(id>=0);
    elements.push_back(c);
    if(id>=0) { Button b; b.id=id; b.x=x; b.y=y; b.w=font->get_text_length(s); b.h=18; b.caption=c; buttons.push_back(b); }
}
void MainMenu::center(const string &s,int y,int id) { caption(s,(640-font->get_text_length(s))/2,y,id); }
void MainMenu::arrow(bool left,int y,int id) {
    OMedia2DAnim *anim=new OMedia2DAnim;
    for(int i=1;i<=8;++i) {
        char name[20]; sprintf(name,left?"arrows%02d":"arrows2%02d",i);
        if(!art->object_exists(OMediaCanvas::db_type,string(name))) continue;
        OMediaCanvas *c=(OMediaCanvas*)art->get_object(OMediaCanvas::db_type,string(name));
        OMedia2DAnimFrame *frame=new OMedia2DAnimFrame;
        frame->setcanvas(c); frame->link(anim,0); frame->setframespeed_tb(15);
        c->db_unlock();
    }
    anim->setcurrentsequence(0); anim->setplay_timebased(true); anim->setplay_loop(true);
    anim->link(world); anim->place(left?140.0f:440.0f,(float)y,10300);
    anim->set_clickable(false); elements.push_back(anim);
    Button b; b.id=id; b.x=left?139:449; b.y=y; b.w=left?51:47; b.h=36; b.caption=NULL; buttons.push_back(b);
}
void MainMenu::rebuild() {
    clear();
    image("bamboo4",-1,-1,10200); // 0x4242b0: centered panel, z 0x461f6000
    if(confirm>=0) {
        string title=confirm==5?"New Game?":"Quit Game?";
        // 0x424e00 quirk: Quit Game? positioned using Exit Game?'s width.
        caption(title,(640-font->get_text_length(confirm==7?"Exit Game?":title))/2,211,-1);
        string yes=data.text(0x4559c8),no=data.text(0x4559c4);
        caption(yes,225+(95-font->get_text_length(yes))/2,251,100);
        caption(no,320+(95-font->get_text_length(no))/2,251,101);
    } else if(page==-1) {
        for(int i=0;i<8;++i) center(data.text(data.word(0x455700+4*i)),91+40*i,i);
        // No scene exists during the startup handoff; retail's script-state query disables resume.
        image("noresume",257,329,10301);
    } else if(page==0) {
        char name[16]; sprintf(name,"help%02d",help_page);
        image(name,-1,-1,10201); // 0x427640
        center("Cancel",380,90);
        if(help_page>1) arrow(true,364,91);
        if(help_page<5) arrow(false,364,92);
    } else if(page==1) {
        for(int i=0;i<5;++i) center(data.text(data.word(0x455750+4*i)),100+55*i,-1);
        int values[3]={host->music,host->sound,host->voice};
        for(int i=0;i<3;++i) {
            image("line",195,128+55*i,10301);
            image("cursor",197+27*values[i],120+55*i,10302);
            for(int j=0;j<10;++j) { Button b; b.id=200+10*i+j; b.x=195+27*j;
                b.y=120+55*i; b.w=27; b.h=17; b.caption=NULL; buttons.push_back(b); }
        }
        string on=data.text(0x455910),off=data.text(0x455914);
        for(int i=0;i<2;++i) {
            caption(on,225+(95-font->get_text_length(on))/2,288+55*i,240+i*2);
            caption(off,320+(95-font->get_text_length(off))/2,288+55*i,241+i*2);
        }
        center(data.text(0x455904),380,90);
    } else if(page==2 || page==3) {
        // 0x427ef0 / 0x428bb0: saves need the scene/VM state; empty imported slots are not saves.
        OMediaBoxElement *box=new OMediaBoxElement;
        box->set_size(322,242); box->set_color(OMediaRGBColor(0,0,0));
        box->set_3d(true); box->set_sunken(true); box->link(world); box->place(159,94,10299);
        box->set_clickable(false); elements.push_back(box);
        if(page==3) center("No Saved Games",311,-1);
        caption(data.text(0x455904),225+(95-font->get_text_length(data.text(0x455904)))/2,380,93);
        caption("Cancel",320+(95-font->get_text_length("Cancel"))/2,380,90);
    } else if(page==4) {
        // 0x424d60: movie availability is driven by VM unlock variables.
        // Initial game permits THQ, SpongeBob and Intro; later unlocks await the VM.
        for(int i=0;i<3;++i) center(data.text(data.word(0x455720+4*i)),91+23*i,300+i);
        for(int i=3;i<11;++i) image("locked",-1,91+23*i,10301);
        center(data.text(data.word(0x455720+4*11)),91+23*11,311);
        center(data.text(0x455904),380,90);
    }
    for(size_t i=0;i<buttons.size();++i)
        if(buttons[i].caption) ((MenuCaption*)buttons[i].caption)->highlighted=buttons[i].id==selected;
}
bool MainMenu::hit(const Button &b,int x,int y) { return b.x<x && x<b.x+b.w && b.y<y && y<b.y+b.h; }
void MainMenu::hover(int x,int y) {
    int id=-1;
    for(size_t i=0;i<buttons.size();++i) if(hit(buttons[i],x,y)) id=buttons[i].id;
    if(id==selected) return;
    selected=id;
    for(size_t i=0;i<buttons.size();++i) {
        MenuCaption *c=(MenuCaption*)buttons[i].caption;
        if(!c) continue;
        c->highlighted=buttons[i].id==id;
        c->set_string(c->get_string());
    }
    if(id>=0) SDL_Log("0x426e00: hover menu item %d",id);
}
void MainMenu::listen_to_message(omt_Message msg,void *param) {
    if(msg!=omsg_ViewPortClicked || !param) return;
    OMediaWorldClickInfo *ci=(OMediaWorldClickInfo*)param;
    if(ci->right_button) return;
    for(size_t i=0;i<buttons.size();++i)
        if(hit(buttons[i],ci->click_x,ci->click_y)) { int id=buttons[i].id; activate(id); return; }
}
void MainMenu::back() { confirm=-1;page=-1;selected=-1;rebuild(); }
void MainMenu::activate(int id) {
    SDL_Log("0x425aa0: click menu item %d -> 0x425580",id);
    if(id==100) {
        SDL_Log("0x4254b0: confirmation Yes (%d)",confirm);
        if(confirm==7) { SDL_Log("0x4254b0 -> 0x40a070: application quit"); quit=true; }
        else { SDL_Log("0x4248b0 -> 0x41c1c0 -> 0x40a090: New Game"); if(!host->menu_new_game()) quit=true; back(); }
        return;
    }
    if(id==101) { SDL_Log("0x425520: confirmation No"); back(); return; }
    if(id==90) { SDL_Log("0x424890: return to main menu"); host->menu_settings_changed(); back(); return; }
    if(id==91 || id==92) { help_page+=id==91?-1:1; rebuild(); return; }
    if(id==93) { SDL_Log("TODO(0x428740): save scene and script VM state; TODO(0x429120): load scene and script VM state"); return; }
    if(id>=200 && id<230) {
        int v=(id-200)%10, i=(id-200)/10;
        if(i==0) host->music=v; else if(i==1) host->sound=v; else host->voice=v;
        SDL_Log("0x4256b0: volume %d=%d",i,v); host->menu_settings_changed(); rebuild(); return;
    }
    if(id>=240 && id<=243) {
        if(id<242) host->text=id==240; else host->shadows=id==242;
        SDL_Log("0x425aa0: option %d",id); host->menu_settings_changed(); rebuild(); return;
    }
    if(id>=300) {
        const char *names[]={"mthq","mspongebob","mintro","mchapter2","mchapter3","mchapter4",
            "mend","mmaking1","mmaking2","mmaking3","mmaking4","mcredits"};
        SDL_Log("0x424bb0: movie %s",names[id-300]);
        if(!host->menu_movie(names[id-300])) quit=true;
        rebuild(); return;
    }
    switch(id) {
    case 0: SDL_Log("0x427640: Help");page=0; break;
    case 1: SDL_Log("0x427870: Options");page=1; break;
    case 2: SDL_Log("0x427ef0: Save Game; TODO(0x428740): scene/VM state serialization");page=2; break;
    case 3: SDL_Log("0x428bb0: Load Game; TODO(0x429120): restore scene/VM state");page=3; break;
    case 4: SDL_Log("0x4246a0: View Movies; TODO(0x424d60): VM unlock variables");page=4; break;
    case 5: SDL_Log("0x425605: New Game confirmation");confirm=5; break;
    case 6: SDL_Log("0x4248b0: Resume Game; TODO(0x41c1c0): resume 3D scene (none at startup)");return;
    case 7: SDL_Log("0x4255ef: Quit Game confirmation");confirm=7; break;
    }
    selected=-1; rebuild();
}
