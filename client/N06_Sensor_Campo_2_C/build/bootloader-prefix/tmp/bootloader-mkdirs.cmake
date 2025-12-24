# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/mrclnndrd/esp/esp-idf/components/bootloader/subproject"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/tmp"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/src/bootloader-stamp"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/src"
  "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mrclnndrd/projetos/greense/client/N06_Sensor_Campo_2_C/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
