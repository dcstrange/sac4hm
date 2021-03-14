#!/bin/bash
cd ..


array_traces=(0 1 2 3 4 5)

cachesize="16G"

for trace in ${array_traces[@]}
do
    tail ./log/wo-systor17/log_cars_wo_LUN${trace}_${cachesize}.log -n 20 | sed -n "3p;5p;11p;16p"
done

for trace in ${array_traces[@]}
do
    tail ./log/wo-systor17/log_most_wo_LUN${trace}_${cachesize}.log -n 20 | sed -n "3p;5p;11p;16p"
done

for trace in ${array_traces[@]}
do
    tail ./log/wo-systor17/log_cars-partrmw_wo_LUN${trace}_${cachesize}.log -n 20 | sed -n "3p;5p;11p;16p"
done

for trace in ${array_traces[@]}
do
    tail ./log/wo-systor17/log_lru_wo_LUN${trace}_${cachesize}.log -n 20 | sed -n "3p;5p;11p;16p"
done