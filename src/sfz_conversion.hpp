#pragma once
// Shared offline WAV/SFZ output. The live engine and DSP are not involved.
#include "sample_builder.hpp"
#include "timbre_format.hpp"
#include <sstream>
#include <locale>
namespace lp {
class WavWriter {
 struct Info {std::int64_t frames;int samplerate,channels,format,sections,seekable;};
#ifdef _WIN32
 HMODULE lib{};void* symbol(const char* name){return reinterpret_cast<void*>(GetProcAddress(lib,name));}
 void* (*open)(const wchar_t*,int,Info*){};
#else
 void* lib{};void* symbol(const char* name){return dlsym(lib,name);}
 void* (*open)(const char*,int,Info*){};
#endif
 std::int64_t (*writeFrames)(void*,const float*,std::int64_t){};int (*closeFile)(void*){};
 void unload();
public:
 explicit WavWriter(const std::filesystem::path& library);
 ~WavWriter(){unload();}WavWriter(const WavWriter&)=delete;WavWriter& operator=(const WavWriter&)=delete;
 void write(const std::filesystem::path& path,int rate,int channels,const std::vector<float>& interleaved);
 void writePcm16(const std::filesystem::path& path,int rate,const std::vector<std::int16_t>& interleaved,int channels=1);
};
std::ostringstream sfzStream();
void writeSfzGroup(std::ostream& out);
bool writeSfzRegion(std::ostream& out,const ConvertedZone& zone,std::vector<std::string>* warnings=nullptr);
double linearGainDb(double gain);
// Audit disjoint velocity layers per key, trigger, pedal condition, stereo lane and RR position.
// Excludes note-on velocity 0. Missing/duplicate layers are an error, never filled silently.
void auditVelocityCoverage(const std::vector<ConvertedZone>& zones);
void prepareSfzZones(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options={});
void calibrateSfzZones(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options,const std::filesystem::path& output);
void auditNpnoPedalBands(const std::vector<ConvertedZone>& zones);
void prepareNpnoSustain(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options,const std::filesystem::path& output,const std::filesystem::path& sndfile);
double npnoLoopDecaySeconds(std::uint32_t coefficient);
std::string writeSfzInstrument(const std::vector<ConvertedZone>& zones);
std::string bytesHex(const std::uint8_t* data,std::size_t size);
}
namespace lp {
// Converters only create a fresh directory; an error removes their own output.
class ConversionOutput {
 std::filesystem::path dir;bool done=false;
public:
 explicit ConversionOutput(const std::filesystem::path& directory);
 ~ConversionOutput(){if(!done){std::error_code ec;std::filesystem::remove_all(dir,ec);}}
 ConversionOutput(const ConversionOutput&)=delete;ConversionOutput& operator=(const ConversionOutput&)=delete;
 std::filesystem::path finish(const std::filesystem::path& original,MetaSidecar& meta,const std::string& sfz);
};
}
