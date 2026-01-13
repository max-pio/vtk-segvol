# VTK Segmentation Volume Rendering Example

Similar to the [SimpleRayCast example](https://examples.vtk.org/site/Cxx/VolumeRendering/SimpleRayCast/), this repository provides a baseline for rendering segmentation volume 3D voxel data
sets.
Each voxel is assumed to store an integer label.
The example can be used for simple timing measurements.

1. Prerequisites: Building VTK from Source

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

2. Build VTK Segmenation Volume Rendering project
```bash
mkdir -p ./cmake-build-release
cd ./cmake-build-release
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```


3. Run the evaluation

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