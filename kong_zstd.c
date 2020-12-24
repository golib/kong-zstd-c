/*
 * Copyright (c) 2020-present, Spring MC, QTT, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <zstd.h>
#include "base64.h"
#include "kong_zstd.h"

void EnableDebug()
{
    isDebug = 1;
    LOGF("[INFO] enable debug(%d) ...", isDebug);
}

void DisableDebug()
{
    isDebug = -1;
    LOGF("[INFO] disable debug(%d) ...", isDebug);
}

void AddDict(GoString name, GoString filename)
{
    if (globalCDicts.len >= dictLen || globalDDicts.len >= dictLen)
    {
        /* error */
        LOGF("[AddDict] %s : dict size is exceeded, limited to %d", name.p, dictLen);
        exit(ERROR_maxDicts);
    }

    LOGF("[INFO] add dict(%s) with %s ...", name.p, filename.p);

    char* const dictFilename = (char* const)filename.p;

    struct GoCDict cdict = {name, createCDict_orDie(dictFilename, 3)};
    struct GoDDcit ddict = {name, createDDict_orDie(dictFilename)};

    globalCDicts.cdicts[globalCDicts.len++] = cdict;
    globalDDicts.ddicts[globalDDicts.len++] = ddict;
}

void ReleaseDict()
{
    int i;
    for (i = 0; i < globalCDicts.len; i++)
    {
        LOGF("[INFO] free cdict(%s) ...", globalCDicts.cdicts[i].key.p);
        ZSTD_freeCDict(globalCDicts.cdicts[i].cdict);
    }

    int j;
    for (j = 0; j < globalDDicts.len; j++)
    {
        LOGF("[INFO] free ddict(%s) ...", globalDDicts.ddicts[i].key.p);
        ZSTD_freeDDict(globalDDicts.ddicts[i].ddict);
    }
}

struct GoCompressResult Compress(GoString gs)
{
    GoCompressResult result = {NULL, -1};

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd compress: data=%s, size=%zu", gs.p, gs.n);
    }
    size_t rSize = (size_t)gs.n;
    void* const rBuff = (void* const)gs.p;

    /* Compress */
    size_t const cBuffSize = ZSTD_compressBound(rSize);
    void* const cBuff = malloc(cBuffSize);

    size_t const cSize = ZSTD_compress(cBuff, cBuffSize, rBuff, rSize, 3);
    if (CHECK_ZSTD(cSize, "invalid compress size of zstd") != 0)
    {
        free(cBuff);

        return result;
    }

    result.data = cBuff;
    result.size = cSize;

    return result;
}

struct GoDecompressResult Decompress(GoString gs)
{
    GoDecompressResult result = { NULL, -1 };

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress: data=%s, size=%zu", gs.p, gs.n);
    }
    size_t cSize = (size_t)gs.n;
    void* const cBuff = (void* const)gs.p;

    /* Read the content size from the frame header. For simplicity we require
     * that it is always present. By default, zstd will write the content size
     * in the header when it is known. If you can't guarantee that the frame
     * content size is always written into the header, either use streaming
     * decompression, or ZSTD_decompressBound().
     */
    unsigned long long const rSize = ZSTD_getFrameContentSize(cBuff, cSize);
    if (CHECK(rSize != ZSTD_CONTENTSIZE_ERROR, "invalid compressed data of zstd") != 0)
    {
        return result;
    }
    if (CHECK(rSize != ZSTD_CONTENTSIZE_UNKNOWN, "original size is unknown for zstd") != 0)
    {
        return StreamDecompress(gs);
    }


    /* Decompress.
     * If you are doing many decompressions, you may want to reuse the context
     * and use ZSTD_decompressDCtx(). If you want to set advanced parameters,
     * use ZSTD_DCtx_setParameter().
     */
    void* const rBuff = malloc_orDie((size_t)rSize);

    size_t const dSize = ZSTD_decompress(rBuff, rSize, cBuff, cSize);
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress: data=%s, size=%zu, rsize=%llu, dsize=%zu", gs.p, gs.n, rSize, dSize);
    }
    if (CHECK_ZSTD(dSize, "invalid decompress size of zstd") != 0)
    {
        free(rBuff);

        return result;
    }

    /* When zstd knows the content size, it will error if it doesn't match. */
    if (CHECK(dSize == rSize, "Impossible because zstd will check this condition!") != 0)
    {
        free(rBuff);

        return result;
    }

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress: result=%s, rsize=%llu, dsize=%zu", (char *)rBuff, rSize, dSize);
    }
    result.data = rBuff;
    result.size = rSize;

    return result;
}

