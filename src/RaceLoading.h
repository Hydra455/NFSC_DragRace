#pragma once

namespace loading {
using StartFunction=void(__cdecl*)(unsigned,int);
using InGameStartFunction=void(__thiscall*)(void*,unsigned);
constexpr int CareerContext=3;
constexpr int QuickRaceContext=7;
constexpr int FrontendCareerMode=8;
constexpr int FrontendQuickRaceMode=4;
constexpr std::size_t RaceContextOffset=0x6A18;
constexpr std::size_t RacePhaseOffset=0x6A14;
constexpr std::uintptr_t TriggerListRva=0x698170;
constexpr unsigned EngageTriggerFlag=0x200;
constexpr unsigned IconVisibilityFlags=0x81;
constexpr unsigned EffectEnabledFlag=0x10;
using ScriptFunction=void(__cdecl*)(void*);
using MemberFunction=void(__thiscall*)(void*);
StartFunction originalStart{};
InGameStartFunction originalInGameStart{};
ScriptFunction originalActivity{},originalRacers{},originalRace{};
MemberFunction originalTrackBegin{},originalTrackEnd{};
MemberFunction originalWorldMapFlowDone{};
volatile LONG active{},launchSerial{},stage{},eventHash{},context{};
volatile LONG dragIntroPending{};
using VoidFunction=void(__cdecl*)();
using PointerMember=void*(__thiscall*)(void*);
MemberFunction originalNotify{};
VoidFunction originalFinished{};
volatile LONG notifyCalls{},startCalls{};
template<class T> bool ReadValue(std::uintptr_t address,T& value) {
    SIZE_T bytes{};
    return address && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof(value),&bytes) && bytes==sizeof(value);
}
void __fastcall NotifyHook(void* object,void*) {
    if (Load(active)) InterlockedIncrement(&notifyCalls);
    originalNotify(object);
}
void __cdecl FinishedHook() {
    if (Load(active)) Store(stage,7);
    originalFinished();
}

bool IsAuthoredDrag(void* parameters) {
    return parameters && gOriginal && gGetKey &&
        drag::IsAuthoredDrag(gOriginal(parameters),gGetKey(parameters));
}

MemberFunction originalUpdateIconVisibility{};
bool engageEffectsHidden{};

void __fastcall UpdateIconVisibilityHook(void* manager,void*) {
    const auto status=*reinterpret_cast<std::uint8_t**>(gBase+RaceStatusRva);
    auto parameters=status ? frontend::Field<void*>(status,RaceParametersOffset) : nullptr;
    const bool hideEngageMarkers=IsAuthoredDrag(parameters);
    const bool restoreEngageEffects=status && !parameters && engageEffectsHidden;
    if (!hideEngageMarkers && !restoreEngageEffects) {
        originalUpdateIconVisibility(manager);
        return;
    }

    auto first=*reinterpret_cast<void**>(gBase+TriggerListRva);

    struct HiddenIcon {
        std::uint8_t* flags;
        std::uint8_t visibility;
    };
    std::array<HiddenIcon,256> hidden{};
    std::size_t count{};
    bool hidEffects=false;
    for (auto trigger=first;trigger;) {
        if (frontend::Field<std::uint16_t>(trigger,0x14)&EngageTriggerFlag) {
            if (restoreEngageEffects) {
                using SetTriggerEffects=void(__thiscall*)(void*,bool);
                if (frontend::Field<std::uint8_t>(trigger,0xA0))
                    reinterpret_cast<SetTriggerEffects>(gBase+0x2114A0)(trigger,true);
            } else {
                auto icon=frontend::Field<std::uint8_t*>(trigger,0x9C);
                if (icon && count<hidden.size()) {
                    auto& flags=icon[1];
                    hidden[count++]={&flags,static_cast<std::uint8_t>(flags&IconVisibilityFlags)};
                    flags&=~IconVisibilityFlags;
                }

                for (auto offset : {0x94,0x98}) {
                    auto effect=frontend::Field<void*>(trigger,offset);
                    if (!effect || !(frontend::Field<unsigned>(effect,0x18)&EffectEnabledFlag)) continue;
                    reinterpret_cast<MemberFunction>(gBase+0x34ABC0)(effect);
                    hidEffects=true;
                }
            }
        }
        trigger=frontend::Field<void*>(trigger,0x20);
        if (trigger==first) break;
    }

    originalUpdateIconVisibility(manager);
    for (std::size_t index=0;index<count;++index)
        *hidden[index].flags|=hidden[index].visibility;
    engageEffectsHidden=(engageEffectsHidden || hidEffects) && !restoreEngageEffects;

}

