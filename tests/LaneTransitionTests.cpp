#include "../src/NativeLane.h"
#include <cassert>
int main() {
    std::array<std::uint8_t,64> p{};
    p[0]=5; p[1]=2;
    int offsets[]={6,2,0,2,6};
    for (unsigned barrier : {5u,7u,13u}) {
        for (int i=0;i<5;++i) {
            unsigned packed=(unsigned(offsets[i])<<18)|(i==2 ? barrier : 1u);
            std::memcpy(p.data()+4+i*4,&packed,4);
        }
        auto table=drag::DecodeDragRecords(p.data(),1,false,true);
        drag::NativeLaneState lane;
        assert(lane.Update(table,-6,1,1,0.01f) && lane.current==-2);
        assert(lane.tapResponseRemaining==0.6f);
        assert(lane.Update(table,-6,1,1,0.01f));
        assert(lane.current==(barrier==5 ? 2 : -2));
        assert(barrier==5 ? lane.tapResponseRemaining==0.6f : lane.tapResponseRemaining<0.6f);
        assert(lane.Update(table,-6,1,-1,0.01f));
        assert(lane.current==(barrier==5 ? -2 : -6));
        for (int i=0;i<62;++i) assert(lane.Update(table,-6,1,0,0.01f));
        assert(lane.tapResponseRemaining==0);
    }
}
