// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/dexception.h"

namespace dino {

namespace core {


extern const int kErrObjectName = RegisterErrorCode(
    1, "Invalid object name -> '{}'", 1);
extern const int kErrInvalidObjectString = RegisterErrorCode(
    2, "Invalid object info string -> '{}'", 1);
extern const int kErrFailedToGetCurrentUserName = RegisterErrorCode(
    3, "Failed to get current user name", 0);
extern const int kErrObjectTypeNotRegistered = RegisterErrorCode(
    4, "Object type '{}' has not been registered", 1);

extern const int kErrObjectAlreadyExists = RegisterErrorCode(
    100, "The object '{}' already exists", 1);
extern const int kErrObjectDataAlreadyExists = RegisterErrorCode(
    101, "Object data of '{}' already exists", 1);
extern const int kErrObjectDoesNotExist = RegisterErrorCode(
    102, "The object '{}' does not exist", 1);
extern const int kErrTopLevelObjectAlreadyInitialized = RegisterErrorCode(
    103, "The directory path of the top level object '{}' has been initialized", 1);
extern const int kErrNoTopLevelObject = RegisterErrorCode(
    104, "Top level object of '{}' does not exist", 1);
extern const int kErrParentObjectNotOpened = RegisterErrorCode(
    105, "The parent object of '{}' has not been opened", 1);
extern const int kErrObjectDataNotOpened = RegisterErrorCode(
    106, "The object data '{}' has not been opened", 1);
extern const int kErrObjectPathEmpty = RegisterErrorCode(
    107, "The object path is empty", 0);
extern const int kErrTopLevelObjectNotExist = RegisterErrorCode(
    108, "The top level object for '{}' does not exist", 1);
extern const int kErrDirPathForNonTop = RegisterErrorCode(
    109, "The directory path can only be specified for top level object", 0);
extern const int kErrTopLevelObjectNotInitialized = RegisterErrorCode(
    110, "The directory path of the top level object '{}' has not been initialized", 1);
extern const int kErrAnyChildOpened = RegisterErrorCode(
    111, "Child data is opened under '{}'", 1);
extern const int kErrWorkspaceFileAlreadyExists = RegisterErrorCode(
    112, "Workspace file '{}' already exists", 1);
extern const int kErrWorkspaceFileDoesNotExist = RegisterErrorCode(
    113, "Workspace file '{}' does not exist", 1);
extern const int kErrFailedToCreateDirectory = RegisterErrorCode(
    114, "Failed to create the directory '{}'", 1);
extern const int kErrFailedToOpenWorkspaceFile = RegisterErrorCode(
    115, "Failed to open the workspace file '{}' for {}", 2);
extern const int kErrWorkspaceFileError = RegisterErrorCode(
    116, "Error in workspace file '{}' -> {}", 2);
extern const int kErrWorkspaceFilePathNotSet = RegisterErrorCode(
    117, "Workspace file path has not been set.", 0);

extern const int kErrObjectExpired = RegisterErrorCode(
    200, "The object handle '{}' already expired", 1);
extern const int kErrObjectIsNotEditable = RegisterErrorCode(
    201, "The object handle '{}' is not editable", 1);

extern const int kErrNoRedoEntry = RegisterErrorCode(
    250, "The command stack of '{}' doesn't have redo entry", 1);
extern const int kErrNoUndoEntry = RegisterErrorCode(
    251, "The command stack of '{}' doesn't have undo entry", 1);
extern const int kErrBatchCommandError = RegisterErrorCode(
    252, "Batch command {}", 1);
extern const int kErrInvalidCommandTypeError = RegisterErrorCode(
    253, "Unexpected command type '{}' found in command stack", 1);


}  // namespace core

}  // namespace dino
