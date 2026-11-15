#pragma once

namespace native_drag_hud {
constexpr char DragPackage[]="HUD_Drag.fng";
constexpr char StockPackage[]="HUD_SingleRace.fng";
constexpr std::uintptr_t PackageManagerRva=0x697B40;
constexpr std::uintptr_t FengRva=0x697A78;
constexpr std::uintptr_t DeviceRva=0x6B0ABC;
struct Position { float x,y,z; };
struct HudSide { void* group{}; Position original{}; };

using UpdateFunction=void(__thiscall*)(void*,void*,float);
using BeginRenderFunction=void(__thiscall*)(void*,void*);
using MultiImageFunction=void(__thiscall*)(void*,void*,void*);
using PopFunction=bool(__thiscall*)(void*,const char*);
UpdateFunction originalUpdate{};
BeginRenderFunction originalBeginRender{};
PopFunction originalPop{};
MultiImageFunction originalMultiImage{};

std::vector<std::uint8_t> layoutBytes;
std::vector<std::uint8_t> resourceBytes;
bool prepared{}, loaded{}, failed{}, pushed{}, ready{}, stockVisible{};
void* dragPackage{};
void* stockPackage{};
LONG attemptedRace{-1};
LONG confirmedLaunch{-1};
LONG boundGeneration{-1};
LONG finishedLaunch{-1};
LONG finishedGeneration{-1};
void (__cdecl* originalPostRace)(){};
void (__cdecl* originalRaceOverCamera)(){};
void (__thiscall* originalSetRoaming)(void*){};
unsigned shiftSerial{};
ULONGLONG shiftUntil{};
const wchar_t* shiftLabel{};
int lastCountdown{-1};
HudSide leftSide{},rightSide{};
ULONGLONG nextAspectCheck{};
float hudSpread{-1.0f};

template<class Function>
Function Native(std::uintptr_t address) {
    return reinterpret_cast<Function>(gBase+address-0x400000);
}

template<class Value>
Value& Field(void* object,std::size_t offset) {
    return *reinterpret_cast<Value*>(static_cast<std::uint8_t*>(object)+offset);
}

constexpr unsigned Hash(const char* name) {
    unsigned hash=0xffffffff;
    for (;*name;++name) {
        unsigned char value=static_cast<unsigned char>(*name);
        if (value>='a' && value<='z') value=static_cast<unsigned char>(value-'a'+'A');
        hash=33*hash+value;
    }
    return hash;
}

void ReadAsset(const std::filesystem::path& path,const char* expected,
    std::size_t maximum,std::vector<std::uint8_t>& data) {
    Require(HashFile(path)==expected,"Drag HUD asset differs from the packaged file.");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    const auto size=static_cast<std::streamoff>(file.tellg());
    Require(size>0 && static_cast<std::uint64_t>(size)<=maximum,"Invalid Drag HUD asset length.");
    data.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    Require(static_cast<bool>(file.read(reinterpret_cast<char*>(data.data()),size)),
        "Could not read the Drag HUD asset.");
}

void Prepare() {
    if (prepared) return;
    auto directory=ModulePath(gModule).parent_path()/L"DragHUD";
    try {
        ReadAsset(directory/L"HUD_Drag.fng",
            "c8a63ea97a087c4173d2bba6fc568b5aba90abdff819761c41e8b40120786a08",
            65536,layoutBytes);
        ReadAsset(directory/L"HUDTEXDRAG.BIN",
            "2926e56da455d53e04c46cca5fb28ad9f9f83758bcafe85140aa8c284c853a1b",
            2097152,resourceBytes);
        prepared=true;
    } catch (const std::exception&) {
    }
}

void* ManagerEntry(const char* name) {
    using Find=void*(__thiscall*)(void*,const char*);
    const auto manager=*reinterpret_cast<void**>(gBase+PackageManagerRva);
    return manager ? Native<Find>(0x5858D0)(manager,name) : nullptr;
}

bool LoadAssets() {
    if (loaded) return true;
    if (!prepared || failed || ManagerEntry(DragPackage)) return false;
    failed=true;
    using LoadChunk=bool(__cdecl*)(void*);
    auto* aligned=VirtualAlloc(nullptr,resourceBytes.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!aligned) return false;
    std::memcpy(aligned,resourceBytes.data(),resourceBytes.size());
    if (!Native<LoadChunk>(0x560580)(aligned)) {
        VirtualFree(aligned,0,MEM_RELEASE);
        return false;
    }
    using GetTexture=void*(__cdecl*)(unsigned,int,int);
    for (const unsigned hash : {Hash("DRAG_RPM_NEEDLE"),Hash("DRAG_RPM_10000_LINES"),
            Hash("DRAG_MAIN_METERMASK"),Hash("FONT_MW_BODY"),Hash("BASEPOLY")}) {
        auto texture=Native<GetTexture>(0x55CFD0)(hash,0,1);
        if (!texture || Field<unsigned>(texture,0x0C)!=hash ||
            !Field<void*>(texture,0x50) || !Field<void*>(texture,0) ||
            !Field<void*>(Field<void*>(texture,0),0x10)) {
            return false;
        }
    }
    auto* bytes=static_cast<std::uint8_t*>(aligned);
    const auto packSize=Field<unsigned>(bytes,4)+8;
    if (!Native<LoadChunk>(0x591E50)(bytes+packSize)) return false;
    const auto firstFontSize=Field<unsigned>(bytes+packSize,4)+8;
    if (!Native<LoadChunk>(0x591E50)(bytes+packSize+firstFontSize)) return false;
    if (!Native<LoadChunk>(0x585AA0)(layoutBytes.data())) return false;
    auto entry=ManagerEntry(DragPackage);
    loaded=entry && Field<void*>(entry,0x18)==layoutBytes.data();
    failed=!loaded;
    return loaded;
}

void* Find(unsigned hash,unsigned type) {
    using FindObject=void*(__thiscall*)(void*,unsigned);
    auto object=Native<FindObject>(0x5F3760)(dragPackage,hash);
    return object && Field<unsigned>(object,0x18)==type ? object : nullptr;
}

struct Objects {
    void* needle{};
    void* tachLines{};
    void* gear{};
    void* speed{};
    void* units{};

