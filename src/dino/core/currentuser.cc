// Copyright (c) 2018 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "dino/core/currentuser.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dino/core/dexception.h"

namespace dino {

namespace core {

namespace fs = boost::filesystem;

namespace {

std::string GetUserName(uid_t uid) {
  const size_t default_bufsize = 1024;
  passwd pw;
  passwd* result;
  char *buf;
  size_t bufsize = default_bufsize;
  int status;
  std::string user_name;

  while (true) {
    buf = static_cast<char*>(std::malloc(bufsize));
    status = getpwuid_r(uid, &pw, buf, bufsize, &result);
    if (result == nullptr) {
      if (status == ERANGE) {
        std::free(buf);
        bufsize += default_bufsize;
      } else {
        BOOST_THROW_EXCEPTION(
            DException(kErrFailedToGetCurrentUserName));
      }
    } else {
      user_name = pw.pw_name;
      break;
    }
  }
  std::free(buf);
  return user_name;
}

}  // namespace

class CurrentUser::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
  uid_t uid;
  std::string name;
  std::vector<gid_t> gids;
};

#if __APPLE__
#define GID_T int
#else
#define GID_T gid_t
#endif

CurrentUser::CurrentUser() :
    impl_(std::make_unique<CurrentUser::Impl>()) {
  impl_->uid = getuid();
  impl_->name = GetUserName(impl_->uid);

  int ngroups = 32;
  GID_T *groups = reinterpret_cast<GID_T*>(std::malloc(ngroups * sizeof(GID_T)));
  GID_T gid = getgid();
  if (getgrouplist(impl_->name.c_str(), gid, groups, &ngroups) == -1) {
    std::free(groups);
    groups = reinterpret_cast<GID_T*>(std::malloc(ngroups * sizeof(GID_T)));
    getgrouplist(impl_->name.c_str(), gid, groups, &ngroups);
  }
  impl_->gids.insert(impl_->gids.begin(), groups, &groups[ngroups]);
  std::free(groups);
}

CurrentUser::~CurrentUser() = default;

std::string CurrentUser::Name() const {
  return impl_->name;
}

bool CurrentUser::IsWritable(const FsPath& path) const {
  struct stat s;
  FsPath p(path);
  if (!fs::exists(path))
    p = ParentFsPath(path);
  if (stat(path.string().c_str(), &s) != -1) {
    if (s.st_mode & S_IWOTH)
      return true;
    if (impl_->uid == s.st_uid
        && s.st_mode & S_IWUSR)
      return true;
    if (std::find(impl_->gids.cbegin(),
                  impl_->gids.cend(), s.st_gid) != impl_->gids.cend()
        && s.st_mode & S_IWGRP)
      return true;
  }
  return false;
}

CurrentUser& CurrentUser::Instance() {
  std::call_once(init_flag_, Init);
  return *instance_;
}

std::once_flag CurrentUser::init_flag_;
std::unique_ptr<CurrentUser> CurrentUser::instance_ = nullptr;

void CurrentUser::Init() {
  instance_ = std::unique_ptr<CurrentUser>(new CurrentUser);
}

}  // namespace core

}  // namespace dino
