#!/bin/bash

DATETIME="`date +%Y-%m-%d,%H:%m:%s`"

cd ..

type ./test > /dev/null 2>&1

CMD_TEST=$?

if [ $CMD_TEST -ne 0 ]; then
        echo "error: No executable program file <sac>, please check if the program has been built."
        exit 1
fi

echo "Testing.. LRUZONE WO 16G"
./test --algorithm LRUZONE  --cache-size 16G  > ./log/log_lruzone_wo_16g_em_${DATETIME}.log
echo "LRUZONE Finished. See: log/log_lruzone_wo_16g_hm_"${DATETIME}".log"

echo "Testing.. LRUZONE WO 24G"
./test --algorithm LRUZONE  --cache-size 24G  > ./log/log_lruzone_wo_24g_em_${DATETIME}.log
echo "LRUZONE Finished. See: log/log_lruzone_wo_24g_hm_"${DATETIME}".log"

echo "Testing.. LRUZONE WO 32G"
./test --algorithm LRUZONE  --cache-size 32G  > ./log/log_lruzone_wo_32g_em_${DATETIME}.log
echo "LRUZONE Finished. See: log/log_lruzone_wo_32g_hm_"${DATETIME}".log"

echo "Testing.. LRUZONE WO 40G"
./test --algorithm LRUZONE  --cache-size 40G  > ./log/log_lruzone_wo_40g_em_${DATETIME}.log
echo "LRUZONE Finished. See: log/log_lruzone_wo_40g_hm_"${DATETIME}".log"
echo "Done. "


