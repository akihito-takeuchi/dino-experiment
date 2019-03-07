// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/logger.h"

namespace dino { namespace core {

namespace logger {

namespace {

std::shared_ptr<spdlog::logger>  InitDefaultLogger() {
  return spdlog::basic_logger_mt("dino_logger", "dino.log");
}

std::shared_ptr<spdlog::logger> logger_ = InitDefaultLogger();

}  // namespace

std::shared_ptr<spdlog::logger> GetDinoLogger() {
  return logger_;
}

void SetDinoLogger(std::shared_ptr<spdlog::logger> logger) {
  logger_ = logger;
}

}  // namespace logger

}} // namespace dino::core
