#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
namespace lp {
// Attack/Decay/Release seconds and Sustain percent, sent to sfizz per region.
inline std::array<float,4> envelopeValues(const std::array<float,16>& p) {
 return {8*p[1]*p[1],8*p[2]*p[2],100*p[3],.005f+11.995f*p[4]*p[4]};
}
}
