#include "../src/NativeLane.h"
#include <cassert>
#include <limits>
static void Record(std::array<std::uint8_t,64>& p,int i,int metres,unsigned type) {
    std::uint32_t v=((static_cast<std::uint32_t>(metres*64)&0x3FFF)<<18)|type;
    std::memcpy(p.data()+4+i*4,&v,4);
}
static auto Profile(unsigned type) {
    std::array<std::uint8_t,64> p{};
    p[0]=6; p[1]=3;
    int offsets[]={10,6,2,2,6,10};
    for (int i=0;i<6;++i) Record(p,i,offsets[i],i==0 || i==5 ? 2 : type);
    return p;
}
int main() {
    auto traffic=Profile(1), extended=Profile(10), shoulder=Profile(2), unknown=Profile(13);
    constexpr float scale=1.0f/64;
    auto original=drag::DecodeLaneTable(traffic.data(),scale,false,true);
    auto primary=drag::ResolveLaneTable(traffic.data(),extended.data(),scale,false,true);
    assert(primary.source==drag::LaneTableSource::LocalTraffic && primary.table.valid && primary.table.count==original.count);
    for (int i=0;i<original.count;++i) assert(primary.table.lanes[i].record==original.lanes[i].record && primary.table.lanes[i].offset==original.lanes[i].offset);
    auto oldRejected=drag::DecodeLaneTable(extended.data(),scale,false,true);
    assert(!oldRejected.valid && oldRejected.error==drag::LaneDecodeError::NoDriving);
    auto recovered=drag::ResolveLaneTable(extended.data(),traffic.data(),scale,false,true);
    assert(recovered.source==drag::LaneTableSource::LocalExtended && recovered.table.count==4);
    assert(recovered.local.types==((1u<<2)|(1u<<10)));
    for (int i=0;i<4;++i) assert(recovered.table.lanes[i].offset==original.lanes[i].offset);
    auto other=drag::ResolveLaneTable(shoulder.data(),traffic.data(),scale,false,true);
    assert(other.source==drag::LaneTableSource::OtherTraffic && other.table.count==4);
    auto otherExtended=drag::ResolveLaneTable(unknown.data(),extended.data(),scale,true,false);
    assert(otherExtended.source==drag::LaneTableSource::OtherExtended && otherExtended.table.count==4);
    for (int i=0;i<4;++i) assert(otherExtended.table.lanes[i].offset==-original.lanes[i].offset);
    auto rejected=drag::ResolveLaneTable(shoulder.data(),unknown.data(),scale,false,true);
    assert(!rejected.table.valid && rejected.source==drag::LaneTableSource::Unavailable);
    assert(rejected.localError==drag::LaneDecodeError::NoDriving && rejected.otherError==drag::LaneDecodeError::NoDriving);
    auto badHeader=traffic; badHeader[1]=7;
    auto fallback=drag::ResolveLaneTable(badHeader.data(),traffic.data(),scale,false,true);
    assert(fallback.table.valid && fallback.source==drag::LaneTableSource::OtherTraffic && fallback.localError==drag::LaneDecodeError::Header);
    auto badScale=drag::ResolveLaneTable(traffic.data(),extended.data(),std::numeric_limits<float>::quiet_NaN(),false,true);
    assert(!badScale.table.valid && badScale.localError==drag::LaneDecodeError::Scale);
    assert(!drag::ResolveLaneTable(nullptr,nullptr,scale,false,true).table.valid);
    auto oversized=traffic; oversized[0]=255;
    auto inspected=drag::InspectLaneProfile(oversized.data());
    assert(inspected.count==255 && inspected.types==0);
    auto mixed=traffic; Record(mixed,5,14,10);
    auto preferred=drag::ResolveLaneTable(mixed.data(),nullptr,scale,false,true);
    assert(preferred.source==drag::LaneTableSource::LocalTraffic && preferred.table.count==4);
    drag::NativeLaneState selection;
    assert(selection.Update(primary.table,-2,1,0,0.01f));
    assert(selection.Update(recovered.table,-2,1,1,0.01f));
    assert(selection.rank==2 && selection.target==2 && selection.current==2);
}
