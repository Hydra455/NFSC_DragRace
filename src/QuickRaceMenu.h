#pragma once

namespace frontend {
constexpr unsigned DragLabel = 0xE3DF9561;
using RaceCountFunction = unsigned(__thiscall*)(void*);
using RaceFlagFunction = bool(__thiscall*)(void*);
using RaceExclusionFunction = bool(__thiscall*)(void*,unsigned);
using EventHashFunction = unsigned(__thiscall*)(void*);
using EventIdFunction = const char*(__thiscall*)(void*);
using TrackParametersFunction = void*(__thiscall*)(void*);

template<class T> T& Field(void* object,unsigned offset) {
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object)+offset);
}

void* RaceDatabase() {
    return *reinterpret_cast<void**>(gBase+0x69828C);
}
bool IsDrag(void* parameters) {
    return parameters && gOriginal && gGetKey &&
        drag::IsAuthoredDrag(gOriginal(parameters),gGetKey(parameters));
}
unsigned EventHash(void* parameters) {
    return reinterpret_cast<EventHashFunction>(gBase+0x213890)(parameters);
}
const char* EventId(void* parameters) {
    return reinterpret_cast<EventIdFunction>(gBase+0x213590)(parameters);
}
bool IsExcluded(void* database,unsigned eventHash) {
    return !eventHash || reinterpret_cast<RaceExclusionFunction>(gBase+0x2454F0)(database,eventHash);
}
unsigned RaceCount(void* database) {
    return reinterpret_cast<RaceCountFunction>(gBase+0x213260)(database);
}
using FindFunction = void*(__thiscall*)(void*,unsigned);
using InitFunction = int(__thiscall*)(void*,int,int);
using LabelFunction = const char*(__cdecl*)(unsigned);
FindFunction originalFind{};
InitFunction originalInit{};
extern "C" {
    void* gDragStringSearch=nullptr;
    extern const char gDragLabelText[]="Drag";
}
static_assert(DragLabel==0xE3DF9561);
using CareerTrackFunction=void*(__thiscall*)(void*,void*);
using MapTrackLocked=bool(__thiscall*)(void*,void*,bool*,unsigned);
using EventQuickRaceLocked=bool(__thiscall*)(void*,int*,unsigned);
CareerTrackFunction originalCareerTrack{};
MapTrackLocked originalMapTrackLocked{};
EventQuickRaceLocked originalEventQuickRaceLocked{};

constexpr unsigned MenuDragTexture=0xAAAB31E9;
constexpr unsigned WorldDragTexture=0x513709C6;
using TextureLookup=void*(__cdecl*)(unsigned,int,int);
using AttributeLookup=const void*(__thiscall*)(void*,unsigned,unsigned);
TextureLookup originalTextureLookup{};
AttributeLookup originalAttributeLookup{};
std::vector<std::uint8_t> iconPack;
bool iconsLoaded{}, iconsLoading{}, emitterReady{};

void PrepareIcons() {
    const auto path=ModulePath(gModule).parent_path()/L"DragHUD"/L"HUDTEXICONS.BIN";
    try {
        Require(HashFile(path)=="0c7543a65b35ff04077fddea0ae092f93fe16107c43c17b05c0cf81fc83e10e1",
            "Drag icon pack differs from the supplied asset.");
        std::ifstream file(path,std::ios::binary);
        iconPack.resize(8960);
        Require(static_cast<bool>(file.read(reinterpret_cast<char*>(iconPack.data()),iconPack.size())),
            "Could not read the Drag icon pack.");
    } catch (const std::exception&) {
        iconPack.clear();
    }
}

bool LoadIcons() {
    if (iconsLoaded) return true;
    if (iconsLoading || iconPack.empty() || !*reinterpret_cast<void**>(gBase+0x6B0ABC) ||
        !*reinterpret_cast<void**>(gBase+0x68FFB4)) return false;
    auto memory=VirtualAlloc(nullptr,iconPack.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!memory) return false;
    std::memcpy(memory,iconPack.data(),iconPack.size());
    iconsLoading=true;
    using LoadChunk=bool(__cdecl*)(void*);
    iconsLoaded=reinterpret_cast<LoadChunk>(gBase+0x160580)(memory);
    iconsLoading=false;
    iconPack.clear();
    if (!iconsLoaded) {
        return false;
    }
    return true;
}

void* __cdecl TextureLookupHook(unsigned hash,int fallback,int packLookup) {
    if (hash==MenuDragTexture || hash==WorldDragTexture) LoadIcons();
    return originalTextureLookup(hash,fallback,packLookup);
}

