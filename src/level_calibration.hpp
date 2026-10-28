#pragma once
#include "level_balance.hpp"
#include "instrument_catalog.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace lp {
// Private offline synths only: no PortAudio, ASIO, MIDI input or live graph.
class LevelCalibrator {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 explicit LevelCalibrator(const std::filesystem::path& sfizzLibrary);
 ~LevelCalibrator();
 LevelResult measure(const Instrument&,const std::atomic<bool>& cancel);
};
struct LevelProgress {
 std::size_t completed=0,total=0,analyzed=0,reused=0,failed=0,silent=0;
 bool done=false,cancelled=false;
 std::string name,error;
 std::map<std::string,float> gains;
};
class LevelCalibrationJob {
 std::atomic<bool> cancel{false};
 mutable std::mutex mutex;LevelProgress progress;
 std::thread worker;
 void run(std::filesystem::path,std::filesystem::path,std::vector<Instrument>);
public:
 LevelCalibrationJob(const std::filesystem::path& library,const std::filesystem::path& cache,std::vector<Instrument> instruments);
 ~LevelCalibrationJob();
 LevelProgress snapshot()const;
};
}
