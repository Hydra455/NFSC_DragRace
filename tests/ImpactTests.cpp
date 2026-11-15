#include "../src/DragMechanics.h"
#include <cassert>
int main() {
    assert(drag::RivalGearRequest(2,6,8000,8000,1,0)==3);
    assert(drag::RivalGearRequest(6,6,8000,8000,1,0)==0);
    assert(drag::RivalGearRequest(4,6,1800,8000,1,0)==3);
    assert(drag::RivalGearRequest(1,6,8000,8000,1,0)==0);
    drag::Mechanics a,b;
    for (int i=0;i<30;++i) { a.Tick(0.1f,8100,8000,false,true); b.Tick(0.1f,7300,8000,false,true); }
    assert(a.blown && !b.blown);
    assert(drag::TotalsImpact(50,20));
    assert(!drag::TotalsImpact(44,0));
    assert(!drag::TotalsImpact(60,58));
    assert(!drag::TotalsImpact(60,65));
}
