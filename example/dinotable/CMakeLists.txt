add_executable(dinotable
  main.cc
  dinotable.cc
  mainwindow.cc
  )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

target_link_libraries(dinotable dino_core dino_qt)
qt5_use_modules(dinotable Widgets)
