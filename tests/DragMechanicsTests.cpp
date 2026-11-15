#include "../src/DragMechanics.h"
#include "../src/RaceGate.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using drag::ShiftGrade;
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("Drag mechanics regression"); };
    require(drag::Mechanics::Grade(4500,6000) == ShiftGrade::Early);
    require(drag::Mechanics::Grade(5500,6000) == ShiftGrade::Good);
    require(drag::Mechanics::Grade(5900,6000) == ShiftGrade::Perfect);
    require(drag::Mechanics::Grade(6300,6000) == ShiftGrade::Late);
    require(drag::Mechanics::Grade(0,6000) == ShiftGrade::None);
    require(drag::Mechanics::Grade(5000,0) == ShiftGrade::None);
    require(drag::Mechanics::Grade(std::numeric_limits<float>::quiet_NaN(),6000) == ShiftGrade::None);
    drag::Mechanics m;
    m.Shift(5900,6000,true); require(m.shifts==1 && m.perfect==1 && m.Multiplier(true)>1.079f && m.Multiplier(true)<1.081f);
    m.Tick(0.1f,5000,6000,false,false); require(m.Multiplier(true)<1.08f && m.Multiplier(true)>1.0f);
    for (int i=0;i<9;++i) m.Tick(0.1f,5000,6000,false,false);
    require(m.Multiplier(true)==1.0f);
    m.Shift(5500,6000,false); require(m.Multiplier(true)==1.0f && m.shifts==2);
    for (int i=0;i<30;++i) require(!m.Tick(0.1f,7000,6000,true,true));
    require(m.heat==0 && !m.blown);
    for (int i=0;i<30;++i) require(!m.Tick(0.1f,7000,6000,false,false));
    require(m.heat==0 && !m.blown);
    unsigned trips=0;
    for (int i=0;i<40;++i) if(m.Tick(0.1f,6200,6000,false,true)) ++trips;
    require(trips==1 && m.blown && m.heat==1.0f && m.Multiplier(true)==1.0f);
    m.Reset(); for(int i=0;i<10;++i)m.Tick(0.1f,6200,6000,false,true);
    require(m.heat>0.49f && m.heat<0.51f);
    for(int i=0;i<10;++i)m.Tick(0.1f,4000,6000,false,true);
    require(m.heat==0 && !m.blown);
    require(!m.Tick(std::numeric_limits<float>::quiet_NaN(),7000,6000,false,true));
    require(!m.Tick(-1,7000,6000,false,true));
    require(!m.Tick(5,7000,6000,false,true) && m.heat<0.051f);
    for(bool player:{false,true}) for(bool racing:{false,true}) for(bool active:{false,true}) {
        require(drag::PhysicsScope(active,racing,player,123,123)==(active&&racing&&player));
    }
    require(drag::PhysicsScope(true,true,true,123,123));
    require(!drag::PhysicsScope(true,true,true,456,123));
    require(!drag::PhysicsScope(true,true,true,0,0));
    std::cout << "Shift grades, bonus decay, disabled options, staging, heat/cooling, one-shot damage, invalid inputs, and player/race scope: PASS\n";
}
