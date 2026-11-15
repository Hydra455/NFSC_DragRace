#include "../src/LaneAssist.h"
#include <cassert>
#include <limits>
int main() {
    constexpr float maximum=drag::RadiansPerTurn*0.125f;
    for (float speed : {0.0f,10.0f,30.0f,60.0f,90.0f}) {
        for (float geometric : {-0.65f,-0.2f,-0.01f,0.0f,0.01f,0.2f,0.65f}) {
            drag::SteeringResult path{geometric,0,true};
            auto unchanged=drag::BoundLaneSteering(geometric,path,speed,maximum);
            assert(unchanged.valid && !unchanged.limited && unchanged.input==geometric);
            assert(unchanged.limit>=std::abs(geometric) && unchanged.limit<=0.65f);
            for (float request : {-1.0f,1.0f}) {
                auto bounded=drag::BoundLaneSteering(request,path,speed,maximum);
                assert(bounded.valid && std::abs(bounded.input)<=bounded.limit && bounded.input*request>0);
                assert(bounded.limited);
            }
        }
    }
    drag::SteeringResult straight{0.01f,0,true};
    auto slow=drag::BoundLaneSteering(1,straight,20,maximum);
    auto fast=drag::BoundLaneSteering(1,straight,80,maximum);
    assert(fast.limit<slow.limit && fast.limit<0.05f);
    assert(!drag::BoundLaneSteering(1,{},40,maximum).valid);
    assert(!drag::BoundLaneSteering(1,straight,40,0).valid);
    assert(!drag::BoundLaneSteering(std::numeric_limits<float>::quiet_NaN(),straight,40,maximum).valid);
    drag::LaneAssist actuator;
    actuator.steering=0.65f;
    actuator.steering=std::clamp(actuator.SmoothSteering(fast.input,1.0f/73),-fast.limit,fast.limit);
    assert(actuator.steering<=fast.limit);
}
