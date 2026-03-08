# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/fangit_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/fangit_autogen.dir/ParseCache.txt"
  "fangit_autogen"
  )
endif()
