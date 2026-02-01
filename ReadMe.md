# VTK Segmentation Volume Rendering

This project accurately renders segmentation volumes* from [Volcanite](https://github.com/max-pio/volcanite) configurations.
In particular, it creates the comparative timing and image evaluation results for the Volcanite paper.
More generally, the code samples in this repository provide a baseline for rendering segmentation volume 3D voxel data
sets (similar to the [VTK SimpleRayCast example](https://examples.vtk.org/site/Cxx/VolumeRendering/SimpleRayCast/)).

*\*Segmentation volumes store an integer object label per voxel (here assumed to be 32 bit unsigned).
All voxels with the same label belong to the same object. This segments the space into separate object regions.*


## 1. Prerequisites: Install VTK

On Ubuntu, the easiest way to install VTK libraries is a package installinstall:
```
sudo apt install libvtk9-dev
```

*Note: Due to an incompatibility bug with the expat library, [VTK](https://gitlab.kitware.com/vtk/) versions before 9.3.1 are unable to open most .vti files.
Unfortunately, Ubuntu system packages currently (2026/01) only provide version 9.3.0.
If one needs to open .vti files, it is necessary to build VTK from source.*

### Optional: Build VTK from source

Checkout new VTK version from git:
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
cmake -GNinja ../source

sudo cmake --build ~/vtk/build --config Release -j --target install
```

## 2. Build Project

In vtk-segvol source directory:
```bash
mkdir -p ./cmake-build-release
cd ./cmake-build-release
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```


## 3. Run the evaluation

Provide data sets using the data download scripts from the [Volcanite evaluation](https://github.com/max-pio/volcanite/tree/main/eval).
This will download the data sets, compile Volcanite, and create a `volcanite-eval-setup.txt` file containing the following keys:
```
volcanite-src: <directory of the Volcanite source cloned from git>
vcfg-dir: <directory storing .vcfg evaluation config files: usually [...]/volcanite/eval/config>
csgv-dir: <directory storing the data sets downloaded by the Volcanite download-evaluation-data.py>
entry-command: <bash command that will be run before the evaluation>
exit-command: <bash command that will be run after the evaluation>
```

The script assumes the Volcanite executable to be located in the directory `<volcanite-src>/cmake-build-release/volcanite/`.
You can optionally define `entry-command:` and `exit-command` to run bash commands before and after the evaluation.
For example, this allows to fix GPU clock speeds for precise timing measurements (example for NVIDIA with GPU clock at 3105 MHz and memory clock at 10501 MHz):
```
entry-command: sudo nvidia-smi --lock-gpu-clocks=3105 && sudo nvidia-smi --lock-memory-clocks=10501 && sudo nvidia-smi -pm 1
exit-command: sudo nvidia-smi --reset-gpu-clocks && sudo nvidia-smi --reset-memory-clocks && sudo nvidia-smi -pm 0
```

Run `./run-all.sh` to run the evaluation and obtain all results in [./results](./results).
This will also build the executable if this was not done before.

## Libraries

[VTK](https://vtk.org/about/) is licensed under the [BSD license](http://en.wikipedia.org/wiki/BSD_licenses). 
This example uses the [stb](https://github.com/nothings/stb/) image library (MIT license) for image file export.   
[Volcanite](https://github.com/max-pio/volcanite) is licensed under the GPLv3 license.