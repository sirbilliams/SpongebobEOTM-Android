#include "MainMenu.h"
#include "OMedia2DWorld.h"
#include "OMediaCanvasElement.h"
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include "OMedia2DAnim.h"
#include "OMediaBoxElement.h"
#include "OMediaDefMessages.h"
#include "../game/SaveGame.h"
#include "VoiceGallery.h"
#include <SDL.h>
#include <stdio.h>
#include <sys/stat.h>
static bool SaveGame_exists(int s) { return sbgame::SaveGame::exists(s); }

// The retail font draws bank 0 or 1 through pooled glyph elements. port: each caption
// selects its bank at draw time so normal and highlighted strings can share the font.
class MenuCaption : public OMediaCCaption {
public:
    GameFont *gamefont; bool highlighted, forced;      // forced: port: the Options row / value in use is shown in the bright bank
    MenuCaption(GameFont *f) : gamefont(f), highlighted(false), forced(false) { set_font(f); }
    virtual void draw(OMediaRect *src,long x,long y,OMediaCanvas *dest,OMediaRefreshList *rl) {
        gamefont->set_bank(highlighted||forced?1:0);
        OMediaCCaption::draw(src,x,y,dest,rl);
        gamefont->set_bank(0);
    }
};
MainMenu::MainMenu(OMedia2DWorld *w,OMediaDataBase *a,GameFont *f,const RetailData &d,MenuHost *h)
 : quit(false),world(w),art(a),font(f),data(d),host(h),page(-1),confirm(-1),selected(0),help_page(1),focus(0),slot(-1) {
    gallery=NULL; vlevel=0; vchap=1; vpage=0;     // port: voice gallery
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
    delete gallery;
    for(size_t i=0;i<controller_art.size();++i) controller_art[i]->db_unlock();
}
void MainMenu::clear() {
    buttons.clear();
    for(size_t i=0;i<elements.size();++i) delete elements[i];
    elements.clear();
    for(size_t i=0;i<owned.size();++i) delete owned[i];      // after the elements that show them
    owned.clear();
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
MenuCaption *MainMenu::caption(const string &s,int x,int y,int id) {
    MenuCaption *c=new MenuCaption(font); c->set_string(s);
    c->link(world); c->place((float)x,(float)y,10300);
    // port: rectangular text hit regions match 0x426e00, including blank spaces.
    c->set_clickable(id>=0);
    elements.push_back(c);
    if(id>=0) { Button b; b.id=id; b.x=x; b.y=y; b.w=font->get_text_length(s); b.h=18; b.caption=c; buttons.push_back(b); }
    return c;
}
MenuCaption *MainMenu::center(const string &s,int y,int id) { return caption(s,(640-font->get_text_length(s))/2,y,id); }
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
        string title=confirm==5?"New Game?":confirm==2?"Overwrite?":confirm==3?"Delete Saved Game?":"Quit Game?";
        // 0x424e00 quirk: Quit Game? positioned using Exit Game?'s width.
        caption(title,(640-font->get_text_length(confirm==7?"Exit Game?":title))/2,211,-1);
        string yes=data.text(0x4559c8),no=data.text(0x4559c4);
        caption(yes,225+(95-font->get_text_length(yes))/2,251,100);
        caption(no,320+(95-font->get_text_length(no))/2,251,101);
    } else if(page==-1) {
        for(int i=0;i<8;++i) center(data.text(data.word(0x455700+4*i)),91+40*i,i);
        // Resume Game is greyed (0x4558e8 noresume) while there is nothing to resume.
        if(!host->menu_can_resume()) image("noresume",257,329,10301);
    } else if(page==0) {
        char name[16]; sprintf(name,"help%02d",help_page);
        image(name,-1,-1,10201); // 0x427640
        center("Cancel",380,90);
        if(help_page>1) arrow(true,364,91);
        if(help_page<5) arrow(false,364,92);
    } else if(page==1) {
        for(int i=0;i<5;++i) center(data.text(data.word(0x455750+4*i)),100+55*i,-1)->forced=(focus==i);   // port: the keyboard row is bright
        int values[3]={host->music,host->sound,host->voice};
        for(int i=0;i<3;++i) {
            image("line",195,128+55*i,10301);
            image("cursor",197+27*values[i],120+55*i,10302);
            for(int j=0;j<10;++j) { Button b; b.id=200+10*i+j; b.x=195+27*j;
                b.y=120+55*i; b.w=27; b.h=17; b.caption=NULL; buttons.push_back(b); }
        }
        string on=data.text(0x455910),off=data.text(0x455914);
        for(int i=0;i<2;++i) {
            int cur=i==0?host->text:host->shadows;       // port: the value in use is bright
            caption(on,225+(95-font->get_text_length(on))/2,288+55*i,240+i*2)->forced=cur!=0;
            caption(off,320+(95-font->get_text_length(off))/2,288+55*i,241+i*2)->forced=cur==0;
        }
        center(data.text(0x455904),380,90)->forced=(focus==5);
    } else if(page==2 || page==3) {
        // 0x427ef0 (Save Game) / 0x428bb0 (Load Game): the slot's 320x240 picture in a 322x242 box, the slot's name, arrows to browse.
        OMediaBoxElement *box=new OMediaBoxElement;
        box->set_size(322,242); box->set_color(OMediaRGBColor(0,0,0));
        box->set_3d(true); box->set_sunken(true); box->link(world); box->place(159,94,10299);
        box->set_clickable(false); elements.push_back(box);
        if(page==3 && slot<0) center("No Saved Games",311,-1);
        else {
            OMediaCanvas *tc=(slot>=0 && SaveGame_exists(slot) && !slotThumb.empty())?sbgame::SaveGame::thumbCanvas(slotThumb):NULL;
            if(tc) {
                owned.push_back(tc);
                OMediaCanvasElement *e=new OMediaCanvasElement(tc);
                e->link(world); e->place((float)((640-tc->getwidth())/2),95.0f,10300); e->set_clickable(false);
                elements.push_back(e);
            } else image((slot>=0 && SaveGame_exists(slot))?"winner":"empty",-1,95,10300);
        }
        // arrows (0x427e40): left from slot 1 on; right while the slot exists (Save: up to the first free slot)
        int existing=0; for(int i=0;i<sbgame::SaveGame::kSlots;++i) if(sbgame::SaveGame::exists(i)) ++existing;
        bool left=page==2?slot>0:existing>1, right=page==2?(slot>=0 && sbgame::SaveGame::exists(slot) && slot<sbgame::SaveGame::kSlots-1):existing>1;
        if(left) arrow(true,344,94);
        if(right) arrow(false,344,95);
        if(page==2 || slot>=0) {
            string nm=slotName;
            // port: placed between the arrows; the exe draws the typed name with the 'line' and 'cursor' pictures
            if(!nm.empty()) caption(nm,(640-font->get_text_length(nm))/2,350,-1);
            if(page==2) {
                image("line",-1,372,10301);
                image("cursor",(640+font->get_text_length(nm))/2+2,348,10302);
            }
        }
        caption(data.text(0x455904),225+(95-font->get_text_length(data.text(0x455904)))/2,380,93);
        caption("Cancel",320+(95-font->get_text_length("Cancel"))/2,380,90);
    } else if(page==4) {
        // 0x4246a0: the 12 movies; a locked one (0x424d60) shows the 'locked' picture instead of its name and cannot be clicked
        for(int i=0;i<12;++i) {
            if(movieUnlocked(i)) center(data.text(data.word(0x455720+4*i)),91+23*i,300+i);
            else image("locked",-1,91+23*i,10301);
        }
        // port: the second page, Voices (0x4246a0 has only Cancel here); same two-button row as the Save Game screen
        { string v="Voices",c=data.text(0x455904);
          caption(v,225+(95-font->get_text_length(v))/2,380,500);
          caption(c,320+(95-font->get_text_length(c))/2,380,90); }
    } else if(page==5) {
        voicesPage();
    }
    for(size_t i=0;i<buttons.size();++i)
        if(buttons[i].caption) ((MenuCaption*)buttons[i].caption)->highlighted=buttons[i].id==selected;
    // port: the keyboard (Android: the on-screen one) is only wanted while a name is typed
    if(page==2 && confirm<0) SDL_StartTextInput(); else SDL_StopTextInput();
}
// port: the Voices page of View Movies (docs/reveal-and-gallery.md): chapters -> speakers -> clips; a clip not heard yet is '???'
static string fit(GameFont *font,const string &in,int maxw) {
    string s; for(size_t i=0;i<in.size();++i) { unsigned char c=(unsigned char)in[i]; if(c>=0x20 && c<0x7f) s+=(char)c; }
    if(font->get_text_length(s)<=maxw) return s;
    while(!s.empty() && font->get_text_length(s+"...")>maxw) s.erase(s.size()-1);
    return s+"...";
}
void MainMenu::voicesPage() {
    if(!gallery) { gallery=new sbgame::VoiceGallery; gallery->build(host->menu_progress()); }
    char b[96];
    string back=vlevel==0?"Movies":"Back",c=data.text(0x455904);
    if(vlevel==0) {
        center("Voices",91,-1)->forced=true;
        for(int ch=1;ch<=4;++ch) {
            int h,t; gallery->count(ch,"",&h,&t);
            sprintf(b,"Chapter %d   %d of %d",ch,h,t);
            if(t>0) center(b,91+34*ch,400+ch); else center(b,91+34*ch,-1);
        }
    } else if(vlevel==1) {
        std::vector<string> sp=gallery->speakers(vchap);
        sprintf(b,"Chapter %d",vchap); center(b,91,-1)->forced=true;
        for(size_t i=0;i<sp.size() && i<10;++i) {
            int h,t; gallery->count(vchap,sp[i],&h,&t);
            sprintf(b,"   %d of %d",h,t);
            center(sp[i]+b,114+21*(int)i,410+(int)i);
        }
    } else {
        vclips=gallery->select(vchap,vspk);
        int pages=((int)vclips.size()+9)/10; if(pages<1) pages=1;
        if(vpage>=pages) vpage=pages-1;
        sprintf(b,"%s   Page %d of %d",vspk.c_str(),vpage+1,pages); center(b,91,-1)->forced=true;
        for(int r=0;r<10;++r) {
            int k=vpage*10+r; if(k>=(int)vclips.size()) break;
            const sbgame::VoiceClip &cl=gallery->clips()[vclips[k]];
            if(cl.heard) center(fit(font,cl.text,330),114+21*r,430+r);
            else center("???",114+21*r,-1);
        }
        if(vpage>0) arrow(true,344,502);
        if(vpage<pages-1) arrow(false,344,503);
    }
    caption(back,225+(95-font->get_text_length(back))/2,380,501);
    caption(c,320+(95-font->get_text_length(c))/2,380,90);
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
    // 0x425aa0: a click on the picture of the Save / Load Game screen does what Ok does
    if((page==2 || page==3) && confirm<0 && ci->click_x>=0xa1 && ci->click_x<=0x1df && ci->click_y>=0x60 && ci->click_y<=0x14e) activate(93);
}
bool MainMenu::movieUnlocked(int i) {       // 0x424d60: 0..2 and 11 always; 3..10 once the script was seen
    static const char *const scripts[8]={"schap1end","schap2end","schap3end","schap4end","svideo1","splankton","svideo3","svideo4"};
    if(i<=2 || i==11) return true;
    if(i>10) return false;
    return host->menu_script_seen(scripts[i-3]);
}
void MainMenu::back() {
    if(page==5 && confirm<0) { activate(501); return; }      // port: Voices steps up one level, then to the movies
    if(gallery) { delete gallery; gallery=NULL; }      // port: rebuilt (with the current progress) the next time Voices opens
    confirm=-1;page=-1;selected=-1;rebuild();
}

