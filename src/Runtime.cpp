#include <windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include <intrin.h>
#include "../third_party/minhook/include/MinHook.h"
#include "RaceGate.h"
#include "DragMechanics.h"
#include "LaneAssist.h"
#include "NativeLane.h"
#include "NativeSteering.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <type_traits>

static_assert(sizeof(void*) == 4, "Build Carbon Drag Runtime for Win32.");

namespace {
constexpr std::uintptr_t RaceTypeRva = 0x2136A0;
constexpr std::uintptr_t CollectionKeyRva = 0x2134E0;
constexpr std::uintptr_t RaceStatusRva = 0x698284;
constexpr std::uintptr_t RaceParametersOffset = 0x6A1C;
constexpr DWORD CarbonImageFileBytes = 0x6E2000;
constexpr LONG CarbonPeHeaderOffset = 0x108;
constexpr char CarbonImageHash[] = "d8148ffe4fdc373341e1141830dded834711f07433f3b614941a58ea686aeee4";
using RaceTypeFunction = int(__thiscall*)(void*);
using KeyFunction = std::uint32_t(__thiscall*)(void*);
HMODULE gModule{};
std::uintptr_t gBase{};
RaceTypeFunction gOriginal{};
KeyFunction gGetKey{};
volatile LONG gLatched{}, gTarget{}, gLatchCount{};
bool gHookCreated{}, gHookEnabled{};
using AutoFunction = void(__thiscall*)(void*);
using GearFunction = int(__thiscall*)(void*, int);
using TickFunction = void(__thiscall*)(void*, float);
using TorqueFunction = float(__thiscall*)(void*, float);
using FloatFunction = float(__thiscall*)(void*);
using BoolFunction = bool(__thiscall*)(void*);
using PtrFunction = void*(__thiscall*)(void*);
using IntFunction = int(__thiscall*)(void*);
AutoFunction gOriginalAuto{};
GearFunction gOriginalGear{};
TickFunction gOriginalTick{};
TorqueFunction gOriginalTorque{};
FloatFunction gRpm{}, gRedline{};
BoolFunction gIsPlayer{}, gIsStaging{}, gBlow{}, gIsBlown{};
PtrFunction gSimable{};
TickFunction gOriginalHuman{}, gNativeController{};
using VectorFunction = void(__thiscall*)(void*, drag::Vector3*);
VectorFunction gDirection{}, gVelocity{};
FloatFunction gMaxSteering{};
using SuspensionSteeringFunction = void(__thiscall*)(void*, void*, void*, void*);
SuspensionSteeringFunction gOriginalWheelSteering{};
using PlayerAngleFunction = float(__thiscall*)(void*, void*);
PlayerAngleFunction gOriginalPlayerAngle{};
thread_local void* gLaneSuspensionContext{};
thread_local float gLaneAngleContext{};
thread_local unsigned gLaneRaceContext{};
PtrFunction gPosition{};
drag::LaneAssist gLaneAssist;
drag::LaneRequests gLaneRequests;
drag::NativeLaneState gNativeLane;
using NativeOffsetFunction = void(__thiscall*)(void*,float,float);
using ProfileFunction = void*(__thiscall*)(void*,const void*,int);
NativeOffsetFunction gNativeOffset{};
ProfileFunction gNativeProfile{};
using NavInitFunction = void(__thiscall*)(void*,const drag::Vector3*,const drag::Vector3*,bool,float);
using NavAdvanceFunction = void(__thiscall*)(void*,float,const drag::Vector3*,float,bool);
using NavPathFunction = void(__thiscall*)(void*,int);
AutoFunction gNavConstruct{}, gNavDestroy{};
NavInitFunction gNavInit{};
NavAdvanceFunction gNavAdvance{};
NavPathFunction gNavPath{};
struct LaneSteering {
    float steering{}, maxSteeringTurns{};
    bool valid{};
    DWORD updated{};
    void* inputObject{};
};
LaneSteering gLane;

void* gPlayerEngine{};
void* gPlayerInput{};
drag::Mechanics gMechanics;
float gPlayerRpm{}, gPlayerRedline{}, gPlayerSpeedKph{};
int gPlayerGear{};
unsigned gTickCount{}, gShiftSerial{};
bool gNativeBlown{};
struct PlayerBinding {
    void* vehicle{};
    void* simable{};
};
PlayerBinding gBinding;

SRWLOCK gGateLock = SRWLOCK_INIT;
struct GateLock {
    GateLock() { AcquireSRWLockExclusive(&gGateLock); }
    ~GateLock() { ReleaseSRWLockExclusive(&gGateLock); }
    GateLock(const GateLock&) = delete;
    GateLock& operator=(const GateLock&) = delete;
};

LONG Load(volatile LONG& value) { return InterlockedCompareExchange(&value, 0, 0); }
void Store(volatile LONG& value, LONG set) { InterlockedExchange(&value, set); }
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::filesystem::path ModulePath(HMODULE module) {
    std::vector<wchar_t> buffer(32768);
    auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    Require(length != 0 && length < buffer.size(), "Cannot resolve a module path.");
    return std::filesystem::path(std::wstring(buffer.data(), length));
}


enum class FileHash { Full, CarbonImage };

void NormalizeCarbonHeader(UCHAR* data, DWORD bytes) {
    constexpr auto fileHeader = CarbonPeHeaderOffset + offsetof(IMAGE_NT_HEADERS32, FileHeader);
    constexpr auto optionalHeader = CarbonPeHeaderOffset + offsetof(IMAGE_NT_HEADERS32, OptionalHeader);
    constexpr auto securityDirectory = optionalHeader + offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory) +
        IMAGE_DIRECTORY_ENTRY_SECURITY * sizeof(IMAGE_DATA_DIRECTORY);
    Require(bytes >= securityDirectory + sizeof(IMAGE_DATA_DIRECTORY), "The Carbon executable header is incomplete.");

    auto characteristics = data + fileHeader + offsetof(IMAGE_FILE_HEADER, Characteristics);
    characteristics[0] &= static_cast<UCHAR>(~IMAGE_FILE_LARGE_ADDRESS_AWARE);
    std::memset(data + fileHeader + offsetof(IMAGE_FILE_HEADER, TimeDateStamp), 0, sizeof(DWORD));
    std::memset(data + optionalHeader + offsetof(IMAGE_OPTIONAL_HEADER32, CheckSum), 0, sizeof(DWORD));
    std::memset(data + securityDirectory, 0, sizeof(IMAGE_DATA_DIRECTORY));
}

std::string HashFile(const std::filesystem::path& path, FileHash mode = FileHash::Full) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; HANDLE file = INVALID_HANDLE_VALUE;
    std::vector<UCHAR> storage;
    try {
        Require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "Cannot open SHA-256 provider.");
        DWORD objectBytes{}, resultBytes{};
        Require(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &resultBytes, 0) >= 0, "Cannot query SHA-256 storage.");
        storage.resize(objectBytes);
        Require(BCryptCreateHash(algorithm, &hash, storage.data(), static_cast<ULONG>(storage.size()), nullptr, 0, 0) >= 0, "Cannot initialize SHA-256.");
        file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        Require(file != INVALID_HANDLE_VALUE, "Cannot read one of the required game files.");
        std::array<UCHAR, 65536> buffer{};
        constexpr auto bufferBytes = static_cast<DWORD>(buffer.size());
        const bool carbonImage = mode == FileHash::CarbonImage;
        DWORD remaining = CarbonImageFileBytes;
        while (!carbonImage || remaining != 0) {
            const DWORD requested = carbonImage ? std::min(remaining, bufferBytes) : bufferBytes;
            DWORD bytes{};
            Require(ReadFile(file, buffer.data(), requested, &bytes, nullptr) != FALSE, "A required game file could not be read completely.");
            if (carbonImage) {
                Require(bytes == requested, "The Carbon executable is shorter than the reference image.");
                if (remaining == CarbonImageFileBytes)
                    NormalizeCarbonHeader(buffer.data(), bytes);
                remaining -= bytes;
            }
            if (!bytes) break;
            Require(BCryptHashData(hash, buffer.data(), bytes, 0) >= 0, "SHA-256 update failed.");
        }
        std::array<UCHAR, 32> digest{};
        Require(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0, "SHA-256 completion failed.");
        BCryptDestroyHash(hash); hash = nullptr; BCryptCloseAlgorithmProvider(algorithm, 0); algorithm = nullptr; CloseHandle(file); file = INVALID_HANDLE_VALUE;
        std::ostringstream output; output << std::hex << std::setfill('0'); for (auto c : digest) output << std::setw(2) << static_cast<unsigned>(c); return output.str();
    } catch (...) {
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}
#include "QuickRaceMenu.h"
#include "RaceLoading.h"