    void* time{};
    void* place{};
    void* fieldSize{};
    void* shiftLight{};
    void* shiftGroup{};
    void* shiftMessage{};
    void* shiftShadow{};
    void* countdown{};
    void* countdownText{};
    void* countdownShadow{};
    void* nitrous{};
    void* nitrousIcon{};
    void* heatIcon{};
    void* heatPulse{};
    void* speedbreakerIcon{};
    void* engineHeat{};
    void* redline{};
    void* speedbreaker{};
} objects;

HudSide FindSide(unsigned hash) {
    auto group=Find(hash,5);
    auto data=group ? Field<void*>(group,0x2C) : nullptr;
    return data ? HudSide{group,Field<Position>(data,0x1C)} : HudSide{};
}

void UpdateLayout() {
    const auto now=GetTickCount64();
    if (now<nextAspectCheck) return;
    nextAspectCheck=now+500;
    auto device=*reinterpret_cast<IDirect3DDevice9**>(gBase+DeviceRva);
    IDirect3DSurface9* backBuffer{};
    if (!device || FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backBuffer)) || !backBuffer)
        return;
    D3DSURFACE_DESC desc{};
    const auto result=backBuffer->GetDesc(&desc);
    backBuffer->Release();
    if (FAILED(result) || !desc.Width || !desc.Height) return;

    const float aspect=static_cast<float>(desc.Width)/static_cast<float>(desc.Height);
    const float spread=std::clamp((aspect*480.0f-640.0f)*0.375f,0.0f,150.0f);
    if (spread==hudSpread) return;
    hudSpread=spread;
    using SetPosition=void(__thiscall*)(void*,const Position&,bool);
    const auto setPosition=Native<SetPosition>(0x5F33A0);
    if (leftSide.group) {
        auto position=leftSide.original;
        position.x-=spread;
        setPosition(leftSide.group,position,false);
    }
    if (rightSide.group) {
        auto position=rightSide.original;
        position.x+=spread;
        setPosition(rightSide.group,position,false);
    }
}