bool RestoreWorldEmitter() {
    if (emitterReady) return true;
    if (!LoadIcons()) return false;
    using FindCollection=void*(__cdecl*)(unsigned,unsigned);
    auto emitter=reinterpret_cast<FindCollection>(gBase+0x65930)(0xB30B18AF,0x3B6CB7BA);
    if (!emitter) return false;
    auto layout=*reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(emitter)+0x18);
    if (!layout) return false;
    auto texture=reinterpret_cast<unsigned*>(layout+0x8C);
    if (texture[0]==0 && texture[1]==24) texture[0]=WorldDragTexture;
    emitterReady=texture[0]==WorldDragTexture;
    return emitterReady;
}

__declspec(noinline) const void* __fastcall TriggerAttributeHook(void* instance,void*,unsigned key,unsigned index) {
#if defined(_MSC_VER)
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#else
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#endif
    const auto value=originalAttributeLookup(instance,key,index);
    if (caller!=gBase+0x21AEC9 || key!=0x5EF34802 || index!=0) return value;
    auto database=*reinterpret_cast<void**>(gBase+0x69828C);
    const auto raceKey=static_cast<const unsigned*>(originalAttributeLookup(instance,0x277566F3,0));
    using FindRace=void*(__thiscall*)(void*,unsigned);
    auto parameters=database && raceKey && *raceKey ?
        reinterpret_cast<FindRace>(gBase+0x21BF80)(database,*raceKey) : nullptr;
    if (!parameters || !gOriginal || !gGetKey ||
        !drag::IsAuthoredDrag(gOriginal(parameters),gGetKey(parameters)) || !RestoreWorldEmitter())
        return value;
    static const char group[]="fxgame_icongrp_drag";
    static const char* const emitterGroup=group;
    const auto configured=value ? *static_cast<const char* const*>(value) : nullptr;
    return configured && *configured ? value : &emitterGroup;
}

void* RaceFromHash(void* database,unsigned hash) {
    return database && hash ? reinterpret_cast<FindFunction>(gBase+0x21BEF0)(database,hash) : nullptr;
}

void* RaceFromTrack(void* track) {
    return track && RaceDatabase() ? reinterpret_cast<TrackParametersFunction>(gBase+0x220D90)(track) : nullptr;
}

void* CareerTrackForRace(void* parameters) {
    auto career=*reinterpret_cast<void**>(gBase+0x6982B8);
    return career && parameters ? originalCareerTrack(career,parameters) : nullptr;
}

using MapLocationFunction=bool(__thiscall*)(void*,unsigned,float*);
MapLocationFunction originalMapLocation{};
using RaceRegionFunction=int(__thiscall*)(void*);
RaceRegionFunction originalRaceRegion{};

bool __fastcall MapLocationHook(void* database,void*,unsigned hash,float* position) {
    const bool hasPosition=originalMapLocation(database,hash,position);
    if (hasPosition && drag::ValidMapPosition(position[0],position[1])) return true;
    auto parameters=RaceFromHash(database,hash);
    if (!IsDrag(parameters)) return hasPosition;
    auto track=CareerTrackForRace(parameters);
    if (!track) return false;
    const auto location=&Field<float>(track,0x24);
    if (!drag::ValidMapPosition(location[0],location[1]) || !std::isfinite(location[2])) return false;
    std::memcpy(position,location,3*sizeof(float));
    return true;
}

int __fastcall RaceRegionHook(void* parameters,void*) {
    const int region=originalRaceRegion(parameters);
    if (drag::ValidRegion(region) || !Field<void*>(parameters,4) || !IsDrag(parameters)) return region;
    auto track=CareerTrackForRace(parameters);
    if (!track) return region;
    using ParentAreaFunction=void*(__thiscall*)(void*);
    auto area=reinterpret_cast<ParentAreaFunction>(gBase+0x22EC60)(track);
    return area ? reinterpret_cast<RaceRegionFunction>(gBase+0x217A30)(area) : region;
}

bool StandaloneDragEligible(void* parameters) {
    if (!IsDrag(parameters)) return false;

    const bool neverInQuickRace=reinterpret_cast<RaceFlagFunction>(gBase+0x23E4E0)(parameters);
    const bool challengeRace=reinterpret_cast<RaceFlagFunction>(gBase+0x23E540)(parameters);
    const bool dDayRace=reinterpret_cast<RaceFlagFunction>(gBase+0x23E0C0)(parameters);
    return drag::StandaloneDragFlags(neverInQuickRace,challengeRace,dDayRace);
}

