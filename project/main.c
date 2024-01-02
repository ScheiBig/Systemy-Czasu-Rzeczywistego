#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "logger.h"

int main() {
    printf("Hello world!\n");

    printf("Error code [%d]", system("pmap -V > /dev/null 2>&1"));

    return 0;
}
