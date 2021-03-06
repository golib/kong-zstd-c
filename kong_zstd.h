/* Code generated by cmd/cgo; DO NOT EDIT. */

/* package command-line-arguments */


#include <stddef.h> /* for ptrdiff_t below */
#include <stdlib.h>    // malloc, free, exit
#include <stdio.h>     // fprintf, perror, fopen, etc.
#include <string.h>    // strerror
#include <errno.h>     // errno
#include <sys/stat.h>  // stat
#include <zstd.h>
#include "base64.h"

#ifndef KONG_ZSTD_H
#define KONG_ZSTD_H

/*
 * Define the returned error code from utility functions.
 */
typedef enum {
    ERROR_fsize = 1,
    ERROR_fopen = 2,
    ERROR_fclose = 3,
    ERROR_fread = 4,
    ERROR_fwrite = 5,
    ERROR_loadFile = 6,
    ERROR_saveFile = 7,
    ERROR_malloc = 8,
    ERROR_largeFile = 9,
    ERROR_maxDicts = 10,
} COMMON_ErrorCode;

typedef struct { const char *p; ptrdiff_t n; } _GoString_;
typedef _GoString_ GoString;

typedef signed char GoInt8;
typedef unsigned char GoUint8;
typedef short GoInt16;
typedef unsigned short GoUint16;
typedef int GoInt32;
typedef unsigned int GoUint32;
typedef long long GoInt64;
typedef unsigned long long GoUint64;
typedef GoInt64 GoInt;
typedef GoUint64 GoUint;
typedef float GoFloat32;
typedef double GoFloat64;
typedef float _Complex GoComplex64;
typedef double _Complex GoComplex128;

typedef char _check_for_64_bit_pointer_matching_GoInt[sizeof(void*)==64/8 ? 1:-1];

typedef void *GoMap;
typedef void *GoChan;
typedef struct { void *t; void *v; } GoInterface;
typedef struct { void *data; GoInt len; GoInt cap; } GoSlice;

/* Return type for Compress */
typedef struct GoCompressResult { void *data; GoInt size; } GoCompressResult;
typedef struct GoDecompressResult { void *data; GoInt size; } GoDecompressResult;

typedef struct GoCDict { GoString key; ZSTD_CDict* cdict; } GoCDict;
typedef struct GoDDcit { GoString key; ZSTD_DDict* ddict; } GoDDict;

typedef struct GlobalGoCDict { GoCDict cdicts[10]; int len; } GlobalGoCDict;
typedef struct GlobalGoDDict { GoDDict ddicts[10]; int len; } GlobalGoDDict;

/* End of boilerplate cgo prologue.  */

