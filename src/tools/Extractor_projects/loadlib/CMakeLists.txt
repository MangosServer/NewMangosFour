# This code is part of MaNGOS. Contributor & Copyright details are in AUTHORS/THANKS.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# mangos's own ADT/WDT/MPQ client-format reader. Lives with the extractors that
# use it (not in dep/, which is third-party libraries).

add_library(loadlib STATIC
  loadlib.cpp
  adt.cpp
  wdt.cpp
  mpq.cpp)

target_include_directories(loadlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(loadlib PUBLIC stormlib)
