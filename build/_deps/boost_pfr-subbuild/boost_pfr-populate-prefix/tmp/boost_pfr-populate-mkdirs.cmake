# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-src")
  file(MAKE_DIRECTORY "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-build"
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix"
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/tmp"
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/src/boost_pfr-populate-stamp"
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/src"
  "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/src/boost_pfr-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/src/boost_pfr-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/Boat-speed-multiplier/Boat-speed-multiplier/build/_deps/boost_pfr-subbuild/boost_pfr-populate-prefix/src/boost_pfr-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
