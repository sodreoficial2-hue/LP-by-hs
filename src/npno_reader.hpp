#pragma once
// SPDX-License-Identifier: AGPL-3.0-or-later
// Source attribution and pinned revisions: third_party/npno/NOTICE.md.
#include "timbre_format.hpp"
#include <array>
namespace lp::npno {
using Bytes=std::vector<std::uint8_t>;
constexpr int sampleRate=35002;
struct Stroke {
 std::size_t index=0,recordOffset=0,audioOffset=0,frames=0,blocks=0;
 int root=0,bank=0,layer=0,threshold=0,velLow=1,velHigh=0,trim=0;
 std::array<std::array<std::int16_t,4>,2> history{};
 std::size_t loopStart=0,loopLength=0;
 std::uint32_t loopDecay=0;
};
struct VelocityBand {int root=0,index=0,low=1,high=127,attackStroke=-1,pedalStroke=-1,pedalClass=-1;bool inherited=false;};
struct KeyZone {int root=0,low=0,high=0,fineTune=0;bool undamped=false;};
struct Module {
 Bytes bytes;CbinHeader header;std::size_t base=0,end=0;
 int streamVersion=0,channels=0,gain=0,damperTop=0;
 bool checksumValid=false;
 std::string name;
 std::array<int,128> keyRoots{},fineTune{};
 std::vector<Stroke> strokes;
 std::vector<KeyZone> zones;
 std::vector<VelocityBand> velocityBands;
 std::vector<std::string> warnings;
 std::string metadataJson()const;
};
struct Audio {std::vector<std::vector<std::int16_t>> lanes;std::size_t overlapChecked=0;};
// Recognition requires both envelope/tag and the body signature at the framed offset.
bool sniff(const std::filesystem::path& path);
Module readModule(const std::filesystem::path& path);
Audio decodeStroke(const Module& module,const Stroke& stroke);
int topVelocity(int threshold);
}
