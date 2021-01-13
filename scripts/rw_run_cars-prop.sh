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

#./tools/zbd_set_full /dev/sdc
#echo "Testing.. CARS-PROP RW 16G"
#./test --algorithm CARS-PROP  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.5 > ./log/${DATETIME}/log_CARS-PROP_16G_rw_55_hm.log
#echo "CARS-PROP Finished. See: log/${DATETIME}/log_CARS-PROP_rw_16g.log"


./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS-PROP RW 32G"
./test --algorithm CARS-PROP  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.3 > ./log/${DATETIME}/log_CARS-PROP_16G_rw_73_hm.log
echo "CARS-PROP Finished. See: log/${DATETIME}/log_CARS-PROP_rw_32g.log"


./tools/zbd_set_full /dev/sdc
echo "Testing.. CARS-PROP RW 24G"
./test --algorithm CARS-PROP  --cache-size 16G --rmw-part 0 --workload-mode rw --dirtycache-proportion 0.7 > ./log/${DATETIME}/log_CARS-PROP_16G_rw_37_hm.log
echo "CARS-PROP Finished. See: log/${DATETIME}/log_CARS-PROP_rw_24g.log"
