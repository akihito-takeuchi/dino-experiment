add_library(dino_core SHARED
  dexception.cc
  dexception_code.cc
  session.cc
  dobject.cc
  currentuser.cc
  objectfactory.cc
  commandexecuter.cc
  commandstack.cc
  detail/objectdata.cc
  detail/dataiofactory.cc
  detail/jsondataio.cc
  detail/dexception_code.cc
)

target_link_libraries(dino_core ${Boost_LIBRARIES} fmt)