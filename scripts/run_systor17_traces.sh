
#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"  

cd ..

type ./test > /dev/null 2>&1

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then 
	echo "error: No executable program file <test>, please check if the program has been built."
	exit 1
fi

mkdir ./log/${DATETIME}

array_cachesize=("16G" "32G")
array_traces=(0 1 2 3 4 5)
# for cachesize in ${array_cachesize[@]}
# do
#     for trace in ${array_traces[@]}
#     do
#         echo "Testing.. CARS RW LUN"${trace}" in "${cachesize}
#         #./tools/zbd_set_full /dev/sdc
#         ./test --algorithm CARS  --workload ${trace} --workload-mode rw --cache-size ${cachesize} --rmw-part 0 > ./log/${DATETIME}/log_cars_rw_LUN${trace}_${cachesize}.log &
#         echo "Testing.. MOST RW LUN"${trace}" in "${cachesize}
#         #./tools/zbd_set_full /dev/sdc
#         ./test --algorithm MOST  --workload ${trace} --workload-mode rw --cache-size ${cachesize} --rmw-part 0 --dirtycache-proportion 0.5 > ./log/${DATETIME}/log_most_rw_LUN${trace}_${cachesize}.log &
#     done
# done
cachesize="16G"
    for trace in ${array_traces[@]}
    do
        # echo "Testing.. CARS WO LUN"${trace}" in "${cachesize}
        # #./tools/zbd_set_full /dev/sdc
        # ./test --algorithm CARS  --workload ${trace} --workload-mode w --cache-size ${cachesize} --rmw-part 0 > ./log/${DATETIME}/log_cars_wo_LUN${trace}_${cachesize}.log &
        
        # echo "Testing.. MOST WO LUN"${trace}" in "${cachesize}
        # #./tools/zbd_set_full /dev/sdc
        # ./test --algorithm MOST  --workload ${trace} --workload-mode w --cache-size ${cachesize} --rmw-part 0 > ./log/${DATETIME}/log_most_wo_LUN${trace}_${cachesize}.log &
        
        # echo "Testing.. CARS-partRMW WO LUN"${trace}" in "${cachesize}
        # #./tools/zbd_set_full /dev/sdc
        # ./test --algorithm CARS  --workload ${trace} --workload-mode w --cache-size ${cachesize} --rmw-part 1 > ./log/${DATETIME}/log_cars-partrmw_wo_LUN${trace}_${cachesize}.log &

       echo "Testing.. LRU WO LUN"${trace}" in "${cachesize}
        #./tools/zbd_set_full /dev/sdc
        ./test --algorithm LRUZONE  --workload ${trace} --workload-mode w --cache-size ${cachesize} --rmw-part 0 >  ./log/${DATETIME}/log_lru_wo_LUN${trace}_${cachesize}.log &
    done