#include "../src/LaneAssist.h"
#include "../src/NativeLane.h"
#include <cassert>
#include <limits>
static void StraightLane(float speed, bool rotated, bool spikes=false) {
    drag::LaneTable table;
    table.valid=true; table.count=4;
    table.lanes[0]={0,-6}; table.lanes[1]={1,-2}; table.lanes[2]={2,2}; table.lanes[3]={3,6};
    drag::NativeLaneState selection;
    drag::LaneAssist actuator;
    float x=-2, z=0, heading=0;
    constexpr float dt=1.0f/120.0f;
    float maximum=drag::SteeringRadians(0.125f);
    for (int i=0;i<1440;++i) {
        int request=i==120 ? 1 : i==720 ? -1 : 0;
        assert(selection.Update(table,x,1,request,dt));
        float preview=drag::LanePreviewDistance(speed);
        drag::Vector3 car=rotated ? drag::Vector3{z,0,-x} : drag::Vector3{x,0,z};
        drag::Vector3 point=rotated ? drag::Vector3{z+preview,0,-selection.current} : drag::Vector3{selection.current,0,z+preview};
        drag::Vector3 tangent=rotated ? drag::Vector3{1,0,0} : drag::Vector3{0,0,1};
        drag::Vector3 forward=rotated ? drag::Vector3{std::cos(heading),0,-std::sin(heading)} : drag::Vector3{std::sin(heading),0,std::cos(heading)};
        auto target=drag::MakeNativeLaneTarget(point,tangent,car);
        auto result=drag::TrackLane(target,car,forward,maximum);
        assert(result.valid);
        float requested=spikes && i%73==0 ? (i%146==0 ? 1.0f : -1.0f) : result.input;
        auto bounded=drag::BoundLaneSteering(requested,result,speed,maximum);
        assert(bounded.valid);
        actuator.steering=std::clamp(actuator.SmoothSteering(bounded.input,dt),-bounded.limit,bounded.limit);
        float input=actuator.steering;
        heading+=speed/2.7f*std::tan(input*maximum)*dt;
        x+=speed*std::sin(heading)*dt;
        z+=speed*std::cos(heading)*dt;
        assert(std::abs(x)<6);
        if (i==700) assert(std::abs(x-2)<0.25f);
    }
    assert(std::abs(x+2)<0.25f);
}
static void CurvedLane(float speed, float sign) {
    constexpr float radius=80,dt=1.0f/120;
    drag::Vector3 car{};
    float heading=0, offset=0;
    drag::LaneAssist actuator;
    for (int i=0;i<1800;++i) {
        float goal=i<900 ? 3.2f : -3.2f;
        offset+=std::clamp(goal-offset,-3.0f*dt,3.0f*dt);
        float currentPhase=std::atan2(car.z,radius-sign*car.x);
        float phase=currentPhase+drag::LanePreviewDistance(speed)/radius;
        drag::Vector3 centre{sign*radius*(1-std::cos(phase)),0,radius*std::sin(phase)};
        drag::Vector3 tangent{sign*std::sin(phase),0,std::cos(phase)};
        drag::Vector3 point{centre.x+tangent.z*offset,0,centre.z-tangent.x*offset};
        auto target=drag::MakeNativeLaneTarget(point,tangent,car);
        auto result=drag::TrackLane(target,car,{std::sin(heading),0,std::cos(heading)},drag::SteeringRadians(0.125f));
        assert(result.valid);
        float input=actuator.SmoothSteering(result.input,dt);
        heading+=speed/2.7f*std::tan(input*drag::SteeringRadians(0.125f))*dt;
        car.x+=speed*std::sin(heading)*dt; car.z+=speed*std::cos(heading)*dt;
        float actual=sign*(radius-std::hypot(car.x-sign*radius,car.z));
        if (i==850 || i==1799) assert(std::abs(actual-goal)<0.8f);
    }
}
static void ConsecutiveLaneChanges(float speed) {
    drag::LaneTable table;
    table.valid=true; table.count=4;
    for (int i=0;i<4;++i) table.lanes[i]={i,-6.0f+4.0f*i};
    drag::NativeLaneState lane;
    drag::LaneAssist actuator;
    float x=-2,z=0,heading=0;
    constexpr float dt=1.0f/120;
    float maximum=drag::SteeringRadians(0.125f);
    for (int i=0;i<1600;++i) {
        int tap=i==120 || i==160 ? 1 : i==200 || i==240 ? -1 : 0;
        assert(lane.Update(table,x,1,tap,dt));
        auto target=drag::MakeNativeLaneTarget({lane.current,0,z+drag::LanePreviewDistance(speed)},{0,0,1},{x,0,z});
        auto control=drag::TrackLane(target,{x,0,z},{std::sin(heading),0,std::cos(heading)},maximum);
        auto bounded=drag::BoundLaneSteering(control.input,control,speed,maximum);
        assert(bounded.valid);
        actuator.steering=std::clamp(actuator.SmoothSteering(bounded.input,dt),-bounded.limit,bounded.limit);
        heading+=speed/2.7f*std::tan(actuator.steering*maximum)*dt;
        x+=speed*std::sin(heading)*dt; z+=speed*std::cos(heading)*dt;
        assert(std::abs(x)<7);
    }
    assert(lane.rank==1 && lane.changes==4 && std::abs(x+2)<0.25f);
}
int main() {
    assert(drag::LanePreviewDistance(0)==8);
    assert(drag::LanePreviewDistance(50)==24);
    assert(drag::LanePreviewDistance(200)==40);
    assert(drag::LanePreviewDistance(std::numeric_limits<float>::quiet_NaN())==8);
    for (float speed : {10.0f,30.0f,60.0f,90.0f}) {
        StraightLane(speed,false); StraightLane(speed,true); StraightLane(speed,false,true);
    }
    for (float speed : {20.0f,40.0f,60.0f}) { CurvedLane(speed,1); CurvedLane(speed,-1); ConsecutiveLaneChanges(speed); }
    auto close=drag::MakeNativeLaneTarget({4,0,18},{0,0,1},{});
    auto distant=drag::MakeNativeLaneTarget({4,0,55},{0,0,1},{});
    auto closeResult=drag::TrackLane(close,{},{0,0,1},drag::SteeringRadians(0.125f));
    auto distantResult=drag::TrackLane(distant,{},{0,0,1},drag::SteeringRadians(0.125f));
    assert(closeResult.valid && distantResult.valid && closeResult.input>5*distantResult.input);
}
