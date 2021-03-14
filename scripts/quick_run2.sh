
#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"  

cd ..
type ./test > /dev/null 2>&1
mkdir ./log/${DATETIME}

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then 
	echo "error: No executable program file <sac>, please check if the program has been built."
	exit 1
fi

echo "Testing.. CARS WO 16G"
./test --algorithm CARS  --cache-size 16G --rmw-part 0 > ./log/${DATETIME}/log_cars_wo_16g_winx1.log &
echo "CARS Finished. See: log/log_cars_wo_16g_hm_"${DATETIME}".log"


echo "Testing.. CARS WO 16G part"
./test --algorithm CARS  --cache-size 16G --rmw-part 1 > ./log/${DATETIME}/log_cars_wo_16g_part_winx1.log &
echo "CARS Finished. See: log/log_cars_wo_16g_part_em_"${DATETIME}".log"

echo "Testing.. MOST WO 16G"
./test --algorithm MOST  --cache-size 16G --rmw-part 0 > ./log/${DATETIME}/log_most_wo_16g_winx1.log &
echo "MOST Finished. See: log/log_most_wo_16g_hm_"${DATETIME}".log"

echo "Testing.. LRUZONE WO 16G"
./test --algorithm LRUZONE  --cache-size 16G --rmw-part 0 > ./log/${DATETIME}/log_lruzone_wo_16g_winx1.log &
echo "MOST Finished. See: log/log_most_wo_16g_hm_"${DATETIME}".log"

