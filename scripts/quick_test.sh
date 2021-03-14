#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"

cd ..

type ./test > /dev/null 2>&1

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then
        echo "error: No executable program file <sac>, please check if the program has been built."
        exit 1
fi

mkdir ./log/${DATETIME}

echo "Testing.. WALFU WO 16G"
./test --algorithm WALFU  --cache-size 16G  > ./log/${DATETIME}/log_PORE_wo_16G_em_z16.log &
echo "WALFU Finished. See: log/log_lruzone_wo_32g_hm_"${DATETIME}".log"

echo "Testing.. WALFU WO 16G"
./test --algorithm CARS  --cache-size 16G  > ./log/${DATETIME}/log_CARS_wo_16G_em_z16.log &
echo "WALFU Finished. See: log/log_lruzone_wo_32g_hm_"${DATETIME}".log"

echo "Testing.. WALFU WO 16G"
./test --algorithm MOST  --cache-size 16G  > ./log/${DATETIME}/log_MOST_wo_16G_em_z16.log &
echo "WALFU Finished. See: log/log_lruzone_wo_32g_hm_"${DATETIME}".log"




# echo "Testing.. LRUZONE WO 16G"
# ./test --algorithm LRUZONE  --cache-size 32G  > ./log/${DATETIME}/log_LRUZONE_wo_32G_em_z256.log &
# echo "LRUZONE Finished. See: log/log_lruzone_wo_16g_hm_"${DATETIME}".log"

# echo "Testing.. LFUZONE WO 16G"
# ./test --algorithm LFUZONE  --cache-size 32G  > ./log/${DATETIME}/log_LFUZONE_wo_32G_em_z256.log &
# echo "LFUZONE Finished. See: log/log_lruzone_wo_24g_hm_"${DATETIME}".log"

# echo "Testing.. WALFU WO 16G"
# ./test --algorithm WALFU  --cache-size 32G  > ./log/${DATETIME}/log_WALFU_wo_32G_em_z256.log &
# echo "WALFU Finished. See: log/log_lruzone_wo_32g_hm_"${DATETIME}".log"

# echo "Testing.. WALRU WO 16G"
# ./test --algorithm WALRU  --cache-size 32G  > ./log/${DATETIME}/log_WALRU_wo_32G_em_z256.log &
# echo "WALRU Finished. See: log/log_lruzone_wo_40g_hm_"${DATETIME}".log"

# echo "Testing.. CARS WO 16G"
# ./test --algorithm CARS  --cache-size 32G  > ./log/${DATETIME}/log_CARS_wo_32G_em_z256.log &
# echo "CARS Finished. See: log/log_lruzone_wo_40g_hm_"${DATETIME}".log"

echo "Done. "


