// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace dino {

namespace core {

namespace logger {

std::shared_ptr<spdlog::logger> GetDinoLogger();
void SetDinoLogger(std::shared_ptr<spdlog::logger> logger);

}  // namespace logger

}  // namespace core

}  // namespace dino


#define DINOLOG_DEBUG(...) dino::core::logger::GetDinoLogger()->debug(__VA_ARGS__)
#define DINOLOG_INFO(...)  dino::core::logger::GetDinoLogger()->info(__VA_ARGS__)
#define DINOLOG_WARN(...)  dino::core::logger::GetDinoLogger()->warn(__VA_ARGS__)
#define DINOLOG_ERROR(...) dino::core::logger::GetDinoLogger()->error(__VA_ARGS__)
