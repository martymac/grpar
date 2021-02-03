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

#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* getopt(3) */
#include <sys/types.h>

#if defined(__FreeBSD__)
/* le32toh(9) */
#include <sys/endian.h>
#elif defined(__linux__)
/* le32toh(3) */
#include <endian.h>
#endif

#ifdef _MSC_VER

#include <winsock2.h>

#if BYTE_ORDER == LITTLE_ENDIAN

#define htobe16(x) htons(x)
#define htole16(x) (x)
#define be16toh(x) ntohs(x)
#define le16toh(x) (x)

#define htobe32(x) htonl(x)
#define htole32(x) (x)
#define be32toh(x) ntohl(x)
#define le32toh(x) (x)

#define htobe64(x) htonll(x)
#define htole64(x) (x)
#define be64toh(x) ntohll(x)
#define le64toh(x) (x)

#elif BYTE_ORDER == BIG_ENDIAN

/* that would be xbox 360 */
#define htobe16(x) (x)
#define htole16(x) __builtin_bswap16(x)
#define be16toh(x) (x)
#define le16toh(x) __builtin_bswap16(x)

#define htobe32(x) (x)
#define htole32(x) __builtin_bswap32(x)
#define be32toh(x) (x)
#define le32toh(x) __builtin_bswap32(x)

#define htobe64(x) (x)
#define htole64(x) __builtin_bswap64(x)
#define be64toh(x) (x)
#define le64toh(x) __builtin_bswap64(x)

#else

#error byte order not supported

#endif

#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __PDP_ENDIAN PDP_ENDIAN

int opterr = 1, /* if error message should be printed */
    optind = 1, /* index into parent argv vector */
    optopt,     /* character checked for validity */
    optreset;   /* reset getopt */
char *optarg;   /* argument associated with option */

#define BADCH ((int)'?')
#define BADARG ((int)':')
#define EMSG ("")

/*
* getopt -- Parse argc/argv argument vector.
*/
int getopt(int nargc, char *const nargv[], const char *ostr)
{
    static char *place = EMSG; /* option letter processing */
    const char *oli;           /* option letter list index */

    if (optreset || !*place)
    {
        /* update scanning pointer */
        optreset = 0;
        if (optind >= nargc || *(place = nargv[optind]) != '-')
        {
            place = EMSG;
            return (-1);
        }
        if (place[1] && *++place == '-')
        { /* found "--" */
            ++optind;
            place = EMSG;
            return (-1);
        }
    }
    /* option letter okay? */
    if ((optopt = (int)*place++) == (int)':' ||
        !(oli = strchr(ostr, optopt)))
    {
        /*
        * if the user didn't specify '-' as an option,
        * assume it means -1.
        */
        if (optopt == (int)'-')
            return (-1);
        if (!*place)
            ++optind;
        if (opterr && *ostr != ':')
            (void)printf("illegal option -- %c\n", optopt);
        return (BADCH);
    }
    if (*++oli != ':')
    {
        /* don't need argument */
        optarg = NULL;
        if (!*place)
            ++optind;
    }
    else
    {
        /* need an argument */
        if (*place) /* no white space */
            optarg = place;
        else if (nargc <= ++optind)
        { /* no arg */
            place = EMSG;
            if (*ostr == ':')
                return (BADARG);
            if (opterr)
                (void)printf("option requires an argument -- %c\n", optopt);
            return (BADCH);
        }
        else /* white space */
            optarg = nargv[optind];
        place = EMSG;
        ++optind;
    }
    return (optopt); /* dump back option letter */
}
#endif // WIN32

#define GRPAR_VERSION "0.3"

// header disk format
typedef struct dgrp_hdr
{
    char magic[12]; // KenSilverman
    uint32_t count;
} dgrp_hdr;

// file disk format
typedef struct dgrp_file
{
    char name[12];
    uint32_t size;
} dgrp_file;

// file memory format
typedef struct grp_file
{
    char file_name[12 + 1]; // file name + '\0'
    int64_t file_size;      // file size in bytes
    int64_t file_offset;    // file offset in group archive
} grp_file;

typedef enum
{
    ACTION_NONE = 0,
    ACTION_LIST,
    ACTION_EXTRACT
} action_t;

typedef struct program_options
{
    char *grp_filename;
    char *dst_dirname;
    action_t action;
    bool verbose;
} program_options;

