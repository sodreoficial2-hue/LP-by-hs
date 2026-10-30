#pragma once
#include "recoverable_file.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
namespace lp {
class BackgroundStore {
 struct Write {std::string bytes;unsigned long long version;};
 std::mutex mutex;std::condition_variable wake,drained;std::map<std::filesystem::path,Write> pending;
 std::vector<std::string> errors;std::thread worker;bool stopping=false,busy=false;unsigned long long serial=0;
 void run(){for(;;){std::filesystem::path file;Write write;
  {std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return stopping||!pending.empty();});if(pending.empty()&&stopping)return;auto item=pending.begin();file=item->first;write=std::move(item->second);pending.erase(item);busy=true;}
  try{backedUpWrite(file,write.bytes);}catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);errors.push_back(e.what());}
  {std::lock_guard<std::mutex> lock(mutex);busy=false;if(pending.empty())drained.notify_all();}
 }}
public:
 BackgroundStore():worker([this]{run();}){}
 ~BackgroundStore(){flush();{std::lock_guard<std::mutex> lock(mutex);stopping=true;}wake.notify_one();worker.join();}
 void submit(const std::filesystem::path& file,std::string bytes){if(file.empty())throw std::invalid_argument("Destino de rascunho indisponível");{std::lock_guard<std::mutex> lock(mutex);pending[file]={std::move(bytes),++serial};}wake.notify_one();}
 std::vector<std::string> takeErrors(){std::lock_guard<std::mutex> lock(mutex);std::vector<std::string> result;result.swap(errors);return result;}
 void flush(){std::unique_lock<std::mutex> lock(mutex);drained.wait(lock,[&]{return pending.empty()&&!busy;});}
};
}
