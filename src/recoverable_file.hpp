#pragma once
#include "atomic_file.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
namespace lp {
inline std::string boundedFile(const std::filesystem::path& file){
 if(std::filesystem::file_size(file)>32*1024*1024)throw std::runtime_error("Arquivo de configurações excede o limite");
 std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Não foi possível ler as configurações");std::ostringstream out;out<<in.rdbuf();if(in.bad())throw std::runtime_error("Falha na leitura das configurações");return out.str();
}
inline void backedUpWrite(const std::filesystem::path& file,const std::string& bytes){
 if(std::filesystem::exists(file)){auto backup=file;backup+=".backup";atomicWrite(backup,boundedFile(file));}atomicWrite(file,bytes);
}
inline void preserveInvalid(const std::filesystem::path& file){
 if(!std::filesystem::exists(file))return;auto invalid=file;invalid+=".invalid."+std::to_string(std::chrono::system_clock::now().time_since_epoch().count());std::filesystem::rename(file,invalid);
}
template<class Read> bool recoverFile(const std::filesystem::path& file,Read read){
 try{read(file);return false;}catch(...){auto backup=file;backup+=".backup";try{read(backup);}catch(...){preserveInvalid(file);throw;}preserveInvalid(file);atomicWrite(file,boundedFile(backup));return true;}
}
}
