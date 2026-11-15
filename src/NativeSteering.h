#pragma once
#include "LaneAssist.h"
#include <cstdint>

namespace drag {
struct HumanSteeringSnapshot {
    std::uint8_t* human;
    unsigned flags;
    float speed, input, debugInput;
    Vector3 target;
    explicit HumanSteeringSnapshot(std::uint8_t* object) : human(object),
        flags(*reinterpret_cast<unsigned*>(object+0xF4)), speed(*reinterpret_cast<float*>(object+0x94)),
        input(*reinterpret_cast<float*>(object+0x108)), debugInput(*reinterpret_cast<float*>(object+0x1D8)),
        target(*reinterpret_cast<Vector3*>(object+0x98)) {}
    void Configure(Vector3 point) const {
        *reinterpret_cast<unsigned*>(human+0xF4)=1;
        *reinterpret_cast<float*>(human+0x94)=80.0f;
        *reinterpret_cast<Vector3*>(human+0x98)=point;
    }
    ~HumanSteeringSnapshot() {
        *reinterpret_cast<unsigned*>(human+0xF4)=flags;
        *reinterpret_cast<float*>(human+0x94)=speed;
        *reinterpret_cast<float*>(human+0x108)=input;
        *reinterpret_cast<float*>(human+0x1D8)=debugInput;
        *reinterpret_cast<Vector3*>(human+0x98)=target;
    }
    HumanSteeringSnapshot(const HumanSteeringSnapshot&)=delete;
    HumanSteeringSnapshot& operator=(const HumanSteeringSnapshot&)=delete;
};
}
