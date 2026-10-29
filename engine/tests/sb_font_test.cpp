// Retail glyph metrics, wrapping, pooled element reuse and caption clipping.
#include "../../game/src/ui/GameFont.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaCanvas.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include <stdlib.h>
#include <stdio.h>
static void check(bool value,const char *what) {
    if(!value) { fprintf(stderr,"FAIL: %s\n",what); exit(1); }
}
void omt_main() {
    omd_REGISTERCLASS(OMediaCanvas);
    OMediaDirectX::install(OMediaDirectX::omcdx_Video);
    OMediaDoubleCanvas *screen=new OMediaDoubleCanvas;
    OMediaDisplay *display=new OMediaDisplay(NULL,screen,"font checks");
    {
    OMedia2DWorld world;
    OMediaViewPort *vp=new OMediaViewPort(&world);
    OMediaRect bounds(0,0,640,480); vp->setbounds(&bounds); vp->setcanvas(screen);
    OMediaFileStream stream; OMediaFilePath path("Font.omt");
    stream.setpath(&path); stream.open(omcfp_Read);
    OMediaDataBase db(&stream); RetailData data;
    {
        GameFont font(&db,data);
        check(font.get_text_length("")==0,"empty");
        check(font.get_text_length(" ")==5,"space");
        check(font.get_text_length("A A")==2*font.get_text_length("A")+5,"proportional advances");
        check(font.get_text_length("\x85\x92")==font.get_text_length("{'"),"CP1252 replacements");
        check(font.get_text_length("*A*")==font.get_text_length("A"),"emphasis marker");
        OMediaCanvas *a=(OMediaCanvas*)db.get_object(OMediaCanvas::db_type,(long)data.word(0x454d44+'A'*4));
        check(font.get_text_length("A")==a->getwidth()-1,"one-pixel glyph overlap"); a->db_unlock();
        font.draw_world(&world,10,10,10000,0,"First frame");
        size_t count=world.get_element_list()->size();
        font.begin_frame(); font.draw_world(&world,10,30,10000,1,"Frame two");
        check(world.get_element_list()->size()==count,"reuse hidden glyph elements");
        font.begin_frame();
        check(font.print_wrapped(&world,0,80,10000,true,0,"short")==18,"single line height");
        string word(35,'W'),wrapped=word+" "+word;
        check(font.get_text_length(wrapped)>=631,"wrapping fixture exceeds threshold");
        check(font.print_wrapped(&world,0,160,10000,true,0,"%s",wrapped.c_str())==40,"wrapped height and 22-pixel spacing");
        font.print(&world,10,200,"value %d",42);
        OMediaRect clip(3,2,20,15);
        font.draw("clipped",screen->getdefaultdrawgfxworld(),&clip,10,240);
    }
    }
    delete display;
    OMediaScreenDevice::setdefaultdevice(NULL);
    puts("font checks PASS");
}
