#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace lp::nki {
struct Node {
 std::string tag;
 std::map<std::string,std::string> attrs;
 std::vector<Node> children;
 const Node* child(const std::string& name)const;
 std::string attr(const std::string& name,const std::string& fallback={})const;
};
struct Document {
 bool recognized=false;
 std::string xml;
 std::vector<unsigned char> bytes;
 std::size_t payloadOffset=0,payloadEnd=0;
 Node root;
};
// No extension recognition is used for the compressed XML payload itself.
// DTDs, external entities, oversized expansion and excessive nesting are rejected.
Document read(const std::filesystem::path& path);
std::map<std::string,std::string> values(const Node& node);
}
