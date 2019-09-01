// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <stdexcept>
#include <boost/lexical_cast.hpp>
#include <boost/exception/all.hpp>
#ifdef ENABLE_STACKTRACE
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>
#endif

namespace dino {

namespace core {

#ifdef ENABLE_STACKTRACE
using StackTraceInfo = boost::error_info<struct StackTraceInfoTag,
                                         boost::stacktrace::stacktrace>;
#endif

class DException : public boost::exception, public std::exception {
 public:
  DException(int error_code)
      : boost::exception(), std::exception(), error_code_(error_code) {
#ifdef ENABLE_STACKTRACE
    *this << StackTraceInfo(boost::stacktrace::stacktrace());
#endif
  }
  DException(const DException& e)
      : boost::exception(e), error_code_(e.error_code_) {}
  virtual ~DException() = default;
  virtual std::string GetErrorMessage() const;
  virtual const char* what() const noexcept {
    error_message_ = GetErrorMessage();
    return error_message_.c_str();
  }

 private:
  mutable std::string error_message_;
  int error_code_;
};

using ExceptionInfo1 = boost::error_info<
  struct ExceptionInfoTag1, std::string>;
template<typename T>
ExceptionInfo1 ExpInfo1(const T& v) {
  return ExceptionInfo1(boost::lexical_cast<std::string>(v));
}

using ExceptionInfo2 = boost::error_info<
  struct ExceptionInfoTag2, std::string>;
template<typename T>
ExceptionInfo2 ExpInfo2(const T& v) {
  return ExceptionInfo2(boost::lexical_cast<std::string>(v));
}

using ExceptionInfo3 = boost::error_info<
  struct ExceptionInfoTag3, std::string>;
template<typename T>
ExceptionInfo3 ExpInfo3(const T& v) {
  return ExceptionInfo3(boost::lexical_cast<std::string>(v));
}

int RegisterErrorCode(int error_code, const char* msg, int info_count);

extern const int kErrObjectName;
extern const int kErrObjectAlreadyExists;
extern const int kErrFailedToGetCurrentUserName;
extern const int kErrObjectTypeNotRegistered;
extern const int kErrObjectDataAlreadyExists;
extern const int kErrObjectDoesNotExist;
extern const int kErrTopLevelObjectAlreadyInitialized;
extern const int kErrNoTopLevelObject;
extern const int kErrParentObjectNotOpened;
extern const int kErrObjectDataNotOpened;
extern const int kErrObjectPathEmpty;
extern const int kErrTopLevelObjectNotExist;
extern const int kErrDirPathForNonTop;
extern const int kErrTopLevelObjectNotInitialized;
extern const int kErrAnyChildOpened;
extern const int kErrWorkspaceFileAlreadyExists;
extern const int kErrWorkspaceFileDoesNotExist;
extern const int kErrFailedToCreateDirectory;
extern const int kErrFailedToOpenWorkspaceFile;
extern const int kErrWorkspaceFileError;
extern const int kErrWorkspaceFilePathNotSet;
extern const int kErrTopObjectDoesNotExist;
extern const int kErrObjectExpired;
extern const int kErrObjectIsNotEditable;
extern const int kErrInvalidObjectString;
extern const int kErrNoRedoEntry;
extern const int kErrNoUndoEntry;
extern const int kErrBatchCommandError;
extern const int kErrInvalidCommandTypeError;

}  // namespace core

}  // namespace dino