bool ActiveParameters(void* parameters) {
    auto status = *reinterpret_cast<std::uint8_t**>(gBase + RaceStatusRva);
    return status && *reinterpret_cast<void**>(status + RaceParametersOffset) == parameters;
}
__declspec(noinline) int __fastcall RaceTypeHook(void* parameters, void*) {
#if defined(_MSC_VER)
    auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#else
    auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#endif
    int native = gOriginal(parameters);
    if (caller!=gBase+0x255FA2 || !ActiveParameters(parameters)) return native;
    auto key=gGetKey(parameters);
    GateLock guard;
    bool activate=drag::IsAuthoredDrag(native,key);
    Store(gLatched,0);
    Store(gTarget,activate ? static_cast<LONG>(key) : 0);
    gPlayerEngine=nullptr; gPlayerInput=nullptr;
    gMechanics.Reset(); gLaneAssist.Reset(); gLaneRequests={}; gNativeLane={};
    gLane={}; gBinding={};
    gPlayerRpm=gPlayerRedline=gPlayerSpeedKph=0; gPlayerGear=0; gNativeBlown=false;
    gTickCount=0;
    InterlockedIncrement(&gLatchCount);
    Store(gLatched,activate ? 1 : 0);
    return native;
}
bool ScopedPlayer(void* engine, bool bindFromTick = false) {
    if (!Load(gLatched) || !engine) return false;
    auto status = *reinterpret_cast<std::uint8_t**>(gBase + RaceStatusRva);
    auto parameters = status ? *reinterpret_cast<void**>(status + RaceParametersOffset) : nullptr;
    if (!parameters) return false;
    if (!status[0x6A3E]) return false;
    if (gGetKey(parameters) != static_cast<std::uint32_t>(Load(gTarget))) return false;
    auto bytes = static_cast<std::uint8_t*>(engine);
    auto engineType = *reinterpret_cast<std::uintptr_t*>(bytes);
    if (engineType != gBase + 0x5E5A18) return false;
    auto vehicle = *reinterpret_cast<void**>(bytes + 0x40);
    if (!vehicle) return false;
    auto vehicleType = *reinterpret_cast<std::uintptr_t*>(vehicle);
    if (vehicleType != gBase + 0x5E4A78) return false;
    auto simable = gSimable(vehicle);
    if (!simable) return false;
    auto simableType = *reinterpret_cast<std::uintptr_t*>(simable);
    if (simableType != gBase + 0x5E4BB0) return false;
    bool player = gIsPlayer(simable);
    if (!player) return false;
    if (!drag::PhysicsScope(Load(gLatched) != 0, status[0x6A3E] != 0, player, gGetKey(parameters), static_cast<std::uint32_t>(Load(gTarget)))) return false;
    if (gPlayerEngine && gPlayerEngine != engine) return false;
    if (!gPlayerEngine) {
        if (!bindFromTick) return false;
        gPlayerEngine = engine; gMechanics.Reset();
        gBinding.vehicle = vehicle;
        gBinding.simable = simable;
    }
    if (bindFromTick) gPlayerInput = *reinterpret_cast<void**>(bytes+0x108);
    return true;
}
using CollisionFunction=void(__thiscall*)(void*,const std::uint8_t*);
CollisionFunction gOriginalCollision{};
struct Rival {
    void* engine{};
    unsigned handle{};
    drag::Mechanics mechanics;
    float cooldown{};
    bool totaled{};
};
std::array<Rival,32> gRivals{};
unsigned gRosterGeneration{};
bool gPlayerTotaled{};
bool LaneRaceActive();
int RosterIndex(void* simable) {
    if (!simable || !LaneRaceActive()) return -1;
    auto status=*reinterpret_cast<void**>(gBase+RaceStatusRva);
    using Index=int(__thiscall*)(void*,void*);
    return reinterpret_cast<Index>(gBase+0x224E00)(status,simable);
}
void SyncRoster() {
    unsigned generation=static_cast<unsigned>(Load(gLatchCount));
    if (generation!=gRosterGeneration) {
        gRivals={}; gPlayerTotaled=false;
        gRosterGeneration=generation;
    }
}
Rival* Opponent(void* engine) {
    auto e=static_cast<std::uint8_t*>(engine);
    if (!e || *reinterpret_cast<std::uintptr_t*>(e)!=gBase+0x5E5A18) return nullptr;
    auto v=*reinterpret_cast<void**>(e+0x40);
    if (!v || *static_cast<std::uintptr_t*>(v)!=gBase+0x5E4A78) return nullptr;
    auto sim=static_cast<std::uint8_t*>(gSimable(v));
    if (!sim || sim!=*reinterpret_cast<void**>(e+0x2C) || *reinterpret_cast<std::uintptr_t*>(sim)!=gBase+0x5E4BB0 || gIsPlayer(sim)) return nullptr;
    int slot=RosterIndex(sim);
    if (slot<0 || slot>=static_cast<int>(gRivals.size())) return nullptr;
    SyncRoster();
    auto& rival=gRivals[slot];
    unsigned handle=*reinterpret_cast<unsigned*>(sim+8);
    if (rival.engine!=engine || rival.handle!=handle) {
        rival={}; rival.engine=engine; rival.handle=handle;
    }
    return &rival;
}
void RivalTick(void* engine,float seconds) {
    auto e=static_cast<std::uint8_t*>(engine);
    int next=0; bool blow=false; float rpm{},redline{};
    unsigned generation{};
    {
        GateLock guard;
        auto rival=Opponent(engine);
        if (!rival || !std::isfinite(seconds) || seconds<=0) return;
        generation=gRosterGeneration;
        auto vehicle=*reinterpret_cast<void**>(e+0x40);
        bool staging=gIsStaging(vehicle);
        rpm=gRpm(e+0x4C); redline=gRedline(e+0x4C);
        blow=rival->mechanics.Tick(seconds,rpm,redline,staging,true);
        rival->cooldown=std::max(0.0f,rival->cooldown-std::min(seconds,0.1f));
        int gear=*reinterpret_cast<int*>(e+0x7C);
        if (!staging && !rival->totaled && !rival->mechanics.blown && !gIsBlown(e+0x70) && !rival->cooldown && gear>=2 && redline>0) {
            int top=reinterpret_cast<IntFunction>(gBase+0x2F12B0)(e+0x44);
            next=drag::RivalGearRequest(gear,top,rpm,redline,rival->handle,rival->mechanics.shifts);
            if (next) rival->cooldown=0.35f;
        }
    }
    if (generation!=static_cast<unsigned>(Load(gLatchCount)) || !Load(gLatched)) return;
    if (blow) gBlow(e+0x70);
    if (next) {
        int old=*reinterpret_cast<int*>(e+0x7C);
        if (gOriginalGear(engine,next)) {
            GateLock guard;
            auto rival=Opponent(engine);
            if (rival && generation==gRosterGeneration && next>old)
                rival->mechanics.Shift(rpm,redline,true);
        }
    }
}
void __fastcall CollisionHook(void* listener,void*,const std::uint8_t* info) {
    gOriginalCollision(listener,info);
    if (!Load(gLatched) || !info) return;
    auto l=static_cast<std::uint8_t*>(listener);
    if (*reinterpret_cast<std::uintptr_t*>(l)!=gBase+0x5E6864) return;
    auto sim=*reinterpret_cast<std::uint8_t**>(l-0x20);
    auto vehicle=*reinterpret_cast<void**>(l-0xC);
    if (!sim || !vehicle || *reinterpret_cast<std::uintptr_t*>(sim)!=gBase+0x5E4BB0 ||
        *static_cast<std::uintptr_t*>(vehicle)!=gBase+0x5E4A78 || gSimable(vehicle)!=sim || gIsStaging(vehicle)) return;
    int slot=RosterIndex(sim);
    if (slot<0) return;
    unsigned flags=*reinterpret_cast<const unsigned*>(info+0x1C);
    unsigned id=*reinterpret_cast<unsigned*>(sim+8);
    if ((flags&7)==3 || !id) return;
    bool first=*reinterpret_cast<const unsigned*>(info+0x3C)==id;
    if (!first && *reinterpret_cast<const unsigned*>(info+0x4C)!=id) return;
    if (flags&(first ? 8u : 32u)) return;
    auto incoming=*reinterpret_cast<const drag::Vector3*>(info+(first ? 0x50 : 0x60));
    drag::Vector3 current{}; gVelocity(sim,&current);
    float before=std::hypot(incoming.x,incoming.y,incoming.z);
    float after=std::hypot(current.x,current.y,current.z);
    if (!drag::TotalsImpact(before,after)) return;
    auto damage=l-8;
    auto destroyed=reinterpret_cast<BoolFunction>(gBase+0x2F77E0);
    if (destroyed(damage)) return;
    reinterpret_cast<AutoFunction>(gBase+0x2EDAE0)(damage);
    if (destroyed(damage)) {
        GateLock guard; SyncRoster();
        if (gIsPlayer(sim)) gPlayerTotaled=true;
        else if (slot<static_cast<int>(gRivals.size()) && gRivals[slot].handle==id) gRivals[slot].totaled=true;
    }
}
void __fastcall AutoShiftHook(void* engine, void*) {
    if (Load(gLatched)) {
        GateLock guard;
        if ((ScopedPlayer(engine) || Opponent(engine)) && *reinterpret_cast<int*>(static_cast<std::uint8_t*>(engine) + 0x7C) >= 2) return;
    }
    gOriginalAuto(engine);
}
int __fastcall GearHook(void* engine, void*, int gear) {
    bool scoped = false; int oldGear = 0; float rpm = 0, redline = 0;
    if (Load(gLatched)) {
        GateLock guard;
        scoped = ScopedPlayer(engine);
        if (!scoped && gear>=2 && Opponent(engine) && *reinterpret_cast<int*>(static_cast<std::uint8_t*>(engine)+0x7C)>=2) return 0;
        if (scoped) {
            auto base = static_cast<std::uint8_t*>(engine);
            oldGear = *reinterpret_cast<int*>(base + 0x7C);
            rpm = gRpm(base + 0x4C); redline = gRedline(base + 0x4C);
        }
    }
    int accepted = gOriginalGear(engine, gear);
    if (scoped && accepted && oldGear >= 2 && gear > oldGear) {
        GateLock guard;
        if (ScopedPlayer(engine)) {
            gMechanics.Shift(rpm, redline, true);
            gPlayerGear = gear; gPlayerRpm = rpm; gPlayerRedline = redline; ++gShiftSerial;
        }
    }
    return accepted;
}
void __fastcall TickHook(void* engine, void*, float seconds) {
    gOriginalTick(engine, seconds);
    if (!Load(gLatched)) return;
    RivalTick(engine,seconds);
    bool blow = false;
    {
        GateLock guard;
        if (!ScopedPlayer(engine, true)) return;
        auto base = static_cast<std::uint8_t*>(engine);
        auto vehicle = *reinterpret_cast<void**>(base + 0x40);
        gPlayerRpm = gRpm(base + 0x4C); gPlayerRedline = gRedline(base + 0x4C);
        gPlayerGear = *reinterpret_cast<int*>(base + 0x7C);
        if (!(gTickCount%4)) {
            drag::Vector3 velocity{};
            gVelocity(gBinding.simable,&velocity);
            gPlayerSpeedKph=std::hypot(velocity.x,velocity.y,velocity.z)*3.6f;
        }
        ++gTickCount;
        blow = gMechanics.Tick(seconds, gPlayerRpm, gPlayerRedline, gIsStaging(vehicle), true);
        gNativeBlown = gIsBlown(base + 0x70);
    }
    if (blow && Load(gLatched)) {
        gBlow(static_cast<std::uint8_t*>(engine) + 0x70);
        GateLock guard; gNativeBlown = gIsBlown(static_cast<std::uint8_t*>(engine) + 0x70);
    }
}
float __fastcall TorqueHook(void* engine, void*, float rpm) {
    float native = gOriginalTorque(engine, rpm);
    if (!Load(gLatched)) return native;
    GateLock guard;
    if (!ScopedPlayer(engine)) {
        auto rival=Opponent(engine);
        if (rival && rival->totaled) return 0;
        return rival ? native*rival->mechanics.Multiplier(true) : native;
    }
    SyncRoster();
    if (gPlayerTotaled) return 0;
    float result = native * gMechanics.Multiplier(true);
    return std::isfinite(result) ? result : native;
}
bool LaneRaceActive() {
    if (!Load(gLatched)) return false;
    auto status = *reinterpret_cast<std::uint8_t**>(gBase + RaceStatusRva);
    auto parameters = status ? *reinterpret_cast<void**>(status + RaceParametersOffset) : nullptr;
    return parameters && status[0x6A3E] && gGetKey(parameters) == static_cast<std::uint32_t>(Load(gTarget));
}
float __fastcall PlayerAngleHook(void* object, void*, void* state) {
    float native=gOriginalPlayerAngle(object,state);
    if (gLaneSuspensionContext!=object || !Load(gLatched)) return native;
    GateLock guard;
    if (!gLane.valid || !LaneRaceActive() || gNativeBlown || GetTickCount()-gLane.updated>200 || gLaneRaceContext!=static_cast<unsigned>(Load(gLatchCount))) return native;
    return gLaneAngleContext;
}
struct AngleContext {
    void* previousObject;
    float previousAngle;
    unsigned previousRace;
    AngleContext(void* object,float angle,unsigned race) : previousObject(gLaneSuspensionContext), previousAngle(gLaneAngleContext), previousRace(gLaneRaceContext) {
        gLaneSuspensionContext=object; gLaneAngleContext=angle; gLaneRaceContext=race;
    }
    ~AngleContext() { gLaneSuspensionContext=previousObject; gLaneAngleContext=previousAngle; gLaneRaceContext=previousRace; }
};
void __fastcall WheelSteeringHook(void* object, void*, void* stateObject, void* leftDirection, void* rightDirection) {
    auto suspension=static_cast<std::uint8_t*>(object);
    auto state=static_cast<std::uint8_t*>(stateObject);
    bool scoped=false; unsigned race{}; float desiredTurns{};
    if (Load(gLatched) && state &&
        *reinterpret_cast<std::uintptr_t*>(suspension)==gBase+0x5E5EE8) {
        GateLock guard;
        if (LaneRaceActive() &&
            *reinterpret_cast<void**>(suspension+0x2C)==gBinding.simable &&
            *reinterpret_cast<void**>(suspension+0x40)==gBinding.vehicle &&
            *reinterpret_cast<void**>(suspension+0x54)==gPlayerInput) {
            if (gLane.valid && gLane.inputObject==gPlayerInput && GetTickCount()-gLane.updated<=200 && !gNativeBlown) {
                *reinterpret_cast<float*>(state+0x8C)=gLane.steering;
                desiredTurns=gLane.steering*gLane.maxSteeringTurns;
                scoped=true; race=static_cast<unsigned>(Load(gLatchCount));
            }
        }
    }
    AngleContext context(scoped ? object : nullptr,desiredTurns,race);
    gOriginalWheelSteering(object,stateObject,leftDirection,rightDirection);
}
struct LocalNavigation {
    alignas(16) std::array<std::uint8_t,0x30C> data{};
    LocalNavigation() { gNavConstruct(data.data()); }
    ~LocalNavigation() { gNavDestroy(data.data()); }
    LocalNavigation(const LocalNavigation&)=delete;
    LocalNavigation& operator=(const LocalNavigation&)=delete;
};
struct NativeSample {
    drag::Vector3 point{}, tangent{};
    bool valid{}, advanced{};
};
void SampleOffset(std::uint8_t* nav,float offset) {
    *reinterpret_cast<float*>(nav+0x2E4)=offset+1.0f;
    gNativeOffset(nav,offset,0.0f);
}
NativeSample SampleNativeLane(std::uint8_t* live,drag::Vector3 car,drag::Vector3 forward,drag::Vector3 guide,float speed,int request,float seconds,drag::NativeLaneState& selection,float previewScale=1.0f) {
    NativeSample result;
    LocalNavigation owned;
    auto nav=owned.data.data();
    *reinterpret_cast<int*>(nav+0x74)=2;
    gNavPath(nav,4);
    std::memcpy(nav+0x59,live+0x59,4);
    nav[0x5D]=0;
    gNavInit(nav,&car,&forward,false,1.0f);
    if (!nav[0x58]) return result;
    auto segment=*reinterpret_cast<short*>(nav+0x8A);
    int direction=nav[0x88];
    auto network=*reinterpret_cast<void**>(gBase+0x777EC0);
    auto segments=*reinterpret_cast<std::uint8_t**>(gBase+0x777ECC);
    if (!network || !segments || segment<0 || direction>1) return result;
    auto segmentRecord=segments+static_cast<unsigned>(segment)*22;
    unsigned flags=*reinterpret_cast<const unsigned short*>(segmentRecord+0xA);
    bool reversed=(((flags>>(direction ? 9 : 10))&1)^static_cast<unsigned>(direction==0))!=0;
    auto profile=static_cast<std::uint8_t*>(gNativeProfile(network,segmentRecord,direction));
    auto otherProfile=static_cast<std::uint8_t*>(gNativeProfile(network,segmentRecord,1-direction));
    SampleOffset(nav,0);
    auto centre=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    auto tangent=*reinterpret_cast<drag::Vector3*>(nav+0xB8);
    SampleOffset(nav,1);
    auto unit=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    if (!drag::Finite(centre) || !drag::Finite(unit) || !drag::Finite(tangent)) return result;
    float ax=unit.x-centre.x, az=unit.z-centre.z;
    float length=std::hypot(ax,az), forwardLength=std::hypot(tangent.x,tangent.z);
    if (!std::isfinite(length) || length<0.25f || length>2.0f || forwardLength<0.01f) return result;
    float right=(ax*tangent.z-az*tangent.x)/forwardLength;
    if (std::abs(right)<length*0.7f) return result;
    int sign=right>0 ? 1 : -1;
    const float scale=*reinterpret_cast<float*>(gBase+0x778C8C);
    auto table=drag::DecodeDragRecords(profile,scale,reversed,sign>0);
    if (!table.valid) table=drag::DecodeDragRecords(otherProfile,scale,reversed,sign>0);
    if (!table.valid) return result;
    float initial=((car.x-centre.x)*ax+(car.z-centre.z)*az)/(length*length);
    if (!std::isfinite(initial) || std::abs(initial)>30.0f || std::abs(centre.y-car.y)>8.0f ||
        std::hypot(centre.x-car.x,centre.z-car.z)>35.0f) return result;
    auto updated=selection;
    if (!updated.Update(table,initial,sign,request,seconds)) return result;
    SampleOffset(nav,updated.current);
    const auto currentPoint=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    if (!drag::Finite(currentPoint)) return result;
    const float actualOffset=((currentPoint.x-centre.x)*ax+(currentPoint.z-centre.z)*az)/(length*length);
    if (!drag::NativeOffsetMatches(updated.current,actualOffset)) return result;
    float responsePhase=updated.tapResponseRemaining/0.6f;
    float tapPreviewScale=1.0f-0.10f*responsePhase*responsePhase*(3.0f-2.0f*responsePhase);
    const float preview=std::clamp(drag::LanePreviewDistance(speed)*tapPreviewScale*previewScale,5.0f,42.0f);
    float guideLength=std::hypot(guide.x,guide.z);
    if (!drag::Finite(guide) || guideLength<0.01f) guide=forward;
    gNavAdvance(nav,preview,&guide,0.0f,false);
    result.advanced=true;
    if (!nav[0x58]) return result;
    SampleOffset(nav,0);
    auto previewCentre=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    result.tangent=*reinterpret_cast<drag::Vector3*>(nav+0xB8);
    SampleOffset(nav,1);
    auto previewUnit=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    if (!drag::Finite(previewCentre) || !drag::Finite(previewUnit) || !drag::Finite(result.tangent)) return result;
    float px=previewUnit.x-previewCentre.x, pz=previewUnit.z-previewCentre.z;
    float previewAxis=std::hypot(px,pz), previewForward=std::hypot(result.tangent.x,result.tangent.z);
    if (previewAxis<0.25f || previewAxis>2.0f || previewForward<0.01f) return result;
    float previewRight=(px*result.tangent.z-pz*result.tangent.x)/previewForward;
    if (std::abs(previewRight)<previewAxis*0.7f) return result;
    float previewOffset=updated.current*sign*(previewRight>0 ? 1 : -1);
    SampleOffset(nav,previewOffset);
    result.point=*reinterpret_cast<drag::Vector3*>(nav+0x94);
    result.tangent=*reinterpret_cast<drag::Vector3*>(nav+0xB8);
    float previewSampled=((result.point.x-previewCentre.x)*px+(result.point.z-previewCentre.z)*pz)/(previewAxis*previewAxis);
    if (!drag::NativeOffsetMatches(previewOffset,previewSampled)) return result;
    auto target=drag::MakeNativeLaneTarget(result.point,result.tangent,car);
    if (!target.valid || std::hypot(result.point.x-car.x,result.point.z-car.z)>preview+35.0f) return result;
    selection=updated;
    result.valid=true;
    return result;
}
bool NativeSteering(void* object,const drag::LaneTarget& target,float seconds,float& output) {
    if (!target.valid) return false;
    auto human=static_cast<std::uint8_t*>(object);
    if (!*reinterpret_cast<void**>(human+0x100) || !*reinterpret_cast<void**>(human+0x1CC) ||
        !*reinterpret_cast<void**>(human+0x1D0) || !*reinterpret_cast<void**>(human+0x1F4)) return false;
    {
        drag::HumanSteeringSnapshot saved(human);
        saved.Configure(target.position);
        gNativeController(human,std::min(seconds,0.05f));
        output=*reinterpret_cast<float*>(human+0x108);
    }
    return std::isfinite(output) && std::abs(output)<=1.01f;
}
void __fastcall HumanUpdateHook(void* object, void*, float seconds) {
    gOriginalHuman(object,seconds);
    if (!Load(gLatched) || !std::isfinite(seconds) || seconds<=0 || seconds>0.25f) return;
    auto human=static_cast<std::uint8_t*>(object);
    if (*reinterpret_cast<std::uintptr_t*>(human)!=gBase+0x5C5260 || *reinterpret_cast<std::uintptr_t*>(human+0x4C)!=gBase+0x5C50D8) return;
    std::uint8_t* nav{}; drag::Vector3 car{}, forward{}, oldPoint{};
    float maximumTurns{}, speed{}; unsigned race{};
    drag::NativeLaneState selection;
    {
        GateLock guard;
        if (!LaneRaceActive() || !gPlayerEngine || gNativeBlown) return;
        if (*reinterpret_cast<void**>(human+0x2C)!=gBinding.simable ||
            *reinterpret_cast<void**>(human+0x40)!=gBinding.vehicle || gPlayerInput!=human+0x4C) return;
        auto vehicle=*reinterpret_cast<void**>(human+0x40);
        auto simable=*reinterpret_cast<void**>(human+0x2C);
        if (gIsStaging(vehicle)) { gLaneAssist.steering=0; gLane.valid=false; return; }
        if (*reinterpret_cast<unsigned*>(human+0xF4)!=0) { gLaneAssist.steering=0; gLane.valid=false; return; }
        if (human[0x29C]) { gLaneAssist.steering=0; gLane.valid=false; return; }
        const float input=*reinterpret_cast<float*>(human+0x108);
        const int request=gLaneRequests.Poll(input,false);
        if (request) gNativeLane.pending=request;
        nav=*reinterpret_cast<std::uint8_t**>(human+0x7C);
        auto suspension=*reinterpret_cast<void**>(human+0x100);
        if (!nav || *reinterpret_cast<std::uintptr_t*>(nav)!=gBase+0x5F30D8 || !nav[0x58] || !suspension ||
            *reinterpret_cast<std::uintptr_t*>(suspension)!=gBase+0x5E5E68) { gLane.valid=false; return; }
        auto position=static_cast<drag::Vector3*>(gPosition(simable));
        if (!position || !drag::Finite(*position) || !std::isfinite(input) || std::abs(input)>1.01f) { gLane.valid=false; return; }
        car=*position; gDirection(vehicle,&forward); maximumTurns=gMaxSteering(suspension);
        drag::Vector3 velocity{}; gVelocity(simable,&velocity);
        speed=std::hypot(velocity.x,velocity.z);
        if (!drag::Finite(velocity) || !std::isfinite(speed) || speed>200.0f) { gLane.valid=false; return; }
        float maximumRadians=drag::SteeringRadians(maximumTurns);
        if (!drag::Finite(forward) || !std::isfinite(maximumRadians) || maximumRadians<0.02f || maximumRadians>1.6f) {
            gLane.valid=false; return;
        }
        oldPoint=*reinterpret_cast<drag::Vector3*>(nav+0x94);
        race=static_cast<unsigned>(Load(gLatchCount)); selection=gNativeLane;
    }
    int attemptedRequest=selection.pending;
    drag::Vector3 guide{oldPoint.x-car.x,oldPoint.y-car.y,oldPoint.z-car.z};
    NativeSample native;
    drag::LaneTarget target;
    drag::SteeringResult result;
    for (unsigned i=0;i<drag::LanePreviewScales.size();++i) {
        auto candidateSelection=selection;
        native=SampleNativeLane(nav,car,forward,guide,speed,attemptedRequest,seconds,candidateSelection,drag::LanePreviewScales[i]);
        target=drag::MakeNativeLaneTarget(native.point,native.tangent,car);
        result=drag::TrackLane(target,car,forward,drag::SteeringRadians(maximumTurns));
        if (native.valid && result.valid) { selection=candidateSelection; break; }
        if (!native.valid && !native.advanced) break;
    }
    if (!native.valid || !result.valid) {
        GateLock guard;
        if (!LaneRaceActive() || race!=static_cast<unsigned>(Load(gLatchCount))) return;
        gLane.valid=false; return;
    }
    selection.pending=0;
    float nativeSteering{};
    bool nativePid=NativeSteering(object,target,seconds,nativeSteering);
    auto bounded=drag::BoundLaneSteering(result.input,result,speed,drag::SteeringRadians(maximumTurns));
    if (nativePid) bounded={std::clamp(nativeSteering,-1.0f,1.0f),1.0f,true,false};
    GateLock guard;
    if (!LaneRaceActive() || gNativeBlown || race!=static_cast<unsigned>(Load(gLatchCount))) return;
    gNativeLane=selection;
    if (!bounded.valid) {
        gLaneAssist.steering=0;
        gLane.valid=false; return;
    }
    gLane.maxSteeringTurns=maximumTurns;
    gLaneAssist.steering=nativePid ? bounded.input : std::clamp(gLaneAssist.SmoothSteering(bounded.input,seconds),-bounded.limit,bounded.limit);
    gLane.steering=gLaneAssist.steering; gLane.valid=true; gLane.updated=GetTickCount(); gLane.inputObject=human+0x4C;
}
struct Snapshot {
    float rpm, redline, heat, speedKph;
    int gear;
    unsigned shiftSerial;
    drag::ShiftGrade grade;
    bool playerBound, blown;
};
Snapshot ReadSnapshot() {
    GateLock guard;
    return {gPlayerRpm,gPlayerRedline,gMechanics.heat,gPlayerSpeedKph,gPlayerGear,
        gShiftSerial,gMechanics.grade,gPlayerEngine!=nullptr,gNativeBlown};
}