void* FindEvent(unsigned hash) {
    const auto database=*reinterpret_cast<void**>(gBase+0x69828C);
    using Lookup=void*(__thiscall*)(void*,unsigned);
    return database && hash ? reinterpret_cast<Lookup>(gBase+0x21BEF0)(database,hash) : nullptr;
}

bool DragContext(int value) {
    return value==QuickRaceContext || value==CareerContext;
}

bool IsDragHudLaunch() {
    return Load(active) && DragContext(Load(context)) &&
        IsAuthoredDrag(FindEvent(static_cast<unsigned>(Load(eventHash))));
}

constexpr unsigned GameplayMessageTarget=0x20D60DBF;
constexpr unsigned AudioTransitionDone=0x50DF9460;
constexpr DWORD MusicFallbackDelayMs=3000;
using UpdateFunction=void(__thiscall*)(void*,float);
UpdateFunction originalStatusUpdate{};
void* pendingMusicParameters{};
DWORD musicRequestTime{};
volatile LONG musicMessageSeen{};
bool musicFallbackPending{};

using MessageFunction=void(__thiscall*)(void*,unsigned,int);
using NisFunction=void(__cdecl*)(void*,const char*,const char*,int,const char*,const char*);
MessageFunction originalMessage{};
NisFunction originalNis{};
VoidFunction originalMusicRequest{};

void __fastcall MessageHook(void* message,void*,unsigned target,int delay) {
    if (target==GameplayMessageTarget && message && delay==0 &&
        *static_cast<const unsigned*>(message)==0x6638715C)
        Store(dragIntroPending,0);
    if (Load(active) && target==GameplayMessageTarget && message) {
        const auto type=*static_cast<const unsigned*>(message);
        if (type==0xBB59E605 || type==AudioTransitionDone)
            Store(musicMessageSeen,1);
    }
    originalMessage(message,target,delay);
}
void __cdecl MusicRequestHook() {
    const bool launchActive=Load(active)!=0;
    if (launchActive) Store(stage,8);
    auto status=*reinterpret_cast<std::uint8_t**>(gBase+RaceStatusRva);
    auto parameters=status ? *reinterpret_cast<void**>(status+RaceParametersOffset) : nullptr;
    if (launchActive && DragContext(Load(context)) && IsAuthoredDrag(parameters)) {
        pendingMusicParameters=parameters;
        musicRequestTime=GetTickCount();
        musicFallbackPending=true;
    }
    originalMusicRequest();
}
void __cdecl NisHook(void* instance,const char* scene,const char* type,int arg,const char* extra,const char* track) {
    if (Load(active) && DragContext(Load(context)) && type && _stricmp(type,"Intro")==0)
        Store(dragIntroPending,1);
    if (Load(active)) Store(stage,9);
    originalNis(instance,scene,type,arg,extra,track);
}
void __fastcall StatusUpdateHook(void* object,void*,float elapsed) {
    originalStatusUpdate(object,elapsed);
    if (!musicFallbackPending || !Load(active) || Load(musicMessageSeen) ||
        GetTickCount()-musicRequestTime<MusicFallbackDelayMs)
        return;

    auto status=*reinterpret_cast<std::uint8_t**>(gBase+RaceStatusRva);
    if (!status || status!=object || *reinterpret_cast<const int*>(gBase+0x699BBC)!=6)
        return;
    auto parameters=*reinterpret_cast<void**>(status+RaceParametersOffset);
    if (parameters!=pendingMusicParameters || status[0x6A58] || status[0x6A5A] ||
        status[0x6A3E] || Load(notifyCalls) || Load(stage)==9 || Load(startCalls))
        return;

    musicFallbackPending=false;
    std::array<unsigned,4> message{};
    auto construct=reinterpret_cast<PointerMember>(gBase+0x135150);
    MessageHook(construct(message.data()),nullptr,GameplayMessageTarget,0);
}
void BeginLaunch(unsigned hash,int raceContext) {
    musicFallbackPending=false;
    pendingMusicParameters=nullptr;
    Store(musicMessageSeen,0);
    Store(dragIntroPending,DragContext(raceContext));
    InterlockedIncrement(&launchSerial);
    Store(active,1);
    Store(eventHash,static_cast<LONG>(hash));
    Store(context,raceContext);
    Store(notifyCalls,0);
    Store(startCalls,0);
}

