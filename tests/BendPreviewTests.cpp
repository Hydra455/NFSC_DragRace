#include "../src/LaneAssist.h"
#include <cassert>
static drag::SteeringResult Curve(float radius,float arc,float sign) {
    float phase=arc/radius;
    drag::Vector3 point{sign*radius*(1-std::cos(phase)),0,radius*std::sin(phase)};
    drag::Vector3 tangent{sign*std::sin(phase),0,std::cos(phase)};
    auto target=drag::MakeNativeLaneTarget(point,tangent,{});
    return drag::TrackLane(target,{},{0,0,1},drag::SteeringRadians(0.125f));
}
int main() {
    for (float sign : {-1.0f,1.0f}) {
        auto normal=Curve(8,20,sign);
        auto shorter=Curve(8,20*drag::LanePreviewScales[1],sign);
        assert(!normal.valid && shorter.valid && shorter.input*sign>0);
    }
    auto offRoad=[&](float scale) {
        auto target=drag::MakeNativeLaneTarget({-2,0,10*scale},{0,0,1},{25,0,0});
        return drag::TrackLane(target,{25,0,0},{0,0,1},drag::SteeringRadians(0.125f));
    };
    assert(!offRoad(drag::LanePreviewScales[0]).valid);
    assert(!offRoad(drag::LanePreviewScales[1]).valid);
    assert(offRoad(drag::LanePreviewScales[2]).valid);
    for (float scale : drag::LanePreviewScales) {
        auto target=drag::MakeNativeLaneTarget({0,0,-10*scale},{0,0,-1},{});
        assert(!drag::TrackLane(target,{},{0,0,1},drag::SteeringRadians(0.125f)).valid);
    }
    auto tableCheck=[](float speed) {
        for (float scale : drag::LanePreviewScales) {
            float distance=std::clamp(drag::LanePreviewDistance(speed)*scale,5.0f,42.0f);
            assert(distance>=5 && distance<=42);
        }
    };
    tableCheck(0); tableCheck(30); tableCheck(90);
}