bool Bind() {
    leftSide=FindSide(0x157C5B37);
    rightSide=FindSide(0x5D0101F1);
    nextAspectCheck=0;
    hudSpread=-1.0f;
    objects.needle=Find(Hash("3rdPersonNeedle"),1);
    objects.tachLines=Find(Hash("TAC_Lines_7500"),1);
    objects.gear=Find(Hash("3rdPersonGear"),2);
    objects.speed=Find(Hash("TAC_NUMBERS"),2);
    objects.units=Find(Hash("3rdPersonSpeedUnits"),2);

    objects.time=Find(Hash("CurrentLapTime"),2);
    objects.place=Find(0x3BBD6268,2);
    objects.fieldSize=Find(0x9C183BF8,2);
    objects.shiftLight=Find(Hash("Shift_light"),1);
    objects.shiftGroup=Find(Hash("SHIFT_GROUP"),5);
    objects.shiftMessage=Find(Hash("ShiftMessage"),2);
    objects.shiftShadow=Find(Hash("ShiftMessage_Shadow"),2);
    objects.countdown=Find(0xA7B3AA16,5);
    objects.countdownText=Find(0xB969E9EF,2);
    objects.countdownShadow=Find(0x2DA220AA,2);
    objects.nitrous=Find(0xEDFB6D37,12);
    objects.nitrousIcon=Find(0x27DDF583,5);
    objects.heatIcon=Find(0x0240496A,5);
    objects.heatPulse=Find(Hash("ENGINE_HEAT_SHIFTLIGHT_GROUP"),5);
    objects.speedbreakerIcon=Find(0x11448B5C,5);
    objects.engineHeat=Find(0x7996C8B1,12);
    objects.redline=Find(Hash("RPM_Redline"),12);
    objects.speedbreaker=Find(Hash("Speedbreaker_Meter_Multi_Image"),12);
    const auto visible=Native<void(__cdecl*)(void*,bool)>(0x570460);
    visible(objects.shiftLight,false);
    visible(objects.heatPulse,false);
    visible(Find(Hash("3rdPersonSpeed"),5),false);
    visible(Find(Hash("RPM_fill"),1),false);
    visible(Find(0xC18C12FD,2),false);
    visible(Find(0xC18C12FE,2),false);
    visible(Find(0xA48393EA,2),false);
    return objects.needle && objects.tachLines && objects.gear && objects.speed && objects.time &&
        objects.place && objects.fieldSize && objects.shiftLight && objects.shiftGroup &&
        objects.shiftMessage && objects.shiftShadow && objects.countdown && objects.countdownText &&
        objects.countdownShadow && objects.nitrous && objects.engineHeat && objects.speedbreaker;
}

void ResetPackage(void* feng) {
    auto entry=pushed ? ManagerEntry(DragPackage) : nullptr;
    const bool ownsLivePackage=entry && Field<void*>(entry,0x20)==dragPackage;
    if (ownsLivePackage && feng) {
        Field<std::uint16_t>(entry,0x24)=0;
        originalPop(feng,DragPackage);
    }
    pushed=ready=stockVisible=false;
    dragPackage=stockPackage=nullptr;
    objects={};
    leftSide=rightSide={};
    shiftUntil=0;
    shiftLabel=nullptr;
    lastCountdown=-1;
    boundGeneration=-1;
}

