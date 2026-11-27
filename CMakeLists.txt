cmake_minimum_required(VERSION 3.16)
project(ubear C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(RL ${CMAKE_SOURCE_DIR}/third_party/raylib/src)
set(RAYLIB_SOURCES
    ${RL}/rcore.c ${RL}/rshapes.c ${RL}/rtextures.c ${RL}/rtext.c
    ${RL}/rmodels.c ${RL}/raudio.c ${RL}/utils.c)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

if(ANDROID)
    # branche Android plus bas
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS_BUILD TRUE)
endif()

if(ANDROID)
    set(GLUE ${ANDROID_NDK}/sources/android/native_app_glue)
    add_library(raylib STATIC ${RAYLIB_SOURCES} ${GLUE}/android_native_app_glue.c)
    target_compile_definitions(raylib PUBLIC PLATFORM_ANDROID GRAPHICS_API_OPENGL_ES2)
    target_include_directories(raylib PUBLIC ${RL} ${GLUE})
    target_compile_options(raylib PRIVATE -w -ffunction-sections -fdata-sections)

    add_library(ubear SHARED ${GAME_SOURCES})
    target_link_libraries(ubear PRIVATE raylib android log EGL GLESv2 OpenSLES m dl)
    target_link_options(ubear PRIVATE -u ANativeActivity_onCreate -Wl,--gc-sections -Wl,-z,max-page-size=16384)
    target_compile_options(ubear PRIVATE -Wall -Wno-format-security -ffunction-sections -fdata-sections)
elseif(IOS_BUILD)
    # OpenGL ES 2 + UIKit. rcore.c inclut rcore_ios.m, donc ce fichier est compilé en Objective-C.
    set_source_files_properties(${RL}/rcore.c PROPERTIES LANGUAGE OBJC COMPILE_FLAGS "-fobjc-arc")
    # miniaudio (dans raudio.c) inclut AVFoundation sur iOS : Objective-C obligatoire.
    set_source_files_properties(${RL}/raudio.c PROPERTIES LANGUAGE OBJC)
    add_library(raylib STATIC ${RAYLIB_SOURCES})
    target_compile_definitions(raylib PUBLIC PLATFORM_IOS GRAPHICS_API_OPENGL_ES2)
    target_include_directories(raylib PUBLIC ${RL})
    target_compile_options(raylib PRIVATE -w)
    # Sans ça, xcodebuild archive range libraylib.a dans l'archive -> "Generic Xcode Archive"
    # au lieu d'une archive d'app iOS, et l'export IPA échoue (method "expected one {}").
    set_target_properties(raylib PROPERTIES XCODE_ATTRIBUTE_SKIP_INSTALL "YES")
    target_link_libraries(raylib PUBLIC
        "-framework UIKit" "-framework Foundation" "-framework QuartzCore"
        "-framework OpenGLES" "-framework CoreGraphics" "-framework CoreVideo"
        "-framework AudioToolbox" "-framework AVFoundation" "-framework CoreAudio"
        "-framework Security" "-framework UserNotifications" "-framework StoreKit" m)

    file(GLOB_RECURSE UBEAR_ASSETS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/*)
    add_executable(ubear MACOSX_BUNDLE ${GAME_SOURCES} ${UBEAR_ASSETS} ${CMAKE_SOURCE_DIR}/ios/Assets.xcassets)
    foreach(asset ${UBEAR_ASSETS})
        file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR}/assets ${asset})
        get_filename_component(asset_dir ${rel} DIRECTORY)
        set_source_files_properties(${asset} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${asset_dir}")
    endforeach()
    # Catalog d'icônes : doit être dans la phase Resources pour être compilé par actool.
    set_source_files_properties(${CMAKE_SOURCE_DIR}/ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_link_libraries(ubear PRIVATE raylib)
    target_compile_definitions(ubear PRIVATE PLATFORM_IOS)
    target_compile_options(ubear PRIVATE -Wall -Wno-format-security)
    set_target_properties(ubear PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/ios/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "Ubear"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ubear.game"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ubear.game"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "iphoneos iphonesimulator"
        XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
else()
    add_library(raylib STATIC ${RAYLIB_SOURCES} ${RL}/rglfw.c)
    target_compile_definitions(raylib PUBLIC PLATFORM_DESKTOP GRAPHICS_API_OPENGL_33)
    target_include_directories(raylib PUBLIC ${RL} ${RL}/external/glfw/include)
    target_compile_options(raylib PRIVATE -w)
    if(WIN32)
        target_link_libraries(raylib PUBLIC opengl32 gdi32 winmm)
    else()
        target_link_libraries(raylib PUBLIC GL m pthread dl)
    endif()

    add_executable(ubear ${GAME_SOURCES})
    target_link_libraries(ubear PRIVATE raylib)
    target_compile_options(ubear PRIVATE -Wall -Wno-format-security)
endif()
