#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    sf_mem_init();

    sf_malloc(3 * PAGE_SZ - ((1 << 6) - sizeof(sf_header)) - 64 - 2*sizeof(sf_header));
    sf_malloc(5);

    //*ptr = 320320320e-320;

    //printf("%e\n", *ptr);
    //sf_show_heap();

    sf_show_heap();
     sf_mem_fini();

    return EXIT_SUCCESS;
}