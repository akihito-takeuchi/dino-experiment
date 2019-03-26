add_library(dino_qt
  dinotablemodel.cc
  )

target_link_libraries(dino_qt dino_core)

qt5_use_modules(dino_qt Widgets)
