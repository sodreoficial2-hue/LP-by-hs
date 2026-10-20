#pragma once
#include <atomic>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace lp {
// Write a sibling temporary file and replace only after all data is flushed.
// Existing shows survive failed writes. No automatic saving is performed.
inline void atomicWrite(const std::filesystem::path& destination,const std::string& bytes) {
 static std::atomic<unsigned long> counter{0};
#ifdef _WIN32
 const auto pid=GetCurrentProcessId();
 HANDLE file=INVALID_HANDLE_VALUE;
#else
 const auto pid=getpid();
 int file=-1;
#endif
 std::filesystem::path temporary;
 for(int attempts=0;attempts<100;++attempts) {
  temporary=destination;temporary+=L".tmp."+std::to_wstring(pid)+L"."+std::to_wstring(counter.fetch_add(1));
#ifdef _WIN32
  file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file!=INVALID_HANDLE_VALUE)break;
  if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("Cannot create temporary profile");
#else
  file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
  if(file>=0)break;
  if(errno!=EEXIST)throw std::runtime_error("Cannot create temporary profile");
#endif
 }
#ifdef _WIN32
 if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot allocate temporary profile");
#else
 if(file<0)throw std::runtime_error("Cannot allocate temporary profile");
#endif
 bool closed=false;
 try {
  std::size_t position=0;
  while(position<bytes.size()) {
#ifdef _WIN32
   DWORD written=0;auto size=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-position,1048576));
   if(!WriteFile(file,bytes.data()+position,size,&written,nullptr)||written==0)throw std::runtime_error("Profile write failed");
#else
   auto written=::write(file,bytes.data()+position,bytes.size()-position);
   if(written<0 && errno==EINTR)continue;
   if(written<=0)throw std::runtime_error("Profile write failed");
#endif
   position+=static_cast<std::size_t>(written);
  }
#ifdef _WIN32
  if(!FlushFileBuffers(file))throw std::runtime_error("Profile flush failed");
  auto closeOk=CloseHandle(file);closed=true;
  if(!closeOk)throw std::runtime_error("Profile close failed");
  if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Profile replace failed");
#else
  if(::fsync(file)!=0)throw std::runtime_error("Profile flush failed");
  auto closeResult=::close(file);closed=true;
  if(closeResult!=0)throw std::runtime_error("Profile close failed");
  std::filesystem::rename(temporary,destination);
#endif
 }catch(...) {
  if(!closed) {
#ifdef _WIN32
   CloseHandle(file);
#else
   ::close(file);
#endif
  }
  std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;
 }
}
}
