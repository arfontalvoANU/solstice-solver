# Copyright (C) CNRS 2016-2017
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.8)
project(solstice-solver C)
enable_testing()

set(SSOL_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../src)
option(NO_TEST "Do not build tests" OFF)

################################################################################
# Check dependencies
################################################################################
find_package(RCMake 0.3 REQUIRED)
find_package(RSys 0.4 REQUIRED)
find_package(Star3D 0.4.1 REQUIRED)
find_package(Star3DUT 0.2 REQUIRED)
find_package(StarCPR 0.1 REQUIRED)
find_package(StarSF 0.1 REQUIRED)
find_package(StarSP 0.4 REQUIRED)
find_package(OpenMP 1.2 REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RCMAKE_SOURCE_DIR})
include(rcmake)
include(rcmake_runtime)

include_directories(
  ${RSys_INCLUDE_DIR}
  ${Star3D_INCLUDE_DIR}
  ${Star3DUT_INCLUDE_DIR}
  ${StarCPR_INCLUDE_DIR}
  ${StarSF_INCLUDE_DIR}
  ${StarSP_INCLUDE_DIR})

rcmake_append_runtime_dirs(_runtime_dirs RSys Star3D Star3DUT StarCPR StarSF StarSP)

################################################################################
# Configure and define targets
################################################################################
set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 1)
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(SSOL_FILES_SRC
  ssol_atmosphere.c
  ssol_camera.c
  ssol_data.c
  ssol_device.c
  ssol_draw.c
  ssol_draw_pt.c
  ssol_draw_draft.c
  ssol_estimator.c
  ssol_image.c
  ssol_material.c
  ssol_mc_receiver.c
  ssol_object.c
  ssol_instance.c
  ssol_param_buffer.c
  ssol_ranst_sun_dir.c
  ssol_ranst_sun_wl.c
  ssol_scene.c
  ssol_shape.c
  ssol_spectrum.c
  ssol_solver.c
  ssol_sun.c)

set(SSOL_FILES_INC_API
  ssol.h)

set(SSOL_FILES_INC
  ssol_atmosphere_c.h
  ssol_c.h
  ssol_camera.h
  ssol_device_c.h
  ssol_draw.h
  ssol_estimator_c.h
  ssol_image_c.h
  ssol_material_c.h
  ssol_object_c.h
  ssol_instance_c.h
  ssol_ranst_sun_dir.h
  ssol_ranst_sun_wl.h
  ssol_scene_c.h
  ssol_shape_c.h
  ssol_spectrum_c.h
  ssol_sun_c.h)

set(SSOL_FILES_DOC COPYING README.md)

# Prepend each file in the `SSOL_FILES_<SRC|INC>' list by `SSOL_SOURCE_DIR'
rcmake_prepend_path(SSOL_FILES_SRC ${SSOL_SOURCE_DIR})
rcmake_prepend_path(SSOL_FILES_INC ${SSOL_SOURCE_DIR})
rcmake_prepend_path(SSOL_FILES_INC_API ${SSOL_SOURCE_DIR})
rcmake_prepend_path(SSOL_FILES_DOC ${PROJECT_SOURCE_DIR}/../)

add_library(solstice-solver SHARED ${SSOL_FILES_SRC} ${SSOL_FILES_INC} ${SSOL_FILES_INC_API})
target_link_libraries(solstice-solver RSys Star3D Star3DUT StarCPR StarSF StarSP)

if(CMAKE_COMPILER_IS_GNUCC)
  target_link_libraries(solstice-solver m)
endif()

set_target_properties(solstice-solver PROPERTIES
  DEFINE_SYMBOL SSOL_SHARED_BUILD
  COMPILE_FLAGS ${OpenMP_C_FLAGS}
  VERSION ${VERSION}
  SOVERSION ${VERSION_MAJOR})
rcmake_copy_runtime_libraries(solstice-solver)

if(CMAKE_COMPILER_IS_GNUCC)
  set_target_properties(solstice-solver PROPERTIES LINK_FLAGS ${OpenMP_C_FLAGS})
endif()

rcmake_setup_devel(solstice-solver SolSolver ${VERSION} solstice/ssol_version.h)

################################################################################
# Add tests
################################################################################
if(NOT NO_TEST)
  function(build_test _name)
    add_executable(${_name} ${SSOL_SOURCE_DIR}/${_name}.c)
    target_link_libraries(${_name}
      solstice-solver solstice-test RSys Star3D StarSP)
  endfunction()

  function(register_test _name)
    add_test(${_name} ${ARGN})
    rcmake_set_test_runtime_dirs(${_name} _runtime_dirs)
  endfunction()

  function(new_test _name)
    build_test(${_name})
    register_test(${_name} ${_name})
  endfunction()

  add_library(solstice-test STATIC
    ${SSOL_SOURCE_DIR}/test_ssol_geometries.h
    ${SSOL_SOURCE_DIR}/test_ssol_materials.h
    ${SSOL_SOURCE_DIR}/test_ssol_utils.h
    ${SSOL_SOURCE_DIR}/test_ssol_utils.c)

  new_test(test_ssol_atmosphere)
  new_test(test_ssol_by_receiver_integration)
  new_test(test_ssol_camera)
  new_test(test_ssol_data)
  new_test(test_ssol_device)
  new_test(test_ssol_image)
  new_test(test_ssol_material)
  new_test(test_ssol_object)
  new_test(test_ssol_param_buffer)
  new_test(test_ssol_instance)
  new_test(test_ssol_scene)
  new_test(test_ssol_shape)
  new_test(test_ssol_spectrum)
  new_test(test_ssol_solver1)
  new_test(test_ssol_solver2)
  new_test(test_ssol_solver2b)
  new_test(test_ssol_solver3)
  new_test(test_ssol_solver4)
  new_test(test_ssol_solver5)
  new_test(test_ssol_solver6)
  new_test(test_ssol_solver7)
  new_test(test_ssol_solver8)
  new_test(test_ssol_solver9)
  new_test(test_ssol_sun)

  build_test(test_ssol_draw)
  register_test(test_ssol_draw_draft test_ssol_draw draft)
  register_test(test_ssol_draw_pt test_ssol_draw pt)

endif()

################################################################################
# Define output & install directories
################################################################################
install(TARGETS solstice-solver
  ARCHIVE DESTINATION bin
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(FILES ${SSOL_FILES_INC_API} DESTINATION include/solstice)
install(FILES ${SSOL_FILES_DOC} DESTINATION share/doc/solstice-solver)

