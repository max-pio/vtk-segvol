#!/bin/bash

for i in {0..6}; do
    ./cmake-build-release/vtk-segvol --data-dir /media/maxpio/data/eval/ --vcfg-dir /home/maxpio/code/volcanite/eval/config/ --results-file ./out/results.csv --image-dir ./out/ -d $i -f 1024
done
