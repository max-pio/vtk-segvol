#!/bin/bash

# Provide path to volcanite-eval-setup.txt (created by Volcanite evaluation) as argument
if [ $# -eq 1 ]; then
    cfg_file="$1"
else
    cfg_file="volcanite-eval-setup.txt"
fi

# Verify file exists
if [ ! -f "$cfg_file" ]; then
    echo "Error: Config file '$cfg_file' not found. Provide path to volcanite-eval-setup.txt." >&2
    exit 1
fi

# Read all variables (config file might end without newline)
while IFS=':' read -r key value || [ -n "$key" ]; do
    key=${key//-/_}          # turn volcanite-src -> volcanite_src (valid Bash name)
    value=${value# }         # strip leading space
    printf -v "$key" '%s' "$value"
done < "$cfg_file"

echo "Volcanite binary: $volcanite_src/cmake-build-release/volcanite/volcanite"
echo "Config file directory (.vcfg): $vcfg_dir"
echo "Data set base directory: $csgv_dir"

# Build vtk-segvolecho "Building vtk-segvol in ./cmake-build-release"
mkdir -p "./cmake-build-release"
cd "./cmake-build-release" || exit 1
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
cd ".." || exit 1

# Execute entry-command if present
if [ -n "${entry_command+x}" ]; then
    echo "Executing entry-command: $entry_command"
    eval "$entry_command"
fi

# VTK Renderings of the "image" evaluation (1024 still camera frames) for data sets that fit into memory
DATA_COUNT=$(./cmake-build-release/vtk-segvol --list-data)
for ((i=0; i<DATA_COUNT; i++)) do
  ./cmake-build-release/vtk-segvol --data-dir $csgv_dir/ --vcfg-dir $vcfg_dir/ --results-file ./results/vtk-eval.csv --image-dir ./results/ -d $i -f 1024
done

# VTK closeup rendering
./cmake-build-release/vtk-segvol --data-dir $csgv_dir/ --vcfg-file $vcfg_dir/Wolny2020-closeup.vcfg --image-output-file ./results/Wolny2020-closeup.png -d 5 -f 32

# Volcanite closeup rendering
eval "$volcanite_src/cmake-build-release/volcanite/volcanite --headless $csgv_dir/Wolny2020.csgv --config $vcfg_dir/Wolny2020-closeup.vcfg -i ./results/Wolny2020-closeup-volcanite.png"
eval "$volcanite_src/cmake-build-release/volcanite/volcanite --headless $csgv_dir/Wolny2020.csgv --config $vcfg_dir/Wolny2020-closeup.vcfg --config path-tracing -i ./results/Wolny2020-closeup-volcanite-pt.png"

# Execute exit-command if present
if [ -n "${exit_command+x}" ]; then
    echo "Executing exit-command: $exit_command"
    eval "$exit_command"
fi

exit 0
