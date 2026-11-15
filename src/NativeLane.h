#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <cstring>

namespace drag {
struct NativeNavSnapshot {
    std::uint8_t* nav;
    int mode;
    float offset;
    std::array<std::uint8_t,32> control{};
    explicit NativeNavSnapshot(std::uint8_t* value) : nav(value), mode(*reinterpret_cast<int*>(value+0x74)), offset(*reinterpret_cast<float*>(value+0x2E4)) {
        std::memcpy(control.data(),nav+0x2E0,control.size());
    }
    template<class Rebuild> void Restore(Rebuild rebuild) const {
        *reinterpret_cast<int*>(nav+0x74)=mode;
        std::memcpy(nav+0x2E0,control.data(),control.size());
        *reinterpret_cast<float*>(nav+0x2E4)=offset+1.0f;
        rebuild(nav,offset);
        std::memcpy(nav+0x2E0,control.data(),control.size());
    }
};
inline bool NativeOffsetMatches(float requested,float sampled) {
    return std::isfinite(requested) && std::isfinite(sampled) && std::abs(requested-sampled)<=std::max(0.35f,std::abs(requested)*0.25f);
}
constexpr unsigned TrafficLaneMask=0x2;
constexpr unsigned ExtendedDrivingMask=0x402;
enum class LaneDecodeError { None, Missing, Header, Scale, Offset, NoDriving };
struct LaneProfileInfo {
    int count{}, split{};
    unsigned types{};
    bool present{};
};
inline LaneProfileInfo InspectLaneProfile(const std::uint8_t* profile) {
    LaneProfileInfo result;
    if (!profile) return result;
    result.present=true; result.count=profile[0]; result.split=profile[1];
    if (result.count<1 || result.count>15) return result;
    for (int i=0;i<result.count;++i) {
        std::uint32_t packed{}; std::memcpy(&packed,profile+4+i*4,4);
        result.types|=1u<<(packed&15);
    }
    return result;
}
struct LaneCentre { int record{-1}; float offset{}; };
struct LaneTable {
    unsigned blockedRecords{};
    std::array<LaneCentre,15> lanes{};
    int count{};
    bool valid{};
    LaneDecodeError error{LaneDecodeError::Missing};
};
inline LaneTable DecodeLaneTable(const std::uint8_t* profile,float scale,bool reversed,bool positiveRight,unsigned mask=TrafficLaneMask) {
    LaneTable result;
    if (!profile) return result;
    if (profile[0]<1 || profile[0]>15 || profile[1]>profile[0]) { result.error=LaneDecodeError::Header; return result; }
    if (!std::isfinite(scale) || scale<=0 || scale>1) { result.error=LaneDecodeError::Scale; return result; }
    for (int record=0;record<profile[0];++record) {
        std::uint32_t packed{}; std::memcpy(&packed,profile+4+record*4,4);
        unsigned typeBit=1u<<(packed&15);
        if ((0xFFFFDF7Fu&typeBit)==0) result.blockedRecords|=1u<<record;
        if ((mask&typeBit)==0) continue;
        int value=static_cast<int>(packed>>18);
        if (value&0x2000) value-=0x4000;
        float offset=static_cast<float>(value)*scale;
        if (record<profile[1]) offset=-offset;
        if (reversed) offset=-offset;
        if (!std::isfinite(offset) || std::abs(offset)>200) { result.count=0; result.error=LaneDecodeError::Offset; return result; }
        bool duplicate=false;
        for (int i=0;i<result.count;++i) if (std::abs(result.lanes[i].offset-offset)<0.05f) duplicate=true;
        if (!duplicate) result.lanes[result.count++]={record,offset};
    }
    std::sort(result.lanes.begin(),result.lanes.begin()+result.count,[&](LaneCentre a,LaneCentre b) {
        return positiveRight ? a.offset<b.offset : a.offset>b.offset;
    });
    result.valid=result.count>0;
    result.error=result.valid ? LaneDecodeError::None : LaneDecodeError::NoDriving;
    return result;
}
enum class LaneTableSource { LocalTraffic, LocalExtended, OtherTraffic, OtherExtended, Unavailable };
struct ResolvedLaneTable {
    LaneTable table;
    LaneTableSource source{LaneTableSource::Unavailable};
    LaneDecodeError localError{LaneDecodeError::Missing}, otherError{LaneDecodeError::Missing};
    LaneProfileInfo local{}, other{};
};
inline ResolvedLaneTable ResolveLaneTable(const std::uint8_t* local,const std::uint8_t* other,float scale,bool reversed,bool positiveRight) {
    ResolvedLaneTable result;
    result.local=InspectLaneProfile(local); result.other=InspectLaneProfile(other);
    auto localTraffic=DecodeLaneTable(local,scale,reversed,positiveRight);
    auto otherTraffic=DecodeLaneTable(other,scale,reversed,positiveRight);
    result.localError=localTraffic.error; result.otherError=otherTraffic.error;
    if (localTraffic.valid) { result.table=localTraffic; result.source=LaneTableSource::LocalTraffic; return result; }
    auto localExtended=DecodeLaneTable(local,scale,reversed,positiveRight,ExtendedDrivingMask);
    if (localExtended.valid) { result.table=localExtended; result.source=LaneTableSource::LocalExtended; return result; }
    if (otherTraffic.valid) { result.table=otherTraffic; result.source=LaneTableSource::OtherTraffic; return result; }
    auto otherExtended=DecodeLaneTable(other,scale,reversed,positiveRight,ExtendedDrivingMask);
    if (otherExtended.valid) { result.table=otherExtended; result.source=LaneTableSource::OtherExtended; return result; }
    result.table=localExtended;
    return result;
}
inline LaneTable DecodeDragRecords(const std::uint8_t* profile,float scale,bool reversed,bool positiveRight) {
    LaneTable table;
    if (!profile) return table;
    int count=profile[0];
    if (count<1 || count>15 || profile[1]>count) { table.error=LaneDecodeError::Header; return table; }
    if (!std::isfinite(scale) || scale<=0 || scale>1) { table.error=LaneDecodeError::Scale; return table; }
    bool ascending=positiveRight!=reversed;
    for (int step=0;step<count;++step) {
        int record=ascending ? step : count-1-step;
        std::uint32_t packed{};
        std::memcpy(&packed,profile+4+record*4,4);
        unsigned bit=1u<<(packed&15);
        if ((0xFFFFDF7Fu&bit)==0) table.blockedRecords|=1u<<record;
        if ((0xFFFFDF5Bu&bit)==0) continue;
        int value=static_cast<int>(packed>>18);
        if (value&0x2000) value-=0x4000;
        float offset=value*scale;
        if ((record<profile[1])!=reversed) offset=-offset;
        if (std::abs(offset)>200) { table.count=0; table.error=LaneDecodeError::Offset; return table; }
        table.lanes[table.count++]={record,offset};
    }
    table.valid=table.count>0;
    table.error=table.valid ? LaneDecodeError::None : LaneDecodeError::NoDriving;
    return table;
}
struct LaneRequests {
    int held{};
    int Poll(float input,bool reverse) {
        if (!std::isfinite(input)) return 0;
        if (std::abs(input)<=0.20f) held=0;
        int direction=input>=0.45f ? 1 : input<=-0.45f ? -1 : 0;
        if (!direction || direction==held) return 0;
        held=direction;
        return direction*(reverse ? -1 : 1);
    }
};
struct NativeLaneState {
    int index{-1}, rank{-1}, count{}, pending{}, axisSign{}, fromRank{-1};
    float fromOffset{};
    float current{}, target{}, tapResponseRemaining{};
    unsigned changes{}, rejected{};
    bool ready{};
    bool Update(const LaneTable& table,float initial,int sign,int request,float seconds) {
        if (!table.valid || table.count<1 || table.count>15 || !std::isfinite(initial) || (sign!=1 && sign!=-1) ||
            !std::isfinite(seconds) || seconds<=0 || seconds>0.25f) return false;
        float reference=ready ? current : initial;
        if (ready && axisSign!=sign) reference=-reference;
        rank=0;
        for (int i=1;i<table.count;++i) {
            if (std::abs(table.lanes[i].offset-reference)<=std::abs(table.lanes[rank].offset-reference)) rank=i;
        }
        ready=true; axisSign=sign; count=table.count;
        fromRank=rank; fromOffset=table.lanes[rank].offset;
        tapResponseRemaining=std::max(0.0f,tapResponseRemaining-seconds);
        if (request) {
            int next=std::clamp(rank+(request>0 ? 1 : -1),0,count-1);
            int first=std::min(table.lanes[rank].record,table.lanes[next].record);
            int last=std::max(table.lanes[rank].record,table.lanes[next].record);
            bool blocked=next==rank;
            for (int i=first+1;i<last;++i) {
                if (table.blockedRecords&(1u<<i)) blocked=true;
            }
            if (blocked) ++rejected;
            else { rank=next; ++changes; tapResponseRemaining=0.6f; }
        }
        index=table.lanes[rank].record;
        current=target=table.lanes[rank].offset;
        return true;
    }
};
}
