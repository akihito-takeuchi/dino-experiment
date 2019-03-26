// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dexception.h"

namespace dino {

namespace core {

namespace detail {

extern const int kErrNotObjectDirectory = RegisterErrorCode(
    300, "The directory '{}' is not an object directory", 1);
extern const int kErrFailedToCreateObjectDirectory = RegisterErrorCode(
    301, "Failed to create object directory at '{}'", 1);
extern const int kErrObjectDataIsNotEditable = RegisterErrorCode(
    302, "Object data '{}' is not editable", 1);
extern const int kErrChildDataAlreadyExists = RegisterErrorCode(
    303, "Child data '{}' already exists in '{}'", 2);
extern const int kErrChildNotExist = RegisterErrorCode(
    304, "Child '{}' does not eixst in '{}'", 2);
extern const int kErrObjectDirectoryNotInitialized = RegisterErrorCode(
    305, "Object directory of '{}' has not been initialized", 1);
extern const int kErrParentDirectoryNotInitialized = RegisterErrorCode(
    306, "Parent directory of '{}' has not been initialized", 1);
extern const int kErrNoWritePermission = RegisterErrorCode(
    307, "The file '{}' is not writable", 1);
extern const int kErrFailedToGetFileLock = RegisterErrorCode(
    308, "Failed to acquire the file lock -> '{}'", 1);
extern const int kErrObjectIsFlattened = RegisterErrorCode(
    309, "The object '{}' has been flattened", 1);
extern const int kErrNoKey = RegisterErrorCode(
    310, "The object '{}' does not have key '{}'", 2);
extern const int kErrExpiredObjectToBase = RegisterErrorCode(
    311, "Can't set the expired object '{}' as the base of '{}'", 2);
extern const int kErrNotBaseObject = RegisterErrorCode(
    312, "The object '{}' is not the base of '{}'", 2);
extern const int kErrChildIndexOutOfRange = RegisterErrorCode(
    313, "The child index '{}' out of range at '{}'", 2);

extern const int kErrUnknownFileFormat = RegisterErrorCode(
    400, "Unknown file format number '{}'", 1);

extern const int kErrJsonWriteData = RegisterErrorCode(
    1000, "Unexpected data type found when writing the data", 0);
extern const int kErrJsonFileOpen = RegisterErrorCode(
    1001, "Can't open the data file '{}' for {}", 2);
extern const int kErrJsonInvalidSectionName = RegisterErrorCode(
    1002, "Invalid section name '{}'", 1);
extern const int kErrJsonFailedToChangeSection = RegisterErrorCode(
    1003, "Failed to change section '{}'", 1);
extern const int kErrJsonInvalidFileReadState = RegisterErrorCode(
    1004, "Invalid state at json data file reading", 0);

}  // namespace detail

}  // namespace core

}  // namespace dino
