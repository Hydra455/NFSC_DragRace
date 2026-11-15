#include "../src/RaceGate.h"
#include <cassert>
#include <limits>
int main() {
    for (int type=-1;type<20;++type) {
        assert(drag::IsAuthoredDrag(type,42)==(type==2));
        assert(!drag::IsAuthoredDrag(type,0));
    }
    for (int flags=0;flags<8;++flags)
        assert(drag::PhysicsScope(flags&1,flags&2,flags&4,42,42)==(flags==7));
    for (int flags=0;flags<8;++flags)
        assert(drag::StandaloneDragFlags(flags&1,flags&2,flags&4)==(flags==0));
    assert(drag::IsAuthoredDrag(2,0xDEADBEEF));
    assert(!drag::PhysicsScope(true,true,true,0,0));
    assert(!drag::PhysicsScope(true,true,true,42,43));

    drag::QuickListDragEntries allAreas;
    allAreas.RecordCareer(0x102,2);
    allAreas.RecordCareer(0x105,5);
    allAreas.RecordCareer(0x106,-1);
    allAreas.RecordCareer(0x107,0);
    allAreas.RecordCareer(0,1);
    assert(allAreas.HasCareerEntry(0x102));
    assert(allAreas.HasCareerEntry(0x105));
    assert(!allAreas.HasCareerEntry(0x106));
    assert(allAreas.HasCareerEntry(0x107));
    assert(!allAreas.HasCareerEntry(0));

    drag::QuickListDragEntries filtered;
    filtered.areaFilter=2;
    filtered.RecordCareer(0x102,2);
    filtered.RecordCareer(0x105,5);
    assert(filtered.HasCareerEntry(0x102));
    assert(!filtered.HasCareerEntry(0x105));
    assert(!drag::QuickListDragEntries{}.HasCareerEntry(0x102));
    assert(drag::ValidMapPosition(-472.53f,3477.25f));
    assert(drag::ValidMapPosition(0.0f,3477.25f));
    assert(!drag::ValidMapPosition(0.0f,0.0f));
    assert(!drag::ValidMapPosition(std::numeric_limits<float>::infinity(),1.0f));
    assert(!drag::ValidMapPosition(1.0f,std::numeric_limits<float>::quiet_NaN()));
    assert(drag::ValidRegion(0));
    assert(drag::ValidRegion(5));
    assert(!drag::ValidRegion(6));
    assert(!drag::ValidRegion(-1));
    assert(!drag::ValidRegion(255));
    assert(drag::IsDragCareerUnlockCaller(0x24B6B1));
    assert(drag::IsDragCareerUnlockCaller(0x24C9EE));
    assert(!drag::IsDragCareerUnlockCaller(0x247EA0));
}
