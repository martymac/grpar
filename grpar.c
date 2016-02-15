/*-
 * Copyright (c) 2010-2014 Ganael LAPLANCHE <ganael.laplanche@martymac.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
   Group file format, see : http://advsys.net/ken/build.htm
   12 bytes : "KenSilverman"
    4 bytes : number of files (little-endian)

   Then, for each file :
   12 bytes : file name (zero-filled)
    4 bytes : file size (little-endian)

   Then, for each file :
    n bytes : file data
    [...]
*/

/* uint32_t, uint8_t */
#include <stdint.h>

/* printf(3) */
#include <stdio.h>

/* malloc(3) */
#include <stdlib.h>

/* strncmp(3) */
#include <string.h>

/* open(2) */
#include <fcntl.h>

/* read(2), getopt(3) */
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#if defined(__FreeBSD__)
  /* le32toh(9) */
  #include <sys/endian.h>
#elif defined(__linux__)
  /* le32toh(3) */
  #include <endian.h>
#else
  #define le32toh(x) (x)
#endif

/* stat(2) */
#include <sys/stat.h>

#define GRPAR_VERSION       "0.2"

#define GRPHDR_MAGIC        "KenSilverman"  /* magic */
#define GRPHDR_MAGICLEN     12              /* magic length */
#define GRPHDR_NUMFILESLEN  4               /* bytes for number of files */

#define GRPHDR_FILENAMELEN  12              /* bytes for file name */
#define GRPHDR_FILESIZELEN  4               /* bytes for file size */

/* File entry within group archive */
struct grp_file;
struct grp_file {
    uint32_t index;                         /* file position */
    char file_name[GRPHDR_FILENAMELEN + 1]; /* file name + '\0' */
    uint32_t file_size;                     /* file size in bytes */
    off_t file_offset;                      /* file offset in group archive */
    struct grp_file *next;                  /* next one */
};

/* Program options */
struct program_options {
    char *grp_filename;
    char *dst_dirname;
#define ACTION_NONE     0
#define ACTION_LIST     1
#define ACTION_EXTRACT  2
    uint8_t action;
    uint8_t verbose;
};

/* Initialize grp_file structures from a grp file
   Returns a file handle on group file as well as num_files found in archive */
int
init_grp_files(const char *filename, struct grp_file **head,
    uint32_t *num_files)
{
    int grp_file_handle;

    /* Main header */
    char headbuf[GRPHDR_MAGICLEN + GRPHDR_NUMFILESLEN];

    /* Per-file header */
    char filebuf[GRPHDR_FILENAMELEN + GRPHDR_FILESIZELEN];
    struct grp_file *current = NULL;
    struct grp_file *previous = NULL;

    if((filename == NULL) || (head == NULL) || (*head != NULL) ||
        (num_files == NULL)) {
        fprintf(stderr, "%s(): invalid argument\n", __func__);
        return (-1);
    }

    /* Open group archive */
    if((grp_file_handle = open(filename, O_RDONLY)) < 0) {
        fprintf(stderr, "cannot open group archive : %s\n", filename);
        return (-1);
    }

    /* Read main header */
    if(read(grp_file_handle, &headbuf[0], GRPHDR_MAGICLEN + GRPHDR_NUMFILESLEN)
        < (GRPHDR_MAGICLEN + GRPHDR_NUMFILESLEN)) {
        fprintf(stderr, "group archive header truncated\n");
        close(grp_file_handle);
        return (-1);
    }

    /* Check file type */
    if(strncmp(&headbuf[0], GRPHDR_MAGIC, GRPHDR_MAGICLEN) != 0) {
        fprintf(stderr, "unrecognized group archive : %s\n", filename);
        close(grp_file_handle);
        return (-1);
    }

    /* Get number of files */
    (*num_files) = le32toh(*((uint32_t *)&headbuf[GRPHDR_MAGICLEN]));

    uint32_t i = 0;
    while(i < (*num_files)) {
        /* Backup current structure pointer and initialize a new structure */
        previous = current;
        if((current = malloc(sizeof(struct grp_file))) == NULL) {
            fprintf(stderr, "cannot allocate memory\n");
            close(grp_file_handle);
            return (-1);
        }

        /* Set head on first pass */
        if(*head == NULL)
            *head = current;

        /* Read group archive's next 16 bytes */
        if(read(grp_file_handle, &filebuf[0],
            GRPHDR_FILENAMELEN + GRPHDR_FILESIZELEN)
            < (GRPHDR_FILENAMELEN + GRPHDR_FILESIZELEN)) {
            fprintf(stderr, "group archive header truncated\n");
            close(grp_file_handle);
            return (-1);
        }

        /* Update current structure */
        current->index = i + 1;
        strncpy(&current->file_name[0], &filebuf[0], GRPHDR_FILENAMELEN);
        current->file_name[GRPHDR_FILENAMELEN] = '\0';
        current->file_size =
            le32toh(*((uint32_t *)&filebuf[GRPHDR_FILENAMELEN]));
        current->file_offset =
            (previous == NULL) ?
            /* first file */
            (GRPHDR_MAGICLEN + GRPHDR_NUMFILESLEN +
            ((GRPHDR_FILENAMELEN + GRPHDR_FILESIZELEN) * (*num_files))) :
            /* next ones */
            (previous->file_offset + previous->file_size);
        current->next = NULL; /* will be set in next pass */

        /* Set previous' next pointer */
        if(previous != NULL)
            previous->next = current;

        i++;
    }
    return (grp_file_handle);
}

