#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cctype>
#include <functional>
namespace lp {
enum class TimbreFormat { SFZ,SF2,NSMP,NKI,NPNO,UNKNOWN };
struct CbinHeader {
 std::uint32_t formatType=0,category=0,version=0,crc32=0;
 std::uint16_t bank=0,slot=0;
 std::string tag;
 std::uintmax_t recordOffset=0,recordLength=0;
};
inline std::string timbreExtension(const std::filesystem::path& path){auto e=path.extension().u8string();std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return e;}
inline bool convertedTimbre(const std::filesystem::path& path){auto e=timbreExtension(path);return e==".nsmp"||e==".nsmp3"||e==".nsmp4"||e==".nki"||e==".npno"||e==".npmo";}
// Append, rather than replace, to keep different originals with the same stem distinct.
inline std::filesystem::path derivedSfz(const std::filesystem::path& path){auto out=path;out+=".converted.sfz";return out;}
CbinHeader lerHeaderCbin(const std::filesystem::path& path);
TimbreFormat detectarVersaoNsmp(const std::filesystem::path& path);
TimbreFormat detectarTimbreFormat(const std::filesystem::path& path);
std::string sha256File(const std::filesystem::path& path);
struct ConvertedZone {
 std::string sample,rootSource;
 int root=60,keyLow=0,keyHigh=127,velLow=1,velHigh=127,rate=0,channels=0;
 std::uintmax_t frames=0;
 bool loop=false;std::uintmax_t loopStart=0,loopEnd=0,altStart=0;
 double volumeDb=0,tuneCents=0,decayDbPerSecond=0,pcmPeak=0;
 int zoneMode=0,zonePlayback=0,zoneOneShot=0;
 // Optional piano-module controls; defaults preserve existing writer output.
 bool hasPan=false,releaseTrigger=false;
 double pan=0,releaseSeconds=-1;
 int cc64Low=-1,cc64High=-1,sequenceLength=0,sequencePosition=0;
 std::uintmax_t sampleStart=0,sampleEnd=0; // end exclusive; 0 means frames
 int sourceLoopMode=-1; // -1 = existing format flags, 0/1/2 = XML modes
 bool muted=false;
 int xfinLowVel=-1,xfinHighVel=-1,xfoutLowVel=-1,xfoutHighVel=-1;
 int program=0;
 int sourceBank=-1; // NPNO: 0 attack, 1 pedal alternative, 2 key release; not a DSP resonance flag.
 int sourceVelLow=1,sourceVelHigh=127;
 double ampVeltrack=100,calibrationOffsetDb=0,calibrationResidualDb=0;
 bool calibrated=false;
};
struct ConversionOptions {
 bool velocityCrossfade=false;
 int crossfadeWidth=8; // Half window in MIDI steps, capped by neighboring layer widths.
 bool velocityCrossfadeExplicit=false; // NPNO defaults on, others retain opt-in behavior.
 bool stereoMonoFallback=false;
 std::filesystem::path calibrationSfizzLibrary; // Explicit offline renderer; empty = measured calibration N/A.

};
struct MetaSidecar {
 std::string name,format,version,originalSha256,signatureHex,status;
 bool signatureConfirmed=false;
 int codec=0;bool checksumValid=false;
 std::vector<std::string> preservedFeatures;
 std::vector<std::string> limitations;
 std::vector<ConvertedZone> zones;
 std::vector<std::filesystem::path> samples;
 std::vector<std::string> sampleHashes;
 std::string sourceMetadataJson;
 std::string json()const;
};
struct MissingNkiSamples:std::runtime_error {using std::runtime_error::runtime_error;};
using ConversionProgress=std::function<void(const std::string&)>;
std::filesystem::path npnoParaSFZ(const std::filesystem::path& entrada,const std::filesystem::path& dirSaida,MetaSidecar& out,
 const std::filesystem::path& sndfileLibrary,const ConversionProgress& progress={},const ConversionOptions& options={});
std::filesystem::path nsmpParaSFZ(const std::filesystem::path& entrada,const std::filesystem::path& dirSaida,MetaSidecar& out,const std::filesystem::path& sndfileLibrary,const ConversionOptions& options={});
std::filesystem::path nkiParaSFZ(const std::filesystem::path& entrada,const std::filesystem::path& dirSaida,MetaSidecar& out,
 const std::filesystem::path& sndfileLibrary,const std::filesystem::path& sampleRoot={},const ConversionOptions& options={});
// Private staging directory, published by rename only after original, SFZ, samples and sidecar succeed.
std::filesystem::path importarTimbreConvertido(const std::filesystem::path& original,const std::filesystem::path& destination,
 const std::filesystem::path& sndfileLibrary,const std::filesystem::path& sampleRoot={},const ConversionProgress& progress={});
}
