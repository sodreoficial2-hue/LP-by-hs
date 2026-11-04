#include "timbre_format.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<4||argc>6){std::cerr<<"Uso: lp_convert_timbre ORIGINAL PASTA_NOVA LIBSNDFILE [PASTA_SAMPLES] [--velocity-crossfade=1..16]\n";return 2;}
 auto input=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]),library=std::filesystem::u8path(argv[3]);lp::ConversionOptions options;std::filesystem::path sampleRoot;
 for(int i=4;i<argc;++i){std::string arg=argv[i];const std::string prefix="--velocity-crossfade=";if(arg.compare(0,prefix.size(),prefix)==0){auto number=arg.substr(prefix.size());std::size_t n=0;options.crossfadeWidth=std::stoi(number,&n);if(n!=number.size()||options.crossfadeWidth<1||options.crossfadeWidth>16)throw std::runtime_error("Crossfade velocity deve ter 1 a 16 passos MIDI");options.velocityCrossfade=true;}else if(sampleRoot.empty())sampleRoot=std::filesystem::u8path(arg);else throw std::runtime_error("Argumento extra desconhecido");}
 lp::MetaSidecar meta;auto format=lp::detectarTimbreFormat(input);std::filesystem::path sfz;
 if(format==lp::TimbreFormat::NPNO)sfz=lp::npnoParaSFZ(input,output,meta,library,[](const std::string& s){std::cerr<<s<<'\n';},options);
 else if(format==lp::TimbreFormat::NSMP)sfz=lp::nsmpParaSFZ(input,output,meta,library,options);
 else if(format==lp::TimbreFormat::NKI)sfz=lp::nkiParaSFZ(input,output,meta,library,sampleRoot,options);
 else throw std::runtime_error("Conversor destinado a NSMP/NSMP3/NSMP4 e NKI/NPNO; SFZ/SF2 seguem o importador existente");
 for(const auto& warning:meta.limitations)std::cerr<<"Aviso: "<<warning<<'\n';std::cout<<sfz.u8string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
