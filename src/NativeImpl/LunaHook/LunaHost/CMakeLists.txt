
include(libuchardet.cmake)


add_library(host host.cpp textthread.cpp ../include/InfoStrings.cpp)
target_compile_definitions(host PRIVATE -DBUILD_HOST=1)
target_precompile_headers(host REUSE_FROM pch)
target_include_directories(host PUBLIC .)
target_link_libraries(host PRIVATE VERSION_DEF libuchardet)

add_library(LunaHostDll MODULE LunaHostDll.cpp ${versioninfo})
target_precompile_headers(LunaHostDll REUSE_FROM pch)
set_target_properties(LunaHostDll PROPERTIES OUTPUT_NAME "LunaHost${bitappendix}")
target_link_libraries(LunaHostDll pch host ${YY_Thunks})

if(BUILD_CLI)
  add_executable(LunaHostCLI LunaHostCLI.cpp injector.cpp ${versioninfo})
  target_precompile_headers(LunaHostCLI REUSE_FROM pch)
  set_target_properties(LunaHostCLI PROPERTIES OUTPUT_NAME "LunaHostCLI${bitappendix}")
  target_link_libraries(LunaHostCLI pch host ${YY_Thunks})
  option(BUILD_GSM_SMOKE_TARGET "Build the isolated GSM integration test process" OFF)
  if(BUILD_GSM_SMOKE_TARGET)
    add_executable(GsmHookSmokeTarget GsmHookSmokeTarget.cpp)
  endif()
endif()

add_subdirectory(GUI)
