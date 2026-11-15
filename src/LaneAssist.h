#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace drag {
constexpr float RadiansPerTurn = 6.2831853071795864769f;
inline float SteeringRadians(float nativeTurns) { return nativeTurns * RadiansPerTurn; }
inline float LanePreviewDistance(float speed) { return std::isfinite(speed) ? 8.0f+0.32f*std::clamp(speed,0.0f,100.0f) : 8.0f; }
inline constexpr std::array<float,3> LanePreviewScales{1.0f,0.55f,1.65f};
struct Vector3 { float x{}, y{}, z{}; };
inline bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
struct LaneTarget {
    Vector3 position{};
    float appliedOffset{}, error{}, lookahead{};
    bool valid{}, edgeLimited{};
};
inline LaneTarget MakeNativeLaneTarget(Vector3 point, Vector3 tangent, Vector3 car) {
    LaneTarget result;
    if (!Finite(point) || !Finite(tangent) || !Finite(car)) return result;
    float length = std::hypot(tangent.x,tangent.z);
    float dx = point.x-car.x, dz = point.z-car.z;
    float distance = std::hypot(dx,dz);
    if (!std::isfinite(length) || length<0.01f || !std::isfinite(distance) ||
        distance<1.0f || distance>140.0f || std::abs(point.y-car.y)>12.0f) return result;
    result.position=point;
    result.lookahead=distance;
    result.error=(dx*tangent.z-dz*tangent.x)/length;
    result.valid=std::isfinite(result.error);
    return result;
}
inline LaneTarget MakeLaneTarget(Vector3 point, Vector3 tangent, Vector3 car, float offset,
                                 bool edgesAvailable, Vector3 edgeA, Vector3 edgeB) {
    LaneTarget result;
    if (!Finite(point) || !Finite(tangent) || !Finite(car) || !std::isfinite(offset)) return result;
    float length = std::hypot(tangent.x, tangent.z);
    if (!std::isfinite(length) || length < 0.01f) return result;
    float tx = tangent.x / length, tz = tangent.z / length;
    if (edgesAvailable) {
        if (!Finite(edgeA) || !Finite(edgeB)) return result;
        float width = (edgeB.x-edgeA.x)*tz - (edgeB.z-edgeA.z)*tx;
        if (!std::isfinite(width) || std::abs(width)<2.6f || std::abs(width)>80.0f) return result;
        point = {(edgeA.x+edgeB.x)*0.5f,(edgeA.y+edgeB.y)*0.5f,(edgeA.z+edgeB.z)*0.5f};
    }
    float dx = point.x-car.x, dz = point.z-car.z;
    float ahead = dx*tx + dz*tz;
    float lateral = dx*tz - dz*tx;
    if (!std::isfinite(ahead) || !std::isfinite(lateral) || ahead < 1.0f || ahead > 140.0f || std::abs(lateral)>22.0f || std::abs(point.y-car.y)>12.0f) return result;
    offset = std::clamp(offset,-3.2f,3.2f);
    if (edgesAvailable) {
        float a = (edgeA.x-point.x)*tz - (edgeA.z-point.z)*tx;
        float b = (edgeB.x-point.x)*tz - (edgeB.z-point.z)*tx;
        float low = std::min(a,b)+1.3f, high = std::max(a,b)-1.3f;
        if (!std::isfinite(low) || !std::isfinite(high) || low>high) return result;
        float limited = std::clamp(offset,low,high);
        result.edgeLimited = std::abs(limited-offset)>0.01f; offset = limited;
    }
    result.lookahead = ahead;
    result.position = {point.x+tz*offset,point.y,point.z-tx*offset};
    result.appliedOffset = offset; result.error = lateral+offset;
    result.valid = Finite(result.position) && std::isfinite(result.error); return result;
}
struct SteeringResult { float input{}, heading{}; bool valid{}; };
inline SteeringResult TrackLane(const LaneTarget& target, Vector3 car, Vector3 forward, float maxSteering) {
    SteeringResult result;
    if (!target.valid || !Finite(car) || !Finite(forward) || !std::isfinite(maxSteering) || maxSteering<0.02f || maxSteering>1.6f) return result;
    float f = std::hypot(forward.x,forward.z);
    float dx = target.position.x-car.x, dz = target.position.z-car.z;
    float d2 = dx*dx+dz*dz;
    if (!std::isfinite(d2) || f<0.01f || d2<1.0f || d2>40000.0f) return result;
    float fx = forward.x/f, fz = forward.z/f;
    float side = fz*dx-fx*dz, ahead = fx*dx+fz*dz;
    result.heading = std::atan2(side,ahead);
    if (!std::isfinite(result.heading) || ahead<=0 || std::abs(result.heading)>1.15f) return result;
    float wheelAngle = std::atan2(2.0f*2.7f*side,d2);
    result.input = std::clamp(wheelAngle/maxSteering,-0.65f,0.65f);
    result.valid = std::isfinite(result.input); return result;
}
struct LaneSteeringCommand {
    float input{}, limit{};
    bool valid{}, limited{};
};
inline LaneSteeringCommand BoundLaneSteering(float requested,const SteeringResult& geometric,float speed,float maximumRadians) {
    LaneSteeringCommand result;
    if (!geometric.valid || !std::isfinite(geometric.input) || !std::isfinite(requested) ||
        !std::isfinite(speed) || !std::isfinite(maximumRadians) || maximumRadians<0.02f || maximumRadians>1.6f) return result;
    float velocity=std::max(std::abs(speed),10.0f);
    float reserve=std::atan2(2.7f*6.0f,velocity*velocity)/maximumRadians;
    result.limit=std::clamp(1.5f*std::abs(geometric.input)+reserve,0.03f,0.65f);
    result.input=std::clamp(requested,-result.limit,result.limit);
    result.limited=std::abs(result.input-requested)>0.00001f;
    result.valid=true;
    return result;
}
struct LaneAssist {
    int lane{}, heldDirection{};
    float offset{}, steering{};
    unsigned changes{};
    void Reset() { *this = LaneAssist{}; }
    void Step(float input, float seconds, bool reverse) {
        if (!std::isfinite(input) || !std::isfinite(seconds) || seconds<=0 || seconds>0.25f) return;
        int direction = input>=0.45f ? 1 : input<=-0.45f ? -1 : 0;
        if (std::abs(input)<=0.20f) heldDirection = 0;
        if (direction && direction!=heldDirection) {
            int next = std::clamp(lane+direction*(reverse ? -1 : 1),-1,1);
            if (next!=lane) { lane=next; ++changes; }
            heldDirection=direction;
        }
        float amount = std::min(seconds,0.05f)*4.5f;
        offset += std::clamp(static_cast<float>(lane)*3.2f-offset,-amount,amount);
    }
    float SmoothSteering(float requested, float seconds) {
        if (!std::isfinite(requested) || !std::isfinite(seconds) || seconds<=0 || seconds>0.25f) return steering;
        float amount = std::min(seconds,0.05f)*2.0f;
        steering += std::clamp(requested-steering,-amount,amount);
        return steering;
    }
};
}