// Initialize grp_file structures from a grp file
// Returns a file handle on group file as well as num_files found in archive
FILE *init_grp_files(
    const char *filename,
    grp_file **head,
    uint32_t *pFileCount)
{
    assert(filename);
    assert(filename[0]);
    assert(head);
    assert(pFileCount);

    *head = NULL;
    *pFileCount = 0;

    FILE *grp_file_handle = fopen(filename, "rb");
    if (!grp_file_handle)
    {
        fprintf(stderr, "cannot open group archive : %s\n", filename);
        return NULL;
    }

    dgrp_hdr hdr = {0};
    if (fread(&hdr, 1, sizeof(hdr), grp_file_handle) != sizeof(hdr))
    {
        fprintf(stderr, "group archive header truncated\n");
        fclose(grp_file_handle);
        return NULL;
    }

    if (memcmp(hdr.magic, "KenSilverman", sizeof(hdr.magic)))
    {
        fprintf(stderr, "unrecognized group archive : %s\n", filename);
        fclose(grp_file_handle);
        return NULL;
    }

    uint32_t num_files = le32toh(hdr.count);
    grp_file *files = calloc(num_files, sizeof(*files));

    size_t offset = sizeof(dgrp_hdr) + num_files * sizeof(dgrp_file);
    for (uint32_t i = 0; i < num_files; ++i)
    {
        dgrp_file dfile = {0};
        if (fread(&dfile, 1, sizeof(dfile), grp_file_handle) != sizeof(dfile))
        {
            fprintf(stderr, "group archive header truncated\n");
            free(files);
            fclose(grp_file_handle);
            return NULL;
        }
        dfile.size = le32toh(dfile.size);

        memcpy(files[i].file_name, dfile.name, sizeof(dfile.name));
        files[i].file_size = dfile.size;
        files[i].file_offset = offset;

        offset += dfile.size;
    }

    *head = files;
    *pFileCount = num_files;
    return grp_file_handle;
}

/* Un-initialize grp_file structures */
void uninit_grp_files(FILE *grp_file_handle, grp_file *files)
{
    if (grp_file_handle)
    {
        fclose(grp_file_handle);
    }
    if (files)
    {
        free(files);
    }
}

/* Dump grp_file structures */
void dump_grp_files(grp_file *files, uint32_t fileCount, bool verbose)
{
    if (verbose)
    {
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            fprintf(stdout,
                    "%s (%d bytes, offset %d (0x%x))\n",
                    files[i].file_name,
                    (int)files[i].file_size,
                    (int)files[i].file_offset,
                    (int)files[i].file_offset);
        }
    }
    else
    {
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            fprintf(stdout, "%s\n", files[i].file_name);
        }
    }
}

/* Extract a single file from group archive */
int extract_single_file(
    FILE *grp_file_handle,
    const char *lookup_filename,
    const char *dest_filename,
    grp_file *files,
    uint32_t fileCount,
    bool verbose)
{
    assert(grp_file_handle);
    assert(lookup_filename);
    assert(dest_filename);

    for (uint32_t i = 0; i < fileCount; ++i)
    {
        if (strncmp(lookup_filename, files[i].file_name, sizeof(files[i].file_name)) != 0)
        {
            continue;
        }

        FILE *dest_file_handle = fopen(dest_filename, "wb");
        if (!dest_file_handle)
        {
            fprintf(stderr, "cannot create destination file : %s\n", dest_filename);
            return (-1);
        }

        /* Seek to file position and copy data */
        fseek(grp_file_handle, files[i].file_offset, SEEK_SET);

        /* And copy file to destination */
        size_t remaining_bytes = files[i].file_size;
        size_t bytes_read = 0;
        size_t bytes_wrote = 0;
        size_t chunkSize = 0;
        char rbuf[1024] = {0};

        while (remaining_bytes)
        {
            chunkSize = (remaining_bytes > sizeof(rbuf)) ? sizeof(rbuf) : remaining_bytes;
            bytes_read = fread(rbuf, 1, chunkSize, grp_file_handle);
            if (bytes_read != chunkSize)
            {
                fprintf(stderr, "incomplete read from source file : %s\n", files[i].file_name);
                fclose(dest_file_handle);
                return (-1);
            }
            bytes_wrote = fwrite(rbuf, 1, chunkSize, dest_file_handle);
            if (bytes_wrote != chunkSize)
            {
                fprintf(stderr, "incomplete write to destination file : %s\n", dest_filename);
                fclose(dest_file_handle);
                return (-1);
            }
            remaining_bytes -= bytes_read;
        }

        fclose(dest_file_handle);
        return (0);
    }

    fprintf(stderr, "%s : not found in group archive\n", lookup_filename);
    return (-1);
}