struct GoCompressResult CompressWithDict(GoString gs, GoString dict)
{
    GoCompressResult result = {NULL, -1};

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd compress with dict: key=%s, data=%s, size=%zu", dict.p, gs.p, gs.n);
    }
    ZSTD_CDict* cdict = load_cdict(dict);
    if (CHECK(cdict != NULL, "cannot load cdict: key=%s", dict.p) != 0)
    {
        return result;
    }

    size_t rSize = (size_t)gs.n;
    void* const rBuff = (void* const)gs.p;

    /* Compress with dict */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    if (CHECK(cctx != NULL, "ZSTD_createCCtx() failed!") != 0)
    {
        return result;
    }

    size_t const cBuffSize = ZSTD_compressBound(rSize);
    void* const cBuff = malloc_orDie(cBuffSize);

    size_t const cSize = ZSTD_compress_usingCDict(cctx, cBuff, cBuffSize, rBuff, rSize, cdict);
    ZSTD_freeCCtx(cctx);

    if (CHECK_ZSTD(cSize, "invalid compress size of zstd with dict") != 0)
    {
        free(cBuff);

        return result;
    }

    result.data = cBuff;
    result.size = cSize;

    return result;
}

struct GoDecompressResult DecompressWithDict(GoString gs, GoString dict)
{
    GoDecompressResult result = { NULL, -1 };

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress with dict: key=%s, data=%s, size=%zu", dict.p, gs.p, gs.n);
    }
    ZSTD_DDict* ddict = load_ddict(dict);
    if (CHECK(ddict != NULL, "cannot load ddict: key=%s", dict.p) != 0)
    {
        return result;
    }

    size_t cSize = (size_t)gs.n;
    void* const cBuff = (void* const)gs.p;

    /* Read the content size from the frame header. For simplicity we require
     * that it is always present. By default, zstd will write the content size
     * in the header when it is known. If you can't guarantee that the frame
     * content size is always written into the header, either use streaming
     * decompression, or ZSTD_decompressBound().
     */
    unsigned long long const rSize = ZSTD_getFrameContentSize(cBuff, cSize);
    if (CHECK(rSize != ZSTD_CONTENTSIZE_ERROR, "invalid compressed data of zstd") != 0)
    {
        return result;
    }
    if (CHECK(rSize != ZSTD_CONTENTSIZE_UNKNOWN, "original size is unknown for zstd") != 0)
    {
        return StreamDecompressWithDict(gs, dict);
    }

    /* Check that the dictionary ID matches.
     * If a non-zstd dictionary is used, then both will be zero.
     * By default zstd always writes the dictionary ID into the frame.
     * Zstd will check if there is a dictionary ID mismatch as well.
     */
    unsigned const expectedDictID = ZSTD_getDictID_fromDDict(ddict);
    unsigned const actualDictID = ZSTD_getDictID_fromFrame(cBuff, cSize);
    if (CHECK(actualDictID == expectedDictID, "ID of dict mismatch: expected %u got %u", expectedDictID, actualDictID) != 0)
    {
        return result;
    }

    /* Decompress.
     * If you are doing many decompressions, you may want to reuse the context
     * and use ZSTD_decompressDCtx(). If you want to set advanced parameters,
     * use ZSTD_DCtx_setParameter().
     */
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    if (CHECK(dctx != NULL, "ZSTD_createDCtx() failed!") != 0)
    {
        return result;
    }

    void* const rBuff = malloc_orDie((size_t)rSize);

    size_t const dSize = ZSTD_decompress_usingDDict(dctx, rBuff, rSize, cBuff, cSize, ddict);
    ZSTD_freeDCtx(dctx);

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress with dict: data=%s, size=%zu, rsize=%llu, dsize=%zu", gs.p, gs.n, rSize, dSize);
    }
    if (CHECK_ZSTD(dSize, "invalid decompress size of zstd") != 0)
    {
        free(rBuff);

        return result;
    }

    /* When zstd knows the content size, it will error if it doesn't match. */
    if (CHECK(dSize == rSize, "Impossible because zstd will check this condition!") != 0)
    {
        free(rBuff);

        return result;
    }

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress with dict: result=%s, rsize=%llu, dsize=%zu", (char *)rBuff, rSize, dSize);
    }
    result.data = rBuff;
    result.size = rSize;

    return result;
}

