// Copyright: (c) Jaromir Veber 2018
// Version: 11052018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// Description: Sink implementation for logging into defined db (SQLite3)

#include <sqlite3.h>  // SQLite communication
#include <thread>
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/fmt/fmt.h"

class db_sink : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
 public:
  explicit db_sink(sqlite3* Db): _pDb(Db) {
    if (sqlite3_prepare_v2(_pDb, kSql, strlen(kSql), &_stmt, nullptr) != SQLITE_OK)
      throw spdlog::spdlog_ex("Unable to prepare default logging statement");
  }
  
  ~db_sink() {
    sqlite3_finalize(_stmt);
    int i = 0;
    for (; i < 2; ++i) {
      if (sqlite3_close(_pDb) == SQLITE_OK)
          break;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    // this is not exactly fast but... lets
    auto secs = std::chrono::time_point_cast<std::chrono::nanoseconds>(msg.time).time_since_epoch().count();
    //std::unique_lock<std::mutex> lck(_sink_mutex); //multithreading support (this should not be needed because it is used in distributed sink tha mutex it's own access)
    sqlite3_bind_int64(_stmt, 1, static_cast<sqlite3_int64>(secs));
    sqlite3_bind_int(_stmt, 2, static_cast<int>(msg.level));
    sqlite3_bind_int64(_stmt, 3, static_cast<sqlite3_int64>(msg.thread_id));
    sqlite3_bind_int64(_stmt, 4, static_cast<sqlite3_int64>(0));  // msq_id was removed
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(msg, formatted);
    sqlite3_bind_text(_stmt, 5, msg.logger_name.data(), msg.logger_name.size(), SQLITE_STATIC);
    sqlite3_bind_text(_stmt, 6, formatted.data(), formatted.size(), SQLITE_STATIC);
    sqlite3_step(_stmt);
    sqlite3_reset(_stmt);
  }

  virtual void flush_() override { 
  }

 private:
  sqlite3* _pDb;
  static constexpr const char* kSql = "INSERT INTO log (timestamp, level, thread, msgid, logger, message) VALUES (?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *_stmt;
};