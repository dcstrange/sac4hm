#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"

cd ..
mkdir ./log/${DATETIME}

type ./test > /dev/null 2>&1

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then
        echo "error: No executable program file <sac>, please check if the program has been built."
        exit 1
fi

./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS RW 16G"
./test --algorithm CARS  --cache-size 16G --rmw-part 0 --workload-mode rw > ./log/${DATETIME}/log_cars_rw_16g_hm.log
echo "CARS Finished. See: log/${DATETIME}/log_cars_rw_16g_em.log"


./tools/zbd_set_full /dev/sdc
echo "Testing.. MOST RW 16G"
./test --algorithm MOST  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.5 > ./log/${DATETIME}/log_most_16G_rw_55_hm.log
echo "MOST Finished. See: log/${DATETIME}/log_most_rw_16g_em.log"

./tools/zbd_set_full /dev/sdc
echo "Testing.. MOST RW 24G"
./test --algorithm MOST  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.7 > ./log/${DATETIME}/log_most_16G_rw_37_hm.log
echo "MOST Finished. See: log/${DATETIME}/log_most_rw_24g_em.log"

./tools/zbd_set_full /dev/sdc
echo "Testing.. MOST RW 32G"
./test --algorithm MOST  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.3 > ./log/${DATETIME}/log_most_16G_rw_73_hm.log
echo "MOST Finished. See: log/${DATETIME}/log_most_rw_32g_em.log"

