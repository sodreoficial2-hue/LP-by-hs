#include "timbre_format.hpp"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=4)return 1;
 if(std::string(argv[1]).empty()){std::cout<<"SKIP: arquivo pessoal e referência auditiva não fornecidos; vetores públicos/sintéticos testados separadamente.\n";return 77;}
 try{auto out=std::filesystem::u8path(argv[3]);std::filesystem::remove_all(out);lp::MetaSidecar meta;auto sfz=lp::npnoParaSFZ(std::filesystem::u8path(argv[1]),out,meta,std::filesystem::u8path(argv[2]));if(meta.zones.empty()||!std::filesystem::exists(sfz))return 1;std::cout<<"Arquivo externo convertido; audição comparativa ainda requer referência do usuário.\n";return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
