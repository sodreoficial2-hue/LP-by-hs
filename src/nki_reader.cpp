#include "nki_reader.hpp"
#include <expat.h>
#include <zlib.h>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <array>
#include <algorithm>
namespace lp::nki {
const Node* Node::child(const std::string& name)const{for(const auto& c:children)if(c.tag==name)return &c;return nullptr;}
std::string Node::attr(const std::string& name,const std::string& fallback)const{auto it=attrs.find(name);return it==attrs.end()?fallback:it->second;}
std::map<std::string,std::string> values(const Node& n){std::map<std::string,std::string> out;for(const auto& c:n.children){if(c.tag=="V")out[c.attr("name")]=c.attr("value");else if(c.tag=="Parameters"){auto v=values(c);out.insert(v.begin(),v.end());}}return out;}
namespace {
constexpr std::size_t maxInput=256u*1024u*1024u,maxXml=16u*1024u*1024u;
struct Parse {
 XML_Parser parser{};Node root;std::vector<Node*> stack;std::string error;std::size_t nodes=0;
 void fail(const std::string& why){error=why;XML_StopParser(parser,XML_FALSE);}
};
void XMLCALL start(void* opaque,const char* name,const char** attrs){
 auto& p=*static_cast<Parse*>(opaque);
 try{
  if(++p.nodes>100000||p.stack.size()>=64){p.fail("NKI XML excede limites de nós/profundidade");return;}
  Node* node;if(p.stack.empty())node=&p.root;else {p.stack.back()->children.emplace_back();node=&p.stack.back()->children.back();}
  node->tag=name;for(;*attrs;attrs+=2)node->attrs[attrs[0]]=attrs[1];p.stack.push_back(node);
 }catch(...){p.fail("NKI XML: memória insuficiente");}
}
void XMLCALL end(void* opaque,const char*){auto& p=*static_cast<Parse*>(opaque);if(!p.stack.empty())p.stack.pop_back();}
void XMLCALL doctype(void* opaque,const char*,const char*,const char*,int){static_cast<Parse*>(opaque)->fail("NKI XML: DTD/entidades externas não permitidos");}
Node parse(const std::string& xml){
 Parse p;p.parser=XML_ParserCreate("UTF-8");if(!p.parser)throw std::runtime_error("NKI XML: parser indisponível");
 std::unique_ptr<XML_ParserStruct,decltype(&XML_ParserFree)> holder(p.parser,XML_ParserFree);
 XML_SetUserData(p.parser,&p);XML_SetElementHandler(p.parser,start,end);XML_SetStartDoctypeDeclHandler(p.parser,doctype);
 if(XML_Parse(p.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE)==XML_STATUS_ERROR){
  auto why=p.error.empty()?std::string(XML_ErrorString(XML_GetErrorCode(p.parser))):p.error;
  throw std::runtime_error("NKI XML inválido na linha "+std::to_string(XML_GetCurrentLineNumber(p.parser))+": "+why);
 }
 return std::move(p.root);
}
}
Document read(const std::filesystem::path& path){
 Document d;std::error_code ec;auto size=std::filesystem::file_size(path,ec);
 if(ec||size<4)throw std::runtime_error("NKI vazio ou truncado");
 if(size>maxInput)throw std::runtime_error("NKI excede limite de 256 MiB");
 std::ifstream f(path,std::ios::binary);d.bytes.resize(size);if(!f.read(reinterpret_cast<char*>(d.bytes.data()),size))throw std::runtime_error("NKI: falha ao ler arquivo");
 std::vector<std::size_t> offsets;for(std::size_t i=0;i<std::min<std::size_t>(256,size-1);++i)if(d.bytes[i]==0x78&&(d.bytes[i+1]==1||d.bytes[i+1]==0x9c||d.bytes[i+1]==0xda))offsets.push_back(i);
 bool gzip=offsets.empty()&&d.bytes[0]==0x1f&&d.bytes[1]==0x8b;if(gzip)offsets.push_back(0);
 if(offsets.empty())return d;
 bool unknownRoot=false;std::string error="NKI: payload zlib/gzip corrompido ou truncado";
 for(auto at:offsets){
  z_stream stream{};stream.next_in=d.bytes.data()+at;stream.avail_in=static_cast<uInt>(size-at);
  if(inflateInit2(&stream,gzip?31:15)!=Z_OK)throw std::runtime_error("NKI: zlib indisponível");
  struct Guard {z_stream* s;~Guard(){inflateEnd(s);}} guard{&stream};
  std::string xml;std::array<char,8192> block{};int rc=Z_OK;
  while(rc==Z_OK){stream.next_out=reinterpret_cast<Bytef*>(block.data());stream.avail_out=block.size();rc=inflate(&stream,Z_NO_FLUSH);xml.append(block.data(),block.size()-stream.avail_out);if(xml.size()>maxXml)throw std::runtime_error("NKI: XML descomprimido excede limite de 16 MiB");}
  if(rc!=Z_STREAM_END)continue;
  // A complete compressed stream with an unknown root can use the declared fallback.
  // A malformed recognized XML must never silently fall back to filename mapping.
  auto first=xml.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
  if(first==std::string::npos||xml[first]!='<')continue;
  auto root=parse(xml);if(root.tag!="K2_Container"){unknownRoot=true;continue;}
  d.recognized=true;d.xml=std::move(xml);d.root=std::move(root);d.payloadOffset=at;d.payloadEnd=at+stream.total_in;return d;
 }
 // Recognizable compressed bytes that fail decompression are corruption, not a fallback.
 if(unknownRoot)return d;throw std::runtime_error(error);
}
}
