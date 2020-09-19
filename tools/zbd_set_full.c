#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h> // struct FILE
#include <string.h> // strerror()
#include <errno.h>
//#include <asm-generic/fcntl.h> // O_flags
#include <fcntl.h>
#include <stdint.h>

#include <libzbc/zbc.h>

char* zbd_path;

int zbd_set_full()
{
/* Open ZBD */
    struct zbc_device *zbd;
    //ret = zbd_open(zbd_path, O_RDWR | __O_DIRECT | ZBC_O_DRV_FAKE, &zbd);
    int ret = zbc_open(zbd_path, O_RDONLY , &zbd);
    if(ret != 0){
		if (ret == -ENODEV)
			fprintf(stderr,
				"Open %s failed (not a zoned block device)\n",
				zbd_path);
		else
			fprintf(stderr, "Open %s failed (%s)\n",
				zbd_path, strerror(-ret));
		return 1;
    }

	/* Get zone number */
    struct zbc_zone * zones, *z;
    unsigned int nr_zones, nz;
    ret = zbc_report_nr_zones(zbd, 0, ZBC_RO_ALL, &nr_zones);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_nr_zones failed %d\n", ret);
		return 1;
	}

	nz = nr_zones;
	zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));

	/* Get zone information */
	ret = zbc_report_zones(zbd, 0, ZBC_RO_ALL, zones, &nz);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_zones failed %d\n", ret);
		return 1;
	}

	fprintf(stdout, "%u / %u zone%s:\n", nz, nr_zones, (nz > 1) ? "s" : "");

    fprintf(stdout, "Start set all zone finish ...");
    fflush(stdout);
	unsigned long long sector = 0;
	for (int i = 0; i < (int)nz; i++) {

		z = &zones[i];

        /* Check */
        if (zbc_zone_start(z) != sector) {
            printf("[WARNING] Zone %05d: sector %llu should be %llu\n",
                    i,
                    zbc_zone_start(z), sector);
            sector = zbc_zone_start(z);
        }

        /* set zone to cond finish */
        if(zbc_zone_condition(z) != ZBC_ZC_NOT_WP){
            ret = zbc_finish_zone(zbd, sector, 0);
            if(ret < 0){    
                fprintf(stderr, "finish zone [%d] failed, err: %d, %s\n", i, ret, strerror(-ret));
		        return 1;
            } 
        }

        sector += zbc_zone_length(z);
	}

    fprintf(stdout, "Success.\n");
    return 0;

}

    void main(int argc, char **argv){
        if(argc != 2){
            printf("zbd_set_full [dev]\n");
            exit(-1);
        }
        zbd_path = argv[1];

        zbd_set_full();
}

