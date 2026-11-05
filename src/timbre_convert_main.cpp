#include "timbre_format.hpp"
#include <iostream>
#include <charconv>
int main(int argc,char** argv){try{
 if(argc<4){std::cerr<<"Uso: lp_convert_timbre ORIGINAL PASTA_NOVA LIBSNDFILE [PASTA_SAMPLES] [--velocity-crossfade|--no-velocity-crossfade] [--crossfade-width=1..16] [--stereo-mono-fallback] [--calibrate-sfizz=LIB] [--sustain-sfizz=LIB] [--no-pedal-resonance]\n";return 2;}
 auto input=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]),library=std::filesystem::u8path(argv[3]);lp::ConversionOptions options;std::filesystem::path sampleRoot;
 for(int i=4;i<argc;++i){std::string arg=argv[i];
  auto width=[&](const std::string& number){auto parsed=std::from_chars(number.data(),number.data()+number.size(),options.crossfadeWidth);if(parsed.ec!=std::errc()||parsed.ptr!=number.data()+number.size()||options.crossfadeWidth<1||options.crossfadeWidth>16)throw std::runtime_error("Crossfade velocity deve ter 1 a 16 passos MIDI");};
  if(arg=="--velocity-crossfade"){options.velocityCrossfade=true;options.velocityCrossfadeExplicit=true;}
  else if(arg=="--no-velocity-crossfade"){options.velocityCrossfade=false;options.velocityCrossfadeExplicit=true;}
  else if(arg.rfind("--velocity-crossfade=",0)==0){width(arg.substr(21));options.velocityCrossfade=true;options.velocityCrossfadeExplicit=true;}
  else if(arg.rfind("--crossfade-width=",0)==0)width(arg.substr(18));
  else if(arg=="--no-pedal-resonance")options.npnoPedalResonance=false;
  else if(arg.rfind("--sustain-sfizz=",0)==0){options.sustainSfizzLibrary=std::filesystem::u8path(arg.substr(16));if(options.sustainSfizzLibrary.empty())throw std::runtime_error("Informe a biblioteca sfizz para sustain");}
  else if(arg=="--stereo-mono-fallback")options.stereoMonoFallback=true;
  else if(arg.rfind("--calibrate-sfizz=",0)==0){options.calibrationSfizzLibrary=std::filesystem::u8path(arg.substr(18));if(options.calibrationSfizzLibrary.empty())throw std::runtime_error("Informe a biblioteca sfizz para calibrar");}
  else if(arg.rfind("--",0)==0)throw std::runtime_error("Opção desconhecida: "+arg);
  else if(sampleRoot.empty())sampleRoot=std::filesystem::u8path(arg);else throw std::runtime_error("Argumento extra desconhecido");
 }
 lp::MetaSidecar meta;auto format=lp::detectarTimbreFormat(input);std::filesystem::path sfz;
 if(format==lp::TimbreFormat::NPNO)sfz=lp::npnoParaSFZ(input,output,meta,library,[](const std::string& s){std::cerr<<s<<'\n';},options);
 else if(format==lp::TimbreFormat::NSMP)sfz=lp::nsmpParaSFZ(input,output,meta,library,options);
 else if(format==lp::TimbreFormat::NKI)sfz=lp::nkiParaSFZ(input,output,meta,library,sampleRoot,options);
 else throw std::runtime_error("Conversor destinado a NSMP/NSMP3/NSMP4 e NKI/NPNO; SFZ/SF2 seguem o importador existente");
 for(const auto& warning:meta.limitations)std::cerr<<"Aviso: "<<warning<<'\n';std::cout<<sfz.u8string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
