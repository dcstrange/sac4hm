#ifndef LOG_SAC_H
#define LOG_SAC_H
#include <stdio.h> // FILE
#include <stdlib.h>
#include <string.h>

#define log_err_sac printf
// {
//     fprintf(stderr, format);
//     exit(EXIT_FAILURE);
// }


#define log_info_sac printf
// {
//     fprintf(stdout, format);
// }

static inline size_t log_write_sac(FILE * file, char *msg) {
    return fwrite((void*)msg, strlen(msg), 1, file);
}

#define DASHHH printf("---- --- ---\n");

#endif