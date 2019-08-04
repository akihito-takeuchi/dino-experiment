// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include <QApplication>

#include <iostream>
#include <boost/filesystem.hpp>

#include <dino/core/session.h>

#include "dinotable.h"
#include "mainwindow.h"

namespace fs = boost::filesystem;

int main(int argc, char* argv[]) {
  DinoTable::Init();
  auto session = dino::core::Session::Create();
  dino::core::DObjectSp data;
  if (argc > 1) {
    fs::path file_path(argv[1]);
    auto name = file_path.filename().string();
    if (fs::exists(argv[1])) {
      session->OpenTopLevelObject(file_path, name);
    } else {
      session->CreateTopLevelObject(name, kDinoTableType);
      session->InitTopLevelObjectPath(name, file_path);
    }
    data = session->GetObject(name);
  } else {
    std::cerr << "Specify directory\n";
    return -1;
  }
  QApplication app(argc, argv);
  MainWindow w(data);
  w.show();
  return app.exec();
}