using FillRaceFunction=void(__cdecl*)(void*,const std::uint8_t*);
FillRaceFunction originalFillRace{};

void __cdecl FillRaceHook(void* customRace,const std::uint8_t* settings) {
    if (Load(context)!=QuickRaceContext || !IsAuthoredDrag(customRace) || !settings) {
        originalFillRace(customRace,settings);
        return;
    }

    std::array<std::uint8_t,40> dragSettings;
    std::memcpy(dragSettings.data(),settings,dragSettings.size());
    dragSettings[4]=1;
    if (dragSettings[7]>3) dragSettings[7]=2;
    dragSettings[0x11]=0;
    originalFillRace(customRace,dragSettings.data());
}

void __cdecl StartHook(unsigned hash,int raceContext) {
    const bool careerDrag=raceContext==FrontendCareerMode && IsAuthoredDrag(FindEvent(hash));
    BeginLaunch(hash,careerDrag ? CareerContext : raceContext);
    Store(stage,1);
    originalStart(hash,raceContext);
}

unsigned EventHashFromTrack(unsigned trackKey) {
    if (!trackKey || !*reinterpret_cast<void**>(gBase+0x6982B8)) return 0;
    std::array<unsigned,19> handle{};
    using InitHandle=void*(__thiscall*)(void*);
    using GetMap=void*(__cdecl*)();
    using GetTrack=bool(__thiscall*)(void*,void*,const unsigned*);
    reinterpret_cast<InitHandle>(gBase+0xBC720)(handle.data());
    auto dal=reinterpret_cast<GetMap>(gBase+0xA80F0)();
    if (!dal || !reinterpret_cast<GetTrack>(gBase+0xD0180)(dal,handle.data(),&trackKey))
        return 0;
    auto track=frontend::Field<void*>(handle.data(),0x1C);
    auto parameters=track ? reinterpret_cast<PointerMember>(gBase+0x220D90)(track) : nullptr;
    using EventHash=unsigned(__thiscall*)(void*);
    return IsAuthoredDrag(parameters) ? reinterpret_cast<EventHash>(gBase+0x213890)(parameters) : 0;
}

void __fastcall WorldMapFlowDoneHook(void* mapFlow,void*) {
    if (*reinterpret_cast<const int*>(gBase+0x699BBC)==3 &&
        frontend::Field<int>(mapFlow,0x08)==0) {
        auto profileManager=*reinterpret_cast<void**>(gBase+0x697A7C);
        using IsGameMode=bool(__thiscall*)(void*,int);

        if (!profileManager ||
            reinterpret_cast<IsGameMode>(gBase+0x1792A0)(profileManager,FrontendQuickRaceMode)) {
            originalWorldMapFlowDone(mapFlow);
            return;
        }
        const int result=frontend::Field<int>(mapFlow,0x0C);

        const unsigned hash=result==1 ? frontend::Field<unsigned>(mapFlow,0xC4) :
            result==3 ? EventHashFromTrack(frontend::Field<unsigned>(mapFlow,0xD4)) : 0;
        if (hash && IsAuthoredDrag(FindEvent(hash))) {
            if (frontend::Field<unsigned>(profileManager,0xEC)==hash)
                frontend::Field<unsigned>(profileManager,0xEC)=0;
            if (result==1) originalWorldMapFlowDone(mapFlow);
            StartHook(hash,FrontendCareerMode);
            return;
        }
    }
    originalWorldMapFlowDone(mapFlow);
}

