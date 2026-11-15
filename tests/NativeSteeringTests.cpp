#include "../src/NativeSteering.h"
#include <array>
#include <cassert>
#include <stdexcept>
int main() {
    alignas(16) std::array<std::uint8_t,0x300> human{};
    auto object=human.data();
    *reinterpret_cast<unsigned*>(object+0xF4)=0;
    *reinterpret_cast<float*>(object+0x94)=13;
    *reinterpret_cast<float*>(object+0x108)=-1;
    *reinterpret_cast<float*>(object+0x1D8)=0.15f;
    *reinterpret_cast<drag::Vector3*>(object+0x98)={11,22,33};
    *reinterpret_cast<float*>(object+0x10C)=0.33f;
    *reinterpret_cast<float*>(object+0x110)=0.66f;
    {
        drag::HumanSteeringSnapshot restore(object);
        restore.Configure({100,20,300});
        assert(*reinterpret_cast<unsigned*>(object+0xF4)==1);
        assert(*reinterpret_cast<float*>(object+0x94)>0);
        auto point=*reinterpret_cast<drag::Vector3*>(object+0x98);
        assert(point.x==100 && point.y==20 && point.z==300);
        *reinterpret_cast<float*>(object+0x108)=0.28f;
        *reinterpret_cast<float*>(object+0x1D8)=0.28f;
        *reinterpret_cast<float*>(object+0x140)=42;
    }
    assert(*reinterpret_cast<unsigned*>(object+0xF4)==0);
    assert(*reinterpret_cast<float*>(object+0x94)==13);
    assert(*reinterpret_cast<float*>(object+0x108)==-1);
    assert(*reinterpret_cast<float*>(object+0x1D8)==0.15f);
    auto point=*reinterpret_cast<drag::Vector3*>(object+0x98);
    assert(point.x==11 && point.y==22 && point.z==33);
    assert(*reinterpret_cast<float*>(object+0x10C)==0.33f);
    assert(*reinterpret_cast<float*>(object+0x110)==0.66f);
    assert(*reinterpret_cast<float*>(object+0x140)==42);
    try {
        drag::HumanSteeringSnapshot restore(object);
        restore.Configure({-100,0,-300});
        *reinterpret_cast<float*>(object+0x108)=0;
        throw std::runtime_error("fixture");
    } catch (const std::runtime_error&) {}
    assert(*reinterpret_cast<float*>(object+0x108)==-1);
    assert(*reinterpret_cast<unsigned*>(object+0xF4)==0);
    point=*reinterpret_cast<drag::Vector3*>(object+0x98);
    assert(point.x==11 && point.z==33);
}
