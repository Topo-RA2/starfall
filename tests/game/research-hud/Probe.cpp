#include "../../../src/Starfall.cpp"
#include <WWMouseClass.h>
#include <WWKeyboardClass.h>
#include <VocClass.h>
#include "ResearchAudio.h"
#include "starfall_hud_v1.h"
#include <Drawing.h>
#include <OwnerDraw.h>

EXPORT_FUNC(SF_ResearchDraw);
#include "../WindowMouse.h"
namespace {
void verify(bool ok,const char* label) {
    std::ofstream("hud.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
void input(Point2D point,int flags) {
    verify(WindowMouse::button(point,flags),"engine blocks click through");
}
void click(Point2D point) {input(point,1);input(point,4);}
int pixel(int x,int y) {
    Point2D p{x,y};
    return DSurface::Composite->GetPixel(&p);
}
// Sample the producer's actual ordered source-over stream, with no GPU or platform DLL.
struct OverlaySample {
    Point2D point;
    unsigned alpha=0;
    void blend(unsigned source) {alpha=source+((255-source)*alpha+127)/255;}
};
struct OverlayProbe {
    OverlaySample shade,card,tab;
    int beginCount=0,endCount=0,complete=0;
    OverlaySample* samples[3]={&shade,&card,&tab};
};
int STARFALL_HUD_CALL beginOverlay(void* context,int,int) {
    ++static_cast<OverlayProbe*>(context)->beginCount;
    return 2;
}
int STARFALL_HUD_CALL fillOverlay(void* context,int x,int y,int w,int h,uint32_t rgba) {
    for(auto* s:static_cast<OverlayProbe*>(context)->samples)
        if(s->point.X>=x && s->point.X<x+w && s->point.Y>=y && s->point.Y<y+h) s->blend(rgba>>24);
    return 1;
}
int STARFALL_HUD_CALL pixelsOverlay(void* context,int x,int y,int w,int h,int pitch,const uint8_t* rgba) {
    for(auto* s:static_cast<OverlayProbe*>(context)->samples) {
        int dx=s->point.X-x,dy=s->point.Y-y;
        if(dx>=0 && dx<w && dy>=0 && dy<h) s->blend(rgba[dy*pitch+dx*4+3]);
    }
    return 1;
}
void STARFALL_HUD_CALL endOverlay(void* context,int complete) {
    auto& probe=*static_cast<OverlayProbe*>(context);
    ++probe.endCount;probe.complete=complete;
}
void verifyTransparentOverlay(Point2D shade,Point2D middleCard,Point2D border) {
    OverlayProbe probe{{shade},{middleCard},{border}};
    StarfallHudSinkV1 sink{sizeof(sink),1,&probe,beginOverlay,fillOverlay,pixelsOverlay,endOverlay};
    verify(Starfall_SetHudSinkV1(&sink)!=0,"registered GPU-only HUD sink");
    auto surface=DSurface::Composite;
    const int original=pixel(border.X,border.Y);
    surface->SetPixel(&border,COLOR_PURPLE);
    SF_ResearchDraw(nullptr);
    verify(pixel(border.X,border.Y)==COLOR_PURPLE,"GPU-only HUD leaves native Composite untouched");
    surface->SetPixel(&border,original);
    verify(Starfall_SetHudSinkV1(nullptr)!=0,"released HUD sink");
    verify(probe.beginCount==1 && probe.endCount==1 && probe.complete,"one complete GPU HUD frame");
    verify(probe.shade.alpha==45*255/100 && probe.card.alpha==255 && probe.tab.alpha==255,
        "transparent battlefield shade, opaque card body and research rim");
}
bool diplomacyChecked=false;
void captureHud(const char* name,int x0,int top,int width,int height,Surface* surface=DSurface::Composite);
bool paintedDiplomacyImage(HWND window) {
    auto& control=OwnerDraw::WinData[window];
    auto image=control.ControlImage;
    auto surface=DSurface::Alternate;
    RECT bounds{};
    OwnerDraw::GetRectangle(window,&bounds);
    if(!image || !surface || bounds.left<0 || bounds.top<0 ||
        bounds.left+image->Width>surface->Width || bounds.top+image->Height>surface->Height) return false;
    RedrawWindow(GetParent(window),nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    int match=0,total=0;
    std::set<int> colors;
    for(int y=2;y<image->Height-2;y+=2) for(int x=2;x<std::min(46,image->Width-2);x+=2) {
        Point2D src{x,y},dst{bounds.left+x,bounds.top+y};
        const int expected=image->GetPixel(&src);
        if(surface->GetPixel(&dst)==expected) {++match;colors.insert(expected);}
        ++total;
    }
    return total && match==total && colors.size()>3;
}
void checkDiplomacy(HWND dialog) {
    const HWND mine=GetDlgItem(dialog,0x5340);
    verify(IsWindowVisible(mine),"diplomacy research is visible");
    const HWND projects=GetDlgItem(dialog,0x5349);
    const auto points=Research::points;
    const auto owned=Research::unlocked;
    auto select=[](HWND button) {
        RECT r{};
        GetClientRect(button,&r);
        const auto center=MAKELPARAM(r.right/2,r.bottom/2);
        SendMessageA(button,WM_LBUTTONDOWN,MK_LBUTTON,center);
        SendMessageA(button,WM_LBUTTONUP,0,center);
    };
    RECT player{},button{},bounds{},list{};
    GetWindowRect(GetDlgItem(dialog,1041),&player);
    GetWindowRect(mine,&button);
    GetWindowRect(dialog,&bounds);
    GetWindowRect(projects,&list);
    verify(button.left>=player.right && button.top<player.bottom && player.top<button.bottom &&
        button.right<=bounds.right && list.bottom<=bounds.bottom,"diplomacy layout fits beside player rows");
    select(mine);
    auto image=OwnerDraw::WinData[projects].ControlImage;
    verify(IsWindowVisible(projects) && image,"diplomacy shows local research icons");
    verify(paintedDiplomacyImage(projects),"diplomacy icon pixels reach the dialog surface");
    int size=std::min(48,image->Height),pitch=size+4;
    auto sample=[&](int x) {
        Point2D point{x,(image->Height-size)/2};
        return image->GetPixel(&point);
    };
    verify(sample(0)==sample(pitch) && sample(pitch)==sample(pitch*2) &&
        sample(0)!=sample(pitch*3),"three badges in one horizontal row");
    SendMessageA(projects,WM_MOUSEMOVE,0,MAKELPARAM(size/2,image->Height/2));
    const auto tip=GetDlgItem(dialog,0x534C);
    verify(IsWindowVisible(tip) && OwnerDraw::WinData[tip].ControlImage,"hover displays project details");
    verify(paintedDiplomacyImage(tip),"diplomacy tooltip pixels reach the dialog surface");
    RECT menu{};
    OwnerDraw::GetRectangle(dialog,&menu);
    auto surface=DSurface::Alternate;
    RECT viewport{0,0,surface->Width,surface->Height},visible{};
    if(IntersectRect(&visible,&menu,&viewport))
        captureHud("diplomacy-research.ppm",visible.left,visible.top,visible.right-visible.left,
            visible.bottom-visible.top,surface);
    SendMessageA(projects,WM_MOUSELEAVE,0,0);
    verify(!IsWindowVisible(tip),"leaving an icon hides its details");
    const auto other=GetDlgItem(dialog,0x5341);
    verify(IsWindowVisible(other),"second player has a research button");
    select(other);
    verify(!IsWindowVisible(projects) && !IsWindowVisible(tip) &&
        IsWindowVisible(GetDlgItem(dialog,0x5348)),"empty house clears icons and tooltip");
    select(mine);
    verify(IsWindowVisible(projects) && Research::points==points &&
        Research::unlocked==owned,"switching players is read-only");
    // Include a partial final page and projects from saves without purchase order.
    const auto originalSize=Research::options.size();
    const int capacity=(image->Width+4)/pitch;
    int house=HouseClass::CurrentPlayer->ArrayIndex;
    for(int i=3;i<=capacity;++i) {
        auto option=Research::options.front();
        option.id="diplomacy-test-"+std::to_string(i);
        Research::unlocked[house].insert(option.id);
        Research::options.push_back(std::move(option));
    }
    SendMessageA(dialog,WM_TIMER,0,0);
    auto next=GetDlgItem(dialog,0x534B),previousPage=GetDlgItem(dialog,0x534A);
    verify(IsWindowVisible(next) && IsWindowEnabled(next),"long research row enables next page");
    select(next);
    verify(!IsWindowEnabled(next) && IsWindowEnabled(previousPage) && sample(0)!=sample(pitch) &&
        sample(pitch)==sample(2*pitch),"last page clears unused icon slots");
    select(previousPage);
    verify(!IsWindowEnabled(previousPage) && sample(0)==sample(pitch),"previous page restores icons");
    Research::options.resize(originalSize);
    Research::unlocked=owned;
    diplomacyChecked=true;
    select(GetDlgItem(dialog,1059));
}
void captureHud(const char* name,int x0,int top,int width,int height,Surface* surface) {
    const int pitch=surface->GetPitch();
    auto base=static_cast<BYTE*>(surface->Lock(x0,top));
    if(!base) return;
    std::ofstream file(name,std::ios::binary);
    file<<"P6\n"<<width<<" "<<height<<"\n255\n";
    for(int y=0;y<height;++y) {
        auto row=reinterpret_cast<const WORD*>(base+y*pitch);
        for(int x=0;x<width;++x) {
            WORD value=row[x];
            for(auto shift:std::array<std::pair<int,int>,3>{{{Drawing::RedShiftLeft,Drawing::RedShiftRight},
                {Drawing::GreenShiftLeft,Drawing::GreenShiftRight},{Drawing::BlueShiftLeft,Drawing::BlueShiftRight}}}) {
                int mask=255>>shift.second;
                file.put(static_cast<char>(((value>>shift.first)&mask)*255/mask));
            }
        }
    }
    surface->Unlock();
}
}
// Run after the native modal loop has initialized its close-result slot.
DEFINE_HOOK(0x658718,SF_TestDiplomacyControls,5) {
    if(!diplomacyChecked && GetFileAttributesA("Starfall.test")!=INVALID_FILE_ATTRIBUTES)
        checkDiplomacy(R->ESI<HWND>());
    return 0;
}
DEFINE_HOOK(0x55B4EB,SF_TestResearchHUD,5) {
    static const bool enabled=GetFileAttributesA("Starfall.test")!=INVALID_FILE_ATTRIBUTES;
    if(!enabled) return 0;
    int frame=Unsorted::CurrentFrame;
    if(frame>=96) {
        Research::clearPresentationCaches(true);
        verify(ResearchAudio::voices().empty() && ResearchAudio::sampleCache().size()==0,"scenario reset releases voices and cached audio");
        TerminateProcess(GetCurrentProcess(),0);return 0;
    }
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    if(loading && frame<=45) return 0;
    if(frame!=20 && frame!=32 && frame!=45 && frame!=60 && frame!=88) return 0;
    // Single-player launches can supply different engine seeds. Pin this fixture
    // and clear any locally generated board before exercising all three refreshes.
    if(frame==20) {
        Game::Seed=static_cast<int>(3261594250u);
        Research::savedOffers.clear();Research::seenOffers.clear();Research::clearOfferCache();
    }
    Research::ensureReady();
    int house=HouseClass::CurrentPlayer->ArrayIndex;
    auto view=DSurface::ViewBounds;
    int x=view.X+view.Width-56,y=view.Y+24;
    Point2D tab{x+24,y+24};
    auto previous=WWMouseClass::Instance->XY1;
    WWMouseClass::Instance->SetCoords(tab);
    WWMouseClass::Instance->XY1=tab;
    int track=Drawing::RGB_To_Int(42,64,79),lit=Drawing::RGB_To_Int(60,218,251);
    int cx=view.X+view.Width/2,cy=view.Y+view.Height/2;
    int gap=view.Width>=720?16:8;
    int cw=std::min({244,(view.Width-24-2*gap)/3,(view.Height-94)*2/3});
    int ch=cw*3/2;
    int refreshY=view.Y+(view.Height-ch-72)/2+38+ch+20;
    const char* country=HouseClass::CurrentPlayer->Type->ID;
    static vector<int> initial;
    if(frame==20) {
        verify(Research::options.size()==9,"registered projects");
        verify(Research::isEnabled(),"research enabled by default");
        Research::points[house]=100;
        const auto board=Research::offers(house,country);
        verify(!board.empty(),"funded board before disable");
        const auto savedBoard=Research::savedOffers.at(house);
        verify(WritePrivateProfileStringA("Starfall.Research","Enabled","no",configPath),"write disabled research");
        Research::readConfiguration(configPath);
        Research::award(house,50);
        verify(!Research::isEnabled() && Research::offers(house,country).empty() &&
            !Research::purchase(house,board[0],country) && !Research::refresh(house,country,0,board[0]) &&
            Research::balance(house)==100 && Research::savedOffers.at(house)==savedBoard,"disabled research blocks rewards and spending");
        Point2D border{x+24,y+1};
        const int marker=Drawing::RGB_To_Int(255,0,255);
        DSurface::Composite->SetPixel(&border,marker);
        SF_ResearchDraw(nullptr);
        verify(pixel(border.X,border.Y)==marker,"disabled HUD not drawn");
        verify(!WindowMouse::button(tab,1) && !WindowMouse::button(tab,4) && !Research::expanded,"disabled HUD passes input");
        verify(WritePrivateProfileStringA("Starfall.Research","Enabled",nullptr,configPath),"restore research default");
        Research::readConfiguration(configPath);
        verify(Research::isEnabled() && Research::offers(house,country)==board,"reenabled board retained");
        char originalPath[MAX_PATH]{};
        strcpy_s(originalPath,configPath);
        GetFullPathNameA("research-unregistered.ini",MAX_PATH,configPath,nullptr);
        {std::ofstream ini(configPath);ini<<"[Starfall.Research]\nOptions=SF_Repair\n";}
        Research::readConfiguration(configPath);
        verify(Research::options.empty(),"legacy options ignored");
        {std::ofstream ini(configPath);ini<<"[Starfall.Research]\n";}
        Research::readConfiguration(configPath);
        verify(Research::options.empty(),"unregistered projects empty");
        {
            std::ofstream ini(configPath);
            ini<<"[Starfall.Research]\nAllowCardRefresh=yes\n[Starfall.ResearchTypes]\n";
            for(int i=0;i<4;++i) ini<<i<<"=Red"<<i<<"\n";
            for(int i=0;i<4;++i) ini<<"[Red"<<i<<"]\nRarity=Insane\nEffect=GiveCredits\nCredits=10\n";
        }
        Research::readConfiguration(configPath);Research::clearOfferCache();
        verify(Research::options.size()==4 && Research::options[0].rarity==Research::Rarity::Insane &&
            Research::offers(house,country).empty(),"insane disabled by default");
        WritePrivateProfileStringA("Starfall.Research","RarityWeight.Insane","1",configPath);
        Research::readConfiguration(configPath);Research::clearOfferCache();
        const auto red=Research::offers(house,country);
        verify(red.size()==3 && Research::refresh(house,country,0,red.front()) &&
            Research::offers(house,country).front()!=red.front(),"insane draw and refresh");
        WritePrivateProfileStringA("Starfall.Research","RarityWeight.Insane","0",configPath);
        Research::readConfiguration(configPath);Research::clearOfferCache();
        verify(Research::offers(house,country).empty(),"zero excludes saved insane offers");
        Research::refreshedSlots.erase(house);
        Research::savedOffers[house]=savedBoard;
        Research::clearOfferCache();
        strcpy_s(configPath,originalPath);
        Research::readConfiguration(configPath);
        verify(Research::options.size()==9,"registered projects restored");
        verify(RulesClass::Instance->GenericClick>=0 && RulesClass::Instance->GenericClick<VocClass::Array.Count,"native click sound configured");
        verify(std::all_of(Research::soundFiles.begin(),Research::soundFiles.end(),[](const auto& p){return p=="ui-test.wav";}) && !ResearchAudio::load("ui-test.wav").data.empty(),"configured wav loaded");
        verify(!ResearchAudio::load("builtin:cards").data.empty() && !ResearchAudio::load("builtin:select").data.empty() &&
            !ResearchAudio::load("builtin:available").data.empty(),"embedded wav loaded");
        auto configured=Research::soundFiles[0];Research::soundFiles[0]="builtin:cards";
        bool builtinPlayed=ResearchAudio::play(Research::SoundCue::Button);
        Research::soundFiles[0]=configured;
        verify(builtinPlayed,"embedded wav playback");
        auto oldVoices=ResearchAudio::voices().size();
        bool wavPlayed=ResearchAudio::play(Research::SoundCue::Button);
        verify(wavPlayed && (GameOptionsClass::Instance.SoundVolume<=0 || ResearchAudio::voices().size()>oldVoices),"wav playback queued");
        auto playing=ResearchAudio::voices().size();
        auto voice=playing?ResearchAudio::voices().back().get():nullptr;
        auto data=voice?voice->header.lpData:nullptr;
        for(int i=0;i<70;++i) ResearchAudio::sampleCache().get("eviction-"+std::to_string(i),[] {
            ResearchAudio::Sample sample;sample.data.resize(32768);return sample;
        },[](const auto& key,const auto& sample){return sample.data.capacity()+2*(key.size()+1)+sizeof(sample);});
        verify(!ResearchAudio::sampleCache().contains("ui-test.wav"),"playing sound evicted from sample cache");
        Research::readConfiguration(configPath);
        verify(ResearchAudio::sampleCache().size()==0 && ResearchAudio::sampleCache().bytes()==0 &&
               ResearchAudio::voices().size()==playing && (!voice || (voice->header.lpData==data &&
               data==reinterpret_cast<LPSTR>(voice->data.data()) && voice->device)),"configuration reload preserves playing audio and clears cache");
        Research::points[house]=0;
        SF_ResearchDraw(nullptr);
        verify(pixel(x+24,y+1)==track && pixel(x+24,y+46)==track && pixel(x+1,y+24)==track,"empty border");
        input(tab,1);input({tab.X-100,tab.Y+100},4);
        input(tab,16);input(tab,64);
        click(tab);verify(!Research::expanded,"disabled at zero");
        Research::award(house,99);
        click(tab);verify(!Research::expanded,"disabled at 99");
        input(tab,1);Research::award(house,1);input(tab,4);
        verify(!Research::expanded,"funded mid-click");
        input(tab,1);
        reinterpret_cast<WNDPROC>(0x7775C0)(Game::hWnd,WM_CANCELMODE,0,0);
        input({view.X+3,view.Y+view.Height+3},4);
        verify(!Research::expanded,"canceled HUD release cannot issue an order");
        verify(WindowMouse::send(tab,WM_LBUTTONDBLCLK) && WindowMouse::button(tab,4) &&
            Research::expanded,"opened at 100");
        SF_ResearchDraw(nullptr);
        int top=pixel(x+24,y+1);
        verify(top!=track && top==pixel(x+1,y+24) && top==pixel(x+24,y+46),"full border");
        Research::points[house]=99;
        SF_ResearchDraw(nullptr);
        verify(!Research::expanded,"closed after balance changed");
        SF_ResearchDraw(nullptr);
        Research::award(house,51);
        click(tab);
        verify(WritePrivateProfileStringA("Starfall.Research","AllowCardRefresh","no",configPath),"write disabled fixture");
        Research::readConfiguration(configPath); Research::clearOfferCache();
        auto disabledBoard=Research::offers(house,country);
        verify(!Research::allowCardRefresh && !Research::refresh(house,country,0,disabledBoard.front()),"ini disables refresh");
        verify(WritePrivateProfileStringA("Starfall.Research","AllowCardRefresh","yes",configPath),"restore fixture");
        Research::readConfiguration(configPath); Research::clearOfferCache();
        initial=Research::offers(house,country);
        verify(initial.size()==3,"three cards");
        WWMouseClass::Instance->XY1={cx,view.Y+4};
        SF_ResearchDraw(nullptr);
        Sleep(160);SF_ResearchDraw(nullptr);
        captureHud("research-opening.ppm",cx-(3*cw+2*gap)/2-18,view.Y+(view.Height-ch-72)/2-10,3*cw+2*gap+36,ch+92);
        Sleep(360);
        SF_ResearchDraw(nullptr);
        verifyTransparentOverlay({view.X+2,view.Y+view.Height-3},{cx,cy},{x+24,y+1});
        captureHud("research-cards.ppm",cx-(3*cw+2*gap)/2-18,view.Y+(view.Height-ch-72)/2-10,3*cw+2*gap+36,ch+92);
        input({cx,cy},1);input({view.X+3,view.Y+view.Height-3},4);
        verify(Research::expanded && Research::unlocked[house].empty(),"card drag cannot select behind");
        input({cx-cw-gap,refreshY},1);input({cx-cw-gap,refreshY},4);
    }
    if(frame==32) {
        auto rows=Research::offers(house,country);
        verify(rows[0]!=initial[0] && rows[1]==initial[1] && rows[2]==initial[2] &&
            Research::slotUsed(house,0) && !Research::slotUsed(house,1) && !Research::slotUsed(house,2),"independent refresh");
        click({cx-cw-gap,refreshY});
        click({cx,refreshY});
        click({cx+cw+gap,refreshY});
    }
    if(frame==45) {
        verify(Research::refreshedSlots.at(house)==7 && Research::balance(house)==150,"three free refreshes");
        auto rows=Research::offers(house,country);
        {
            std::ofstream diagnostic("hud.log",std::ios::app);
            diagnostic<<"INFO offers seed="<<static_cast<unsigned>(Game::Seed)<<" house="<<house<<" initial=";
            for(int index:initial) diagnostic<<Research::options[index].id<<',';
            diagnostic<<" refreshed=";
            for(int index:rows) diagnostic<<Research::options[index].id<<',';
            diagnostic<<'\n';
        }
        const std::set<string> eligible={"repair","drain","riposte","reserve","focus","extra"};
        verify(std::set<int>(rows.begin(),rows.end()).size()==3 && std::all_of(rows.begin(),rows.end(),[&](int i){
            return i>=0 && i<static_cast<int>(Research::options.size()) && eligible.contains(Research::options[i].id);
        }),"unique eligible projects");
        {std::ofstream output("expected-cards.txt");for(int index:rows) output<<Research::options[index].id<<"\n";}
        click(tab); click(tab);
        verify(Research::offers(house,country)==rows && !Research::canRefresh(house,country,0),"reopen retains cards");
        verify(ScenarioClass::SaveGame("Saved Games\\ResearchHUD.sav",L"ResearchHUD"),"saved");
    }
    if(frame==60) {
        auto rows=Research::offers(house,country);
        std::ifstream expected("expected-cards.txt");
        for(int index:rows) {string id;expected>>id;verify(id==Research::options[index].id,"retained option");}
        verify(rows.size()==3 && Research::refreshedSlots.at(house)==7 && !Research::canRefresh(house,country,1),"retained refresh limits");
        if(!Research::expanded) click(tab);
        input({cx,cy},1);input({cx,cy},4);
        verify(Research::expanded,"purchase queued");
    }
    if(frame==88) {
        verify(Research::balance(house)==50 && Research::unlocked[house].size()==1 && !Research::refreshedSlots.contains(house),"purchase");
        Sleep(750);SF_ResearchDraw(nullptr);
        verify(!Research::expanded,"purchase animation closes panel");
        Research::award(house,200);
        for(int i=0;i<2;++i) {
            auto option=Research::offers(house,country).front();
            verify(Research::purchase(house,option,country),"prior research");
        }
        verify(Research::balance(house)==50,"retained points");
        SF_ResearchDraw(nullptr);
        verify(pixel(x+1,y+23)==track && pixel(x+1,y+24)==lit &&
            pixel(x+46,y+23)==track && pixel(x+46,y+24)==lit &&
            pixel(x+24,y+1)==track && pixel(x+24,y+46)==lit,"half border");
        click(tab);verify(!Research::expanded,"disabled at 50");
        verify(Research::unlockOrder[house].size()==3,"completed icons");
        auto& firstId=Research::unlockOrder[house].front();
        auto first=std::find_if(Research::options.begin(),Research::options.end(),
            [&](const auto& o){return o.id==firstId;});
        verify(first!=Research::options.end(),"first badge");
        first->icon.clear(); // Exercise the default rather than a randomly chosen project icon.
        int left=x-4-40,top=y+4;
        WWMouseClass::Instance->XY1=tab;
        SF_ResearchDraw(nullptr);
        int plate=Drawing::RGB_To_Int(14,23,35);
        int transparent=0,colored=0;
        for(int py=2;py<38;++py) for(int px=2;px<38;++px)
            if(pixel(left+px,top+py)==plate) ++transparent;else ++colored;
        verify(pixel(left+1,top+1)==plate && transparent>40 && colored>40,"transparent badge on dark plate");
        captureHud("completed-badges.ppm",left-92,top-4,136,48);
        Point2D badge{left+20,top+20};
        WWMouseClass::Instance->XY1=badge;
        SF_ResearchDraw(nullptr);
        verify(pixel(left,top)==Drawing::RGB_To_Int(182,159,255),"left badge hover");
        input({left+38,top+20},1);input({left+38,top+20},4);
        WWMouseClass::Instance->XY1=tab;
        SF_ResearchDraw(nullptr);
        auto cur=WWMouseClass::Instance->XY1;
        std::ofstream("hud.log",std::ios::app)<<"pointer "<<cur.X<<","<<cur.Y<<" tab "<<x<<","<<y<<" sample "<<pixel(x+42,y+59)<<" expected "<<Drawing::RGB_To_Int(14,23,36)<<"\n";
        captureHud("hud-half.ppm",view.X+view.Width-276,y-2,276,128);
        verify(pixel(x+42,y+59)==Drawing::RGB_To_Int(14,23,36),"balance tooltip");
        Research::award(house,50);
        verify(Research::offers(house,country).size()==3 && !Research::canRefresh(house,country,0) &&
            !Research::slotUsed(house,0),"no alternative preserves allowance");
        reinterpret_cast<void(__cdecl*)()>(0x6586D0)();
        verify(diplomacyChecked,"native diplomacy dialog opens and returns");
    }
    WWMouseClass::Instance->SetCoords(previous);
    return 0;
}
