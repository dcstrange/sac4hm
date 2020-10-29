#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <libzbc/zbc.h>
#include "config.h"
#include "zbc_private.h"
#include "libzone.h"

struct zbc_device *dev;

int main(){
	char smr_filename[100];
	printf("please input the filepath:\n");/* /dev/sdc/ */
	scanf("%s", smr_filename);
	int ret = zbc_open(smr_filename, O_RDWR | O_DIRECT, &dev);
	printf("ret:%d\n",ret);
	return 0;
} 
