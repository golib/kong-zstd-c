local ffi = require("ffi")
local zstd = ffi.load("/data/web/kong-plugins/current/plugins/zstd/lua-zstd/libzstd_linux_amd64.so")


ffi.cdef([[
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
typedef struct GoCompressResult { void* data; GoInt size; } GoCompressResult;
typedef struct GoDecompressResult { void *data; GoInt size; } GoDecompressResult;

extern void EnableDebug();
extern void DisableDebug();
extern void AddDict(GoString name, GoString filename);
extern void ReleaseDict();
extern struct GoCompressResult Compress(GoString src);
extern struct GoDecompressResult Decompress(GoString dst);
extern struct GoCompressResult CompressWithDict(GoString src, GoString dict);
extern struct GoDecompressResult DecompressWithDict(GoString dst, GoString dict);

]])

-- debug
zstd.EnableDebug()

-- define go types
local goStringType = ffi.metatype("GoString", {})
local goSliceType = ffi.metatype("GoSlice", {})

function AddDict(key, path)
    local dictName = goStringType(key, #key)
    local dictFilename = goStringType(path, #path)
    zstd.AddDict(dictName, dictFilename)
end

function Compress(src)
    local input = goStringType(src, #src)
    local output = zstd.Compress(input)
    local result = ffi.new("struct GoCompressResult", output)
    local data = ffi.string(result.data, result.size)
    ffi.C.free(result.data)
    return data
end

function Decompress(src)
    local c = ffi.new("char[?]", #src, src)
    local input = ffi.new("struct GoCompressResult", c, #src)
    local output = zstd.Decompress(input.data, input.size)
    local result = ffi.new("struct GoDecompressResult", output)
    local data = ffi.string(result.data, result.size)
    ffi.C.free(data)
    return data
end

function CompressWithDict(src, dictKey)
    local dict = goStringType(dictKey, #dictKey)
    local input = goStringType(src, #src)
    local output = zstd.CompressWithDict(input, dict)
    local result = ffi.new("struct GoCompressResult", output)
    local data = ffi.string(result.data, result.size)
    ffi.C.free(result.data)
    return data
end

function DecompressWithDict(src, dictKey)
    local dict = goStringType(dictKey, #dictKey)
    local input = goStringType(src, #src)
    local output = zstd.DecompressWithDict(input, dict)
    local result = ffi.new("struct GoDecompressResult", output)
    local data = ffi.string(result.data, result.size)
    ffi.C.free(result.data)
    return data
end

return {
    AddDict = AddDict,
    Compress = Compress,
    Decompress = Decompress,
    CompressWithDict = CompressWithDict,
    DecompressWithDict = DecompressWithDict,
}