/* Un-initialize grp_file structures */
void
uninit_grp_files(int grp_file_handle, struct grp_file *head)
{
    struct grp_file *current = head;
    struct grp_file *next;

    while(current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    close(grp_file_handle);
    return;
}

/* Dump grp_file structures */
void
dump_grp_files(struct grp_file *head, uint8_t verbose)
{
    struct grp_file *current = head;

    while(current != NULL) {
        if(verbose == 1)
            fprintf(stdout, "%s (%d bytes, offset %d (0x%x))\n",
                current->file_name, current->file_size,
                (int)current->file_offset,
                (int)current->file_offset);
        else
            fprintf(stdout, "%s\n", current->file_name);
        current = current->next;
    }
    return;
}

/* Extract a single file from group archive */
int
extract_single_file(int grp_file_handle, const char *lookup_filename,
    const char *dest_filename, struct grp_file *head, uint8_t verbose)
{
    struct grp_file *current = head;
    int dest_file_handle;
#define RBUF_SIZE   512
    char rbuf[RBUF_SIZE]; /* our read buffer */

    if((lookup_filename == NULL) || (dest_filename == NULL)) {
        fprintf(stderr, "%s(): invalid argument\n", __func__);
        return (-1);
    }

    while(current != NULL) {
        /* If requested file found */
        if(strncmp(lookup_filename, current->file_name, GRPHDR_FILENAMELEN)
            == 0) {
            if(verbose == 1)
                fprintf(stdout, "%s\n", lookup_filename);

            /* Open output file */
            if((dest_file_handle =
                open(dest_filename, O_WRONLY|O_CREAT|O_TRUNC, 0660)) < 0) {
                fprintf(stderr, "cannot create destination file : %s\n",
                    dest_filename);
                return (-1);
            }

            /* Seek to file position and copy data */
            lseek(grp_file_handle, current->file_offset, SEEK_SET);

            /* And copy file to destination */
            uint32_t remaining_bytes = current->file_size;
            int bytes_read;
            while((bytes_read =
                read(grp_file_handle, &rbuf[0], (remaining_bytes > RBUF_SIZE) ?
                RBUF_SIZE : remaining_bytes)) > 0) {
                if(write(dest_file_handle, &rbuf[0], bytes_read) < bytes_read) {
                    fprintf(stderr, "incomplete write to destination file : "
                        "%s\n", dest_filename);
                    close(dest_file_handle);
                    return (-1);
                }
                remaining_bytes -= bytes_read;
            }

            /* Has whole file been copied ? */
            if(remaining_bytes > 0) {
                fprintf(stderr, "file partially extracted : %s\n",
                    lookup_filename);
                close(dest_file_handle);
                return (-1);
            }

            close(dest_file_handle);
            return (0);
        }
        current = current->next;
    }
    fprintf(stderr, "%s : not found in group archive\n", lookup_filename);
    return (-1);
}

/* Extract all files from group archive */
int
extract_all_files(int grp_file_handle, const char *base_path,
    struct grp_file *head, uint8_t verbose)
{
    struct grp_file *current = head;
    char *dest_path;
    int err = 0;

    if(base_path == NULL) {
        fprintf(stderr, "%s(): invalid argument\n", __func__);
        return (-1);
    }

    while(current != NULL) {
        dest_path = (char *)malloc(strlen(base_path) + 1 +
            strlen(current->file_name) + 1); /* includes '/' and final '\0' */
        if(dest_path == NULL) {
            fprintf(stderr, "cannot allocate memory\n");
            return (-1);
        }
        dest_path[0] = '\0';
        strcat(dest_path, base_path);
        strcat(dest_path, "/");
        strcat(dest_path, current->file_name);

        err |= extract_single_file(grp_file_handle,
            current->file_name, dest_path, head, verbose);

        free(dest_path);
        current = current->next;
    }
    return (err);
}

/* Print grpar version */
void
version(void)
{
    fprintf(stderr, "grpar, v." GRPAR_VERSION 
        ", (c) 2010 - Ganael LAPLANCHE, http://contribs.martymac.org\n");
}

/* Print grpar usage */
void
usage(void)
{
    version();
    fprintf(stderr, "usage: grpar [-h] [-V] [-t|-x] [-C path] [-v] "
        "-f grp_file [file_1] [file_2] [...]\n");
    fprintf(stderr, "-h : this help\n");
    fprintf(stderr, "-V : version\n");
    fprintf(stderr, "-t : list files from group archive\n");
    fprintf(stderr, "-x : extract files from group archive\n");
    fprintf(stderr, "-C : specify destination directory\n");
    fprintf(stderr, "-v : verbose mode\n");
    fprintf(stderr, "-f : group archive\n");
    return;
}

/* Initialize global options structure */
void
init_options(struct program_options *options)
{
    /* Set default options */
    options->grp_filename = NULL;
    options->dst_dirname = NULL;
    options->action = ACTION_NONE;
    options->verbose = 0;
}

/* Un-initialize global options structure */
void
uninit_options(struct program_options *options)
{
    if(options->grp_filename != NULL)
        free(options->grp_filename);
    if(options->dst_dirname != NULL)
        free(options->dst_dirname);
    options->action = ACTION_NONE;
    options->verbose = 0;
}

int
main(int argc, char **argv)
{
    int ch;

    struct grp_file *head = NULL;
    uint32_t num_files = 0;
    int grp_file_handle;

    /* Program options */
    struct program_options options;

    /* Set default options */
    init_options(&options);

    if(argc <= 1) {
         usage();
         uninit_options(&options);
         return (1);
    }

    /* Options handling */
    while ((ch = getopt(argc, argv, "?hVtxC:vf:")) != -1) {
        switch(ch) {
            case '?':
            case 'h':
                usage();
                uninit_options(&options);
                return (0);
                break;
            case 'V':
                version();
                uninit_options(&options);
                return (0);
                break;
            case 't':
                if(options.action != ACTION_NONE) {
                    fprintf(stderr, "please specify either -t or -x option, "
                        "not both\n");
                    uninit_options(&options);
                    return (1);
                }
                options.action = ACTION_LIST;
                break;
            case 'x':
                if(options.action != ACTION_NONE) {
                    fprintf(stderr, "please specify either -t or -x option, "
                        "not both\n");
                    uninit_options(&options);
                    return (1);
                }
                options.action = ACTION_EXTRACT;
                break;
            case 'C':
                options.dst_dirname = malloc(strlen(optarg) + 1);
                if(options.dst_dirname == NULL) {
                    fprintf(stderr, "cannot allocate memory\n");
                    uninit_options(&options);
                    return (1);
                }
                strcpy(options.dst_dirname, optarg); 
                break;
            case 'v':
                options.verbose = 1;
                break;
            case 'f':
                options.grp_filename = malloc(strlen(optarg) + 1);
                if(options.grp_filename == NULL) {
                    fprintf(stderr, "cannot allocate memory\n");
                    uninit_options(&options);
                    return (1);
                }
                strcpy(options.grp_filename, optarg); 
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if(options.action == ACTION_NONE) {
        fprintf(stderr, "please specify either -t or -x option\n");
        uninit_options(&options);
        return (1);
    }

    if(options.grp_filename == NULL) {
        fprintf(stderr, "please specify a group archive\n");
        uninit_options(&options);
        return (1);
    }

    /* Load grp file TOC into memory */
    if((grp_file_handle = init_grp_files(options.grp_filename, &head,
        &num_files)) < 0) {
        fprintf(stderr, "error reading group archive TOC\n");
        uninit_grp_files(grp_file_handle, head);
        uninit_options(&options);
        return (1);
    }

    /* Let's go */
    if(options.action == ACTION_LIST) {
        dump_grp_files(head, options.verbose);
        if(options.verbose == 1)
            fprintf(stdout, "%d files found\n", num_files);
    }
    else if(options.action == ACTION_EXTRACT) {
        struct stat dst_dirname_stat;

        /* Set default destination directory */
        if(options.dst_dirname == NULL) {
            options.dst_dirname = malloc(strlen(".") + 1);
            if(options.dst_dirname == NULL) {
                fprintf(stderr, "cannot allocate memory\n");
                uninit_grp_files(grp_file_handle, head);
                uninit_options(&options);
                return (1);
            }
            strcpy(options.dst_dirname, "."); 
        }

        /* Cleanup destination directory */
        while((strlen(options.dst_dirname) > 0) && 
            (options.dst_dirname[strlen(options.dst_dirname) - 1] == '/')) {
            options.dst_dirname[strlen(options.dst_dirname) - 1] = '\0';
        }

        /* Validate destination directory */
        if((stat(options.dst_dirname, &dst_dirname_stat) < 0) ||
            (!S_ISDIR(dst_dirname_stat.st_mode))) {
            fprintf(stderr, "invalid destination directory specified : %s\n",
                options.dst_dirname);
            uninit_grp_files(grp_file_handle, head);
            uninit_options(&options);
            return (1);
        }

        if(argc <= 0) {
            /* No file specified, extract everything */
            if(extract_all_files(grp_file_handle, options.dst_dirname, head,
                options.verbose) < 0) {
                fprintf(stderr, "files extracted, with error(s)\n");
            }
            else {
                if(options.verbose == 1)
                    fprintf(stdout, "%d files extracted\n", num_files);
            }
        }
        else {
            int i;
            char *dest_path = NULL;
            /* Extract specified file(s) */
            for(i = 0 ; i < argc ; i++) {
                dest_path = (char *)malloc(strlen(options.dst_dirname) + 1 +
                    strlen(argv[i]) + 1); /* includes '/' and final '\0' */
                if(dest_path == NULL) {
                    fprintf(stderr, "cannot allocate memory\n");
                    uninit_grp_files(grp_file_handle, head);
                    uninit_options(&options);
                    return (1);
                }
                dest_path[0] = '\0';
                strcat(dest_path, options.dst_dirname);
                strcat(dest_path, "/");
                strcat(dest_path, argv[i]);
                extract_single_file(grp_file_handle, argv[i], dest_path, head,
                    options.verbose);
                free(dest_path);
            }
        }
    }
    else {
        /* NOTREACHED */
        fprintf(stderr, "congratulations, you have reached an unreachable part "
            "of the code\n");
        uninit_grp_files(grp_file_handle, head);
        uninit_options(&options);
        return (1);
    }
    uninit_grp_files(grp_file_handle, head);
    uninit_options(&options);
    return (0);
}
