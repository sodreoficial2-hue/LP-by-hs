// SPDX-License-Identifier: AGPL-3.0-or-later
// Port: OpenNord ns4/nw1-dsp.ts + nsmp.ts loopDecayDbPerSecond.
#include "nsmp.hpp"
#include <cmath>
namespace lp::nsmp {
double dsp2Level(std::uint32_t raw){return raw?20*std::log10(raw/1048576.): -std::numeric_limits<double>::infinity();}
double dsp2Detune(std::int32_t raw){return std::trunc(raw*100./256+(raw<0?-.5:.5));}
double loopDecayDbPerSecond(std::uint32_t raw){auto signedRaw=raw>=0x800000?std::int32_t(raw)-0x1000000:std::int32_t(raw);double c=std::abs(double(signedRaw))/0x800000;return c>0&&c<1?-20*std::log10(c)*21700:0;}
}
