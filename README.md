# Solstice Solver

The purpose of this library is to integrate the solar flux in complex solar
facilities. It has been developed in the scope of the Solstice project, in
collaboration with the
[Laboratory of Excellence Solstice](http://www.labex-solstice.fr) and the
[PROMES](http://www.promes.cnrs.fr/index.php?page=home-en) laboratory of the
National Center for Scientific Research ([CNRS](http://www.cnrs.fr/index.php)).

## How to build

The Solstice-Solver library relies on the [CMake](http://www.cmake.org) and the
[RCMake](https://gitlab.com/vaplv/rcmake/) package to build.
It also depends on the
[RSys](https://gitlab.com/vaplv/rsys/),
[Star-3D](https://gitlab.com/meso-star/star-3d/) and
[Star-SP](https://gitlab.com/meso-star/star-sp/) libraries as well as on the
[OpenMP](http://www.openmp.org) 1.2 specification to parallelize its
computations.

First ensure that CMake and a compiler that implements the OpenMP 1.2
specification are installed on your system. Then install the RCMake package as
well as all the aforementioned prerequisites. Finally generate the project from
the `cmake/CMakeLists.txt` file by appending to the `CMAKE_PREFIX_PATH`
variable the install directories of its dependencies.

## Release notes

### Version 0.2

- Add normal maps to describe spatially varying normals in the tangent space of
  the surface.
- Add support of spectral data to the atmosphere and the materials.
- Fix the per primitive irradiance estimate by dividing the result by the area
  of the primitive in order to have watts per square meter.

## Licenses

Solstice-Solver is developed by [|Meso|Star>](http://www.meso-star.com) for the
[National Center for Scientific Research](http://www.cnrs.fr/index.php) (CNRS).
It is a free software copyright (C) CNRS 2016-2017 and it is released under the
[OSI](http://opensource.org)-approved GPL v3+ license. You are welcome to
redistribute it under certain conditions; refer to the COPYING file for
details.

