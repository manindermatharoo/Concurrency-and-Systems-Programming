#include "lab_png.h"  /* simple PNG data structures  */
#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */

int main (int argc, char **argv)
{
    if (argc == 1 || argc > 2) {
        fprintf(stderr, "Usage: %s <png file>\n", argv[0]);
        exit(1);
    }

    FILE * input_file = fopen(argv[1], "rb");
    char ar[16];
    fread(ar, 16, 1, input_file);
    for(int i = 0; i < 16; i++)
    {
        printf("%c", ar[i]);
    }
}