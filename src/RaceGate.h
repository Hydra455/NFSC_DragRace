#pragma once
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace drag {
constexpr bool IsAuthoredDrag(int resolvedType,std::uint32_t key) {
    return resolvedType==2 && key!=0;
}
constexpr bool StandaloneDragFlags(bool neverInQuickRace,bool challengeRace,bool dDayRace) {
    return !neverInQuickRace && !challengeRace && !dDayRace;
}
inline bool ValidMapPosition(float x,float y) {
    return std::isfinite(x) && std::isfinite(y) && (x!=0.0f || y!=0.0f);
}
constexpr bool ValidRegion(int region) {
    return region>=0 && region<=5;
}
constexpr bool IsDragCareerUnlockCaller(std::uintptr_t returnRva) {
    return returnRva==0x24B6B1 || returnRva==0x24C9EE;
}
struct QuickListDragEntries {
    int areaFilter=-1;

    void RecordCareer(std::uint32_t eventHash,int region) {
        if (eventHash && ValidRegion(region) && (areaFilter==-1 || areaFilter==region))
            careerEvents.insert(eventHash);
    }

    bool HasCareerEntry(std::uint32_t eventHash) const {
        return careerEvents.find(eventHash)!=careerEvents.end();
    }

private:
    std::unordered_set<std::uint32_t> careerEvents;
};

constexpr bool PhysicsScope(bool latched, bool racing, bool player, std::uint32_t key, std::uint32_t target) {
    return latched && racing && player && key != 0 && key == target;
}
}
