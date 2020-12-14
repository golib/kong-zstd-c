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
	size_t rSize = (size_t)gs.n;
	void* const rBuff = (void* const)gs.p;

	size_t const cBuffSize = ZSTD_compressBound(rSize);
	void* const cBuff = malloc(cBuffSize);

	/* Compress */
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd compress: data=%s, size=%zu", gs.p, gs.n);
    }
    size_t const cSize = ZSTD_compress(cBuff, cBuffSize, rBuff, rSize, 3);
    if (CHECK_ZSTD(cSize, "invalid compress size of zstd") != 0)
    {
        struct GoCompressResult result = {NULL, -1};
        return result;
    }

	struct GoCompressResult result = {cBuff, cSize};
	return result;
}

struct GoDecompressResult Decompress(GoString gs)
{
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
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }
    if (CHECK(rSize != ZSTD_CONTENTSIZE_UNKNOWN, "original size is unknown for zstd") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    void* const rBuff = malloc_orDie((size_t)rSize);

    /* Decompress.
     * If you are doing many decompressions, you may want to reuse the context
     * and use ZSTD_decompressDCtx(). If you want to set advanced parameters,
     * use ZSTD_DCtx_setParameter().
     */
    size_t const dSize = ZSTD_decompress(rBuff, rSize, cBuff, cSize);
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress: data=%s, size=%zu, rsize=%llu, dsize=%zu", gs.p, gs.n, rSize, dSize);
    }
    if (CHECK_ZSTD(dSize, "invalid decompress size of zstd") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    /* When zstd knows the content size, it will error if it doesn't match. */
    if (CHECK(dSize == rSize, "Impossible because zstd will check this condition!") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress: result=%s, rsize=%llu, dsize=%zu", (char *)rBuff, rSize, dSize);
    }
    struct GoDecompressResult result = {rBuff, rSize};
	return result;
}

struct GoCompressResult CompressWithDict(GoString gs, GoString dict)
{
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd load cdict: key=%s", dict.p);
    }
    ZSTD_CDict* cdict = load_cdict(dict);
    if (CHECK(cdict != NULL, "cannot load cdict: key=%s", dict.p) != 0)
    {
        struct GoCompressResult result = {NULL, -1};
        return result;
    }

	size_t rSize = (size_t)gs.n;
	void* const rBuff = (void* const)gs.p;

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
	size_t const cBuffSize = ZSTD_compressBound(rSize);
	void* const cBuff = malloc_orDie(cBuffSize);

	/* Compress with dict */
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd compress with dict: data=%s, size=%zu", gs.p, gs.n);
    }
    size_t const cSize = ZSTD_compress_usingCDict(cctx, cBuff, cBuffSize, rBuff, rSize, cdict);
    if (CHECK_ZSTD(cSize, "invalid compress size of zstd with dict") != 0)
    {
        struct GoCompressResult result = {NULL, -1};
	    return result;
    }

    ZSTD_freeCCtx(cctx);

	struct GoCompressResult result = {cBuff, cSize};
	return result;
}

struct GoDecompressResult DecompressWithDict(GoString gs, GoString dict)
{
    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd load ddict: key=%s", dict.p);
    }
    ZSTD_DDict* ddict = load_ddict(dict);
    if (CHECK(ddict != NULL, "cannot load ddict: key=%s", dict.p) != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
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
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }
    if (CHECK(rSize != ZSTD_CONTENTSIZE_UNKNOWN, "original size is unknown for zstd") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    void* const rBuff = malloc_orDie((size_t)rSize);

    /* Check that the dictionary ID matches.
     * If a non-zstd dictionary is used, then both will be zero.
     * By default zstd always writes the dictionary ID into the frame.
     * Zstd will check if there is a dictionary ID mismatch as well.
     */
    unsigned const expectedDictID = ZSTD_getDictID_fromDDict(ddict);
    unsigned const actualDictID = ZSTD_getDictID_fromFrame(cBuff, cSize);
    if (CHECK(actualDictID == expectedDictID, "ID of dict mismatch: expected %u got %u", expectedDictID, actualDictID) != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
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
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    size_t const dSize = ZSTD_decompress_usingDDict(dctx, rBuff, rSize, cBuff, cSize, ddict);
    ZSTD_freeDCtx(dctx);

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress with dict: data=%s, size=%zu, rsize=%llu, dsize=%zu", gs.p, gs.n, rSize, dSize);
    }
    if (CHECK_ZSTD(dSize, "invalid decompress size of zstd") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    /* When zstd knows the content size, it will error if it doesn't match. */
    if (CHECK(dSize == rSize, "Impossible because zstd will check this condition!") != 0)
    {
        struct GoDecompressResult result = { NULL, -1 };
        return result;
    }

    if (isDebug == 1)
    {
        LOGF("[DEBUG] zstd decompress with dict: result=%s, rsize=%llu, dsize=%zu", (char *)rBuff, rSize, dSize);
    }

	struct GoDecompressResult result = {rBuff, rSize};
    return result;
}