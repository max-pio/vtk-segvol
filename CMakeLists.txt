cmake_minimum_required(VERSION 3.30)
project(vtk-segvol)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Find Packages ----------------------------------------------------------------------------------------

find_package(VTK REQUIRED COMPONENTS
        CommonColor
        CommonCore
        CommonDataModel
        IOLegacy
        IOXML
        IOHDF
        InteractionStyle
        RenderingContextOpenGL2
        RenderingCore
        RenderingFreeType
        RenderingGL2PSOpenGL2
        RenderingOpenGL2
        RenderingVolume
        RenderingVolumeOpenGL2
        RenderingAnnotation
)

# extern HighFive simplified hdf5 library if libhdf5-dev is installed
option(ENABLE_HDF5_SUPPORT  "Includes the hdf5 library for importing and exporting .hdf5 files" ON)
if (ENABLE_HDF5_SUPPORT)
    find_package(HDF5 QUIET)
    if (HDF5_FOUND)
        # set HighFive CMake cache options
        set(USE_BOOST OFF CACHE INTERNAL "")
        set(USE_EIGEN OFF CACHE INTERNAL "")
        set(USE_XTENSOR OFF CACHE INTERNAL "")
        set(USE_OPENCV OFF CACHE INTERNAL "")
        mark_as_advanced(USE_BOOST USE_EIGEN USE_XTENSOR)

        set(HIGHFIVE_UNIT_TESTS OFF CACHE INTERNAL "")
        set(HIGHFIVE_EXAMPLES OFF CACHE INTERNAL "")
        set(HIGHFIVE_BUILD_DOCS OFF CACHE INTERNAL "")
        set(HDF5_PREFER_PARALLEL ON CACHE INTERNAL "")

        # fetch HighFive library
        # current repo: https://github.com/highfive-devs/highfive/
#        FetchContent_Declare(
#                HighFive
#                GIT_REPOSITORY https://github.com/BlueBrain/HighFive
#                GIT_TAG v2.2.2
#                SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/HighFive
#        )
#        FetchContent_MakeAvailable(HighFive)
        add_subdirectory(extern/HighFive)
    else ()
        message(WARNING "ENABLE_HDF5_SUPPORT was set but hdf5 library could not be found.")
    endif ()
endif ()

find_package(OpenGL)


# Create Executable ----------------------------------------------------------------------------------------

add_executable(vtk-segvol
        extern/stb/stb_image.hpp
        extern/stb/stb_image.cpp
        extern/stb/stb_image_write.hpp
        #
        main.cpp
        read_hdf5.hpp
        read_vcfg_tf.hpp
)

# link libraries
target_link_libraries(vtk-segvol PRIVATE ${VTK_LIBRARIES} OpenGL::OpenGL)

if (HDF5_FOUND)
    target_link_libraries(vtk-segvol PRIVATE HighFive)
    target_compile_definitions(vtk-segvol PRIVATE -DLIB_HIGHFIVE)
endif ()

# add include directories
target_include_directories(vtk-segvol PRIVATE extern volume/include)

# initialize VTK
vtk_module_autoinit(
        TARGETS vtk-segvol
        MODULES ${VTK_LIBRARIES}
)