bool Activate(void* feng,void* stock) {
    const auto race=Load(loading::launchSerial);
    if (attemptedRace==race) return false;
    if (!*reinterpret_cast<void**>(gBase+DeviceRva) ||
        !*reinterpret_cast<void**>(gBase+0x68FFB4))
        return false;
    attemptedRace=race;
    if (!LoadAssets()) {
        return false;
    }
    using Push=void*(__thiscall*)(void*,const char*,int);
    dragPackage=Native<Push>(0x572260)(feng,DragPackage,0x66);
    if (!dragPackage) {
        return false;
    }
    pushed=true;
    if (!Bind()) {
        ResetPackage(feng);
        return false;
    }
    stockPackage=stock;
    auto setPermanent=Native<void(__thiscall*)(void*,const char*,int)>(0x591DD0);
    setPermanent(*reinterpret_cast<void**>(gBase+PackageManagerRva),DragPackage,1);
    ready=true;
    return true;
}

using Visibility=void(__cdecl*)(void*,bool);
using Text=int(__cdecl*)(void*,const char*,...);
using Script=void(__cdecl*)(void*,unsigned,bool);
using Color=void(__cdecl*)(void*,unsigned);
using SetString=void(__cdecl*)(void*,const wchar_t*);

void __fastcall MultiImageHook(void* renderer,void*,void* object,void* renderObject) {
    auto data=ready && object && (object==objects.nitrous || object==objects.speedbreaker ||
        object==objects.engineHeat) ? Field<void*>(object,0x2C) : nullptr;
    if (!data) {
        originalMultiImage(renderer,object,renderObject);
        return;
    }

    auto& pivotX=Field<float>(data,0x84);
    auto& pivotY=Field<float>(data,0x88);
    const float x=pivotX,y=pivotY;
    pivotX=x-0.5f;
    pivotY=y-0.5f;
    originalMultiImage(renderer,object,renderObject);
    pivotX=x;
    pivotY=y;
}

