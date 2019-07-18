// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/detail/jsondataio.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "dino/core/dexception.h"
#include "dino/core/dobjpath.h"
#include "dino/core/detail/jsonwritersupport.h"

#include "rapidjson/reader.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

namespace dino {

namespace core {

namespace detail {

namespace {

const size_t kDataFileIOBufSize = 65536;
const std::string kJsonFileSuffix = "json";

const std::string kDataSectionName = "data";
const std::string kAttributeSectionName = "attr";
const std::string kChildrenSectionName = "children";

}

namespace fs = boost::filesystem;
using namespace rapidjson;

class ReadData : public BaseReaderHandler<UTF8<>, ReadData> {
 public:
  ReadData(const ReadDataArgPtr& arg) {
    PushNewStack(arg);
  }
  template<typename T>
  bool StoreValue(T value) {
    if (cxt->current_array_stack.empty())
      (*(cxt->current_dict))[cxt->current_key] = value;
    else
      cxt->current_array_stack.back()->emplace_back(value);
    return true;
  }
  bool Null() {
    return StoreValue(nil);
  }
  bool Bool(bool v) {
    return StoreValue(v);
  }
  bool Int(int v) {
    return StoreValue(v);
  }
  bool Uint(unsigned v) {
    return StoreValue(static_cast<int>(v));
  }
  bool Int64(int64_t v) {
    return StoreValue(static_cast<int>(v));
  }
  bool Double(double v) {
    return StoreValue(v);
  }
  bool String(const char* str, SizeType length, bool) {
    return StoreValue(std::string(str, str + length));
  }
  bool Key(const char* str, SizeType length, bool) {
    std::string key(str, str + length);
    if (cxt->reading_dict) {
      cxt->current_key = key;
    } else if (cxt->in_child_section) {
      auto arg = cxt->arg->create_child(DObjInfo::FromString(key));
      PushNewStack(arg);
    } else if (key == kDataSectionName) {
      cxt->current_dict = cxt->arg->values;
    } else if (key == kAttributeSectionName) {
      cxt->current_dict = cxt->arg->attrs;
    } else if (key == kChildrenSectionName) {
      cxt->in_child_section = true;
    }
    return true;
  }
  bool StartObject() {
    if (cxt->current_dict)
      cxt->reading_dict = true;
    return true;
  }
  bool EndObject(SizeType) {
    if (cxt->reading_dict) {
      cxt->reading_dict = false;
      cxt->current_dict = nullptr;
    } else if (cxt->in_child_section) {
      cxt->in_child_section = false;
    } else {
      PopStack();
    }
    return true;
  }
  bool StartArray() {
    DValueArray* new_array;
    if (cxt->current_array_stack.empty()) {
      (*(cxt->current_dict))[cxt->current_key] = DValueArray();
      new_array = &boost::get<DValueArray&>(
          (*(cxt->current_dict))[cxt->current_key]);
    } else {
      cxt->current_array_stack.back()->push_back(DValueArray());
      new_array = cxt->current_array_stack.back();
    }
    cxt->current_array_stack.push_back(new_array);
    return true;
  }
  bool EndArray(SizeType) {
    cxt->current_array_stack.pop_back();
    return true;
  }
  void PushNewStack(const ReadDataArgPtr& arg) {
    cxt_stack.emplace_back(ReadContext(arg));
    cxt = &(cxt_stack.back());
  }
  void PopStack() {
    if (cxt_stack.size() == 0)
      BOOST_THROW_EXCEPTION(
          JsonException(kErrJsonInvalidFileReadState));
    cxt_stack.pop_back();
    cxt = &(cxt_stack.back());
  }

