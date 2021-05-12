#include <stdlib.h>
#include <stdio.h>
#include <mcheck.h>

int main(int argc, char **argv) {
    mtrace();

    char *p = malloc(16);
//    printf("%p\n", &p);
    printf("%p\n", p);

    free(p);

    p = malloc(32);
//    printf("%p\n", &p);
    printf("%p\n", p);

    muntrace();

    return 0;
}