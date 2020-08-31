#include <stdio.h>

void log_err(char* format, ...)
{
    fprintf(stderr, format);
    exit(EXIT_FAILURE);
}