/* Extract all files from group archive */
int extract_all_files(
    FILE *grp_file_handle,
    const char *base_path,
    grp_file *files,
    uint32_t fileCount,
    bool verbose)
{
    int err = 0;

    assert(grp_file_handle);
    assert(base_path);
    assert(base_path[0]);

    for (uint32_t i = 0; i < fileCount; ++i)
    {
        char dest_path[260] = {0};
        strcat(dest_path, base_path);
        strcat(dest_path, "/");
        strcat(dest_path, files[i].file_name);
        err |= extract_single_file(grp_file_handle, files[i].file_name, dest_path, files, fileCount, verbose);
    }

    return err;
}

void version(void)
{
    fprintf(stderr, "grpar, v." GRPAR_VERSION ", (c) 2010 - Ganael LAPLANCHE, http://contribs.martymac.org\n");
}

void usage(void)
{
    version();
    fprintf(stderr, "usage: grpar [-h] [-V] [-t|-x] [-C path] [-v] -f grp_file [file_1] [file_2] [...]\n");
    fprintf(stderr, "-h : this help\n");
    fprintf(stderr, "-V : version\n");
    fprintf(stderr, "-t : list files from group archive\n");
    fprintf(stderr, "-x : extract files from group archive\n");
    fprintf(stderr, "-C : specify destination directory\n");
    fprintf(stderr, "-v : verbose mode\n");
    fprintf(stderr, "-f : group archive\n");
    return;
}

int main(int argc, char **argv)
{
    program_options options = {0};
    if (argc <= 1)
    {
        usage();
        return (1);
    }

    int ch = 0;
    while ((ch = getopt(argc, argv, "?hVtxC:vf:")) != -1)
    {
        switch (ch)
        {
        default:
        case '?':
        case 'h':
            usage();
            return (0);
            break;
        case 'V':
            version();
            return (0);
            break;
        case 't':
            if (options.action != ACTION_NONE)
            {
                fprintf(stderr, "please specify either -t or -x option, not both\n");
                return (1);
            }
            options.action = ACTION_LIST;
            break;
        case 'x':
            if (options.action != ACTION_NONE)
            {
                fprintf(stderr, "please specify either -t or -x option, not both\n");
                return (1);
            }
            options.action = ACTION_EXTRACT;
            break;
        case 'C':
            options.dst_dirname = _strdup(optarg);
            break;
        case 'v':
            options.verbose = 1;
            break;
        case 'f':
            options.grp_filename = _strdup(optarg);
            break;
        }
    }
    argc -= optind;
    argv += optind;

    if (!options.grp_filename)
    {
        fprintf(stderr, "please specify a group archive\n");
        return (1);
    }

    if (!options.dst_dirname)
    {
        options.dst_dirname = _strdup(".");
    }

    size_t dirlen = strlen(options.dst_dirname);
    while (dirlen && (options.dst_dirname[dirlen - 1] == '/'))
    {
        options.dst_dirname[dirlen - 1] = 0;
        dirlen = strlen(options.dst_dirname);
    }

    grp_file *files = NULL;
    uint32_t fileCount = 0;
    FILE *grp_file_handle = init_grp_files(options.grp_filename, &files, &fileCount);
    if (!grp_file_handle)
    {
        fprintf(stderr, "error reading group archive TOC\n");
        uninit_grp_files(grp_file_handle, files);
        return (1);
    }

    switch (options.action)
    {
    default:
        fprintf(stderr, "please specify either -t or -x option\n");
        break;
    case ACTION_LIST:
    {
        dump_grp_files(files, fileCount, options.verbose);
        if (options.verbose)
            fprintf(stdout, "%d files found\n", fileCount);
    }
    break;
    case ACTION_EXTRACT:
    {
        if (argc <= 0)
        {
            /* No file specified, extract everything */
            if (extract_all_files(grp_file_handle, options.dst_dirname, files, fileCount, options.verbose) < 0)
            {
                fprintf(stderr, "files extracted, with error(s)\n");
            }
            else
            {
                if (options.verbose)
                    fprintf(stdout, "%d files extracted\n", fileCount);
            }
        }
        else
        {
            /* Extract specified file(s) */
            for (int i = 0; i < argc; i++)
            {
                char dest_path[260] = {0};
                strcat(dest_path, options.dst_dirname);
                strcat(dest_path, "/");
                strcat(dest_path, argv[i]);
                extract_single_file(grp_file_handle, argv[i], dest_path, files, fileCount, options.verbose);
            }
        }
    }
    break;
    }

    uninit_grp_files(grp_file_handle, files);
    return (0);
}