void Drive(void* hud,void* player,const Snapshot& snapshot,bool totaled,std::uintptr_t simable) {
    UpdateLayout();
    auto visible=Native<Visibility>(0x570460);
    auto text=Native<Text>(0x5910E0);
    auto setString=Native<SetString>(0x583B10);
    if (objects.units) setString(objects.units,L"KM/H");
    auto script=Native<Script>(0x5704A0);
    const float rpm=std::isfinite(snapshot.rpm) ? snapshot.rpm : 0;
    const float redline=std::isfinite(snapshot.redline) ? snapshot.redline : 10000.0f;
    const unsigned limit=redline<=0 ? 10000 : redline<=7000 ? 7000 :
        redline<=8000 ? 8000 : redline<=9000 ? 9000 : 10000;
    const auto lines=limit==7000 ? Hash("DRAG_RPM_7000_LINES") :
        limit==8000 ? Hash("DRAG_RPM_8000_LINES") : limit==9000 ?
        Hash("DRAG_RPM_9000_LINES") : Hash("DRAG_RPM_10000_LINES");
    using SetTexture=void(__cdecl*)(void*,unsigned);
    Native<SetTexture>(0x571040)(objects.tachLines,lines);
    visible(objects.tachLines,true);
    using Rotate=void(__cdecl*)(void*,float);
    Native<Rotate>(0x5705A0)(objects.needle,std::clamp(rpm/static_cast<float>(limit),0.0f,1.0f)*87.5f-43.0f);

    const int gear=std::clamp(snapshot.gear,2,10)-1;
    text(objects.gear,"%c",snapshot.gear>=2 ? '0'+gear : snapshot.gear==1 ? 'N' : 'R');
    const float speed=std::isfinite(snapshot.speedKph) ? snapshot.speedKph : 0;
    const int displaySpeed=static_cast<int>(std::clamp(speed,0.0f,999.0f)+0.5f);
    visible(objects.speed,true);
    Native<Color>(0x570C90)(objects.speed,0xFFECECEC);
    text(objects.speed,"%03d",displaySpeed);

    auto status=*reinterpret_cast<void**>(gBase+RaceStatusRva);
    if (!simable) {
        setString(objects.place,L"-");
        setString(objects.fieldSize,L"-");
        setString(objects.time,L"0.00");
    }
    if (status && simable) {
        using RacerInfo=void*(__thiscall*)(void*,void*);
        using RacerCount=int(__thiscall*)(void*);
        using Elapsed=float(__thiscall*)(void*);
        if (const auto racer=Native<RacerInfo>(0x629670)(status,reinterpret_cast<void*>(simable))) {
            text(objects.place,"%d",std::clamp(Field<int>(racer,0x34),0,99));
            text(objects.fieldSize,"%d",std::clamp(Native<RacerCount>(0x612230)(status),0,99));
            const float elapsed=Native<Elapsed>(0x61B440)(status);
            text(objects.time,"%.2f",std::isfinite(elapsed) ? static_cast<double>(std::clamp(elapsed,0.0f,999.99f)) : 0.0);
        }
    }

    using MeterRotation=void(__cdecl*)(void*,float,float,float,float,float);
    auto meter=Native<MeterRotation>(0x570790);
    const auto unit=[](float value) { return std::isfinite(value) ? std::clamp(value,0.0f,1.0f) : 0.0f; };
    const auto rotateMeter=[&](void* object,float value,float emptyAngle,float fullAngle) {
        if (!object || !Field<void*>(object,0x2C)) return;
        meter(object,unit(value),0.0f,1.0f,emptyAngle,fullAngle);
        Field<unsigned>(object,0x1C)|=0x2000000;
    };
    const float heat=snapshot.blown ? 1.0f : unit(snapshot.heat);
    visible(objects.engineHeat,heat>0.0f);
    visible(objects.heatIcon,true);
    rotateMeter(objects.engineHeat,heat,-26.5f,0.0f);
    if (objects.heatIcon)
        Native<Color>(0x570C90)(objects.heatIcon,heat>=0.5f ? 0xFFF55439 : 0xFF752E23);
    rotateMeter(objects.redline,redline/static_cast<float>(limit),-43.0f,44.5f);

    using PlayerIndex=int(__thiscall*)(void*);
    using DalFloat=bool(__thiscall*)(void*,int,float*,int,int,int);
    float nitrous{},speedbreaker{};
    bool hasNitrous=false,hasSpeedbreaker=false;
    if (player) {
        const auto playerIndex=reinterpret_cast<PlayerIndex>(Field<void**>(player,0)[7])(player);
        auto dal=reinterpret_cast<void*>(gBase+0x68AD30);
        auto get=Native<DalFloat>(0x4A0350);
        hasNitrous=get(dal,0x7D7,&nitrous,playerIndex,-1,-1);
        hasSpeedbreaker=get(dal,0x7D8,&speedbreaker,playerIndex,-1,-1);
    }
    visible(objects.nitrous,hasNitrous && unit(nitrous)>0.0f);
    visible(objects.nitrousIcon,hasNitrous);
    visible(objects.speedbreaker,hasSpeedbreaker && unit(speedbreaker)>0.0f);
    visible(objects.speedbreakerIcon,hasSpeedbreaker);
    if (objects.nitrousIcon)
        Native<Color>(0x570C90)(objects.nitrousIcon,0xFF40E8E9);
    if (objects.speedbreakerIcon)
        Native<Color>(0x570C90)(objects.speedbreakerIcon,0xFF40E8E9);
    rotateMeter(objects.nitrous,nitrous,-48.0f,0.0f);
    rotateMeter(objects.speedbreaker,speedbreaker,-48.0f,0.0f);

    const auto index=Field<int>(hud,0x34);
    if (index>=0 && index<2) {
        auto countdown=reinterpret_cast<void**>(gBase+0x697DE8)[index];
        const int state=countdown ? Field<int>(countdown,0x30) : -1;
        if (state!=lastCountdown && state>=-1 && state<=3) {
            constexpr unsigned scripts[]={0x16A259,0x535,0xCE01,0xE479,0x3C3C2F7};
            script(objects.countdown,scripts[state+1],true);
            lastCountdown=state;
        }
        if (state>=0 && state<=3) {
            const auto label=state==0 ? L"GO!" : state==1 ? L"1" : state==2 ? L"2" : L"3";
            setString(objects.countdownText,label);
            setString(objects.countdownShadow,label);
        }
    }

    if (snapshot.shiftSerial!=shiftSerial) {
        shiftSerial=snapshot.shiftSerial;
        unsigned animation{};
        switch (snapshot.grade) {
        case drag::ShiftGrade::Early: shiftLabel=L"EARLY SHIFT"; animation=Hash("EARLY"); break;
        case drag::ShiftGrade::Good: shiftLabel=L"GOOD SHIFT"; animation=Hash("GOOD"); break;
        case drag::ShiftGrade::Perfect: shiftLabel=L"PERFECT SHIFT"; animation=Hash("PERFECT"); break;
        case drag::ShiftGrade::Late: shiftLabel=L"OVER REV"; animation=Hash("OVERREV"); break;
        default: shiftLabel=nullptr; break;
        }
        shiftUntil=GetTickCount64()+1900;
        if (animation) script(objects.shiftGroup,animation,true);
    }
    const wchar_t* message=totaled ? L"TOTALLED!" : snapshot.blown ? L"ENGINE BLOWN!" :
        GetTickCount64()<shiftUntil ? shiftLabel : nullptr;
    visible(objects.shiftGroup,message!=nullptr);
    visible(objects.shiftMessage,message!=nullptr);
    visible(objects.shiftShadow,message!=nullptr);
    if (message) {
        setString(objects.shiftMessage,message);
        setString(objects.shiftShadow,message);
        if (totaled || snapshot.blown)
            Native<Color>(0x570C90)(objects.shiftGroup,0xFFFF5740);
    }
    const auto timing=drag::Mechanics::Grade(snapshot.rpm,snapshot.redline);
    const bool shiftWindow=snapshot.playerBound && snapshot.gear>=2 && !snapshot.blown && !totaled &&
        (timing==drag::ShiftGrade::Good || timing==drag::ShiftGrade::Perfect || timing==drag::ShiftGrade::Late);
    visible(objects.shiftLight,shiftWindow);
    visible(objects.heatPulse,shiftWindow && timing==drag::ShiftGrade::Late && heat>=0.5f);
    if (shiftWindow)
        Native<Color>(0x570C90)(objects.shiftLight,timing==drag::ShiftGrade::Late ? 0xFFFF5740 :
            timing==drag::ShiftGrade::Perfect ? 0xFF40E8E9 : 0xFF82ADBA);
}