bool QuickRaceDragAvailable(void* database,unsigned hash,void* parameters) {
    float position[3]{};
    return database && StandaloneDragEligible(parameters) && !IsExcluded(database,hash) &&
        MapLocationHook(database,nullptr,hash,position);
}

bool CareerDragAvailable(void* database,unsigned hash,void* parameters) {
    if (!database || !IsDrag(parameters) || IsExcluded(database,hash)) return false;
    const bool challenge=reinterpret_cast<RaceFlagFunction>(gBase+0x23E540)(parameters);
    const bool dDay=reinterpret_cast<RaceFlagFunction>(gBase+0x23E0C0)(parameters);
    return !challenge && !dDay;
}

using CareerAvailable=bool(__thiscall*)(void*);
using RaceUnlocked=bool(__thiscall*)(void*,unsigned);
CareerAvailable originalCareerAvailable{};
RaceUnlocked originalCareerRaceUnlocked{}, originalQuickRaceUnlocked{};

bool __fastcall CareerAvailableHook(void* track,void*) {
    if (originalCareerAvailable(track)) return true;
    auto parameters=RaceFromTrack(track);
    return IsDrag(parameters) && CareerDragAvailable(RaceDatabase(),EventHash(parameters),parameters);
}

bool __fastcall EventQuickRaceLockedHook(void* events,void*,int* locked,unsigned hash) {
    auto database=RaceDatabase();
    auto parameters=RaceFromHash(database,hash);
    if (!locked || !IsDrag(parameters))
        return originalEventQuickRaceLocked(events,locked,hash);
    *locked=QuickRaceDragAvailable(database,hash,parameters) ? 0 : 1;
    return false;
}

bool __fastcall QuickRaceUnlockedHook(void* database,void*,unsigned hash) {
    auto parameters=RaceFromHash(database,hash);
    if (IsDrag(parameters)) return QuickRaceDragAvailable(database,hash,parameters);
    return originalQuickRaceUnlocked(database,hash);
}

__declspec(noinline) bool __fastcall CareerRaceUnlockedHook(void* database,void*,unsigned hash) {
#if defined(_MSC_VER)
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#else
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#endif
    if (drag::IsDragCareerUnlockCaller(caller-gBase)) {
        auto parameters=RaceFromHash(database,hash);
        if (CareerDragAvailable(database,hash,parameters) && CareerTrackForRace(parameters)) return true;
    }
    return originalCareerRaceUnlocked(database,hash);
}

bool IsQuickRaceMode() {
    using IsMode=bool(__thiscall*)(void*,int);
    auto manager=*reinterpret_cast<void**>(gBase+0x697A7C);
    return manager && reinterpret_cast<IsMode>(gBase+0x1792A0)(manager,4);
}

__declspec(noinline) void* __fastcall CareerTrackHook(void* career,void*,void* parameters) {
#if defined(_MSC_VER)
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#else
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#endif
    if (caller==gBase+0x1C4026 && IsQuickRaceMode() && StandaloneDragEligible(parameters)) {
        return nullptr;
    }
    return originalCareerTrack(career,parameters);
}

using TrackRegionFunction=bool(__thiscall*)(void*,void*,int*,unsigned);

struct QuickListBuild;
thread_local QuickListBuild* activeQuickList{};
struct QuickListBuild {
    QuickListBuild* previous=activeQuickList;
    drag::QuickListDragEntries entries;

    QuickListBuild() { activeQuickList=this; }
    ~QuickListBuild() { activeQuickList=previous; }
};

bool __fastcall MapTrackLockedHook(void* map,void*,void* handle,bool* locked,unsigned eventHash) {
    if (!locked) return originalMapTrackLocked(map,handle,locked,eventHash);

    auto database=RaceDatabase();
    if (eventHash) {
        auto parameters=RaceFromHash(database,eventHash);
        if (!IsDrag(parameters)) return originalMapTrackLocked(map,handle,locked,eventHash);
        *locked=!QuickRaceDragAvailable(database,eventHash,parameters) ||
            (activeQuickList && activeQuickList->entries.HasCareerEntry(eventHash));
        return true;
    }

    auto parameters=handle ? RaceFromTrack(Field<void*>(handle,0x1C)) : nullptr;
    if (!IsDrag(parameters)) return originalMapTrackLocked(map,handle,locked,eventHash);
    const auto hash=EventHash(parameters);
    const bool quickRace=IsQuickRaceMode();
    *locked=quickRace ? !QuickRaceDragAvailable(database,hash,parameters) :
        !CareerDragAvailable(database,hash,parameters);
    if (!*locked && activeQuickList) {
        int region=-1;
        reinterpret_cast<TrackRegionFunction>(gBase+0xA9150)(map,handle,&region,0);
        activeQuickList->entries.RecordCareer(hash,region);
    }
    return true;
}
std::unordered_map<unsigned,unsigned> dragIconIndices;
unsigned indexedRaceCount{};

