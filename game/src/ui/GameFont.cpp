#include "GameFont.h"
#include "OMediaCanvas.h"
#include "OMediaDataBase.h"
#include "OMediaCanvasElement.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
GameFont::GameFont(OMediaDataBase *database, const RetailData &data) : db(database), bank(0), emphasis(0), next(0) {
    // 0x414820 / lookup at 0x454d44. port: read the table from the installed executable.
    for(int i=0;i<128;++i) {
        unsigned long v=data.word(0x454d44+4*i);
        table[i]=v==0xffffffffUL ? -1 : (long)v;
    }
}
GameFont::~GameFont() {
    for(size_t p=0;p<pool.size();++p) delete pool[p];
    for(std::map<long,OMediaCanvas*>::iterator i=cache.begin();i!=cache.end();++i)
        if(i->second) i->second->db_unlock();
}
OMediaCanvas *GameFont::glyph(unsigned char c, short variant) {
    if(c==0x85) c='{';
    if(c==0x92) c='\'';
    // port: guard the original signed-char table access for unsupported extended characters.
    if(c>=128 || table[c]==-1) return NULL;
    long id=table[c]+variant*0x47;
    std::map<long,OMediaCanvas*>::iterator i=cache.find(id);
    if(i!=cache.end()) return i->second;
    OMediaCanvas *g=db->object_exists(OMediaCanvas::db_type,id)?
        (OMediaCanvas*)db->get_object(OMediaCanvas::db_type,id):NULL;
    cache[id]=g;
    return g;
}
short GameFont::get_text_length(string text) {
    short width=0;
    for(size_t i=0;i<text.size();++i) {
        if(text[i]==' ') width=(short)(width+5);
        else { OMediaCanvas *g=glyph((unsigned char)text[i],0);
            if(g) width=(short)(width+g->getwidth()-1); }
    }
    return width;
}
void GameFont::draw_full(string text, OMediaGfxWorld *dest, short x, short y) {
    OMediaRect r(0,0,get_text_length(text)+1,18);
    draw(text,dest,&r,x,y);
}
void GameFont::draw(string text, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y) {
    // 0x414de0: space +5; '*' toggles emphasis but emits no glyph; each glyph advances width-1.
    // port: the retail function pools CanvasElements; captions draw the same glyphs into a canvas.
    long px=x-clip->left, py=y-clip->top;
    for(size_t i=0;i<text.size();++i) {
        unsigned char c=(unsigned char)text[i];
        if(c==' ') { px+=5; continue; }
        if(c=='*') { emphasis^=1; continue; }
        OMediaCanvas *g=glyph(c,bank);
        if(!g) continue;
        long l=px>x?px:x, t=py>y?py:y;
        long r=px+g->getwidth(), b=py+g->getheight();
        if(r>x+clip->get_width()) r=x+clip->get_width();
        if(b>y+clip->get_height()) b=y+clip->get_height();
        if(l<r && t<b) { OMediaRect src(l-px,t-py,r-px,b-py); g->draw(dest,&src,l,t,NULL); }
        px+=g->getwidth()-1;
    }
}

void GameFont::begin_frame() {
    // 0x414a90 also destroys the retail temporary list (the port has none).
    for(size_t i=0;i<pool.size();++i) if(!pool[i]->gethide()) pool[i]->hide(true);
}
void GameFont::draw_world(OMediaWorld *world,int x,int y,int z,short variant,const string &text) {
    for(size_t i=0;i<text.size();++i) {
        unsigned char c=(unsigned char)text[i];
        if(c==' ') { x+=5; continue; }
        if(c=='*') { emphasis^=1; continue; }
        OMediaCanvas *g=glyph(c,variant);
        if(!g) continue;
        OMediaCanvasElement *e=NULL;
        for(size_t n=0;n<pool.size();++n) {
            if(next==pool.size()) next=0;
            OMediaCanvasElement *candidate=pool[next++];
            if(candidate->gethide()) { e=candidate; e->hide(false); break; }
        }
        if(!e) { e=new OMediaCanvasElement; e->link(world); pool.push_back(e); }
        e->setcanvas(g); e->place((float)x,(float)y,(float)z);
        x+=g->getwidth()-1;
    }
}
void GameFont::print(OMediaWorld *world,int x,int y,const char *format,...) {
    char buffer[256]; va_list args; va_start(args,format);
    // port: bound the original vsprintf to its 256-byte buffer.
    vsnprintf(buffer,sizeof buffer,format,args); va_end(args);
    print_wrapped(world,x,y,0x2774,false,0,"%s",buffer);
}
short GameFont::print_wrapped(OMediaWorld *world,int x,int y,int z,bool centered,
                             short variant,const char *format,...) {
    char buffer[256]; va_list args; va_start(args,format);
    vsnprintf(buffer,sizeof buffer,format,args); va_end(args);
    emphasis=0;
    short width=get_text_length(buffer);
    if(width<0x277) {
        draw_world(world,centered?(640-width)/2:x,y,z,variant,buffer);
        return 18;
    }
    // 0x414b50: strtok on spaces, five lines, threshold 631, append a trailing
    // space to each word; draw the last line at y, preceding lines upwards by 22.
    string lines[5]; int line=0; short length=0;
    char *word=strtok(buffer," ");
    while(word) {
        short w=get_text_length(word); length=(short)(length+w);
        if(length>=0x277) { if(++line==5) { line=4; break; } length=w; }
        lines[line]+=word; lines[line]+=' '; length=(short)(length+5);
        word=strtok(NULL," ");
    }
    for(int i=line;i>=0;--i) {
        draw_world(world,centered?(640-get_text_length(lines[i]))/2:x,y,z,variant,lines[i]);
        y-=22;
    }
    return (short)(line*22+18);
}
