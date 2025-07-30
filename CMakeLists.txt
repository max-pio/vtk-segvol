cmake_minimum_required(VERSION 3.30)
project(vtk-segvol)

set(CMAKE_CXX_STANDARD 20)

find_package(VTK REQUIRED COMPONENTS
        CommonColor
        CommonCore
        CommonDataModel
        IOLegacy
        IOXML
        InteractionStyle
        RenderingContextOpenGL2
        RenderingCore
        RenderingFreeType
        RenderingGL2PSOpenGL2
        RenderingOpenGL2
        RenderingVolume
        RenderingVolumeOpenGL2
)

find_package(OpenGL)

add_executable(vtk-segvol
        main.cpp
        extern/stb/stb_image.hpp
        extern/stb/stb_image.cpp
        extern/stb/stb_image_write.hpp
)

target_link_libraries(vtk-segvol PRIVATE ${VTK_LIBRARIES} OpenGL::OpenGL)

target_include_directories(vtk-segvol PRIVATE extern)

vtk_module_autoinit(
        TARGETS vtk-segvol
        MODULES ${VTK_LIBRARIES}
)
