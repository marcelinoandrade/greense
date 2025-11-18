# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/greense/esp/esp-idf/components/bootloader/subproject"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/tmp"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/src/bootloader-stamp"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/src"
  "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/greense/projetoGreense/client/N06_Sensor_Campo_C++/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