bool RaceFinished() {
    return finishedLaunch==Load(loading::launchSerial) && finishedGeneration==Load(gLatchCount);
}

void ReleaseFinishedHud() {
    const auto launch=Load(loading::launchSerial);
    if (!pushed && confirmedLaunch!=launch) return;
    finishedLaunch=launch;
    finishedGeneration=Load(gLatchCount);
    if (!pushed) return;
    auto feng=*reinterpret_cast<void**>(gBase+FengRva);
    ResetPackage(feng);
    attemptedRace=-1;
}

void __cdecl PostRaceHook() {
    ReleaseFinishedHud();
    originalPostRace();
}

void __cdecl RaceOverCameraHook() {
    ReleaseFinishedHud();
    originalRaceOverCamera();
}

void __fastcall SetRoamingHook(void* status,void*) {
    const bool leavingCareerDrag=status && Field<int>(status,loading::RaceContextOffset)==loading::CareerContext &&
        loading::IsAuthoredDrag(Field<void*>(status,RaceParametersOffset));
    ReleaseFinishedHud();
    originalSetRoaming(status);
    if (leavingCareerDrag && !Field<void*>(status,RaceParametersOffset))
        RepairDragEngineForRoaming();
}

bool IsVisible() {
    return ready && stockVisible && !RaceFinished() && !Load(loading::dragIntroPending) &&
        *reinterpret_cast<int*>(gBase+0x699BBC)==6 &&
        !Native<bool(__cdecl*)()>(0x4A62E0)();
}

