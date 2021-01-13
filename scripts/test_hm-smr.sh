
#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"  

cd ..

type ./test > /dev/null 2>&1

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then 
	echo "error: No executable program file <sac>, please check if the program has been built."
	exit 1
fi

 ./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS WO 16G"
./test --algorithm CARS  --cache-size 16G --rmw-part 0 > ./log/log_cars_wo_16g_hm_${DATETIME}.log
echo "CARS Finished. See: log/log_cars_wo_16g_hm_"${DATETIME}".log"

 ./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS WO 24G"
./test --algorithm CARS  --cache-size 24G --rmw-part 0 > ./log/log_cars_wo_24g_hm_${DATETIME}.log
echo "CARS Finished. See: log/log_cars_wo_24g_hm_"${DATETIME}".log"

 ./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS WO 32G"
./test --algorithm CARS  --cache-size 32G --rmw-part 0 > ./log/log_cars_wo_32g_hm_${DATETIME}.log
echo "CARS Finished. See: log/log_cars_wo_32g_hm_"${DATETIME}".log"

 ./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS WO 40G"
./test --algorithm CARS  --cache-size 40G --rmw-part 0 > ./log/log_cars_wo_40g_hm_${DATETIME}.log
echo "CARS Finished. See: log/log_cars_wo_40g_hm_"${DATETIME}".log"
echo "Done. "





