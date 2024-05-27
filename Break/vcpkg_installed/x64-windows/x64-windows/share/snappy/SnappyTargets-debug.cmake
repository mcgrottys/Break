#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Snappy::snappy" for configuration "Debug"
set_property(TARGET Snappy::snappy APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Snappy::snappy PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/snappy.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/snappy.dll"
  )

list(APPEND _cmake_import_check_targets Snappy::snappy )
list(APPEND _cmake_import_check_files_for_Snappy::snappy "${_IMPORT_PREFIX}/debug/lib/snappy.lib" "${_IMPORT_PREFIX}/debug/bin/snappy.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
