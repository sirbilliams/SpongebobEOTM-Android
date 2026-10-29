#include "MiniGame.h"
#include "Game.h"
#include "DialogueUI.h"
#include "OMediaTextFont.h"
#include <stdlib.h>
#include <stdio.h>

namespace sbgame {
MiniGame::MiniGame(Game* g) : game_(g), step_(0), spins_(0), armed_(false), failed_(false), resultPending_(false), elapsed_(0), phone_(g)
{
    throws_=dartX_=dartY_=score_=hit_=highScore_=0;
    dartPending_=bullSound_=false; hitMs_=0;
    for (int i=0;i<3;++i) { target_[i]=-1; running_[i]=false; }
    for(int i=0;i<5;++i) dartText_[i]=new TextBlock;
}
void MiniGame::stopSound() { game_->stopChannel(7); } // 0x4368f0
void MiniGame::sound(const char* name, int pan) // 0x4367a0
{
    bool shared=false;
    OMediaSound* s=game_->findSound(name,&shared);
    stopSound();
    if (s) game_->playOnChannel(7,s,pan);
}
void MiniGame::pickSymbols() // 0x435d70: three distinct non-clue symbols
{
    for(int i=0;i<3;++i) {
        bool duplicate;
        do {
            target_[i]=(short)(rand()%5); duplicate=false;
            for(int j=0;j<i;++j) if(target_[j]==target_[i]) duplicate=true;
        } while(duplicate);
    }
    armed_=false;
}
void MiniGame::pickClue() // 0x435e10
{
    target_[0]=(short)((rand()&1)+6);
    target_[1]=(rand()&1)?5:8;
    target_[2]=target_[0]==6?7:6;
    armed_=true; step_=0; spins_=0;
    game_->log("mini slots clue: %d %d %d\n",target_[0]-5,target_[1]-5,target_[2]-5);
}
void MiniGame::showSymbol(int reel) // 0x435e70
{
    GameScene* s=game_->scene(); if(!s) return;
    char name[32]; sprintf(name,"symbol0%d",target_[reel]+1);
    SceneObject* o=s->find(name); if(!o) return;
    o->px=reel==0?143.0f:reel==1?289.0f:430.0f;
    o->py=reel==1?149.0f:150.0f;
    if(o->elem) o->elem->place(o->px,o->py,o->pz);
    s->show(name);
}
void MiniGame::slotsInit() // 0x436030
{
    spins_=0; stopSound();
    if(target_[0]==-1 || target_[1]==-1 || target_[2]==-1) pickSymbols();
    for(int i=0;i<3;++i) if(!running_[i]) showSymbol(i);
}
void MiniGame::slotsSpin() // 0x436090
{
    if(running_[0] || running_[1] || running_[2]) return;
    GameScene* s=game_->scene(); if(!s) return;
    ++spins_; stopSound();
    for(int i=0;i<3;++i) {
        char name[32]; sprintf(name,"253light%d",i+1); s->hide(name);
        SceneObject* light=s->find(name); if(light) light->playing=false;
        sprintf(name,"symbol0%d",target_[i]+1); s->hide(name); // 0x435f40
    }
    if(rand()%3==1 || spins_==4) pickClue(); else pickSymbols();
    const char* names[]={"spin","spin2","spin3"};
    for(int i=0;i<3;++i) {
        SceneObject* o=s->find(names[i]);
        if(o) { o->continuous=1; o->loops=1; o->stayOnLast=1; }
        s->animate(names[i]); running_[i]=true;
    }
    elapsed_=0;
}
void MiniGame::update(float dt) // slots branch of 0x435830
{
    updateDarts(dt); // lane c: 0x435830 darts branch
    phone_.update(dt); // chapter 4 phone (Phone.cpp)
    GameScene* s=game_->scene(); if(!s || s->name()!="c2l5s3") return;
    if(running_[2]) {
        elapsed_+=dt;
        const char* names[]={"spin","spin2","spin3"};
        // The exe checks the third, second, then first reel in this order.
        for(int i=2;i>=0;--i) if(running_[i] && elapsed_>(i+1)*1000.0f) {
            SceneObject* o=s->find(names[i]); if(o) o->playing=false;
            s->hide(names[i]); showSymbol(i); running_[i]=false; sound("slotstop",i-1); // 0x40c360: left, centre, right
            game_->log("mini slots reel %d stopped\n",i+1);
            if(i==2) { resultPending_=true; game_->wake(); }
        }
    } else if(resultPending_ && !game_->runner() && !game_->channelBusy(7)) {
        resultPending_=false; stopSound();
        if(armed_) {
            sound("sb5");
            for(int i=1;i<=3;++i) { char name[32]; sprintf(name,"253light%d",i); s->show(name); s->animate(name); }
            game_->log("mini slots winner\n"); game_->switchTo("swinner");
        } else { char name[16]; sprintf(name,"sb%d",rand()%4+1); sound(name); game_->log("mini slots lose\n"); }
    }
}
// 0x43a310's first picking pass: the maze's TRIG rectangles precede 3D actors.
SceneObject* MiniGame::pickMazeDoor(int x, int y)
{
    GameScene* s=game_->scene();
    if(!s || s->name()!="c2l5s4") return 0;
    for(size_t i=0;i<s->objects().size();++i) {
        SceneObject* o=s->objects()[i];
        if(o->kind!=SceneObject::Trigger || !game_->cond(o->visibleCond.c_str())) continue;
        if(o->x1<o->x2 && o->y1<o->y2 && x>=o->x1 && x<=o->x2 && y>=o->y1 && y<=o->y2) return o;
    }
    return 0;
}
void MiniGame::maze(int n) // 0x436510
{
    if(!armed_ || step_<0 || step_>=3 || n+5!=target_[step_]) {
        step_=0; failed_=true; game_->log("mini maze wrong direction %d\n",n); game_->switchTo("c2l5s2p2"); return;
    }
    ++step_; game_->log("mini maze correct direction %d step %d\n",n,step_);
    if(step_==3) { game_->switchTo("c2l5s5p1"); step_=0; return; }
    const char* exits[]={"c2l5s4p4","c2l5s4p3","c2l5s4p2","c2l5s4p1"};
    if(n>=0 && n<4) game_->switchTo(exits[n]);
}
void MiniGame::sceneEntered() // 0x436250 and 0x437cf0
{
    GameScene* s=game_->scene(); if(!s) return;
    if(s->name()=="c2l5s4") {
        for(size_t i=0;i<s->objects().size();++i) {
            SceneObject* o=s->objects()[i];
            if(o->event.compare(0,5,"smaze")==0) game_->log("mini maze door %s rect %d %d %d %d\n",o->event.c_str(),o->x1,o->y1,o->x2,o->y2);
        }
        const bool second=step_==1;
        if(s->background_) s->background_->setcanvas(s->canvasNamed(second?"background2":"background"));
        const char* hidden[]={"254drip1","254archa01","254archa02"};
        const char* shown[]={"254drip2","254archb01","254archb02"};
        for(int i=0;i<3;++i) { if(second) { s->hide(hidden[i]); s->show(shown[i]); } else { s->show(hidden[i]); s->hide(shown[i]); } }
    }
    if(s->name()=="c2l5s2" && failed_) { failed_=false; game_->switchTo("h252d"); }
}
}

