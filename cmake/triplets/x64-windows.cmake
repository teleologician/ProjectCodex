if(DEFINED VCPKG_ROOT_DIR AND NOT VCPKG_ROOT_DIR STREQUAL "")
    include(${VCPKG_ROOT_DIR}/triplets/x64-windows.cmake)
elseif(DEFINED VCPKG_ROOT AND NOT VCPKG_ROOT STREQUAL "")
    include(${VCPKG_ROOT}/triplets/x64-windows.cmake)
else()
    message(FATAL_ERROR "VCPKG_ROOT_DIR is not set; cannot locate base triplet.")
endif()

set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)
