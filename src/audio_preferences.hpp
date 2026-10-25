#pragma once
#include "audio_host.hpp"
#include "audio_settings.hpp"
namespace lp {
inline int resolveAsioDriver(const std::vector<AudioDevice>& devices,const std::string& name){
 int id=-1;for(const auto& item:devices)if(item.name==name){if(id!=-1)throw std::runtime_error("Nome ASIO duplicado; selecione manualmente o driver");id=item.id;}
 if(id<0)throw std::runtime_error("O driver ASIO salvo não está disponível; selecione seu driver ASIO");
 return id;
}
inline void rememberConnectedAudio(const AudioHost& host,const std::filesystem::path& path){
 if(!host.running())return;
 for(const auto& item:host.devices())if(item.id==host.selectedDevice()){
  if(path.empty())throw std::runtime_error("Não foi possível localizar a pasta de configurações de áudio");
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  AudioSettings{item.name,host.bufferSize()}.save(path);return;
 }
 throw std::runtime_error("Não foi possível identificar o driver ASIO conectado para salvar");
}
inline void connectAndRemember(AudioHost& host,int device,unsigned long buffer,const std::filesystem::path& path){
 host.start(device,buffer);rememberConnectedAudio(host,path);
}
inline AudioSettings restoreRememberedAudio(AudioHost& host,const std::filesystem::path& path){
 // Legacy files remain readable; the driver now owns the buffer selection.
 auto saved=AudioSettings::load(path);host.start(resolveAsioDriver(host.devices(),saved.driver),0);return saved;
}
}
