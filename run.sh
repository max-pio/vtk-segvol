#!/bin/bash

# VTK Renderings of the "image" evaluation (1024 still camera frames) for data sets that fit into memory
for i in {0..6}; do
    ./cmake-build-release/vtk-segvol --data-dir /media/maxpio/data/eval/ --vcfg-dir /home/maxpio/code/volcanite/eval/config/ --results-file ./out/results.csv --image-dir ./out/ -d $i -f 1024
done

# VTK closeup rendering
./cmake-build-release/vtk-segvol --data-dir /media/maxpio/data/eval/ --vcfg-file /home/maxpio/code/volcanite/eval/config/Wolny2020-closeup.vcfg --image-output-file ./out/Wolny2020-closeup.png -d 5 -f 32

# Volcanite closeup rendering
~/volcanite --headless /media/maxpio/data/eval/Wolny2020.csgv --config /home/maxpio/code/volcanite/eval/config/Wolny2020-closeup.vcfg -i ./out/Wolny2020-closeup-volcanite.png
~/volcanite --headless /media/maxpio/data/eval/Wolny2020.csgv --config /home/maxpio/code/volcanite/eval/config/Wolny2020-closeup.vcfg --config path-tracing -i ./out/Wolny2020-closeup-volcanite-pt.png