void __fastcall InGameStartHook(void* manager,void*,unsigned hash) {
    const auto status=*reinterpret_cast<const std::uint8_t**>(gBase+RaceStatusRva);
    if (!status) return;
    if (*reinterpret_cast<const int*>(status+RacePhaseOffset)!=0) {
        originalInGameStart(manager,hash);
        return;
    }

    auto parameters=FindEvent(hash);
    if (!IsAuthoredDrag(parameters)) {
        originalInGameStart(manager,hash);
        return;
    }

    reinterpret_cast<MemberFunction>(gBase+0x23C3C0)(parameters);
    if (!reinterpret_cast<PointerMember>(gBase+0x21C480)(parameters)) {
        char message[256]{};
        const char* id=frontend::EventId(parameters);
        std::snprintf(message,sizeof(message),"Carbon could not load the race activity for %s. Check the race's gameplay vault and engage trigger.",id && *id ? id : "this Drag event");
        MessageBoxA(nullptr,message,"NFSC_DragRace",MB_OK|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND);
        return;
    }

    if (*reinterpret_cast<const int*>(status+RaceContextOffset)==CareerContext) {
        BeginLaunch(hash,CareerContext);
        Store(stage,1);
    }
    originalInGameStart(manager,hash);
}
void __fastcall TrackBeginHook(void* object,void*) {
    if (!Load(active)) { originalTrackBegin(object); return; }
    Store(stage,2);
    originalTrackBegin(object);
}
void __fastcall TrackEndHook(void* object,void*) {
    if (!Load(active)) { originalTrackEnd(object); return; }
    Store(stage,3);
    originalTrackEnd(object);
}
void __cdecl ActivityHook(void* instance) {
    if (!Load(active)) { originalActivity(instance); return; }
    Store(stage,4);
    originalActivity(instance);
}
void __cdecl RacersHook(void* instance) {
    if (!Load(active)) { originalRacers(instance); return; }
    Store(stage,5);
    originalRacers(instance);

}
struct GridVector {
    float x;
    float y;
    float z;
};

VoidFunction originalRestoreStart{};
using GridSlotFunction=bool(__cdecl*)(int,GridVector*,GridVector*,void*);
using RacerInfoFunction=std::uint8_t*(__thiscall*)(void*,int);
using SaveGridPoseFunction=void(__thiscall*)(void*,const GridVector*,const GridVector*);

bool FiniteGridVector(const GridVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

void __cdecl RestoreStartHook() {
    auto status=*reinterpret_cast<std::uint8_t**>(gBase+RaceStatusRva);
    auto parameters=status ? *reinterpret_cast<void**>(status+RaceParametersOffset) : nullptr;
    if (!Load(active) || !DragContext(Load(context)) || !IsAuthoredDrag(parameters)) {
        originalRestoreStart();
        return;
    }

    const int count=*reinterpret_cast<const int*>(status+0x6A08);
    if (count!=3) {
        originalRestoreStart();
        return;
    }

    auto getSlot=reinterpret_cast<GridSlotFunction>(gBase+0x23D900);
    GridVector left{},right{},leftDirection{},rightDirection{};
    if (!getSlot(0,&left,&leftDirection,nullptr) ||
        !getSlot(1,&right,&rightDirection,nullptr) ||
        !FiniteGridVector(left) || !FiniteGridVector(right) || !FiniteGridVector(leftDirection)) {
        originalRestoreStart();
        return;
    }

    const GridVector middle{(left.x+right.x)*0.5f,(left.y+right.y)*0.5f,(left.z+right.z)*0.5f};
    const float width=std::hypot(right.x-left.x,right.z-left.z);
    if (width<5.0f || width>7.0f) {
        originalRestoreStart();
        return;
    }

    auto getRacer=reinterpret_cast<RacerInfoFunction>(gBase+0x2121E0);
    std::array<std::uint8_t*,3> racers{};
    std::array<GridVector,3> positions{left,middle,right};
    for (int i=0;i<count;++i) {
        auto racer=getRacer(status,i);
        GridVector saved{};
        if (!racer || !*reinterpret_cast<const unsigned*>(racer) ||
            !ReadValue(reinterpret_cast<std::uintptr_t>(racer)+0x1A8,saved) ||
            !FiniteGridVector(saved) || std::hypot(saved.x-middle.x,saved.z-middle.z)>25.0f ||
            std::fabs(saved.y-middle.y)>6.0f) {
            originalRestoreStart();
            return;
        }
        racers[i]=racer;
        positions[i].y=saved.y;
    }

    auto savePose=reinterpret_cast<SaveGridPoseFunction>(gBase+0x212FE0);
    for (int i=0;i<count;++i)
        savePose(racers[i],&positions[i],&leftDirection);
    originalRestoreStart();
}

void __cdecl RaceHook(void* instance) {
    Store(dragIntroPending,0);
    if (!Load(active)) { originalRace(instance); return; }
    InterlockedIncrement(&startCalls);
    Store(stage,6);
    originalRace(instance);
    Store(active,0);
}
}
