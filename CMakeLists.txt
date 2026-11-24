cmake_minimum_required(VERSION 3.21)

project(ProjectManga VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# ---------------------------------------------------------------- the backend (no Qt)
# Everything under backend/ is plain C++20. It is built as its own library and does not link to Qt,
# so a Qt include in there fails to compile.
file(GLOB_RECURSE BACKEND_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/backend/*.cpp")

add_library(mangacore STATIC ${BACKEND_SOURCES})
target_include_directories(mangacore PUBLIC "${CMAKE_CURRENT_LIST_DIR}/backend")
target_compile_options(mangacore PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------- the app
find_package(Qt6 REQUIRED COMPONENTS Quick Network ShaderTools)
qt_standard_project_setup(REQUIRES 6.10)

# wrapper/  : QObject classes that connect the backend to QML (any subfolders)
# frontend/ : the QML files (any subfolders). File names must start with a capital letter and be
#             unique across all subfolders.
file(GLOB_RECURSE WRAPPER_SOURCES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/wrapper/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/wrapper/*.h")

file(GLOB_RECURSE FRONTEND_QML CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/frontend/*.qml")

set_source_files_properties(frontend/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(appProjectManga
    main.cpp
)

qt_add_qml_module(appProjectManga
    URI ProjectManga
    QML_FILES
        Main.qml
        ${FRONTEND_QML}
    SOURCES
        ${WRAPPER_SOURCES}
)

# Qt's generated registration code includes the wrapper headers by bare file name,
# so every folder under wrapper/ has to be on the include path.
set(WRAPPER_DIRS "")
foreach(f ${WRAPPER_SOURCES})
    get_filename_component(d "${CMAKE_CURRENT_LIST_DIR}/${f}" DIRECTORY)
    list(APPEND WRAPPER_DIRS "${d}")
endforeach()
list(REMOVE_DUPLICATES WRAPPER_DIRS)

target_include_directories(appProjectManga PRIVATE "${CMAKE_CURRENT_LIST_DIR}" ${WRAPPER_DIRS})
target_compile_definitions(appProjectManga PRIVATE PROJECTMANGA_VERSION="${PROJECT_VERSION}")
target_compile_options(appProjectManga PRIVATE -Wall -Wextra)
target_link_libraries(appProjectManga PRIVATE Qt6::Quick Qt6::Network mangacore)

# Shaders are compiled at build time; the app loads them as <name>.qsb next to their QML.
qt_add_shaders(appProjectManga "reader_shaders" PREFIX "/qt/qml/ProjectManga"
               FILES frontend/readerView/pageCurl.frag)

# The sync server: its address ("https://host") and secret key, given at configure time
# (-DPROJECTMANGA_SYNC_SERVER=... -DPROJECTMANGA_SYNC_KEY=...) so neither is in the source.
# Without them the app runs with sync off.
set(PROJECTMANGA_SYNC_SERVER "" CACHE STRING "Sync server address")
set(PROJECTMANGA_SYNC_KEY "" CACHE STRING "Sync server secret key")
if(PROJECTMANGA_SYNC_SERVER AND PROJECTMANGA_SYNC_KEY)
    target_compile_definitions(appProjectManga PRIVATE
        PROJECTMANGA_SYNC_SERVER="${PROJECTMANGA_SYNC_SERVER}"
        PROJECTMANGA_SYNC_KEY="${PROJECTMANGA_SYNC_KEY}")
endif()

# The window icon, from the app's resources (":/icons/icon.svg").
qt_add_resources(appProjectManga app_icon PREFIX "/icons" BASE "${CMAKE_CURRENT_LIST_DIR}/resources/icons"
                 FILES resources/icons/icon.svg)

# Qt's HTTPS on Android needs OpenSSL inside the APK (built for arm64 only).
if(ANDROID)
    set(OPENSSL_ANDROID_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/openssl/${ANDROID_ABI}")
    set_property(TARGET appProjectManga APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${OPENSSL_ANDROID_DIR}/libcrypto_3.so" "${OPENSSL_ANDROID_DIR}/libssl_3.so")
    set_target_properties(appProjectManga PROPERTIES
        QT_ANDROID_PACKAGE_NAME io.github.chandandasgit.projectmanga
        QT_ANDROID_APP_NAME ProjectManga
        # Android only updates the app to a higher version code: raise it with every new version.
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/packaging/android"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher")
    # Wi-Fi sync: many phones drop broadcasts unless the app holds a multicast lock.
    qt_add_android_permission(appProjectManga NAME android.permission.CHANGE_WIFI_MULTICAST_STATE)
endif()

# ---------------------------------------------------------------- the tests
# tests/abc/abc_test.cpp is found by itself, and ctest gets one entry named abc.
enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tests/*_test.cpp")

add_executable(backendtests tests/backendtests.cpp ${TEST_SOURCES})
target_include_directories(backendtests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tests")
target_compile_options(backendtests PRIVATE -Wall -Wextra)
target_link_libraries(backendtests PRIVATE mangacore)

foreach(file ${TEST_SOURCES})
    get_filename_component(stem "${file}" NAME_WE)
    string(REGEX REPLACE "_test$" "" suite "${stem}")
    add_test(NAME ${suite} COMMAND backendtests ${suite})
endforeach()

# The wrappers on Qt's network and event loop, against a local test server (no internet).
file(GLOB_RECURSE WRAPPER_CPP CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/wrapper/*.cpp")
add_executable(wrappertests tests/wrapper/views_qtcheck.cpp ${WRAPPER_CPP})
target_include_directories(wrappertests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/tests" ${WRAPPER_DIRS})
target_compile_options(wrappertests PRIVATE -Wall -Wextra)
target_link_libraries(wrappertests PRIVATE Qt6::Qml Qt6::Network mangacore)
foreach(check qtlibrary qtdetails qtreader qtimages qtnovels qtdiscover)
    add_test(NAME ${check} COMMAND wrappertests ${check})
endforeach()
