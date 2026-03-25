cmake_minimum_required(VERSION 3.25)

# UI service unit tests
# Pure C++ tests (no Qt runtime needed): VDF parser, tar helpers, CDN config
# Qt-dependent tests: version comparison, path security

# Support both standalone and subdirectory builds
if(NOT DEFINED MAKINE_PROJECT_ROOT)
    set(MAKINE_PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
endif()

find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)

enable_testing()

# Pure C++ tests (header-only, no vcpkg)
add_executable(test_vdfparser test_vdfparser.cpp)
target_include_directories(test_vdfparser PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_vdfparser PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME test_vdfparser COMMAND test_vdfparser)

# Qt-dependent tests
add_executable(test_updateservice test_updateservice.cpp)
target_include_directories(test_updateservice PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_updateservice PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_updateservice COMMAND test_updateservice)

add_executable(test_pathsecurity test_pathsecurity.cpp)
target_include_directories(test_pathsecurity PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_pathsecurity PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_pathsecurity COMMAND test_pathsecurity)

add_executable(test_cdnconfig test_cdnconfig.cpp)
target_include_directories(test_cdnconfig PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_cdnconfig PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME test_cdnconfig COMMAND test_cdnconfig)

# CatalogStore tests (Qt-dependent, uses core path_utils.hpp header-only)
add_executable(test_catalogstore test_catalogstore.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/catalogstore.cpp)
target_include_directories(test_catalogstore PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services
    ${MAKINE_PROJECT_ROOT}/core/include)
target_link_libraries(test_catalogstore PRIVATE GTest::gtest Qt6::Core)
add_test(NAME test_catalogstore COMMAND test_catalogstore)
