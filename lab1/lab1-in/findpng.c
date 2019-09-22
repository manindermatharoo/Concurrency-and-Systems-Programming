#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>   /* for printf().  man 3 printf */
#include <stdlib.h>  /* for exit().    man 3 exit   */
#include <string.h>  /* for strcat().  man strcat   */
#include <sys/types.h>
#include "lab_png.h"   /* simple PNG data structures  */
#include <errno.h>

char *file_type(char *file_path);
void list_dir(int *png_found, char *dir_name);

char *file_type(char *file_path)
{
    char *ptr;
    struct stat buf;

    if (lstat(file_path, &buf) < 0)
    {
        perror("lstat error");
        ptr = "error";
    }
    if(S_ISREG(buf.st_mode))
    {
        ptr = "regular";
    }
    else if(S_ISDIR(buf.st_mode))
    {
        ptr = "directory";
    }
    else if(S_ISCHR(buf.st_mode))
    {
        ptr = "character special";
    }
    else if(S_ISBLK(buf.st_mode))
    {
        ptr = "block special";
    }
    else if(S_ISFIFO(buf.st_mode))
    {
        ptr = "fifo";
    }
#ifdef S_ISLNK
    else if(S_ISLNK(buf.st_mode))
    {
        ptr = "symbolic link";
    }
#endif
#ifdef S_ISSOCK
    else if(S_ISSOCK(buf.st_mode))
    {
        ptr = "socket";
    }
#endif
    else
    {
        ptr = "**unknown mode**";
    }

    return ptr;
}

void list_dir(int *png_found, char *dir_name)
{
    DIR *parent_dir;
    struct dirent *parent_dirent;

    if((parent_dir = opendir(dir_name)) == NULL)
    {
        fprintf(stderr, "opendir(%s): %s\n", dir_name, strerror(errno));
        exit(2);
    }

    while((parent_dirent = readdir(parent_dir)) != NULL)
    {
        char *str_path = parent_dirent->d_name;  /* relative path name! */

        /* convert from relative to full path */
        char full_path[strlen(dir_name) + 1 + strlen(str_path)];
        strcpy(full_path, dir_name);
        strcat(full_path, "/");
        strcat(full_path, str_path);

        if(str_path == NULL)
        {
            fprintf(stderr,"Null pointer found!");
            exit(3);
        }
        else
        {
            char *type = file_type(full_path);

            const char *directory_type = "directory";
            const char *regular_type = "regular";
            const char *ignore_curr = ".";
            const char *ignore_prev = "..";

            if((strcmp(type,directory_type) == 0) && (strcmp(str_path,ignore_curr)) && (strcmp(str_path,ignore_prev)))
            {
                list_dir(png_found, full_path);
            }
            else if(strcmp(type,regular_type) == 0)
            {
                U8 png_file_header[PNG_SIG_SIZE];

                /* Open binary png file */
                FILE *png_file = fopen(full_path, "rb");

                /* make sure the file is a valid file to open (exists) */
                if(png_file == NULL)
                {
                    fprintf(stderr, "Couldn't open %s: %s\n", full_path, strerror(errno));
                    exit(1);
                }

                /* Read the first 8 bytes of png file which should be the header */
                int header_bytes = fread(png_file_header, 1, PNG_SIG_SIZE, png_file);

                if(is_png(png_file_header, header_bytes) == 0)
                {
                    *png_found = 0;
                    printf("%s\n", full_path);
                }

                fclose(png_file);
            }
        }
        /* reset the full path */
        memset(full_path, 0, sizeof(full_path));
    }

    if(closedir(parent_dir) != 0)
    {
        perror("closedir");
        exit(3);
    }
}

int main(int argc, char **argv)
{
    if(argc == 1 || argc > 2)
    {
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }

    /* so far a png hasn't been found */
    int png_found = 1;

    /* list the directory and files associated */
    list_dir(&png_found, argv[1]);

    /* did not find any png in a directory */
    if(png_found == 1)
    {
        printf("findpng: No PNG file found \n");
    }

    return 0;
}
