#!/bin/sh

for i in `ls *.vgm`; do
    echo "Converting $i"
    ./vgm2wav $i `basename $i .vgm`.wav 0
done

for i in `ls *.nsf`; do
    echo "Converting $i"
    for j in 0 1 2 3 4 5 6 7 8 9; do
        echo "-- track $j"
        ./vgm2wav $i `basename $i .nsf`-$j.wav $j
    done
done