// lane c: darts portion of the same persistent object, 0x436990..0x436cf2.
namespace sbgame {
MiniGame::~MiniGame() { for(int i=0;i<5;++i) delete dartText_[i]; }
int MiniGame::dartsScore(int x, int y) // 0x436b70, nested GDI elliptic regions
{
    const int boxes[6][4]={{302,226,336,260},{275,201,360,285},
        {234,162,400,323},{200,125,433,360},{161,90,475,398},{133,60,496,424}};
    const int points[6]={50,25,20,15,10,5};
    // port: replace Win32 PtInRegion with a pixel-centred ellipse test.
    for(int i=0;i<6;++i) {
        double rx=(boxes[i][2]-boxes[i][0])*0.5, ry=(boxes[i][3]-boxes[i][1])*0.5;
        double dx=(x+0.5-boxes[i][0]-rx)/rx, dy=(y+0.5-boxes[i][1]-ry)/ry;
        if(dx*dx+dy*dy<1.0) return points[i];
    }
    return 0;
}
void MiniGame::darts(int op)
{
    if(op==0) { // 0x436990: high score survives reset
        throws_=dartX_=dartY_=score_=hit_=0; dartPending_=bullSound_=false;
        sound("ready"); dartsText(); return;
    }
    GameScene* s=game_->scene(); if(!s || s->name()!="c3l2s5" || dartPending_ || bullSound_) return;
    if(throws_==3) { // fourth click clears the board; it does not throw
        s->hide("Darts"); s->hide("Darts01"); s->hide("Darts03"); s->hide("Darts05");
        throws_=dartX_=dartY_=score_=hit_=0; dartPending_=bullSound_=false; dartsText(); return;
    }
    dartX_=(short)(game_->mouseX()-47+rand()%50-25);
    dartY_=(short)(game_->mouseY()-66+rand()%50-25);
    char name[16]; sprintf(name,"dart%d",rand()%3+1); sound(name);
    SceneObject* o=s->find("Darts"); if(!o) return;
    dartPending_=true; o->px=dartX_; o->py=dartY_; o->pz=dartY_-640;
    if(o->elem) o->elem->place(o->px,o->py,o->pz);
    s->animate("Darts");
    game_->log("mini darts throw %d at %d,%d\n",throws_+1,dartX_+47,dartY_+66);
}
void MiniGame::dartsText() // 0x4355a0
{
    GameScene* s=game_->scene();
    for(int i=0;i<5;++i) dartText_[i]->clear();
    if(!s || s->name()!="c3l2s5") return;
    char text[80];
    OMediaTextFont* font=game_->font();
    dartText_[0]->show(game_,20,435,10100,false,"Your Score",150);
    sprintf(text,"%d",score_); dartText_[3]->show(game_,20+(font->get_text_length("Your Score")-font->get_text_length(text))/2,457,10100,false,text,100);
    dartText_[1]->show(game_,620-font->get_text_length("High Score"),435,10100,false,"High Score",120);
    sprintf(text,"%d",highScore_); dartText_[4]->show(game_,620-(font->get_text_length("High Score")+font->get_text_length(text))/2,457,10100,false,text,80);
    if(hit_>0 && hitMs_>0) {
        sprintf(text,"%d",hit_); dartText_[2]->show(game_,(int)(dartX_+27-hitMs_*0.05f),(int)(dartY_+46+hitMs_*0.1f),10100,hit_==50,text,60);
    }
}
void MiniGame::updateDarts(float dt) // darts branch, 0x435830
{
    GameScene* s=game_->scene();
    if(!s || s->name()!="c3l2s5") { for(int i=0;i<5;++i) dartText_[i]->clear(); return; }
    if(throws_ && hit_ && hitMs_>0) { hitMs_+=dt; if(hitMs_>800) { hitMs_=0; } dartsText(); }
    if(bullSound_ && !game_->channelBusy(7)) { bullSound_=false; sound("yipee"); }
    SceneObject* o=s->find("Darts");
    if(!dartPending_ || !o || o->playing) return;
    hitMs_=1; hit_=(short)dartsScore(dartX_+47,dartY_+66); score_+=hit_;
    if(score_>highScore_) highScore_=score_;
    bullSound_=hit_==50; dartPending_=false; s->hide("Darts");
    const char* names[]={"Darts01","Darts03","Darts05"};
    if(throws_<3) {
        SceneObject* stuck=s->find(names[throws_]);
        if(stuck) { stuck->px=dartX_; stuck->py=dartY_; stuck->pz=dartY_-640;
            if(stuck->elem) stuck->elem->place(stuck->px,stuck->py,stuck->pz); s->show(names[throws_]); }
        ++throws_;
    }
    game_->log("mini darts landed: hit %d score %d throws %d high %d\n",hit_,score_,throws_,highScore_);
    dartsText();
}
}