// ---- save / load screens ----
void MainMenu::selectSlot(int s) {       // 0x4295d0: the slot's name and picture (a free slot has an empty name)
    slot=s; slotName.clear(); slotThumb.clear();
    if(s>=0 && sbgame::SaveGame::exists(s)) {
        sbgame::SaveFile f;
        if(sbgame::SaveGame::readHeader(sbgame::SaveGame::slotPath(s),f)) { slotName=f.name; slotThumb=f.thumb; }
    }
}
void MainMenu::stepSlot(int dir) {
    if(slot<0) return;
    int n=sbgame::SaveGame::kSlots, s=slot;
    if(page==2) {                                      // 0x4291b0 / 0x4293c0: Save: slot-1, or up to the slot after the last used one
        if(dir<0) { if(s>0) --s; }
        else if(sbgame::SaveGame::exists(s) && s<n-1) ++s;
    } else {                                           // Load: the previous / next saved game, wrapping
        for(int i=0;i<n;++i) { s=(s+dir+n)%n; if(sbgame::SaveGame::exists(s)) break; }
    }
    if(s==slot) return;
    SDL_Log("0x4291b0/0x4293c0: slot %d -> %d",slot,s);
    selectSlot(s); rebuild();
}
void MainMenu::saveAction() {                          // 0x428740
    if(slot<0) return;
    if(sbgame::SaveGame::exists(slot) && confirm!=2) { SDL_Log("0x428740: Overwrite? slot %d",slot); confirm=2; rebuild(); return; }
    if(!host->menu_save(slot,slotName.c_str())) { SDL_Log("0x428740: save failed"); confirm=-1; rebuild(); return; }
    SDL_Log("0x428740 -> 0x41db50: saved slot %d \"%s\"",slot,slotName.c_str());
    confirm=-1; page=-1; selected=-1; host->menu_resume();      // 0x4248b0: the menu closes and the game goes on
}
void MainMenu::loadAction() {                          // 0x429120
    if(slot<0) return;
    SDL_Log("0x429120: load slot %d",slot);
    if(host->menu_load(slot)) { confirm=-1; page=-1; selected=-1; }
}
void MainMenu::adjust(int dir) {              // 0x425aa0 case 7 / 6: the focused Options row, one step
    int *v[3]={&host->music,&host->sound,&host->voice};
    if(focus<3) {
        int n=*v[focus]+dir;
        if(n<0 || n>9) return;
        activate(200+10*focus+n);
    } else if(focus==3) activate(dir>0?240:241);
    else if(focus==4) activate(dir>0?242:243);
}
void MainMenu::key(int sym) {
    if(confirm>=0 && page!=2 && page!=3) {      // port: Yes / No for the New Game and Quit prompts
        if(sym==SDLK_y) activate(100); else if(sym==SDLK_n) activate(101);
        return;
    }
    if(page==0) {                                // Help: the arrows page, Cancel leaves
        if(sym==SDLK_LEFT && help_page>1) activate(91);
        else if(sym==SDLK_RIGHT && help_page<5) activate(92);
        else if(sym==SDLK_RETURN || sym==SDLK_KP_ENTER || sym==SDLK_SPACE) activate(90);
        return;
    }
    if(page==1) {                                // Options: rows M S V D 3 O as in 0x425aa0, arrows move / change
        int f=-1;
        switch(sym) {
        case SDLK_m: f=0; break; case SDLK_s: f=1; break; case SDLK_v: f=2; break;
        case SDLK_d: f=3; break; case SDLK_3: f=4; break; case SDLK_o: activate(90); return;
        case SDLK_UP: f=(focus+5)%6; break; case SDLK_DOWN: f=(focus+1)%6; break;
        case SDLK_LEFT: adjust(-1); return; case SDLK_RIGHT: adjust(1); return;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
            if(focus==5) activate(90); else if(focus>=3) adjust(1); return;
        }
        if(f>=0 && f!=focus) { focus=f; rebuild(); }
        return;
    }
    if(page!=2 && page!=3) return;
    if(confirm>=0) { if(sym==SDLK_RETURN || sym==SDLK_y) activate(100); else if(sym==SDLK_n) activate(101); return; }
    if(sym==SDLK_LEFT) stepSlot(-1);
    else if(sym==SDLK_RIGHT) stepSlot(1);
    else if(sym==SDLK_RETURN || sym==SDLK_KP_ENTER) { if(page==2) saveAction(); else loadAction(); }
    else if(page==2 && sym==SDLK_BACKSPACE) { if(!slotName.empty()) { slotName.erase(slotName.size()-1); rebuild(); } }
    else if(page==3 && sym==SDLK_DELETE && slot>=0) { confirm=3; rebuild(); }       // "Delete Saved Game?"
}
void MainMenu::padButton(int b) {       // port: pad d-pad / A / B drive the same keys (R3 / A as the mouse button are the input layer's)
    switch(b) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key(SDLK_LEFT); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key(SDLK_RIGHT); break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: key(SDLK_UP); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key(SDLK_DOWN); break;
    case SDL_CONTROLLER_BUTTON_B: if(!atTop()) back(); break;
    }
}
void MainMenu::text(const char *utf8) {
    if(page!=2 || confirm>=0 || !utf8) return;
    for(;*utf8;++utf8) {
        unsigned char c=(unsigned char)*utf8;
        if(c<0x20 || c>=0x7f) continue;                 // port: the font has the ASCII glyphs
        string t=slotName+(char)c;
        if(slotName.size()>0x1e || font->get_text_length(t)>0xfa) return;      // 0x425aa0: at most 30 characters, 250 pixels wide
        slotName=t;
    }
    rebuild();
}
void MainMenu::activate(int id) {
    SDL_Log("0x425aa0: click menu item %d -> 0x425580",id);
    if(id==100) {
        SDL_Log("0x4254b0: confirmation Yes (%d)",confirm);
        if(confirm==2) { saveAction(); return; }
        if(confirm==3) {
            int s=slot; sbgame::SaveGame::deleteSlot(s);
            if(!sbgame::SaveGame::exists(s)) { int n=sbgame::SaveGame::kSlots; s=-1; for(int i=0;i<n;++i) if(sbgame::SaveGame::exists(i)) { s=i; break; } }
            SDL_Log("0x428a70: saved game deleted"); confirm=-1; selectSlot(s); rebuild(); return;
        }
        if(confirm==7) { SDL_Log("0x4254b0 -> 0x40a070: application quit"); quit=true; }
        else { SDL_Log("0x4248b0 -> 0x41c1c0 -> 0x40a090: New Game"); if(!host->menu_new_game()) quit=true; back(); }
        return;
    }
    if(id==101) { SDL_Log("0x425520: confirmation No"); if(confirm==2||confirm==3) { confirm=-1; rebuild(); } else back(); return; }
    if(id==94 || id==95) { stepSlot(id==94?-1:1); return; }
    if(id==90) { SDL_Log("0x424890: return to main menu"); host->menu_settings_changed(); page=-1; if(gallery) { delete gallery; gallery=NULL; } back(); return; }
    if(id==91 || id==92) { help_page+=id==91?-1:1; rebuild(); return; }
    if(id==93) { if(page==2) saveAction(); else loadAction(); return; }
    if(id>=200 && id<230) {
        int v=(id-200)%10, i=(id-200)/10;
        if(i==0) host->music=v; else if(i==1) host->sound=v; else host->voice=v;
        SDL_Log("0x4256b0: volume %d=%d",i,v); focus=i; host->menu_settings_changed(); if(i>0) host->menu_preview(i+1); rebuild(); return;
    }
    if(id>=240 && id<=243) {
        if(id<242) host->text=id==240; else host->shadows=id==242;
        SDL_Log("0x425aa0: option %d",id); host->menu_settings_changed(); rebuild(); return;
    }
    if(id==500) { SDL_Log("port: View Movies -> Voices"); page=5; vlevel=0; selected=-1; rebuild(); return; }
    if(id==501) { if(vlevel>0) --vlevel; else { if(gallery) gallery->stop(); page=4; } selected=-1; rebuild(); return; }
    if(id==502 || id==503) { vpage+=id==502?-1:1; if(vpage<0) vpage=0; selected=-1; rebuild(); return; }
    if(id>=400 && id<500) {
        if(id<410) { vchap=id-400; vlevel=1; }
        else if(id<430) { std::vector<string> sp=gallery->speakers(vchap); if(id-410<(int)sp.size()) { vspk=sp[id-410]; vlevel=2; vpage=0; } }
        else { int k=vpage*10+(id-430); if(k<(int)vclips.size()) gallery->play(vclips[k]); return; }
        selected=-1; rebuild(); return;
    }
    if(id>=300 && id<400) {
        const char *names[]={"mthq","mspongebob","mintro","mchapter2","mchapter3","mchapter4",
            "mend","mmaking1","mmaking2","mmaking3","mmaking4","mcredits"};
        if(!movieUnlocked(id-300)) return;                 // 0x424bb0 checks 0x424d60 again
        SDL_Log("0x424bb0: movie %s",names[id-300]);
        if(!host->menu_movie(names[id-300])) quit=true;
        rebuild(); return;
    }
    switch(id) {
    case 0: SDL_Log("0x427640: Help");page=0; help_page=1; break;
    case 1: SDL_Log("0x427870: Options");page=1; focus=0; break;
    case 2: SDL_Log("0x427ef0: Save Game");
        if(!host->menu_game_active()) { SDL_Log("0x427ef0: no game to save"); return; }      // port: the startup menu has no scene to save
        page=2; { int f=sbgame::SaveGame::firstFreeSlot(); selectSlot(f<sbgame::SaveGame::kSlots?f:0); } break;   // the first free slot, else slot 0 (overwrite)
    case 3: SDL_Log("0x428bb0: Load Game");page=3;
        { int s=-1; if(host->save>=0 && sbgame::SaveGame::exists(host->save)) s=host->save;
          for(int i=0;s<0 && i<sbgame::SaveGame::kSlots;++i) if(sbgame::SaveGame::exists(i)) s=i;
          selectSlot(s); } break;
    case 4: SDL_Log("0x4246a0: View Movies");page=4; break;
    case 5: SDL_Log("0x425605: New Game confirmation");confirm=5; break;
    case 6: SDL_Log("0x4248b0: Resume Game");
        if(host->menu_can_resume()) host->menu_resume();
        return;
    case 7: SDL_Log("0x4255ef: Quit Game confirmation");confirm=7; break;
    }
    selected=-1; rebuild();
}
