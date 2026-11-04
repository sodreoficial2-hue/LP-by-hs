// SPDX-License-Identifier: AGPL-3.0-or-later
#include "sfizz_probe.hpp"
#include "npno_reader.hpp"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=6)return 1;
 if(std::string(argv[3]).empty()){std::cout<<"N/A: fixture pessoal NPNO v"<<argv[5]<<" não fornecido; sem prova auditiva do teclado.\n";return 77;}
 try{
  auto input=std::filesystem::u8path(argv[3]),output=std::filesystem::u8path(argv[4]);auto module=lp::npno::readModule(input);
  if(module.streamVersion!=(std::string(argv[5])=="5"?0x450:0x464))throw std::runtime_error("Fixture pessoal tem versão interna diferente da solicitada");
  std::filesystem::remove_all(output);lp::MetaSidecar meta;lp::ConversionOptions options;options.calibrationSfizzLibrary=std::filesystem::u8path(argv[1]);
  auto sfz=lp::npnoParaSFZ(input,output,meta,std::filesystem::u8path(argv[2]),{},options);double residual=0;
  for(const auto& z:meta.zones)if(!z.muted){if(!z.calibrated||z.volumeDb< -144||z.volumeDb>10.8)throw std::runtime_error("Fixture pessoal fora dos limites de calibração");residual=std::max(residual,z.calibrationResidualDb);}
  if(residual>0.5)throw std::runtime_error("Fixture pessoal excede erro RMS de projeto 0.5 dB: "+std::to_string(residual));
  for(int pedal:{0,127})for(int velocity:{1,64,127}){SfizzProbe synth(argv[1]);synth.load(sfz);synth.cc(synth.synth,0,64,pedal);int key=module.zones.front().low;synth.on(synth.synth,0,key,velocity);auto pcm=synth.render(1024);double energy=0;for(const auto& c:pcm)for(float v:c)energy+=double(v)*v;if(!(energy>0))throw std::runtime_error("Fixture pessoal produziu nota silenciosa");}
  std::cout<<"PASSOU: fixture pessoal v"<<argv[5]<<", residual RMS máximo="<<residual<<" dB; referência de projeto, sem equivalência auditiva Nord.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
