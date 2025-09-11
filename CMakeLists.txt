cmake_minimum_required(VERSION 3.16)
project(iDescriptor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(/usr/local/include)
include_directories(/usr/include/python3.13)
set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg SvgWidgets Multimedia Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg SvgWidgets Multimedia Network)

# Link directly to libraries in /usr/local/lib instead of using pkg-config
# Force NO_DEFAULT_PATH to only search in /usr/local/lib
find_library(IMOBILEDEVICE_LIBRARY 
    NAMES imobiledevice-1.0
    PATHS /usr/local/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(IMOBILEDEVICE_GLUE_LIBRARY 
    NAMES imobiledevice-glue-1.0
    PATHS /usr/local/lib
    NO_DEFAULT_PATH
    REQUIRED
)

# find_library(PLIST_LIBRARY 
#     NAMES plist-2.0
#     PATHS /usr/local/lib
#     NO_DEFAULT_PATH
#     REQUIRED
# )

find_library(TATSU_LIBRARY 
    NAMES tatsu
    PATHS /usr/local/lib
    NO_DEFAULT_PATH
    REQUIRED
)

# Add QR code generation library
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)

find_library(IRECOVERY_LIBRARY 
    NAMES irecovery-1.0
    PATHS /usr/local/lib
    NO_DEFAULT_PATH
    REQUIRED
)

# # Add missing libraries
# find_library(USBMUXD_LIBRARY 
#     NAMES usbmuxd-2.0
#     PATHS /usr/local/lib
#     NO_DEFAULT_PATH
#     REQUIRED
# )

find_library(SSL_LIBRARY 
    NAMES ssl
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
    REQUIRED
)

find_library(CRYPTO_LIBRARY 
    NAMES crypto
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
    REQUIRED
)

find_library(FRIDA_LIBRARY 
    NAMES frida-core
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
    REQUIRED
)

find_library(SSH_LIBRARY 
    NAMES ssh
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu
    REQUIRED
)

find_library(ZIP_LIBRARY 
    NAMES zip
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu
    REQUIRED
)


pkg_check_modules(PUGIXML REQUIRED IMPORTED_TARGET pugixml)
pkg_check_modules(USB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(PLIST REQUIRED IMPORTED_TARGET libplist-2.0)
# Use system plist library instead of manually built one - exclude /usr/local/lib
find_library(PLIST_LIBRARY 
    NAMES plist-2.0
    PATHS /usr/lib
    NO_DEFAULT_PATH
    REQUIRED
)

# set(PROJECT_SOURCES
#    src/*.cpp
#    src/*.h
#    src/*.ui
#    resources.qrc
# )

file(GLOB PROJECT_SOURCES
    src/*.cpp
    src/core/helpers/*.cpp
    src/core/services/*.cpp
    src/*.h
    src/*.ui
    resources.qrc
)

add_subdirectory(src/core/airplay)
add_subdirectory(src/lib/ipatool-go)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS DBus)
find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Location)
find_package(Qt6 REQUIRED COMPONENTS Positioning)
find_package(Qt6 REQUIRED COMPONENTS QuickWidgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)
find_package(Qt6 REQUIRED COMPONENTS MultimediaWidgets)


    qt_add_executable(iDescriptor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/core/helpers/warn.cpp
    )
    # Define target properties for Android with Qt 6 as:
    # set_property(TARGET iDescriptor APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    # ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(iDescriptor SHARED
            ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        # set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(iDescriptor
            ${PROJECT_SOURCES}
        )
    endif()
endif()


target_link_libraries(iDescriptor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets 
    Qt${QT_VERSION_MAJOR}::Svg 
    Qt${QT_VERSION_MAJOR}::SvgWidgets 
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Location
    Qt${QT_VERSION_MAJOR}::Positioning
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    ${IMOBILEDEVICE_LIBRARY}
    ${IMOBILEDEVICE_GLUE_LIBRARY}
    # ${PLIST_LIBRARY}
    ${TATSU_LIBRARY}
    ${IRECOVERY_LIBRARY}
    ${SSL_LIBRARY}
    ${CRYPTO_LIBRARY}
    ${FRIDA_LIBRARY}
    ${SSH_LIBRARY}
    ${ZIP_LIBRARY}
    PkgConfig::PUGIXML
    PkgConfig::USB
    PkgConfig::PLIST
    PkgConfig::QRENCODE
    airplay
    ipatool-go
)

# Add compile definition for source directory
target_compile_definitions(iDescriptor PRIVATE 
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.iDescriptor)
endif()

set_target_properties(iDescriptor PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    # Control library search order - system libs first, then /usr/local/lib
    INSTALL_RPATH "/usr/local/lib:/usr/lib/x86_64-linux-gnu:/usr/lib"
    # INSTALL_RPATH "/usr/local/lib:/usr/lib/"
    BUILD_WITH_INSTALL_RPATH TRUE
)

include(GNUInstallDirs)
install(TARGETS iDescriptor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(iDescriptor)
endif()
