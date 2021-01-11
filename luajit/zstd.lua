local ffi = require("ffi")
local encoding = require("luajit/base64")

io.write(string.format("Running os=%s, arch=%s\n", jit.os, jit.arch))

local zstd
if jit.os == 'OSX' then
    zstd = ffi.load("lib/libzstd_darwin_amd64.so")
elseif jit.os == 'Linux' then
    zstd = ffi.load("lib/libzstd_linux_amd64.so")
end

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

/* for c free */
void free(void *ptr);

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
]])

-- define go types
local goStringType = ffi.metatype("GoString", {})

-- debug mode
zstd.EnableDebug()

-- init dict
local name = "testing"
local filename = "luajit/zstd.dict"

local dictName = goStringType(name, #name)
local dictFilename = goStringType(filename, #filename)

zstd.AddDict(dictName, dictFilename)

-- compress/decompress without dict
io.write("\n-- compress/decompress without dict\n")
local actual = "Hello, world! This is a golang zstd binding for c with luajit."

-- compress with GoString
local compressInput = goStringType(actual, #actual)
local compressOutput = zstd.Compress(compressInput)
io.write(string.format("Compressed without dict output => lua type=%s, ffi type=%s, ffi size=%d\n", type(compressOutput), ffi.typeof(compressOutput), ffi.sizeof(compressOutput)))

local compressResult = ffi.new("struct GoCompressResult", compressOutput)
io.write(string.format("Compressed without dict result => lua type=%s, ffi type=%s, size=%d\n", type(compressResult.data), ffi.typeof(compressResult.data), tonumber(compressResult.size)))

-- decompress with char* and int
local decompressData = ffi.string(compressResult.data, compressResult.size)
ffi.C.free(compressResult.data)

local decompressInput = goStringType(decompressData, #decompressData)
local decompressOutput = zstd.Decompress(decompressInput)
local decompressResult = ffi.new("struct GoDecompressResult", decompressOutput)
io.write(string.format("Decompressed without dict output => lua type=%s, ffi type=%s, size=%d\n", type(decompressResult.data), ffi.typeof(decompressResult.data), tonumber(decompressResult.size)))
io.write(string.format("Decompressed without dict output => %s\n", ffi.string(decompressResult.data, decompressResult.size)))
assert(ffi.string(decompressResult.data, decompressResult.size) == actual)
ffi.C.free(decompressResult.data)

-- compress/decompress with dict
io.write("\n-- compress/decompress with dict\n")
local dictActual = "Hello, world! This is a golang zstd binding for c with luajit and dict."

-- compress with GoString by dict
local dictCompressInput = goStringType(dictActual, #dictActual)
local dictCompressOutput = zstd.CompressWithDict(dictCompressInput, dictName)
io.write(string.format("Compressed with dict output => lua type=%s, ffi type=%s, ffi size=%d\n", type(dictCompressOutput), ffi.typeof(dictCompressOutput), ffi.sizeof(dictCompressOutput)))

local dictCompressResult = ffi.new("struct GoCompressResult", dictCompressOutput)
io.write(string.format("Compressed with dict result => lua type=%s, ffi type=%s, size=%d\n", type(dictCompressResult.data), ffi.typeof(dictCompressResult.data), tonumber(dictCompressResult.size)))

-- decompress with char* and int by dict
local dictDecompressData = ffi.string(dictCompressResult.data, dictCompressResult.size)
ffi.C.free(dictCompressResult.data)

local dictDecompressInput = goStringType(dictDecompressData, #dictDecompressData)
local dictDecompressOutput = zstd.DecompressWithDict(dictDecompressInput, dictName)
local dictDecompressResult = ffi.new("struct GoDecompressResult", dictDecompressOutput)
io.write(string.format("Decompressed with dict output => %s\n", ffi.string(dictDecompressResult.data, dictDecompressResult.size)))
assert(ffi.string(dictDecompressResult.data, dictDecompressResult.size) == dictActual)
ffi.C.free(dictDecompressResult.data)

-- for ngx
io.write("\n-- for ngx\n")
local ngData = from_base64("KLUv/SA+8QEASGVsbG8sIHdvcmxkISBUaGlzIGlzIGEgZ29sYW5nIHpzdGQgYmluZGluZyBmb3IgYyB3aXRoIGx1YWppdC4=")
io.write(string.format("ngData => data=%s, size=%d\n", type(ngData), #ngData))

--local ngResult = goCompressResultType(ffi.new("char[?]", #ngData, ngData), #ngData)
local ngInput = goStringType(ngData, #ngData)

local ngOutput = zstd.Decompress(ngInput)
local ngResult = ffi.new("struct GoDecompressResult", ngOutput)
io.write(string.format("Decompressed without dict => %s\n", ffi.string(ngResult.data, ngResult.size)))
ffi.C.free(ngResult.data)

-- local counter = 0
-- repeat
--    counter = counter + 1

--    local ngDictData = from_base64("KLUv/SN7BtpvRzkCAEhlbGxvLCB3b3JsZCEgVGhpcyBpcyBhIGdvbGFuZyB6c3RkIGJpbmRpbmcgZm9yIGMgd2l0aCBsdWFqaXQgYW5kIGRpY3Qu")
--    io.write(string.format("ngDictData => data=%s, size=%d\n", type(ngData), #ngData))

--    local ngDictInput = goStringType(ngDictData, #ngDictData)

--    local ngDictOutput = zstd.DecompressWithDict(ngDictInput, dictName)
--    io.write(string.format("Decompressed with dict => %s\n", ffi.string(ngDictOutput)))
-- until(counter > 10000)

-- for nodict.data
io.write("\n-- for nodict.data\n")
local fd = io.open("./luajit/nodict.data", "rb")
local fileContent = fd:read "*a"
fd.close()

local fileInput = goStringType(fileContent, #fileContent)
local fileOutput = zstd.Decompress(fileInput)
local fileResult = ffi.new("struct GoDecompressResult", fileOutput)
io.write(string.format("Decompressed without dict output => lua type=%s, ffi type=%s, size=%d\n", type(fileResult.data), ffi.typeof(fileResult.data), tonumber(fileResult.size)))
io.write(string.format("Decompressed without dict output => %s\n", ffi.string(fileResult.data, fileResult.size)))
ffi.C.free(fileResult.data)

io.write("\n-- for nodict.data * 2\n")
local file2Content = fileContent .. fileContent
local file2Input = goStringType(file2Content, #file2Content)
local file2Output = zstd.Decompress(file2Input)
local file2Result = ffi.new("struct GoDecompressResult", file2Output)
io.write(string.format("Decompressed without dict output => lua type=%s, ffi type=%s, size=%d\n", type(file2Result.data), ffi.typeof(file2Result.data), tonumber(file2Result.size)))
io.write(string.format("Decompressed without dict output => %s\n", ffi.string(file2Result.data, file2Result.size)))
ffi.C.free(file2Result.data)