constexpr unsigned Hash(const char* text) {
    unsigned value=0xFFFFFFFF;
    while (*text) {
        unsigned char c=static_cast<unsigned char>(*text++);
        if (c>='a' && c<='z') c-=32;
        value=value*33+c;
    }
    return value;
}
void IndexDragIcons(unsigned count) {
    while (indexedRaceCount<count) {
        const auto name="MMICON_DRAG_"+std::to_string(indexedRaceCount);
        dragIconIndices.emplace(Hash(name.c_str()),indexedRaceCount);
        ++indexedRaceCount;
    }
}

using GetTrackDisplayName=bool(__thiscall*)(void*,void*,char*,int,unsigned);
GetTrackDisplayName originalGetTrackDisplayName{};

bool __fastcall GetTrackDisplayNameHook(void* map,void*,void* handle,char* output,int capacity,unsigned eventHash) {
    const bool result=originalGetTrackDisplayName(map,handle,output,capacity,eventHash);
    if (!result || !output || capacity<=0) return result;
    const char* fallback=reinterpret_cast<LabelFunction>(gBase+0x178830)(0x881F8EFA);
    if (!fallback || std::strncmp(output,fallback,static_cast<std::size_t>(capacity))!=0) return result;

    void* parameters{};
    if (eventHash) {
        if (auto database=RaceDatabase())
            parameters=reinterpret_cast<FindFunction>(gBase+0x21BEF0)(database,eventHash);
    } else if (handle) {
        if (auto track=Field<void*>(handle,0x1C))
            parameters=reinterpret_cast<TrackParametersFunction>(gBase+0x220D90)(track);
    }
    if (!IsDrag(parameters)) return result;
    const char* id=EventId(parameters);
    if (!id || !*id) return result;

    const char* label=id;
    if (std::strcmp(id,"2.1.dr")==0) label="Angie Drag";
    else if (std::strcmp(id,"3.1.dr")==0 || std::strcmp(id,"drag.example")==0) label="Hotels Drag";
    else if (std::strcmp(id,"4.1.dr")==0) label="University Drag";
    else if (std::strcmp(id,"5.1.dr")==0) label="Darius Drag";
    std::snprintf(output,static_cast<std::size_t>(capacity),"%s",label);
    return result;
}

using SetupOptions=void(__thiscall*)(void*);
SetupOptions originalSetupOptions{};
constexpr int DragOptionsType=4;
constexpr unsigned RaceSettingsBase=0x2B0C4;
constexpr unsigned RaceSettingsSize=40;

void __fastcall SetupOptionsHook(void* screen,void*) {
    const auto hash=Field<unsigned>(screen,0x168);
    auto database=*reinterpret_cast<void**>(gBase+0x69828C);
    using Lookup=void*(__thiscall*)(void*,unsigned);
    auto parameters=database && hash ? reinterpret_cast<Lookup>(gBase+0x21BEF0)(database,hash) : nullptr;
    if (parameters && drag::IsAuthoredDrag(gOriginal(parameters),gGetKey(parameters))) {
        auto manager=*reinterpret_cast<void**>(gBase+0x697A7C);
        using Profile=void*(__thiscall*)(void*,int);
        auto profile=manager ? reinterpret_cast<Profile>(gBase+0x172B90)(manager,0) : nullptr;
        if (profile) {
            auto settings=static_cast<std::uint8_t*>(profile)+RaceSettingsBase+DragOptionsType*RaceSettingsSize;
            settings[4]=1;
            if (settings[7]>3) settings[7]=2;
            settings[0x11]=0;
        }
    }
    originalSetupOptions(screen);
}

