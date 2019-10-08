// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include <map>
#include <mutex>
#include <memory>
#include <boost/preprocessor.hpp>
#include <boost/algorithm/string.hpp>
#ifdef ENABLE_STACKTRACE
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>
#endif

#include "dino/core/dexception.h"
#include "dino/core/fspath.h"
#include "fmt/format.h"

namespace dino {

namespace core {

namespace {

struct MsgInfo {
  const char* fmt;
  int info_count;
};

const char* kUnknownErrorCodeMessageFmt = "No error message for code {}";

class Messages {
 public:
  ~Messages() = default;
  Messages(const Messages&) = delete;
  Messages& operator=(const Messages&) = delete;

  void Register(int error_code, const char* msg, int info_count);
  MsgInfo GetMessageInfo(int error_code) const;
  static Messages& Instance();

 private:
  Messages() = default;

  static void Init();
  static std::once_flag init_flag_;
  static std::unique_ptr<Messages> instance_;

  std::map<int, MsgInfo> error_message_info_;
};

std::once_flag Messages::init_flag_;
std::unique_ptr<Messages> Messages::instance_ = nullptr;

void Messages::Register(int error_code, const char* msg, int info_count) {
  error_message_info_[error_code] = MsgInfo{msg, info_count};
}

MsgInfo Messages::GetMessageInfo(int error_code) const {
  auto itr = error_message_info_.find(error_code);
  if (itr == error_message_info_.cend())
    return MsgInfo{kUnknownErrorCodeMessageFmt, 1};
  return itr->second;
}

Messages& Messages::Instance() {
  std::call_once(init_flag_, Init);
  return *instance_;
}

void Messages::Init() {
  instance_ = std::unique_ptr<Messages>(new Messages());
}

#ifndef NDEBUG
std::string GetErrorLocation(const boost::exception& e) {
  std::ostringstream ss;
  std::string file_path(*boost::get_error_info<boost::throw_file>(e));
  if (FsPath(file_path).is_absolute()) {
    std::string proj_root(BOOST_PP_STRINGIZE(DINO_ROOT));
    std::vector<std::string> file_path_elems;
    std::vector<std::string> proj_root_elems;
    boost::split(file_path_elems, file_path, boost::is_any_of("/"));
    boost::split(proj_root_elems, proj_root, boost::is_any_of("/"));
    file_path = boost::join(
        std::vector<std::string>(file_path_elems.begin() + proj_root_elems.size(),
                                 file_path_elems.end()), "/");
  }
  ss << "File: " << file_path
     << ", Line: " << *boost::get_error_info<boost::throw_line>(e)
     << ", Func: " << *boost::get_error_info<boost::throw_function>(e);
  return ss.str();
}
#endif  // NDEBUG

}  // namespace

int RegisterErrorCode(int error_code, const char* msg, int info_count) {
  Messages::Instance().Register(error_code, msg, info_count);
  return error_code;
}

#ifdef ENABLE_STACKTRACE
using StackTraceInfo = boost::error_info<struct StackTraceInfoTag,
                                         boost::stacktrace::stacktrace>;
#endif

DException::DException(int error_code)
    : boost::exception(), std::exception(), error_code_(error_code) {
#ifdef ENABLE_STACKTRACE
  *this << StackTraceInfo(boost::stacktrace::stacktrace());
#endif
}

DException::DException(const DException& e)
    : boost::exception(e), error_code_(e.error_code_) {
}

std::string DException::GetErrorMessage() const {
  MsgInfo info = Messages::Instance().GetMessageInfo(error_code_);
  std::string msg;
  if (info.info_count == 1)
    msg = fmt::format(info.fmt,
                      *boost::get_error_info<ExceptionInfo1>(*this));
  else if (info.info_count == 2)
    msg = fmt::format(info.fmt,
                      *boost::get_error_info<ExceptionInfo1>(*this),
                      *boost::get_error_info<ExceptionInfo2>(*this));
  else if (info.info_count == 3)
    msg = fmt::format(info.fmt,
                      *boost::get_error_info<ExceptionInfo1>(*this),
                      *boost::get_error_info<ExceptionInfo2>(*this),
                      *boost::get_error_info<ExceptionInfo3>(*this));
  else
    msg = info.fmt;

#ifndef NDEBUG
  msg += " (" + GetErrorLocation(*this) + ')';
#endif  // NDEBUG
  return msg;
}

const char* DException::what() const noexcept {
  std::ostringstream ss;
  ss << GetErrorMessage();

#ifdef ENABLE_STACKTRACE
  const boost::stacktrace::stacktrace* st =
       boost::get_error_info<StackTraceInfo>(*this);
  if (st)
    ss << '\n' << *st;
#endif

  error_message_ = ss.str();

  return error_message_.c_str();
}

}  // namespace core

}  // namespace dino
