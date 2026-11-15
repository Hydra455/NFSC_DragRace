#include "../src/LaneAssist.h"
#include <cassert>
#include <limits>
static void SimulateLane(float speed, bool rotated) {
    drag::LaneAssist lane;
    float lateral=0, distance=0, heading=0;
    constexpr float dt=1.0f/120.0f;
    const float maximum=drag::SteeringRadians(0.125f);
    for (int step=0;step<1200;++step) {
        float ahead=10.0f+speed*0.5f;
        float request=step==0 ? 1.0f : step==600 ? -1.0f : 0.0f;
        lane.Step(request,dt,false);
        drag::Vector3 car=rotated ? drag::Vector3{distance,0,-lateral} : drag::Vector3{lateral,0,distance};
        drag::Vector3 forward=rotated ? drag::Vector3{std::cos(heading),0,-std::sin(heading)} : drag::Vector3{std::sin(heading),0,std::cos(heading)};
        drag::Vector3 point=rotated ? drag::Vector3{distance+ahead,0,-lateral} : drag::Vector3{lateral,0,distance+ahead};
        drag::Vector3 tangent=rotated ? drag::Vector3{1,0,0} : drag::Vector3{0,0,1};
        drag::Vector3 edgeA=rotated ? drag::Vector3{distance+ahead,0,7} : drag::Vector3{-7,0,distance+ahead};
        drag::Vector3 edgeB=rotated ? drag::Vector3{distance+ahead,0,-7} : drag::Vector3{7,0,distance+ahead};
        auto target=drag::MakeLaneTarget(point,tangent,car,lane.offset,true,edgeA,edgeB);
        auto control=drag::TrackLane(target,car,forward,maximum);
        assert(control.valid);
        float steering=lane.SmoothSteering(control.input,dt);
        heading += speed/2.7f*std::tan(steering*maximum)*dt;
        lateral += speed*std::sin(heading)*dt;
        distance += speed*std::cos(heading)*dt;
        assert(std::abs(lateral)<5.0f);
        if (step==590) assert(std::abs(lateral-3.2f)<0.25f);
    }
    assert(std::abs(lateral)<0.25f);
}
static void SimulateCurve(float speed) {
    constexpr float radius=250, dt=1.0f/120.0f;
    const float maximum=drag::SteeringRadians(0.125f);
    drag::Vector3 car{};
    float heading=0;
    drag::LaneAssist lane;
    for (int step=0;step<1800;++step) {
        lane.Step(step==0 ? 1.0f : 0.0f,dt,false);
        float phase=std::atan2(car.z,radius-car.x)+(10+speed*0.5f)/radius;
        drag::Vector3 point{radius-radius*std::cos(phase),0,radius*std::sin(phase)};
        drag::Vector3 tangent{std::sin(phase),0,std::cos(phase)};
        drag::Vector3 a{point.x-7*tangent.z,0,point.z+7*tangent.x};
        drag::Vector3 b{point.x+7*tangent.z,0,point.z-7*tangent.x};
        auto target=drag::MakeLaneTarget(point,tangent,car,lane.offset,true,a,b);
        auto control=drag::TrackLane(target,car,{std::sin(heading),0,std::cos(heading)},maximum);
        assert(control.valid);
        float steering=lane.SmoothSteering(control.input,dt);
        heading += speed/2.7f*std::tan(steering*maximum)*dt;
        car.x += speed*std::sin(heading)*dt; car.z += speed*std::cos(heading)*dt;
        if (step>1200) assert(std::abs(std::hypot(car.x-radius,car.z)-(radius-3.2f))<0.7f);
    }
}
static void NativeCurveTargets() {
    for (float sign : {-1.0f,1.0f}) {
        constexpr float radius=60.0f, angle=1.0f;
        drag::Vector3 point{sign*radius*(1-std::cos(angle)),0,radius*std::sin(angle)};
        drag::Vector3 tangent{sign*std::sin(angle),0,std::cos(angle)};
        auto old=drag::MakeLaneTarget(point,tangent,{},0,false,{},{});
        auto target=drag::MakeNativeLaneTarget(point,tangent,{});
        assert(!old.valid);
        assert(target.valid && std::abs(target.error)>22.0f);
        auto control=drag::TrackLane(target,{},{0,0,1},drag::SteeringRadians(0.125f));
        assert(control.valid && control.input*sign>0);
        auto rotated=drag::MakeNativeLaneTarget({point.z,0,-point.x},{tangent.z,0,-tangent.x},{});
        auto rotatedControl=drag::TrackLane(rotated,{},{1,0,0},drag::SteeringRadians(0.125f));
        assert(rotated.valid && rotatedControl.valid && std::abs(control.input-rotatedControl.input)<0.00001f);
    }
    assert(!drag::MakeNativeLaneTarget({0,0,141},{0,0,1},{}).valid);
    assert(!drag::MakeNativeLaneTarget({0,0,0.5f},{0,0,1},{}).valid);
    assert(!drag::MakeNativeLaneTarget({0,13,50},{0,0,1},{}).valid);
    assert(!drag::MakeNativeLaneTarget({0,0,50},{},{}).valid);
    assert(!drag::MakeNativeLaneTarget({std::numeric_limits<float>::quiet_NaN(),0,50},{0,0,1},{}).valid);
}
int main() {
    NativeCurveTargets();
    assert(std::abs(drag::SteeringRadians(0.125f)-0.7853981634f)<0.000001f);
    assert(std::abs(drag::SteeringRadians(0.25f)-1.5707963268f)<0.000001f);
    drag::LaneAssist lane;
    for (int i=0;i<120;++i) lane.Step(1.0f,1.0f/60,false);
    assert(lane.lane==1 && lane.changes==1 && std::abs(lane.offset-3.2f)<0.001f);
    lane.Step(0.0f,1.0f/60,false); lane.Step(1.0f,1.0f/60,false);
    assert(lane.lane==1 && lane.changes==1);
    lane.Step(-1.0f,1.0f/60,false);
    assert(lane.lane==0 && lane.changes==2);
    lane.Step(-0.3f,1.0f/60,false); lane.Step(-1.0f,1.0f/60,false);
    assert(lane.lane==0);
    lane.Step(0.0f,1.0f/60,false); lane.Step(-1.0f,1.0f/60,false);
    assert(lane.lane==-1);
    lane.Reset(); lane.Step(1,0.01f,true); assert(lane.lane==-1);
    lane.Reset(); lane.Step(1,5,false); lane.Step(std::numeric_limits<float>::quiet_NaN(),0.01f,false);
    assert(lane.lane==0 && lane.offset==0);
    auto a=drag::MakeLaneTarget({0,0,20},{0,0,1},{0,0,0},3.2f,false,{},{});
    auto b=drag::MakeLaneTarget({20,0,0},{1,0,0},{0,0,0},3.2f,false,{},{});
    assert(a.valid && b.valid && a.position.x==3.2f && b.position.z==-3.2f);
    auto bounded=drag::MakeLaneTarget({0,0,20},{0,0,1},{0,0,0},3.2f,true,{-3,0,20},{3,0,20});
    assert(bounded.valid && bounded.edgeLimited && std::abs(bounded.appliedOffset-1.7f)<0.001f);
    assert(!drag::MakeLaneTarget({0,0,-5},{0,0,1},{},0,false,{},{}).valid);
    assert(!drag::MakeLaneTarget({0,0,20},{},{},0,false,{},{}).valid);
    assert(!drag::MakeLaneTarget({0,0,20},{0,0,1},{},0,true,{0,0,20},{1,0,20}).valid);
    assert(!drag::MakeLaneTarget({0,100,20},{0,0,1},{},0,false,{},{}).valid);
    assert(!drag::MakeLaneTarget({std::numeric_limits<float>::infinity(),0,20},{0,0,1},{},0,false,{},{}).valid);
    for (float speed : {10.0f,30.0f,60.0f,90.0f}) {
        SimulateLane(speed,false); SimulateLane(speed,true);
    }
    SimulateCurve(30.0f); SimulateCurve(60.0f);
    auto target=drag::MakeLaneTarget({0,0,90},{0,0,1},{},3.2f,true,{-7,0,90},{7,0,90});
    auto right=drag::TrackLane(target,{},{0,0,1},0.6f);
    assert(target.valid && target.lookahead==90 && right.valid && right.input>0);
    auto reversed=drag::TrackLane(target,{},{0,0,-1},0.6f);
    assert(!reversed.valid);
    assert(!drag::TrackLane(target,{},{0,0,1},0).valid);
    assert(!drag::TrackLane(target,{},{},0.6f).valid);
    auto correct=drag::TrackLane(target,{},{0,0,1},drag::SteeringRadians(0.125f));
    auto oldUnits=drag::TrackLane(target,{},{0,0,1},0.125f);
    assert(correct.valid && oldUnits.valid && std::abs(oldUnits.input/correct.input-drag::RadiansPerTurn)<0.0001f);
}
