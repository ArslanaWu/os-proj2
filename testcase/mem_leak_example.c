#include <stdlib.h>
#include <stdio.h>
#include <mcheck.h>

int main(int argc, char **argv) {
    mtrace();

    char *p = malloc(16);

    free(p);

    p = malloc(32);

    muntrace();

    return 0;
}