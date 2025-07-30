# VTK Segmentation Volume Rendering Example

Similar to the [SimpleRayCast example](https://examples.vtk.org/site/Cxx/VolumeRendering/SimpleRayCast/), this repository provides a baseline for rendering segmentation volume 3D voxel data
sets.
Each voxel is assumed to store an integer label.
The example can be used for simple timing measurements.

## Building VTK from Source

Due to a current incompatibility bug with the expat library, [VTK](https://gitlab.kitware.com/vtk/) versions before 9.3.1 are unable to open most .vti files.
Unfortunately, Ubuntu system packages only provide version 9.3.0.
To build VTK from source:

Checkout new VTK version git:
```bash
mkdir -p ~/vtk
git clone --recursive https://gitlab.kitware.com/vtk/vtk.git ~/vtk/source

cd ~/vtk/source
git checkout v9.5.0 
```

Build and Install
```bash
sudo apt install build-essential cmake cmake-curses-gui mesa-common-dev mesa-utils freeglut3-dev ninja-build

mkdir -p ~/vtk/build
cd ~/vtk/build
cmake -GNinja ../path/to/vtk/source

cmake --build ~/vtk/build --config Release -j --target install
```

## Libraries

The example uses the [stb](https://github.com/nothings/stb/) image library (MIT license) for image file export.   