void RepairDragEngineForRoaming() {
    {
        GateLock guard;
        Store(gLatched,0);
        Store(gTarget,0);
        using GetPlayer=void*(__stdcall*)(int);
        using FindInterface=void*(__thiscall*)(void*,void*);
        auto player=reinterpret_cast<GetPlayer>(gBase+0xB62B0)(0);
        auto simable=player ? reinterpret_cast<PtrFunction>((*reinterpret_cast<void***>(player))[1])(player) : nullptr;
        auto object=simable ? frontend::Field<void*>(simable,4) : nullptr;
        auto damage=object ? reinterpret_cast<FindInterface>(gBase+0x20CB50)(
            object,reinterpret_cast<void*>(gBase+0xA9D00)) : nullptr;
        if (damage && frontend::Field<std::uintptr_t>(damage,0)==gBase+0x5E595C && gIsBlown(damage)) {
            reinterpret_cast<AutoFunction>(gBase+0x2F11A0)(damage);
        }
        gMechanics.Reset();
        gNativeBlown=false;
        gPlayerEngine=nullptr;
        gPlayerInput=nullptr;
        gPlayerTotaled=false;
    }
}
#include "NativeDragHud.h"

void CheckCode() {
    Require(gBase != 0, "Cannot locate the loaded Carbon executable.");
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(gBase);
    Require(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew == CarbonPeHeaderOffset,
        "The loaded Carbon executable has an unexpected DOS header.");
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(gBase + CarbonPeHeaderOffset);
    Require(nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC,
        "Expected a 32-bit Carbon executable.");
    Require(nt->OptionalHeader.ImageBase == gBase && nt->OptionalHeader.SizeOfImage > 0x778C8C + sizeof(float),
        "The loaded Carbon executable has an unexpected image layout.");
    Require(*reinterpret_cast<const unsigned*>(gBase+0x45E6B8)==gBase+0x45E691,"The Drag Quick Race options slot differs from the verified build.");
    Require(*reinterpret_cast<const unsigned*>(gBase+0x6590E4)==0,"The Drag frontend label differs from the verified build.");
    if (!gHookCreated) {
        Require(std::abs(*reinterpret_cast<const float*>(gBase+0x5C2480)-1.0f/360.0f)<0.00000001f, "The native steering-angle conversion constant differs from the verified turn convention.");
        Require(*reinterpret_cast<const unsigned*>(gBase+0x5F2E2C)==drag::ExtendedDrivingMask, "The native extended driving mask differs from the verified baseline.");
        struct Guard { std::uintptr_t rva; std::array<std::uint8_t,16> code; };
        const Guard guards[] = {
            {0x23C3C0, {0x56,0x8B,0xF1,0x8B,0x4E,0x0C,0x85,0xC9,0x74,0x0A,0x83,0x39,0x00,0x75,0x05,0xE8}},
            {0x21C480, {0x8B,0x41,0x04,0x85,0xC0,0x74,0x0C,0x8B,0x00,0x50,0xE8,0x51,0xC6,0xFF,0xFF,0x83}},
            {0x217A30, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0xAE,0x07,0x9A,0x00,0x50,0xF6,0x05}},
            {0x22EC60, {0x51,0x8B,0x41,0x20,0x85,0xC0,0x89,0x0C,0x24,0x74,0x05,0x8B,0x40,0x20,0x59,0xC3}},
            {0x23C490, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x06,0x0F,0xB6,0x40,0x29,0x5E,0xC3}},
            {0x247CB0, {0x8B,0x41,0x40,0x53,0x8B,0x5C,0x24,0x08,0x56,0x57,0x8B,0x39,0x33,0xD2,0x85,0xFF}},
            {0xA2290, {0x83,0xEC,0x0C,0x57,0x8B,0x7C,0x24,0x18,0x85,0xFF,0x0F,0x84,0x93,0x00,0x00,0x00}},
            {0xA81B0, {0xA0,0xC0,0xE6,0xA9,0x00,0x8B,0x0D,0x7C,0x7A,0xA9,0x00,0x83,0xEC,0x0C,0x84,0xC0}},
            {0x63A80, {0x8B,0x09,0x85,0xC9,0x74,0x05,0xE9,0xF5,0xF9,0xFF,0xFF,0x33,0xC0,0xC2,0x08,0x00}},
            {0x1ADBF7, {0xE8,0x64,0x5B,0x04,0x00,0x8B,0xF0,0x85,0xF6,0x0F,0x84,0xA1,0x07,0x00,0x00,0x83}},
            {0x1ADFC1, {0xE8,0x9A,0x57,0x04,0x00,0x8B,0xF8,0x85,0xFF,0x0F,0x84,0x81,0x00,0x00,0x00,0x6A}},
            {0x1AE172, {0xE8,0xE9,0x55,0x04,0x00,0x8B,0xF8,0x85,0xFF,0x0F,0x84,0x85,0x00,0x00,0x00,0x6A}},
            {0x1AE5A4, {0xE8,0xB7,0x51,0x04,0x00,0x85,0xC0,0x74,0x0E,0x83,0x78,0x18,0x01,0x75,0x08,0x8B}},
            {0x45E5D0, {0x51,0x56,0x57,0x6A,0xFF,0x8B,0xF1,0x8B,0x86,0x68,0x01,0x00,0x00,0x6A,0xFF,0x50}},
            {0x172B90, {0x8B,0x81,0xD4,0x00,0x00,0x00,0x8B,0x4C,0x24,0x04,0x8B,0x04,0x88,0xC2,0x04,0x00}},
            {0x3A9DC0, {0x57,0x8B,0x7C,0x24,0x08,0x85,0xFF,0x0F,0x84,0xC2,0x00,0x00,0x00,0x56,0x8B,0x74}},
            {0x45E639, {0x8B,0xCE,0xE8,0x30,0xA0,0xFF,0xFF,0xEB,0x4F,0x8B,0xCE,0xE8,0xE7,0xA0,0xFF,0xFF}},
            {0x65930, {0xA1,0x9C,0x49,0xA8,0x00,0x56,0x8B,0x70,0x04,0x8B,0x44,0x24,0x08,0x83,0xC6,0x08}},
            {0x246670, {0x51,0x53,0x56,0x57,0x33,0xDB,0x8B,0xF9,0x53,0x68,0xFD,0x99,0x64,0x7B,0x8D,0x4F}},
            {0x613E0, {0x8B,0x01,0x85,0xC0,0x74,0x04,0x8B,0x40,0x10,0xC3,0x33,0xC0,0xC3,0xCC,0xCC,0xCC}},
            {0x21C120, {0x83,0xEC,0x18,0x53,0x55,0x56,0x57,0x8B,0xF9,0x8B,0x07,0x8B,0x4F,0x04,0x33,0xDB}},
            {0x21BF80, {0x53,0x55,0x56,0x57,0x8B,0xF9,0x8B,0x07,0x8B,0x4F,0x04,0x33,0xDB,0x03,0xC8,0x74}},
            {0x21D750, {0xA1,0x84,0x82,0xA9,0x00,0x8B,0x90,0x14,0x6A,0x00,0x00,0x83,0xEC,0x1C,0x53,0x55}},
            {0x189DB0, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0x4D,0xC1,0x99,0x00,0x50,0x64,0x89}},
            {0x1792A0, {0x56,0x57,0x8B,0x7C,0x24,0x0C,0x8B,0xF1,0x8B,0x06,0x57,0xFF,0x50,0x50,0x3C,0x01}},
            {0xD0180, {0xA1,0xB8,0x82,0xA9,0x00,0x56,0x8B,0x74,0x24,0x08,0x83,0xC0,0x50,0x89,0x46,0x0C}},
            {0xBC720, {0x8B,0xC1,0x33,0xC9,0xC7,0x00,0x74,0x8F,0x9C,0x00,0x89,0x48,0x04,0x89,0x48,0x08}},
            {0xA80F0, {0xA1,0x1C,0xAD,0xA8,0x00,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x220D90, {0x6A,0x00,0x68,0xFD,0x99,0x64,0x7B,0x83,0xC1,0x04,0xE8,0xE1,0x2C,0xE4,0xFF,0x85}},
            {0x213890, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x05,0x8B,0x40,0x14,0x5E,0xC3,0x8B}},
            {0xB62B0, {0xA1,0x64,0xFF,0xA9,0x00,0x56,0x8B,0x35,0x5C,0xFF,0xA9,0x00,0x8D,0x0C,0x86,0x3B}},
            {0x20CB50, {0x83,0xEC,0x08,0x56,0x8B,0x71,0x04,0x8B,0x09,0x57,0x8B,0x7C,0x24,0x14,0x8D,0x44}},
            {0x2F11A0, {0x33,0xC0,0x89,0x81,0x10,0x01,0x00,0x00,0x88,0x81,0x0C,0x01,0x00,0x00,0xC3,0xCC}},
            {0x2114A0, {0x53,0x8A,0x5C,0x24,0x08,0x55,0x56,0x57,0x8B,0xF9,0x8D,0xB7,0x94,0x00,0x00,0x00}},
            {0x34ABC0, {0x8B,0x41,0x08,0x56,0x8D,0x71,0x08,0x3B,0xC6,0x74,0x23,0xEB,0x03,0x8D,0x49,0x00}},
            {0x25EF00, {0x6A,0xFF,0x68,0xCF,0x29,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x25BA40, {0xA1,0x64,0xFF,0xA9,0x00,0x85,0xC0,0x74,0x09,0xA1,0x5C,0xFF,0xA9,0x00,0x8B,0x08}},
            {0x241310, {0x6A,0xFF,0x64,0xA1,0x00,0x00,0x00,0x00,0x68,0xFC,0x20,0x9A,0x00,0x50,0x64,0x89}},

            {0x23D820, {0xA1,0x84,0x82,0xA9,0x00,0x8B,0x88,0x14,0x6A,0x00,0x00,0x85,0xC9,0x75,0x66,0x8B}},
            {0x198EF0, {0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x81,0xEC,0xF4,0x00,0x00,0x00,0x53,0x56,0x57,0x8B}},
            {0x1F33A0, {0x83,0xEC,0x0C,0x56,0x8B,0xF1,0x81,0x7E,0x18,0x00,0x01,0x00,0x00,0x7D,0x55,0x53}},
            {0x183B10, {0x64,0xA1,0x00,0x00,0x00,0x00,0x8B,0x4C,0x24,0x08,0x6A,0xFF,0x68,0xF8,0xBD,0x99}},

            {0x170790, {0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x35,0xD9,0x44,0x24,0x08,0x8B,0x48,0x2C,0xD8}},

            {0x191DD0, {0x8B,0x44,0x24,0x04,0x50,0xE8,0xF6,0x3A,0xFF,0xFF,0x85,0xC0,0x74,0x09,0x66,0x8B}},

            {0x1E6F70, {0x53,0x55,0x8B,0x6C,0x24,0x0C,0x56,0x57,0x55,0x8B,0xF1,0xE8,0x30,0x55,0xFF,0xFF}},
            {0x1982C0, {0x55,0x8B,0x6C,0x24,0x08,0x57,0x8B,0xF9,0xC6,0x47,0x04,0x01,0xA1,0x6C,0x95,0xA9}},
            {0x1983B0, {0x8B,0x44,0x24,0x04,0x56,0x8B,0xF1,0x8B,0x0D,0x40,0x7B,0xA9,0x00,0x50,0xE8,0x0D}},
            {0x160580, {0x8B,0x44,0x24,0x04,0x8B,0x08,0x81,0xF9,0x00,0x00,0x30,0xB3,0x0F,0x85,0xE4,0x00}},
            {0x191E50, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0xFB,0xC6,0x99,0x00,0x50,0x64,0x89}},
            {0x15CFD0, {0x53,0x55,0x56,0x57,0x8B,0x7C,0x24,0x14,0x85,0xFF,0x0F,0x84,0x02,0x01,0x00,0x00}},
            {0x172260, {0x8B,0x44,0x24,0x08,0x8B,0x54,0x24,0x04,0x8B,0x09,0x6A,0x00,0x6A,0x00,0x50,0x52}},
            {0x170460, {0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x2A,0x8A,0x4C,0x24,0x08,0x84,0xC9,0x8B,0x48}},
            {0x1910E0, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0x28,0xC6,0x99,0x00,0x50,0x64,0x89}},
            {0x1704A0, {0x57,0x8B,0x7C,0x24,0x08,0x85,0xFF,0x74,0x2D,0x8B,0x44,0x24,0x0C,0x56,0x50,0x8B}},
            {0x170C90, {0x83,0xEC,0x10,0x56,0x8B,0x74,0x24,0x18,0x85,0xF6,0x74,0x18,0x8B,0x44,0x24,0x1C}},
            {0x171040, {0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x13,0x8B,0x4C,0x24,0x08,0x39,0x48,0x24,0x74}},
            {0x1705A0, {0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x6C,0x56,0x8B,0x75,0x08,0x85,0xF6,0x0F}},
            {0x229670, {0x8B,0x44,0x24,0x04,0x56,0x50,0x8B,0xF1,0xE8,0x83,0xB7,0xFF,0xFF,0x83,0xF8,0xFF}},
            {0x212230, {0x8B,0x81,0x08,0x6A,0x00,0x00,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x21B440, {0x56,0x8B,0xF1,0x8B,0x86,0x1C,0x6A,0x00,0x00,0x85,0xC0,0x75,0x08,0xD9,0x05,0x8C}},
            {0xA0350, {0x56,0x8B,0x74,0x24,0x08,0xB8,0xD3,0x4D,0x62,0x10,0xF7,0xEE,0xC1,0xFA,0x06,0x8B}},
            {0xA62E0, {0x8B,0x0D,0x18,0xAD,0xA8,0x00,0x33,0xC0,0x85,0xC9,0x0F,0x9F,0xC0,0xC3,0xCC,0xCC}},

            {0x1858D0, {0x53,0x56,0x8B,0x74,0x24,0x0C,0x57,0x56,0x8B,0xF9,0xE8,0xD1,0xB8,0xEE,0xFF,0x03}},
            {0x185AA0, {0x57,0x8B,0x7C,0x24,0x08,0x8B,0x07,0x3D,0x03,0x02,0x03,0x00,0x74,0x0B,0x3D,0x10}},

            {0x22F590, {0x6A,0xFF,0x68,0xC8,0x13,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0xA9150, {0x8B,0x44,0x24,0x0C,0x85,0xC0,0x74,0x47,0x8B,0x0D,0x8C,0x82,0xA9,0x00,0x50,0xE8}},
            {0x1C3DB0, {0xE8,0xAB,0xB8,0xF0,0xFF,0x8A,0x44,0x24,0x13,0x84,0xC0,0x0F,0x85,0x9C,0x01,0x00}},
            {0x1C417E, {0xE8,0xDD,0xB4,0xF0,0xFF,0x8A,0x44,0x24,0x13,0x84,0xC0,0x0F,0x85,0x31,0x01,0x00}},
            {0x247C50, {0x8B,0x41,0x40,0x53,0x8B,0x5C,0x24,0x08,0x56,0x57,0x8B,0x39,0x33,0xD2,0x85,0xFF}},
            {0xA230E, {0xE8,0x9D,0x59,0x1A,0x00,0x8B,0x54,0x24,0x18,0xF6,0xD8,0x5E,0x5F,0x1B,0xC0,0x40}},
            {0xA833D, {0xE8,0x6E,0xF9,0x19,0x00,0x8B,0x4C,0x24,0x1C,0xF6,0xD8,0x1A,0xC0,0xFE,0xC0,0x5F}},
            {0x247F03, {0xE8,0xA8,0xFD,0xFF,0xFF,0x5F,0x5E,0xC2,0x04,0x00,0xCC,0xCC,0xCC,0x6A,0xFF,0x68}},
            {0x24B6AC, {0xE8,0x9F,0xC5,0xFF,0xFF,0x5E,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x24C9E9, {0xE8,0x62,0xB2,0xFF,0xFF,0x5E,0xC3,0x32,0xC0,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x1C4020, {0x56,0xE8,0x6A,0xB5,0x06,0x00,0x85,0xC0,0x74,0x33,0x8B,0x48,0x20,0x85,0xC9,0x0F}},
            {0x1C4177, {0xE8,0x74,0x3F,0xEE,0xFF,0x8B,0xC8,0xE8,0xDD,0xB4,0xF0,0xFF,0x8A,0x44,0x24,0x13}},
            {0x23E540, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x0A,0x8B,0x40,0x18,0xC1,0xE8,0x0F}},
            {0x23E0C0, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x0A,0x8B,0x40,0x18,0xC1,0xE8,0x04}},
            {0x21BEF0, {0x53,0x55,0x56,0x57,0x8B,0xF9,0x8B,0x07,0x8B,0x4F,0x04,0x33,0xDB,0x03,0xC8,0x74}},
            {0x2454F0, {0x8B,0x44,0x24,0x04,0x56,0x50,0xE8,0xF5,0x69,0xFD,0xFF,0x8B,0xF0,0x85,0xF6,0x75}},

            {0x251320, {0xE8,0xFB,0xA3,0x15,0x00,0x8B,0xC8,0xE8,0x84,0x39,0x18,0x00,0xB9,0xD0,0x4D,0xB7}},
            {0x23D900, {0x83,0xEC,0x24,0x56,0x8B,0x74,0x24,0x38,0x85,0xF6,0x74,0x1A,0x8D,0x44,0x24,0x1C}},
            {0x2121E0, {0x8B,0x44,0x24,0x04,0x69,0xC0,0x84,0x03,0x00,0x00,0x8D,0x44,0x08,0x18,0xC2,0x04}},
            {0x212FE0, {0x8B,0x54,0x24,0x04,0x8D,0x81,0xA8,0x01,0x00,0x00,0x56,0x8B,0x32,0x89,0x30,0x8B}},

            {0x246B00, {0x83,0xEC,0x4C,0x53,0x55,0x56,0x8B,0xF1,0x8B,0x86,0x14,0x6A,0x00,0x00,0x85,0xC0}},
            {0x135150, {0x8B,0xC1,0x33,0xC9,0xC7,0x00,0x60,0x94,0xDF,0x50,0x89,0x48,0x04,0xC7,0x40,0x08}},

            {0x166070, {0x8B,0x54,0x24,0x08,0x8B,0x44,0x24,0x04,0x52,0x51,0x89,0x41,0x04,0xE8,0x8E,0xFE}},
            {0x252080, {0xA1,0xB8,0x31,0xA6,0x00,0x83,0xEC,0x24,0x85,0xC0,0x56,0x75,0x1B,0x8D,0x4C,0x24}},

            {0x250DB0, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0x3E,0x26,0x9A,0x00,0x50,0x64,0x89}},

            {0x2B6A10, {0x6A,0x01,0xE8,0x09,0x80,0x05,0x00,0xA1,0x2C,0x70,0xB7,0x00,0x83,0xC4,0x04,0x85}},
            {0x22D950, {0x51,0x53,0x56,0x8B,0xF1,0xE8,0x06,0x82,0xFF,0xFF,0x84,0xC0,0x75,0x1F,0x8B,0xCE}},

            {0x3BBED0, {0x8B,0x0D,0xBC,0x9B,0xA9,0x00,0xB8,0x03,0x00,0x00,0x00,0x83,0xEC,0x0C,0x3B,0xC8}},
            {0x2BBF80, {0x56,0x6A,0x00,0x6A,0x00,0x8B,0xF1,0xC7,0x05,0xBC,0x9B,0xA9,0x00,0x05,0x00,0x00}},
            {0x2A8A30, {0x8A,0x0D,0xB0,0x9B,0xA9,0x00,0x33,0xC0,0x3A,0xC8,0x74,0x05,0xA2,0xB0,0x9B,0xA9}},
            {0x24B4C0, {0x8B,0x44,0x24,0x04,0x8B,0x0D,0x84,0x82,0xA9,0x00,0x56,0x50,0xE8,0x3F,0xE2,0xFD}},
            {0x268A10, {0x6A,0xFF,0x68,0x59,0x2C,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x255F70, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0xBF,0x26,0x9A,0x00,0x50,0xA1,0x84}},

            {0x1F3760, {0x6A,0xFF,0x68,0xE8,0xF9,0x99,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x1C3D4F, {0xE8,0x0C,0xFA,0x02,0x00,0x85,0xC0,0x0F,0x84,0x01,0x02,0x00,0x00,0x83,0x78,0x18}},
            {0x1C411B, {0xE8,0x40,0xF6,0x02,0x00,0x85,0xC0,0x0F,0x84,0x98,0x01,0x00,0x00,0x83,0x78,0x18}},
            {0x1C3B30, {0x6A,0xFF,0x68,0xCB,0xE1,0x99,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x1785D0, {0x85,0xDB,0x75,0x03,0x33,0xC0,0xC3,0x55,0x56,0x8B,0xF1,0x57,0x33,0xFF,0x4E,0x8B}},
            {0xA97C0, {0x8B,0x44,0x24,0x10,0x85,0xC0,0x74,0x0E,0x8B,0x0D,0x8C,0x82,0xA9,0x00,0x50,0xE8}},
            {0x1FFCD0, {0x6A,0xFF,0x68,0xEB,0x01,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x1EC6F0, {0x56,0x8B,0xF1,0x8B,0x46,0x14,0x85,0xC0,0x57,0x74,0x10,0x50,0xE8,0x9F,0x4E,0x0B}},
            {0x1EC910, {0x56,0x8B,0x71,0x4C,0x85,0xF6,0x57,0x74,0x2C,0x8B,0x7C,0x24,0x0C,0x8D,0x49,0x00}},
            {0x1F78A0, {0x8B,0xC1,0xC7,0x00,0x28,0x76,0x9D,0x00,0xB9,0xFE,0xCA,0xAD,0xAB,0x89,0x48,0x04}},
            {0x1A0910, {0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF,0x68,0x38,0xCC,0x99,0x00,0x50,0x8B,0x44}},
            {0x213590, {0x8B,0x41,0x04,0x85,0xC0,0x74,0x04,0x83,0xC0,0x04,0xC3,0x8B,0x49,0x08,0x6A,0x00}},
            {0x213260, {0x8B,0x41,0x04,0x03,0x01,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x23E4E0, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x0A,0x8B,0x40,0x18,0xC1,0xE8,0x0E}},

            {0x255F9D, {0xe8,0xfe,0xd6,0xfb,0xff,0x83,0xf8,0x4,0x75,0x1f,0x8b,0xd,0x84,0x82,0xa9,0x0}},
            {0x224E00, {0x53,0x55,0x8b,0x6c,0x24,0xc,0x85,0xed,0x56,0x57,0x8b,0xd9,0x74,0x51,0x8b,0x83}},
            {0x2F12B0, {0x8b,0x89,0x4,0x1,0x0,0x0,0xe8,0x85,0x8,0xd7,0xff,0x48,0xc3,0xcc,0xcc,0xcc}},
            {0x2F7830, {0x6a,0xff,0x68,0x90,0x87,0x9a,0x0,0x64,0xa1,0x0,0x0,0x0,0x0,0x50,0x64,0x89}},
            {0x2F77E0, {0xd9,0x41,0x34,0xd8,0x1d,0x78,0x24,0x9c,0x0,0xdf,0xe0,0xf6,0xc4,0x1,0x75,0x6}},
            {0x2EDAE0, {0x6a,0xff,0x68,0x1f,0x7f,0x9a,0x0,0x64,0xa1,0x0,0x0,0x0,0x0,0x50,0x64,0x89}},
            {0x406820, {0x6A,0xFF,0x68,0x8A,0xE5,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x3F7BF0, {0x6A,0xFF,0x68,0xCA,0xE0,0x9A,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x40F180, {0x8B,0x54,0x24,0x10,0x53,0x8B,0x5C,0x24,0x08,0x56,0x57,0x8B,0x7C,0x24,0x14,0x8B}},
            {0x40C600, {0x56,0x8B,0xF1,0x8A,0x46,0x58,0x84,0xC0,0x0F,0x84,0xA7,0x00,0x00,0x00,0x8A,0x46}},
            {0x3EC0D0, {0x8B,0x44,0x24,0x04,0x89,0x41,0x78,0xC2,0x04,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}},
            {0x2C0D40, {0x8B,0x41,0x4C,0x85,0xC0,0x74,0x1E,0x8B,0xC8,0x8B,0x01,0xFF,0x50,0x24,0x8B,0x10}},

            {0x31700, {0x83,0xEC,0x40,0x56,0x8B,0xF1,0x8B,0x46,0x44,0x57,0x8D,0x7E,0x44,0x8B,0xCF,0xFF}},

            {0x402800, {0xD9,0x44,0x24,0x08,0x56,0xD8,0x1D,0x8C,0x24,0x9C,0x00,0x8B,0xF1,0xDF,0xE0,0xF6}},
            {0x3EB290, {0x8B,0x4C,0x24,0x04,0x8B,0x44,0x24,0x08,0x0F,0xB7,0x04,0x41,0x8B,0x0D,0xC8,0x7E}},

            {0x2EF540, {0x83,0xEC,0x0C,0x56,0x8B,0xF1,0x8B,0x8E,0x04,0x01,0x00,0x00,0x89,0x4C,0x24,0x04}},

            {0x2EF820, {0x83,0xEC,0x24,0x56,0x8B,0xF1,0x8B,0x8E,0xD8,0x00,0x00,0x00,0x85,0xC9,0x57,0x74}},
            {0x2EE2E0, {0x83,0xEC,0x10,0xD9,0x44,0x24,0x14,0x53,0x55,0x56,0x57,0x8B,0x7C,0x24,0x28,0xD9}},
            {0x2F8A70, {0x8B,0x44,0x24,0x04,0x83,0xF8,0x02,0x73,0x10,0xD9,0x84,0x81,0xC4,0x00,0x00,0x00}},

            {0x2C6610, {0x83,0xEC,0x1C,0x8B,0x81,0x6C,0xFF,0xFF,0xFF,0x81,0xC1,0x6C,0xFF,0xFF,0xFF,0x56}},
            {0x2F8AA0, {0xD9,0x81,0xCC,0x00,0x00,0x00,0xD8,0x0D,0x80,0x24,0x9C,0x00,0xC3,0xCC,0xCC,0xCC}},

            {0x3C670, {0x6A,0xFF,0x64,0xA1,0x00,0x00,0x00,0x00,0x68,0x1B,0x42,0x99,0x00,0x50,0xB8,0x30}},
            {0x2D7EA0, {0x8B,0x41,0x4C,0x85,0xC0,0x74,0x07,0x8B,0xC8,0x8B,0x01,0xFF,0x60,0x20,0xB8,0x0C}},

            {0x24A6C0, {0x8A,0x44,0x24,0x04,0x88,0x81,0x3E,0x6A,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC}},
            {0x2FFBD0, {0x83,0xec,0x08,0x56,0x8b,0xf1,0x8b,0x46,0x7c,0x85,0xc0,0x0f,0x84,0x2d,0x01,0x00}},
            {0x2E3590, {0x56,0x8b,0xf1,0x8b,0x8e,0x48,0x01,0x00,0x00,0x57,0xe8,0xa1,0xe5,0xd7,0xff,0x8b}},
            {0x2FFE60, {0x83,0xec,0x40,0x55,0x56,0x8b,0xf1,0x8b,0xae,0x08,0x01,0x00,0x00,0x85,0xed,0x0f}},
            {0x2E31E0, {0x8b,0x44,0x24,0x04,0x56,0x8b,0xf1,0x50,0x8d,0x8e,0x34,0x01,0x00,0x00,0x51,0xe8}},
            {0x2F0FE0, {0xd9,0x81,0x18,0x01,0x00,0x00,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}},
            {0x2F1010, {0x8b,0x81,0xec,0x00,0x00,0x00,0xd9,0x40,0x58,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}},
            {0x2F12C0, {0x6a,0xff,0x68,0x1f,0x81,0x9a,0x00,0x64,0xa1,0x00,0x00,0x00,0x00,0x50,0x64,0x89}},
            {0x2D6C30, {0x8b,0x51,0x58,0x33,0xc0,0x85,0xd2,0x0f,0x95,0xc0,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc}},
            {0x2D7EC0, {0x8d,0x81,0x48,0xff,0xff,0xff,0x85,0xc0,0x74,0x07,0x8d,0x81,0x6c,0xff,0xff,0xff}},
            {0x2D7FD0, {0x8a,0x91,0xad,0x00,0x00,0x00,0x33,0xc0,0x84,0xd2,0x0f,0x95,0xc0,0xc3,0xcc,0xcc}},
            {0x2F1190, {0x8a,0x81,0x0c,0x01,0x00,0x00,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}},
        };
        for (const auto& guard : guards) {
            if (std::memcmp(reinterpret_cast<void*>(gBase+guard.rva),guard.code.data(),guard.code.size())==0)
                continue;
            char message[160]{};
            std::snprintf(message,sizeof(message),"Carbon code at RVA 0x%06X differs from the supported executable or was changed by another mod.",static_cast<unsigned>(guard.rva));
            throw std::runtime_error(message);
        }
    }
    constexpr std::array<std::uint8_t, 16> race{0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x06,0x0F,0xBE,0x40,0x2B,0x5E,0xC3};
    constexpr std::array<std::uint8_t, 10> key{0x8B,0x41,0x04,0x85,0xC0,0x74,0x03,0x8B,0x00,0xC3};
    if (!gHookCreated) Require(std::memcmp(reinterpret_cast<void*>(gBase + RaceTypeRva), race.data(), race.size()) == 0, "Race-type code differs from the verified build or is already hooked.");
    Require(std::memcmp(reinterpret_cast<void*>(gBase + CollectionKeyRva), key.data(), key.size()) == 0, "The collection-key routine differs from the verified build.");
}
void InstallRuntime() {
    frontend::PrepareIcons();
    native_drag_hud::Prepare();
    if (!gHookCreated) {
        auto status = MH_Initialize(); Require(status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED, "MinHook initialization failed.");
        LPVOID original{};
        status = MH_CreateHook(reinterpret_cast<LPVOID>(gBase + RaceTypeRva), reinterpret_cast<LPVOID>(&RaceTypeHook), &original);
        Require(status == MH_OK, "Could not create the race-type trampoline.");
        gOriginal = reinterpret_cast<RaceTypeFunction>(original);
        auto create = [&](std::uintptr_t rva, LPVOID callback, auto& destination) {
            LPVOID trampoline{};
            const auto result=MH_CreateHook(reinterpret_cast<LPVOID>(gBase+rva),callback,&trampoline);
            if (result!=MH_OK) {
                char message[160]{};
                std::snprintf(message,sizeof(message),"Could not install the Carbon hook at RVA 0x%06X (%s).",static_cast<unsigned>(rva),MH_StatusToString(result));
                throw std::runtime_error(message);
            }
            destination = reinterpret_cast<std::decay_t<decltype(destination)>>(trampoline);
        };
        try {
            create(0x166070,reinterpret_cast<LPVOID>(&loading::MessageHook),loading::originalMessage);
            create(0x252080,reinterpret_cast<LPVOID>(&loading::MusicRequestHook),loading::originalMusicRequest);
            create(0x250DB0,reinterpret_cast<LPVOID>(&loading::NisHook),loading::originalNis);
            create(0x246B00,reinterpret_cast<LPVOID>(&loading::StatusUpdateHook),loading::originalStatusUpdate);
            create(0x21D750,reinterpret_cast<LPVOID>(&loading::UpdateIconVisibilityHook),loading::originalUpdateIconVisibility);
            create(0x189DB0,reinterpret_cast<LPVOID>(&loading::WorldMapFlowDoneHook),loading::originalWorldMapFlowDone);
            create(0x251320,reinterpret_cast<LPVOID>(&loading::RestoreStartHook),loading::originalRestoreStart);
            create(0x246670,reinterpret_cast<LPVOID>(&frontend::CareerAvailableHook),frontend::originalCareerAvailable);
            create(0x22F590,reinterpret_cast<LPVOID>(&frontend::CareerTrackHook),frontend::originalCareerTrack);
            create(0xA81B0,reinterpret_cast<LPVOID>(&frontend::MapTrackLockedHook),frontend::originalMapTrackLocked);
            create(0xA2290,reinterpret_cast<LPVOID>(&frontend::EventQuickRaceLockedHook),frontend::originalEventQuickRaceLocked);
            create(0x247CB0,reinterpret_cast<LPVOID>(&frontend::QuickRaceUnlockedHook),frontend::originalQuickRaceUnlocked);
            create(0x21C120,reinterpret_cast<LPVOID>(&frontend::MapLocationHook),frontend::originalMapLocation);
            create(0x23C490,reinterpret_cast<LPVOID>(&frontend::RaceRegionHook),frontend::originalRaceRegion);
            create(0x247C50,reinterpret_cast<LPVOID>(&frontend::CareerRaceUnlockedHook),frontend::originalCareerRaceUnlocked);
            create(0x1E6F70,reinterpret_cast<LPVOID>(&native_drag_hud::UpdateHook),native_drag_hud::originalUpdate);
            create(0x15CFD0,reinterpret_cast<LPVOID>(&frontend::TextureLookupHook),frontend::originalTextureLookup);
            create(0x063A80,reinterpret_cast<LPVOID>(&frontend::TriggerAttributeHook),frontend::originalAttributeLookup);
            create(0x45E5D0,reinterpret_cast<LPVOID>(&frontend::SetupOptionsHook),frontend::originalSetupOptions);
            create(0x3A9DC0,reinterpret_cast<LPVOID>(&loading::FillRaceHook),loading::originalFillRace);
            create(0x25EF00,reinterpret_cast<LPVOID>(&native_drag_hud::PostRaceHook),native_drag_hud::originalPostRace);
            create(0x25BA40,reinterpret_cast<LPVOID>(&native_drag_hud::RaceOverCameraHook),native_drag_hud::originalRaceOverCamera);
            create(0x241310,reinterpret_cast<LPVOID>(&native_drag_hud::SetRoamingHook),native_drag_hud::originalSetRoaming);
            create(0x198EF0,reinterpret_cast<LPVOID>(&native_drag_hud::MultiImageHook),native_drag_hud::originalMultiImage);
            create(0x1982C0,reinterpret_cast<LPVOID>(&native_drag_hud::BeginRenderHook),native_drag_hud::originalBeginRender);
            create(0x1983B0,reinterpret_cast<LPVOID>(&native_drag_hud::PopHook),native_drag_hud::originalPop);
            create(0x2B6A10,reinterpret_cast<LPVOID>(&loading::FinishedHook),loading::originalFinished);
            create(0x22D950,reinterpret_cast<LPVOID>(&loading::NotifyHook),loading::originalNotify);
            create(0x23D820,reinterpret_cast<LPVOID>(&loading::InGameStartHook),loading::originalInGameStart);
            create(0x3BBED0,reinterpret_cast<LPVOID>(&loading::StartHook),loading::originalStart);
            create(0x2BBF80,reinterpret_cast<LPVOID>(&loading::TrackBeginHook),loading::originalTrackBegin);
            create(0x2A8A30,reinterpret_cast<LPVOID>(&loading::TrackEndHook),loading::originalTrackEnd);
            create(0x24B4C0,reinterpret_cast<LPVOID>(&loading::ActivityHook),loading::originalActivity);
            create(0x268A10,reinterpret_cast<LPVOID>(&loading::RacersHook),loading::originalRacers);
            create(0x255F70,reinterpret_cast<LPVOID>(&loading::RaceHook),loading::originalRace);
            create(0x1F3760,reinterpret_cast<LPVOID>(&frontend::FindHook),frontend::originalFind);
            create(0x1C3B30,reinterpret_cast<LPVOID>(&frontend::InitHook),frontend::originalInit);
            create(0x1785D0,reinterpret_cast<LPVOID>(&frontend::LabelHook),frontend::gDragStringSearch);
            create(0xA97C0,reinterpret_cast<LPVOID>(&frontend::GetTrackDisplayNameHook),frontend::originalGetTrackDisplayName);
            create(0x2F7830, reinterpret_cast<LPVOID>(&CollisionHook), gOriginalCollision);
            create(0x2FFBD0, reinterpret_cast<LPVOID>(&AutoShiftHook), gOriginalAuto);
            create(0x2E3590, reinterpret_cast<LPVOID>(&GearHook), gOriginalGear);
            create(0x2FFE60, reinterpret_cast<LPVOID>(&TickHook), gOriginalTick);
            create(0x2E31E0, reinterpret_cast<LPVOID>(&TorqueHook), gOriginalTorque);
            create(0x3C670, reinterpret_cast<LPVOID>(&HumanUpdateHook), gOriginalHuman);
            create(0x2EF820, reinterpret_cast<LPVOID>(&WheelSteeringHook), gOriginalWheelSteering);
            create(0x2EF540, reinterpret_cast<LPVOID>(&PlayerAngleHook), gOriginalPlayerAngle);
        } catch (...) { MH_Uninitialize(); gOriginal = nullptr; throw; }
        gGetKey = reinterpret_cast<KeyFunction>(gBase + CollectionKeyRva);
        gRpm = reinterpret_cast<FloatFunction>(gBase + 0x2F0FE0);
        gRedline = reinterpret_cast<FloatFunction>(gBase + 0x2F1010);
        gBlow = reinterpret_cast<BoolFunction>(gBase + 0x2F12C0);
        gIsBlown = reinterpret_cast<BoolFunction>(gBase + 0x2F1190);
        gSimable = reinterpret_cast<PtrFunction>(gBase + 0x2D7EC0);
        gIsPlayer = reinterpret_cast<BoolFunction>(gBase + 0x2D6C30);
        gIsStaging = reinterpret_cast<BoolFunction>(gBase + 0x2D7FD0);
        gNativeController=reinterpret_cast<TickFunction>(gBase+0x31700);
        gNavConstruct=reinterpret_cast<AutoFunction>(gBase+0x406820);
        gNavDestroy=reinterpret_cast<AutoFunction>(gBase+0x3F7BF0);
        gNavInit=reinterpret_cast<NavInitFunction>(gBase+0x40F180);
        gNavAdvance=reinterpret_cast<NavAdvanceFunction>(gBase+0x40C600);
        gNavPath=reinterpret_cast<NavPathFunction>(gBase+0x3EC0D0);
        gVelocity=reinterpret_cast<VectorFunction>(gBase+0x2C0D40);
        gDirection = reinterpret_cast<VectorFunction>(gBase + 0x2C6610);
        gMaxSteering = reinterpret_cast<FloatFunction>(gBase + 0x2F8AA0);
        gPosition = reinterpret_cast<PtrFunction>(gBase + 0x2D7EA0);
        gNativeOffset=reinterpret_cast<NativeOffsetFunction>(gBase+0x402800);
        gNativeProfile=reinterpret_cast<ProfileFunction>(gBase+0x3EB290);
        gHookCreated = true;
    }
    if (!gHookEnabled) {
        auto status = MH_EnableHook(MH_ALL_HOOKS); Require(status == MH_OK, "Could not enable the race-type hook."); gHookEnabled = true;
    }
    frontend::EnableDragMenu();
}

bool CheckCompatibility() {
    try {
        if (HashFile(ModulePath(nullptr), FileHash::CarbonImage) != CarbonImageHash)
            return false;
        CheckCode();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

DWORD WINAPI RuntimeThread(void*) {
    gBase=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!CheckCompatibility()) return 1;
    try {
        InstallRuntime();
        return 0;
    } catch (const std::exception& error) {
        MessageBoxA(nullptr,error.what(),"NFSC_DragRace",MB_OK|MB_ICONWARNING|MB_TASKMODAL|MB_SETFOREGROUND);
        return 1;
    }
}
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        gModule = module; auto thread = CreateThread(nullptr,0,RuntimeThread,nullptr,0,nullptr); if (thread) CloseHandle(thread);
    }
    return TRUE;
}