void* FindOwner(void* list,void* target,unsigned depth,unsigned& budget) {
    if (depth>32) return nullptr;
    for (void* item=Field<void*>(list,8); item && budget; item=Field<void*>(item,4)) {
        --budget;
        if (item==target) return list;
        if (Field<unsigned>(item,0x18)==5) {
            if (void* owner=FindOwner(static_cast<unsigned char*>(item)+0x5C,target,depth+1,budget)) return owner;
        }
    }
    return nullptr;
}
__declspec(noinline) void* __fastcall FindHook(void* package,void*,unsigned hash) {
#if defined(_MSC_VER)
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#else
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#endif
    void* found=originalFind(package,hash);
    if (found || !package) return found;
    switch (caller-gBase) {
    case 0x1C3D54: case 0x1C4120:
    case 0x1ADBFC: case 0x1ADFC6: case 0x1AE177: case 0x1AE5A9:
        break;
    default:
        return nullptr;
    }
    if (auto database=RaceDatabase()) IndexDragIcons(RaceCount(database));
    const auto match=dragIconIndices.find(hash);
    if (match==dragIconIndices.end()) return nullptr;
    if (Field<std::uintptr_t>(package,0)!=gBase+0x5D7628) return nullptr;
    const auto index=match->second;
    std::string sourceName="MMICON_SPRINT_"+std::to_string(index);
    void* source=originalFind(package,Hash(sourceName.c_str()));
    if (!source) source=originalFind(package,Hash("MMICON_SPRINT_0"));
    if (!source || Field<std::uintptr_t>(source,0)!=gBase+0x5D7664 || Field<unsigned>(source,0x18)!=1) {
        return nullptr;
    }
    const unsigned traversalLimit=4096+indexedRaceCount;
    unsigned budget=traversalLimit;
    void* owner=FindOwner(static_cast<unsigned char*>(package)+0x44,source,0,budget);
    if (!owner || Field<unsigned>(owner,4)>traversalLimit) return nullptr;
    void* tail=Field<void*>(owner,12);
    if (!tail || Field<void*>(tail,4)!=nullptr) return nullptr;
    using Clone=void*(__thiscall*)(void*,bool);
    void* copy=reinterpret_cast<Clone>(gBase+0x1FFCD0)(source,true);
    if (!copy) return nullptr;
    std::string name="MMICON_DRAG_"+std::to_string(index);
    using SetName=void(__thiscall*)(void*,const char*);
    reinterpret_cast<SetName>(gBase+0x1EC6F0)(copy,name.c_str());
    Field<void*>(copy,4)=nullptr;
    Field<void*>(copy,8)=tail;
    Field<void*>(tail,4)=copy;
    Field<void*>(owner,12)=copy;
    ++Field<unsigned>(owner,4);
    return copy;
}
#if defined(_MSC_VER)
__declspec(naked) void LabelHook() {
    __asm {
        cmp edx, 0E3DF9561h
        jne nativeSearch
        mov eax, offset gDragLabelText
        ret
    nativeSearch:
        jmp dword ptr [gDragStringSearch]
    }
}
#else
__attribute__((naked)) void LabelHook() {
    __asm__ volatile(
        "cmpl $0xe3df9561, %edx\n\t"
        "jne 1f\n\t"
        "movl $_gDragLabelText, %eax\n\t"
        "ret\n"
        "1: jmp *_gDragStringSearch");
}
#endif
int __fastcall InitHook(void* list,void*,int selection,int probe) {
    QuickListBuild build;
    build.entries.areaFilter=Field<int>(list,0xE0);
    int count=originalInit(list,selection,probe);
    if (!probe) {
        int filter=Field<int>(list,0xD8);
        if (filter==2) {
            using Print=void(__cdecl*)(const char*,unsigned,const char*,...);
            const char* package=Field<const char*>(list,0xC);
            if (package) reinterpret_cast<Print>(gBase+0x1A0910)(package,0x0D94B769,"%s","Drag");
        }
    }
    return count;
}
void ReplaceWord(std::uintptr_t rva,unsigned expected,unsigned value,const char* failure) {
    auto target=reinterpret_cast<unsigned*>(gBase+rva);
    Require(*target==expected,failure);
    DWORD previous{};
    Require(VirtualProtect(target,sizeof(*target),PAGE_READWRITE,&previous)!=FALSE,failure);
    *target=value;
    DWORD ignored{};
    VirtualProtect(target,sizeof(*target),previous,&ignored);
}

void EnableDragMenu() {
    ReplaceWord(0x6590E4,0,DragLabel,"The Drag label slot differs from the verified build.");

    ReplaceWord(0x45E6B8,static_cast<unsigned>(gBase+0x45E691),static_cast<unsigned>(gBase+0x45E639),
        "The Drag Quick Race options slot differs from the verified build.");
}
}
