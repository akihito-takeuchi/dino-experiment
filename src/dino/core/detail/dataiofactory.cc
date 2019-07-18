// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/detail/dataiofactory.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <fmt/format.h>

#include "dino/core/detail/jsondataio.h"
#include "dino/core/detail/dataioexception.h"

namespace dino {

namespace core {

namespace fs = boost::filesystem;

namespace detail {

namespace {

const std::vector<FileFormat> kSupportedFileFormats = {FileFormat::kJson};

}

DataIOFactory::DataIOFactory() = default;

DataIOFactory::~DataIOFactory() = default;

DataIOPtr DataIOFactory::Create(FileFormat file_format) {
  switch (file_format) {
    case FileFormat::kJson:
      return std::make_unique<JsonDataIO>();
    case FileFormat::kUnknown:
    default:
      BOOST_THROW_EXCEPTION(
          DataIOException(kErrUnknownFileFormat)
          << ExpInfo1(static_cast<int>(file_format)));
  }
}

DObjFileInfo DataIOFactory::GetDataFileInfo(const FsPath& path) {
  DObjFileInfo info;
  info = JsonDataIO::GetDataFileInfo(path);
  if (info.IsValid())
    return info;
  return info;
}

std::string DataIOFactory::DataFileName(const std::string& type,
                                        FileFormat file_format) {
  std::string file_name;
  switch (file_format) {
    case FileFormat::kJson:
      file_name = JsonDataIO::FileName(type);
      break;
    case FileFormat::kUnknown:
    default:
      file_name = fmt::format(
          "{}.unknown_{}", type, static_cast<int>(file_format));
      break;
  }
  return file_name;
}

DObjFileInfo DataIOFactory::FindDataFileInfo(const FsPath& path) {
  DObjFileInfo info;
  if (!fs::is_directory(path))
    return info;
  auto dirs = boost::make_iterator_range(
      fs::directory_iterator(path),
      fs::directory_iterator());
  for (auto dir_itr : dirs) {
    info = GetDataFileInfo(dir_itr.path().string());
    if (info.IsValid())
      return info;
  }
  return info;
}

std::once_flag DataIOFactory::init_flag_;
std::unique_ptr<DataIOFactory> DataIOFactory::instance_ = nullptr;

DataIOFactory& DataIOFactory::Instance() {
  std::call_once(init_flag_, Init);
  return *instance_;
}

void DataIOFactory::Init() {
  instance_ = std::unique_ptr<DataIOFactory>(new DataIOFactory());
}

}  // namespace detail

}  // namespace core

}  // namespace dino
