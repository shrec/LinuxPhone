cmake_minimum_required(VERSION 3.22)
project(revive-shell LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Test)
qt_standard_project_setup(REQUIRES 6.5)

# Pure logic, no GUI: unit-tested on its own.
qt_add_library(revive_core STATIC src/desktopentry.cpp src/desktopentry.h)
target_include_directories(revive_core PUBLIC src)
target_link_libraries(revive_core PUBLIC Qt6::Core)

enable_testing()
qt_add_executable(tst_desktopentry tests/tst_desktopentry.cpp)
target_link_libraries(tst_desktopentry PRIVATE revive_core Qt6::Test)
add_test(NAME desktopentry COMMAND tst_desktopentry)

if(UNIX AND NOT APPLE)
qt_add_executable(revive-shell src/main.cpp src/appmodel.cpp src/appmodel.h src/iconprovider.h)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(revive-shell
    URI Revive.Shell
    VERSION 1.0
    QML_FILES qml/Main.qml qml/Theme.qml qml/StatusBar.qml qml/Launcher.qml)
target_link_libraries(revive-shell PRIVATE revive_core Qt6::Gui Qt6::Qml Qt6::Quick)
install(TARGETS revive-shell RUNTIME DESTINATION bin)
endif()

if(WIN32)
  get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  # Some Windows machines carry other MinGW runtime DLLs in System32, which Windows loads before
  # PATH. Put the matching runtime and Qt DLLs next to the test exe, which is searched first.
  add_custom_command(TARGET tst_desktopentry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:Qt6::Core> $<TARGET_FILE:Qt6::Test>
      ${_cxx_bin}/libstdc++-6.dll ${_cxx_bin}/libgcc_s_seh-1.dll ${_cxx_bin}/libwinpthread-1.dll
      $<TARGET_FILE_DIR:tst_desktopentry>)
endif()
