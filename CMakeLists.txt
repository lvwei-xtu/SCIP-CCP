cmake_minimum_required(VERSION 3.3)

if(POLICY CMP0074)
      cmake_policy(SET CMP0074 NEW)
endif()

project(CCP)
include(CTest)

if(TARGET SCIP::SCIP)
  # find package by SCIP PATH
  find_package(SCIP CONFIG PATHS ${SCIP_BINARY_DIR} REQUIRED)
else()
  find_package(SCIP REQUIRED)
endif()

include_directories(${SCIP_INCLUDE_DIRS})

add_executable(ccp
   src/graph.c
   src/queue.c
   src/processdata.c
   src/cmain.c
   src/reader_ccrp.c
   src/reader_ccmpp.c
   src/reader_ccls.c
   src/chancecons_ccp.c
)

target_link_libraries(ccp ${SCIP_LIBRARIES})

if( TARGET examples )
   add_dependencies( examples ccp )
endif()

enable_testing()