void __fastcall UpdateHook(void* hud,void*,void* player,float step) {
    originalUpdate(hud,player,step);
    const auto name=Field<const char*>(hud,0x2C);
    if (RaceFinished() || !prepared || !name || std::strcmp(name,StockPackage)!=0 ||
        Field<int>(hud,0x28)!=1 ||
        *reinterpret_cast<int*>(gBase+0x699BBC)!=6) return;
    auto entry=ManagerEntry(StockPackage);
    auto stock=entry ? Field<void*>(entry,0x20) : nullptr;
    auto feng=*reinterpret_cast<void**>(gBase+FengRva);
    if (!stock || !feng) return;
    const auto generation=Load(gLatchCount);
    const auto launch=Load(loading::launchSerial);
    const auto status=*reinterpret_cast<std::uint8_t**>(gBase+RaceStatusRva);
    auto parameters=status ? *reinterpret_cast<void**>(status+RaceParametersOffset) : nullptr;
    const bool currentDrag=loading::IsAuthoredDrag(parameters);
    const bool supportedContext=loading::DragContext(Load(loading::context));
    const bool sameLaunch=confirmedLaunch==launch;
    const bool eligible=supportedContext && (loading::IsDragHudLaunch() ||
        (currentDrag && (sameLaunch || LaneRaceActive())));
    auto dragEntry=pushed ? ManagerEntry(DragPackage) : nullptr;
    if (pushed && (!eligible || stock!=stockPackage || !dragEntry ||
        Field<void*>(dragEntry,0x20)!=dragPackage)) {
        ResetPackage(feng);
        attemptedRace=-1;
    }
    if (!eligible) return;
    if (!ready && !Activate(feng,stock)) return;
    confirmedLaunch=launch;
    if (generation!=boundGeneration) {
        shiftSerial=ReadSnapshot().shiftSerial;
        shiftUntil=0;
        shiftLabel=nullptr;
        lastCountdown=-1;
        boundGeneration=generation;
    }
    stockVisible=(Field<unsigned>(hud,0x20)|Field<unsigned>(hud,0x24))!=0;
    const auto playerIndex=Field<int>(hud,0x34);
    if (playerIndex>=0 && playerIndex<2) {
        const auto countdown=reinterpret_cast<void**>(gBase+0x697DE8)[playerIndex];
        stockVisible=stockVisible || (countdown && Field<int>(countdown,0x30)>=0 &&
            Field<int>(countdown,0x30)<=3);
    }
    if (stock!=stockPackage || !IsVisible()) return;
    bool totaled{};
    std::uintptr_t simable{};
    {
        GateLock guard;
        totaled=gPlayerTotaled;
        if (gPlayerEngine) simable=reinterpret_cast<std::uintptr_t>(gBinding.simable);
    }
    Drive(hud,player,ReadSnapshot(),totaled,simable);
}

void __fastcall BeginRenderHook(void* renderer,void*,void* package) {
    originalBeginRender(renderer,package);
    if (ready && ((package==stockPackage && IsVisible()) ||
                  (package==dragPackage && !IsVisible()))) {
        Field<std::uint8_t>(renderer,4)=0;
    }
}

bool __fastcall PopHook(void* feng,void*,const char* name) {
    if (name && std::strcmp(name,StockPackage)==0) {
        attemptedRace=-1;
        if (pushed) {
            ResetPackage(feng);
        }
    }
    return originalPop(feng,name);
}
}
