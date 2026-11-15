#pragma once
#include <algorithm>
#include <cmath>

namespace drag {
inline int RivalGearRequest(int gear,int top,float rpm,float redline,unsigned driver,unsigned shifts) {
    if (gear<2 || gear>top || !std::isfinite(rpm) || !std::isfinite(redline) || rpm<=0 || redline<=0) return 0;
    float shiftPoint=0.95f+0.01f*((driver+shifts)%5);
    if (gear<top && rpm>=redline*shiftPoint) return gear+1;
    if (gear>2 && rpm<redline*0.40f) return gear-1;
    return 0;
}
inline bool TotalsImpact(float incoming,float outgoing) {
    return std::isfinite(incoming) && std::isfinite(outgoing) && outgoing>=0 && incoming>44.703f && incoming-outgoing>17.8812f;
}
enum class ShiftGrade { None, Early, Good, Perfect, Late };
struct Mechanics {
    float bonusSeconds{};
    float bonusAmount{};
    float heat{};
    ShiftGrade grade{ShiftGrade::None};
    unsigned shifts{};
    unsigned perfect{};
    bool blown{};
    void Reset() { *this = Mechanics{}; }
    static ShiftGrade Grade(float rpm, float redline) {
        if (!std::isfinite(rpm) || !std::isfinite(redline) || rpm <= 0 || redline <= 0) return ShiftGrade::None;
        float ratio = rpm / redline;
        if (ratio < 0.90f) return ShiftGrade::Early;
        if (ratio < 0.96f) return ShiftGrade::Good;
        if (ratio <= 1.015f) return ShiftGrade::Perfect;
        return ShiftGrade::Late;
    }
    void Shift(float rpm, float redline, bool bonusEnabled) {
        grade = Grade(rpm, redline);
        if (grade == ShiftGrade::None) return;
        ++shifts;
        if (grade == ShiftGrade::Perfect) ++perfect;
        bonusAmount = bonusEnabled ? (grade == ShiftGrade::Perfect ? 0.08f : grade == ShiftGrade::Good ? 0.04f : 0.0f) : 0.0f;
        bonusSeconds = bonusAmount > 0 ? 0.8f : 0.0f;
    }
    float Multiplier(bool enabled) const {
        return enabled && !blown && bonusSeconds > 0 ? 1.0f + bonusAmount * std::clamp(bonusSeconds / 0.8f, 0.0f, 1.0f) : 1.0f;
    }
    bool Tick(float seconds, float rpm, float redline, bool staging, bool damageEnabled) {
        if (!std::isfinite(seconds) || seconds <= 0) return false;
        seconds = std::min(seconds, 0.1f);
        bonusSeconds = std::max(0.0f, bonusSeconds - seconds);
        if (!damageEnabled) { heat = 0; return false; }
        if (blown || staging || !std::isfinite(rpm) || !std::isfinite(redline) || redline <= 0) return false;
        heat = std::clamp(heat + seconds * (rpm >= redline ? 0.5f : -0.75f), 0.0f, 1.0f);
        if (heat >= 0.9999f) { blown = true; bonusSeconds = 0; return true; }
        return false;
    }
};
inline const char* GradeName(ShiftGrade grade) {
    switch (grade) {
    case ShiftGrade::Early: return "EARLY";
    case ShiftGrade::Good: return "GOOD";
    case ShiftGrade::Perfect: return "PERFECT";
    case ShiftGrade::Late: return "LATE";
    default: return "--";
    }
}
}