#ifdef __cplusplus
extern "C" {
#endif

static int dictLen = 10;
static int isDebug = -1;

static GlobalGoCDict globalCDicts = {};
static GlobalGoDDict globalDDicts = {};

extern void EnableDebug();
extern void DisableDebug();

extern void AddDict(GoString name, GoString filename);
extern void ReleaseDict();

extern struct GoCompressResult Compress(GoString src);
extern struct GoDecompressResult Decompress(GoString dst);
extern struct GoDecompressResult StreamDecompress(GoString dst);

extern struct GoCompressResult CompressWithDict(GoString src, GoString dict);
extern struct GoDecompressResult DecompressWithDict(GoString dst, GoString dict);
extern struct GoDecompressResult StreamDecompressWithDict(GoString dst, GoString dict);

/*! LOGF
 * println logs
 */
#define LOGF(fmt, ...)                                  \
    ({                                                  \
        fprintf(stderr, "%s:%d ", __FILE__, __LINE__);  \
        fprintf(stderr, fmt, __VA_ARGS__);              \
        fprintf(stderr, "\n");                          \
    })

/*! CHECK
 * Check that the condition holds. If it doesn't print a message and return NULL.
 */
#define CHECK(cond, ...)                                \
    ({                                                  \
        int ret = 0;                                    \
        if (!(cond)) {                                  \
            fprintf(stderr,                             \
                    "[ERROR] %s:%d CHECK(%s) failed: ", \
                    __FILE__,                           \
                    __LINE__,                           \
                    #cond);                             \
            fprintf(stderr, "" __VA_ARGS__);            \
            fprintf(stderr, "\n");                      \
            ret = -1;                                   \
        }                                               \
        (ret);                                          \
    })

/*! CHECK_ZSTD
 * Check the zstd error code and return NULL if an error occurred after printing a
 * message.
 */
#define CHECK_ZSTD(fn, ...)                                         \
    ({                                                              \
        size_t const err = (fn);                                    \
        int ret = CHECK(!ZSTD_isError(err),                         \
                        "zstd error: %s", ZSTD_getErrorName(err));  \
        (ret);                                                      \
    })

/*! fsize_orDie() :
 * Get the size of a given file path.
 *
 * @return The size of a given file path.
 */
static size_t fsize_orDie(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        /* error */
        perror(filename);
        exit(ERROR_fsize);
    }

    off_t const fileSize = st.st_size;
    size_t const size = (size_t)fileSize;
    /* 1. fileSize should be non-negative,
     * 2. if off_t -> size_t type conversion results in discrepancy,
     *    the file size is too large for type size_t.
     */
    if ((fileSize < 0) || (fileSize != (off_t)size)) {
        fprintf(stderr, "%s : filesize too large \n", filename);
        exit(ERROR_largeFile);
    }
    return size;
}

/*! fopen_orDie() :
 * Open a file using given file path and open option.
 *
 * @return If successful this function will return a FILE pointer to an
 * opened file otherwise it sends an error to stderr and exits.
 */
static FILE* fopen_orDie(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    perror(filename);
    exit(ERROR_fopen);
}

// /*! fclose_orDie() :
//  * Close an opened file using given FILE pointer.
//  */
// static void fclose_orDie(FILE* file)
// {
//     if (!fclose(file)) { return; };
//     /* error */
//     perror("fclose");
//     exit(ERROR_fclose);
// }

// /*! fread_orDie() :
//  *
//  * Read sizeToRead bytes from a given file, storing them at the
//  * location given by buffer.
//  *
//  * @return The number of bytes read.
//  */
// static size_t fread_orDie(void* buffer, size_t sizeToRead, FILE* file)
// {
//     size_t const readSize = fread(buffer, 1, sizeToRead, file);
//     if (readSize == sizeToRead) return readSize;   /* good */
//     if (feof(file)) return readSize;   /* good, reached end of file */
//     /* error */
//     perror("fread");
//     exit(ERROR_fread);
// }

// /*! fwrite_orDie() :
//  *
//  * Write sizeToWrite bytes to a file pointed to by file, obtaining
//  * them from a location given by buffer.
//  *
//  * Note: This function will send an error to stderr and exit if it
//  * cannot write data to the given file pointer.
//  *
//  * @return The number of bytes written.
//  */
// static size_t fwrite_orDie(const void* buffer, size_t sizeToWrite, FILE* file)
// {
//     size_t const writtenSize = fwrite(buffer, 1, sizeToWrite, file);
//     if (writtenSize == sizeToWrite) return sizeToWrite;   /* good */
//     /* error */
//     perror("fwrite");
//     exit(ERROR_fwrite);
// }

/*! loadFile_orDie() :
 * load file into buffer (memory).
 *
 * Note: This function will send an error to stderr and exit if it
 * cannot read data from the given file path.
 *
 * @return If successful this function will load file into buffer and
 * return file size, otherwise it will printout an error to stderr and exit.
 */
static size_t loadFile_orDie(const char* fileName, void* buffer, size_t bufferSize)
{
    size_t const fileSize = fsize_orDie(fileName);
    CHECK(fileSize <= bufferSize, "File too large!");

    FILE* const inFile = fopen_orDie(fileName, "rb");
    size_t const readSize = fread(buffer, 1, fileSize, inFile);
    if (readSize != (size_t)fileSize) {
        fprintf(stderr, "fread: %s : %s \n", fileName, strerror(errno));
        exit(ERROR_fread);
    }
    fclose(inFile);  /* can't fail, read only */
    return fileSize;
}

// /*! saveFile_orDie() :
//  *
//  * Save buffSize bytes to a given file path, obtaining them from a location pointed
//  * to by buff.
//  *
//  * Note: This function will send an error to stderr and exit if it
//  * cannot write to a given file.
//  */
// static void saveFile_orDie(const char* fileName, const void* buff, size_t buffSize)
// {
//     FILE* const oFile = fopen_orDie(fileName, "wb");
//     size_t const wSize = fwrite(buff, 1, buffSize, oFile);
//     if (wSize != (size_t)buffSize) {
//         fprintf(stderr, "fwrite: %s : %s \n", fileName, strerror(errno));
//         exit(ERROR_fwrite);
//     }
//     if (fclose(oFile)) {
//         perror(fileName);
//         exit(ERROR_fclose);
//     }
// }

/*! malloc_orDie() :
 * Allocate memory.
 *
 * @return If successful this function returns a pointer to allo-
 * cated memory.  If there is an error, this function will send that
 * error to stderr and exit.
 */
static void* malloc_orDie(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    perror("malloc");
    exit(ERROR_malloc);
}

/*! mallocAndLoadFile_orDie() :
 * allocate memory buffer and then load file into it.
 *
 * Note: This function will send an error to stderr and exit if memory allocation
 * fails or it cannot read data from the given file path.
 *
 * @return If successful this function will return buffer and bufferSize(=fileSize),
 * otherwise it will printout an error to stderr and exit.
 */
static void* mallocAndLoadFile_orDie(const char* fileName, size_t* bufferSize) {
    size_t const fileSize = fsize_orDie(fileName);
    *bufferSize = fileSize;
    void* const buffer = malloc_orDie(*bufferSize);
    loadFile_orDie(fileName, buffer, *bufferSize);
    return buffer;
}

/* createCDict_orDie() :
   `dictFileName` is supposed to have been created using `zstd --train` */
static ZSTD_CDict* createCDict_orDie(const char* dictFileName, int cLevel)
{
    printf("loading dictionary %s \n", dictFileName);

    size_t fileSize, dictSize;
    void* const fileBuffer = mallocAndLoadFile_orDie(dictFileName, &fileSize);
    unsigned char* dictBuffer = base64_decode(fileBuffer, fileSize, &dictSize);

    ZSTD_CDict* const cdict = ZSTD_createCDict(dictBuffer, dictSize, cLevel);
    CHECK(cdict != NULL, "ZSTD_createCDict() failed!");
    free(dictBuffer);

    return cdict;
}

static ZSTD_CDict* load_cdict(GoString dict)
{
    if (dict.n <= 0)
    {
        return NULL;
    }

    int i;
    for (i = 0; i < globalCDicts.len; i++)
    {
        if ((globalCDicts.cdicts[i].key.n == dict.n) && strcmp(globalCDicts.cdicts[i].key.p, dict.p) == 0)
        {
            return globalCDicts.cdicts[i].cdict;
        }
    }

    return NULL;
}

/* createDict_orDie() :
   `dictFileName` is supposed to have been created using `zstd --train` */
static ZSTD_DDict* createDDict_orDie(const char* dictFileName)
{
    printf("loading dictionary %s \n", dictFileName);

    size_t fileSize, dictSize;
    void* const fileBuffer = mallocAndLoadFile_orDie(dictFileName, &fileSize);
    unsigned char* dictBuffer = base64_decode(fileBuffer, fileSize, &dictSize); 

    ZSTD_DDict* const ddict = ZSTD_createDDict(dictBuffer, dictSize);
    CHECK(ddict != NULL, "ZSTD_createDDict() failed!");
    free(dictBuffer);

    return ddict;
}

static ZSTD_DDict* load_ddict(GoString dict)
{
    if (dict.n <= 0)
    {
        return NULL;
    }

    int i;
    for (i = 0; i < globalDDicts.len; i++)
    {
        if ((globalDDicts.ddicts[i].key.n == dict.n) && strcmp(globalDDicts.ddicts[i].key.p, dict.p) == 0)
        {
            return globalDDicts.ddicts[i].ddict;
        }
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* KONG_ZSTD_H */