struct GoDecompressResult StreamDecompress(GoString gs)
{
    GoString dict = { NULL, -1 };

    return StreamDecompressWithDict(gs, dict);
}

struct GoDecompressResult StreamDecompressWithDict(GoString gs, GoString dict)
{
    GoDecompressResult result = { NULL, -1 };

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd stream decompress with dict: key=%s, data=%s, size=%zu", dict.p, gs.p, gs.n);
    }
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    if (CHECK(dctx != NULL, "ZSTD_createDCtx() failed!") != 0)
    {
        return result;
    }

    /* Apply dict if supplied */
    if (dict.n > 0)
    {
        ZSTD_DDict* ddict = load_ddict(dict);
        if (CHECK(ddict != NULL, "cannot load ddict: key=%s", dict.p) != 0)
        {
            ZSTD_freeDCtx(dctx);

            return result;
        }

        size_t const dret = ZSTD_DCtx_refDDict(dctx, ddict);
        if (CHECK_ZSTD(dret, "cannot init dict for stream decompress") != 0)
        {
            ZSTD_freeDCtx(dctx);

            return result;
        }
    }

    size_t cSize = (size_t)gs.n;
    void* const cBuff = (void* const)gs.p;

    size_t const buffOutSize = ZSTD_DStreamOutSize();
    void* const buffOut = malloc_orDie(buffOutSize);

    /* Given a valid frame, zstd won't consume the last byte of the frame
    * until it has flushed all of the decompressed data of the frame.
    * Therefore, instead of checking if the return code is 0, we can
    * decompress just check if input.pos < input.size.
    */
    ZSTD_inBuffer input = { cBuff, cSize, 0 };

    while (input.pos < input.size) {
        ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
        /* The return code is zero if the frame is complete, but there may
        * be multiple frames concatenated together. Zstd will automatically
        * reset the context when a frame is complete. Still, calling
        * ZSTD_DCtx_reset() can be useful to reset the context to a clean
        * state, for instance if the last decompression call returned an
        * error.
        */
        size_t const ret = ZSTD_decompressStream(dctx, &output , &input);
        if (CHECK_ZSTD(ret, "invalid frame of zstd") != 0)
        {
            result.data = NULL;
            result.size = -1;

            break;
        }

        if (result.size == -1 )
        {
            if (isDebug == 1)
            {
                LOGF("[DEBUG] zstd stream dict decompress: alloc data=%s, size=%zu", (char *)buffOut, output.pos);
            }
            result.data = malloc_orDie(output.pos);
            result.size = output.pos;

            memcpy(result.data, buffOut, output.pos);
        }
        else
        {
            if (isDebug == 1)
            {
                LOGF("[DEBUG] zstd stream dict decompress: realloc data=%s, size=%zu", (char *)buffOut, output.pos);
            }
            size_t const size = result.size+output.pos;
            void* const data = malloc_orDie(size);

            memcpy(data, result.data, result.size);
            memcpy(data+result.size, buffOut, output.pos);

            free(result.data);

            result.data = data;
            result.size = size;
        }
    }

    ZSTD_freeDCtx(dctx);
    free(buffOut);

    return result;
}