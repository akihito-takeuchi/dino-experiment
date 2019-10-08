// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include "dino/core/dexception.h"

namespace dino {

namespace core {

namespace detail {

class ObjectDataException : public DException {
 public:
  ObjectDataException(int error_id)
      : DException(error_id) {}
  ObjectDataException(const DException& e)
      : DException(e) {}
};

extern const int kErrNotObjectDirectory;
extern const int kErrFailedToCreateObjectDirectory;
extern const int kErrObjectDataIsNotEditable;
extern const int kErrChildDataAlreadyExists;
extern const int kErrChildNotExist;
extern const int kErrObjectDirectoryNotInitialized;
extern const int kErrParentDirectoryNotInitialized;
extern const int kErrNoWritePermission;
extern const int kErrFailedToGetFileLock;
extern const int kErrObjectIsFlattened;
extern const int kErrNoKey;
extern const int kErrExpiredObjectToBase;
extern const int kErrNotBaseObject;
extern const int kErrCommandStackAlreadyEnabled;
extern const int kErrObjectIsNotActual;

}  // namespace detail

}  // namespace core

}  // namespace dino
