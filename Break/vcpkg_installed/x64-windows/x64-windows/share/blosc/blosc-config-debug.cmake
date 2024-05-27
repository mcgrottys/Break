#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "blosc_shared" for configuration "Debug"
set_property(TARGET blosc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(blosc_shared PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/blosc.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/blosc.dll"
  )

list(APPEND _cmake_import_check_targets blosc_shared )
list(APPEND _cmake_import_check_files_for_blosc_shared "${_IMPORT_PREFIX}/debug/lib/blosc.lib" "${_IMPORT_PREFIX}/debug/bin/blosc.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
