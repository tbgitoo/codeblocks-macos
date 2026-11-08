cmake_minimum_required(VERSION 3.16)
project(OgreNextWindowTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT OGRE_SDK)
    message(FATAL_ERROR "Pass -DOGRE_SDK= to the installed OGRE-Next SDK")
endif()

add_executable(window-test Tutorial00_Basic.cpp)

target_include_directories(window-test PRIVATE
    "${OGRE_SDK}/include/OGRE"
    "${OGRE_SDK}/include/OGRE/Hlms/Common"
    "${OGRE_SDK}/include/OGRE/Hlms/Unlit"
    "${OGRE_SDK}/include/OGRE/Hlms/Pbs"
)

target_link_directories(window-test PRIVATE
    "${OGRE_SDK}/lib/Release"
)

target_link_libraries(window-test PRIVATE
    OgreHlmsPbs
    OgreHlmsUnlit
    OgreMain
)
