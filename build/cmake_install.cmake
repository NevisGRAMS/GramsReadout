# Install script for directory: /home/pgrams/pGRAMSBalloon/GramsReadout

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/quill/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/lib/pcie_driver/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/networking/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/PGramsCommCodec/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/ReadoutDataMonitor/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/bin/daq_orchestrator")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/bin" TYPE EXECUTABLE FILES "/home/pgrams/pGRAMSBalloon/GramsReadout/build/daq_orchestrator")
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator"
         OLD_RPATH "/home/pgrams/WD1630LNX86_64/lib:/home/pgrams/tof_readout/dev/grams-tof-library/00build/00install/lib:/home/pgrams/pGRAMSBalloon/GramsReadout/build/PGramsCommCodec:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/bin/daq_orchestrator")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/CMakeFiles/daq_orchestrator.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/orchestrator" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/orchestrator")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/bin/orchestrator"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/bin/orchestrator")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/bin" TYPE EXECUTABLE FILES "/home/pgrams/pGRAMSBalloon/GramsReadout/build/orchestrator")
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/orchestrator" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/orchestrator")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/bin/orchestrator"
         OLD_RPATH "/home/pgrams/WD1630LNX86_64/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/bin/orchestrator")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/CMakeFiles/orchestrator.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/tpc_daq" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/tpc_daq")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/bin/tpc_daq"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/bin/tpc_daq")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/bin" TYPE EXECUTABLE FILES "/home/pgrams/pGRAMSBalloon/GramsReadout/build/tpc_daq")
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/tpc_daq" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/tpc_daq")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/bin/tpc_daq"
         OLD_RPATH "/home/pgrams/WD1630LNX86_64/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/bin/tpc_daq")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/CMakeFiles/tpc_daq.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/data_monitor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/data_monitor")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/bin/data_monitor"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/bin/data_monitor")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/bin" TYPE EXECUTABLE FILES "/home/pgrams/pGRAMSBalloon/GramsReadout/build/data_monitor")
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/data_monitor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/data_monitor")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/bin/data_monitor"
         OLD_RPATH "/home/pgrams/WD1630LNX86_64/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/bin/data_monitor")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/CMakeFiles/data_monitor.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/tof_daq" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/tof_daq")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/bin/tof_daq"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/bin/tof_daq")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/bin" TYPE EXECUTABLE FILES "/home/pgrams/pGRAMSBalloon/GramsReadout/build/tof_daq")
  if(EXISTS "$ENV{DESTDIR}/usr/local/bin/tof_daq" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/bin/tof_daq")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/bin/tof_daq"
         OLD_RPATH "/home/pgrams/WD1630LNX86_64/lib:/home/pgrams/tof_readout/dev/grams-tof-library/00build/00install/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/bin/tof_daq")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/pgrams/pGRAMSBalloon/GramsReadout/build/CMakeFiles/tof_daq.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/pgrams/pGRAMSBalloon/GramsReadout/build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/pgrams/pGRAMSBalloon/GramsReadout/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