  struct ReadContext {
    ReadContext() = default;
    ReadContext(const ReadDataArgPtr& arg) : arg(arg) {}
    bool reading_dict = false;
    bool reading_array = false;
    bool in_child_section = false;
    std::string current_key;
    DValueDict* current_dict = nullptr;
    std::vector<DValueArray*> current_array_stack;
    ReadDataArgPtr arg;
  };
  std::vector<ReadContext> cxt_stack;
  ReadContext* cxt;
};

class JsonDataIO::Impl {
 public:
  Impl(const std::string& file_path) : file_path(file_path) {}
  ~Impl() = default;
  void Init() {
    working_path = file_path + ".writing";
    f = std::fopen(working_path.c_str(), "wb");
    std::FILE* test_f = std::fopen(file_path.c_str(), "ab");
    if (!f || !test_f) {
      if (test_f)
        std::fclose(test_f);
      if (f)
        std::fclose(f);
      BOOST_THROW_EXCEPTION(
          JsonException(kErrJsonFileOpen)
          << ExpInfo1(file_path) << ExpInfo2("writing"));
    }
    std::fclose(test_f);
    fs = std::make_unique<FileWriteStream>(f, buf, sizeof(buf));
    writer = std::make_unique<PrettyWriter<FileWriteStream>>(*fs);
    writer->SetIndent(' ', 2);
    data_writer = std::make_unique<
      WriteData<PrettyWriter<FileWriteStream>>>(*writer);
  }
  std::string file_path;
  std::string working_path;
  std::FILE* f;
  char buf[kDataFileIOBufSize];
  std::vector<std::string> current_path;
  std::unique_ptr<FileWriteStream> fs;
  std::unique_ptr<PrettyWriter<FileWriteStream>> writer;
  std::unique_ptr<WriteData<PrettyWriter<FileWriteStream>>> data_writer;
};

JsonDataIO::JsonDataIO() = default;

JsonDataIO::~JsonDataIO() = default;

void JsonDataIO::OpenForWrite(const FsPath& file_path) {
  impl_ = std::make_unique<JsonDataIO::Impl>(file_path.string());
  impl_->Init();
  impl_->writer->StartObject();
}

void JsonDataIO::ToDataSection() {
  ToSection(kDataSectionName);
}

void JsonDataIO::ToAttributeSection() {
  ToSection(kAttributeSectionName);
}

void JsonDataIO::ToChildrenSection() {
  ToSection(kChildrenSectionName);
}

void JsonDataIO::ToSection(const std::string& section_name) {
  if (!DObjPath::IsValidName(section_name))
    BOOST_THROW_EXCEPTION(
        JsonException(kErrJsonInvalidSectionName) << ExpInfo1(section_name));
  impl_->writer->Key(section_name.c_str());
  impl_->writer->StartObject();
  impl_->current_path.emplace_back(section_name);
}

void JsonDataIO::ToSection(const DObjInfo& obj_info) {
  auto section_name = obj_info.ToString(true);
  impl_->writer->Key(section_name.c_str());
  impl_->writer->StartObject();
  impl_->current_path.emplace_back(section_name);
}

void JsonDataIO::ToSectionUp() {
  if (impl_->current_path.size() == 0)
    BOOST_THROW_EXCEPTION(
        JsonException(kErrJsonFailedToChangeSection) << ExpInfo1(".."));
  impl_->writer->EndObject();
  impl_->current_path.pop_back();
}

void JsonDataIO::WriteDict(const DValueDict& values) {
  for (auto& kv : values) {
    impl_->writer->Key(kv.first.c_str());
    WriteValue(kv.second);
  }
}

void JsonDataIO::WriteValue(const DValue& value) {
  if (IsArrayValue(value)) {
    impl_->writer->StartArray();
    for (auto& v : boost::get<DValueArray>(value))
      WriteValue(v);
    impl_->writer->EndArray();
  } else {
    boost::apply_visitor(*(impl_->data_writer), value);
  }
}

void JsonDataIO::CloseForWrite() {
  for (auto v : impl_->current_path)
    impl_->writer->EndObject();
  impl_->writer->EndObject();
  std::fclose(impl_->f);
  fs::rename(impl_->working_path, impl_->file_path);
}

void JsonDataIO::Load(const FsPath& file_path,
                      const ReadDataArgPtr& arg) {
  ReadData data_reader(arg);
  Reader reader;
  std::FILE* f = std::fopen(file_path.string().c_str(), "rb");
  if (!f)
      BOOST_THROW_EXCEPTION(
          JsonException(kErrJsonFileOpen)
          << ExpInfo1(file_path) << ExpInfo2("writing"));
  char buf[kDataFileIOBufSize];
  FileReadStream fs(f, buf, sizeof(buf));
  reader.Parse(fs, data_reader);
  std::fclose(f);
}

std::string JsonDataIO::FileName(const std::string& type) {
  return type + "." + kJsonFileSuffix;
}

DObjFileInfo JsonDataIO::GetDataFileInfo(const FsPath& path) {
  if (fs::is_directory(path))
    return DObjFileInfo();
  std::string file_name = path.filename().string();
  std::string name = ParentFsPath(path).filename().string();
  std::vector<std::string> elems;
  boost::split(elems, file_name, boost::is_any_of("."));
  if (elems.back() != kJsonFileSuffix)
    return DObjFileInfo();
  return DObjFileInfo(elems[0], path, FileFormat::kJson);
}

}  // namespace detail

}  // namespace core

}  // namespace dino
