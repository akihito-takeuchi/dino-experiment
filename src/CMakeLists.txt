add_library(dino_core SHARED
  dino/core/dexception.cc
  dino/core/dexception_code.cc
  dino/core/dobjpath.cc
  dino/core/dobjinfo.cc
  dino/core/session.cc
  dino/core/dobject.cc
  dino/core/currentuser.cc
  dino/core/objectfactory.cc
  dino/core/commandexecuter.cc
  dino/core/commandstack.cc
  dino/core/detail/objectdata.cc
  dino/core/detail/dataiofactory.cc
  dino/core/detail/jsondataio.cc
  dino/core/detail/dexception_code.cc
  )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

target_link_libraries(dino_core PUBLIC fmt ${Boost_LIBRARIES})

install(
  TARGETS dino_core
  EXPORT dino_core_export
  DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(enable-qt)
  add_library(dino_qt
    dino/qt/columninfo.cc
    dino/qt/dobjecttablemodel.cc
    dino/qt/dobjecttreemodel.cc
    )

  target_link_libraries(dino_qt dino_core)

  qt5_use_modules(dino_qt Widgets)

  install(
    TARGETS dino_qt
    EXPORT dino_qt_export
    DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif(enable-qt)
