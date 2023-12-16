// STYML - an efficient C++ single-header STrictYaML parser and emitter
//
// The MIT License (MIT)
//
// Copyright(c) 2023, Damien Feneyrou <dfeneyrou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

// ==========================================================================================
// Version
// ==========================================================================================

#define STYML_VERSION_MAJOR 1
#define STYML_VERSION_MINOR 0
#define STYML_VERSION_PATCH 0
#define STYML_VERSION       (STYML_VERSION_MAJOR * 100 * 100 + STYML_VERSION_MINOR * 100 + STYML_VERSION_PATCH)

// ==========================================================================================
// Includes and macros
// ==========================================================================================

#include <stdarg.h>

#include <cassert>
#include <climits>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_umul128)  // For Wyhash
#endif

// Macros for likely and unlikely branching
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#define STYML_LIKELY(x)   __builtin_expect(!!(x), 1)
#define STYML_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define STYML_LIKELY(x)   (x)
#define STYML_UNLIKELY(x) (x)
#endif

// Macro to check the printf-like API and detect formatting mismatch at compile time
#if defined(__GNUC__)
#define STYML_PRINTF_CHECK(formatStringIndex_, firstArgIndex_) __attribute__((__format__(__printf__, formatStringIndex_, firstArgIndex_)))
#define STYML_PRINTF_FORMAT_STRING
#elif _MSC_VER
#define STYML_PRINTF_CHECK(formatStringIndex_, firstArgIndex_)
#define STYML_PRINTF_FORMAT_STRING _Printf_format_string_
#else
#define STYML_PRINTF_CHECK(formatStringIndex_, firstArgIndex_)
#define STYML_PRINTF_FORMAT_STRING
#endif

// ==========================================================================================
// Exceptions
// ==========================================================================================

namespace styml
{

class Exception : public std::runtime_error
{
   public:
    Exception(const std::string& message) : std::runtime_error(message) {}
    Exception(const Exception&) = default;
};

class ParseException : public Exception
{
   public:
    ParseException(const std::string& message) : Exception(message) {}
    ParseException(const ParseException&) = default;
};

class AccessException : public Exception
{
   public:
    AccessException(const std::string& message) : Exception(message) {}
    AccessException(const AccessException&) = default;
};

class ConvertException : public Exception
{
   public:
    ConvertException(const std::string& message) : Exception(message) {}
    ConvertException(const ConvertException&) = default;
};

// Helpers

template<class E>
inline void STYML_PRINTF_CHECK(1, 2) throwMessage(STYML_PRINTF_FORMAT_STRING const char* format, ...)
{
    constexpr int BufferSize = 512;
    char          tmpStr[BufferSize];
    va_list       args;
    va_start(args, format);
    vsnprintf(tmpStr, BufferSize, format, args);
    va_end(args);
    throw E(std::string(tmpStr));
}

inline void STYML_PRINTF_CHECK(3, 4) throwParsing(int lineNbr, const char* lineStartPtr, STYML_PRINTF_FORMAT_STRING const char* format, ...)
{
    assert(lineStartPtr);
    constexpr const char* FooterMessage = "\n  In line %d: \"";
    constexpr int         BufferSize    = 512;
    constexpr int         LineCopySize  = 128;
    constexpr int         ReservedSize  = 32;  // Include the footer + "..." + zero termination + margin
    char                  tmpStr[BufferSize];

    // Write the main message
    va_list args;
    va_start(args, format);
    int alreadyWritten = vsnprintf(tmpStr, BufferSize - LineCopySize - ReservedSize, format, args);
    va_end(args);

    // Add footer
    alreadyWritten += snprintf(tmpStr + alreadyWritten, ReservedSize, FooterMessage, lineNbr);

    // Add the line copy
    const char* endLinePtr = lineStartPtr;
    while (*endLinePtr != '\0' && *endLinePtr != '\r' && *endLinePtr != '\n' && endLinePtr - lineStartPtr < LineCopySize) ++endLinePtr;
    int lengthToWrite = (int)(endLinePtr - lineStartPtr);

    // Add the line copy
    if (lengthToWrite > 0) {
        alreadyWritten += std::min(snprintf(tmpStr + alreadyWritten, lengthToWrite + 1, "%s", lineStartPtr),
                                   lengthToWrite);  // +1 for zero termination
        if (lengthToWrite == LineCopySize) { alreadyWritten += snprintf(tmpStr + alreadyWritten, 4, "..."); }
        snprintf(tmpStr + alreadyWritten, 2, "\"");
    }

    throw ParseException(std::string(tmpStr));
}

// ==========================================================================================
// Built-in conversions versus string
// ==========================================================================================

template<class T, class Enable = void>
struct convert {
    static std::string encode(const T& /*typedValue*/)
    {
        throwMessage<ConvertException>("No converter defined for (mangled) type '%s'", typeid(T).name());
        return "";
    }
    static void decode(const char* /*strValue*/, T& /*typedValue*/)
    {
        throwMessage<ConvertException>("No converter defined for (mangled) type '%s'", typeid(T).name());
    }
};

template<>
struct convert<const char*> {
    static std::string encode(const char* typedValue) { return std::string(typedValue); }
    static void        decode(const char* strValue, const char*& typedValue) { typedValue = strValue; }
};

template<std::size_t N>
struct convert<char[N]> {
    static std::string encode(const char* typedValue) { return std::string(typedValue); }
};

template<>
struct convert<std::string> {
    static std::string encode(const std::string& typedValue) { return typedValue; }
    static void        decode(const char* strValue, std::string& typedValue) { typedValue = strValue; }
};

template<class SignedInt>
struct convert<SignedInt, std::enable_if_t<std::is_integral<SignedInt>::value && std::is_signed<SignedInt>::value, void>> {
    static std::string encode(const SignedInt& typedValue) { return std::to_string(typedValue); }
    static void        decode(const char* strValue, SignedInt& typedValue)
    {
        errno            = 0;
        char*     endptr = nullptr;
        long long number = strtoll(strValue, &endptr, 0);
        if (endptr == strValue || errno != 0) {
            throwMessage<ConvertException>("Convert error: unable to convert the string into a signed integer: '%s'", strValue);
        }
        if (*endptr != 0) {
            throwMessage<ConvertException>(
                "Convert error: cannot convert the string into a signed integer, as there are some extra trailing characters: '%s'",
                strValue);
        }
        typedValue = (SignedInt)number;
    }
};

template<class UnsignedInt>
struct convert<UnsignedInt, std::enable_if_t<std::is_integral<UnsignedInt>::value && !std::is_signed<UnsignedInt>::value, void>> {
    static std::string encode(const UnsignedInt& typedValue) { return std::to_string(typedValue); }
    static void        decode(const char* strValue, UnsignedInt& typedValue)
    {
        errno            = 0;
        char*     endptr = nullptr;
        long long number = strtoull(strValue, &endptr, 0);
        if (endptr == strValue || errno != 0) {
            throwMessage<ConvertException>("Convert error: unable to convert the string into an unsigned integer: '%s'", strValue);
        }
        if (*endptr != 0) {
            throwMessage<ConvertException>(
                "Convert error: cannot convert the string into an unsigned integer, as there are some extra trailing characters: '%s'",
                strValue);
        }
        typedValue = (UnsignedInt)number;
    }
};

template<class Float>
struct convert<Float, std::enable_if_t<std::is_floating_point<Float>::value, void>> {
    static std::string encode(const Float& typedValue) { return std::to_string(typedValue); }
    static void        decode(const char* strValue, Float& typedValue)
    {
        errno         = 0;
        char*  endptr = nullptr;
        double number = strtod(strValue, &endptr);
        if (endptr == strValue || errno != 0) {
            throwMessage<ConvertException>("Convert error: unable to convert the string into a floating point: '%s'", strValue);
        }
        if (*endptr != 0) {
            throwMessage<ConvertException>(
                "Convert error: cannot convert the string into a floating point, as there are some extra trailing characters: '%s'",
                strValue);
        }
        typedValue = (Float)number;
    }
};

// ==========================================================================================
// Public declarations
// ==========================================================================================

enum NodeType { UNKNOWN, KEY, VALUE, SEQUENCE, MAP, COMMENT };

inline const char*
to_string(NodeType t)
{
    switch (t) {
        case UNKNOWN:
            return "Unknown";
        case KEY:
            return "Key";
        case VALUE:
            return "Value";
        case SEQUENCE:
            return "Sequence";
        case MAP:
            return "Map";
        case COMMENT:
            return "Comment";
        default:
            return "**Not a valid node type**";
    }
}

// ==========================================================================================
// Internals
// ==========================================================================================

namespace detail
{

// This structure represent one element of the tree, with a type (key, map, sequence or value), value or sub elements
#pragma pack(push, 1)
class Element
{
    static constexpr uint32_t TypeShift    = 29;                    // Type is on 3 bits
    static constexpr uint32_t CompoundMask = (1 << TypeShift) - 1;  // The 29 remaining bits are for the first data
   public:
    Element(NodeType kind) : d(((uint32_t)kind) << TypeShift), typed{0, 0, 0} {}
    Element(NodeType kind, uint32_t stringIdx, uint32_t stringSize)
        : d((((uint32_t)kind) << TypeShift) | (stringSize & CompoundMask)), typed{stringIdx, 0, 0}
    {
        assert(kind == KEY || kind == VALUE || kind == COMMENT);
    }
    Element(NodeType kind, uint32_t stringIdx, uint32_t stringSize, int eltIdx)
        : d((((uint32_t)kind) << TypeShift) | (stringSize & CompoundMask)), typed{stringIdx, 0, 0}
    {
        assert(kind == KEY);
        typed.key.eltIdx = eltIdx;
    }
    Element(Element&& rhs) noexcept
    {
        memcpy(this, (char*)&rhs, sizeof(Element));
        memset((char*)&rhs, 0, sizeof(Element));
    }
    Element(const Element& rhs) = delete;
    ~Element()
    {
        if (getType() == MAP || getType() == SEQUENCE) { clearSubs(); }
    }

    void reset(NodeType kind)
    {
        if (getType() == SEQUENCE || getType() == MAP) { clearSubs(); }
        d             = ((uint32_t)kind) << TypeShift;
        typed.unknown = {0, 0, 0};
    }

    void add(uint32_t eltIdx)
    {
        if (getType() == KEY) {
            typed.key.eltIdx = eltIdx;
        } else {
            assert(getType() == SEQUENCE || getType() == MAP);
            ensureSpaceForOne();
            typed.container.subs[typed.container.subQty++] = eltIdx;
        }
    }
    uint32_t getKeyValue() const
    {
        assert(getType() == KEY);
        return typed.key.eltIdx;  // Cannot be zero in practice, as it is root. So zero means no value
    }
    void insert(uint32_t idx, uint32_t eltIdx)
    {
        assert(getType() == SEQUENCE || getType() == MAP);
        assert(idx <= typed.container.subQty);
        ensureSpaceForOne();
        if (idx < typed.container.subQty) {
            memmove(typed.container.subs + idx + 1, typed.container.subs + idx, (typed.container.subQty - idx) * sizeof(int));
        }
        typed.container.subs[idx] = eltIdx;
        ++typed.container.subQty;
    }
    void erase(uint32_t idx)
    {
        assert(getType() == SEQUENCE || getType() == MAP);
        assert(idx < typed.container.subQty);
        if (idx < (--typed.container.subQty)) {
            memmove(typed.container.subs + idx, typed.container.subs + idx + 1, (typed.container.subQty - idx) * sizeof(int));
        }
    }
    void replace(uint32_t idx, uint32_t newEltIdx)  // NOLINT
    {
        assert(getType() == SEQUENCE || getType() == MAP);
        assert(idx < typed.container.subQty);
        typed.container.subs[idx] = newEltIdx;
    }
    void clearSubs()
    {
        assert(getType() == SEQUENCE || getType() == MAP);
        delete[] typed.container.subs;
        typed.container.subs   = nullptr;
        typed.container.subQty = 0;
        setCompound(0);  // Clear capacity
    }

    void setString(uint32_t stringIdx, uint32_t stringSize)
    {
        assert(getType() == KEY || getType() == VALUE);
        setCompound(stringSize);
        typed.key.stringIdx = stringIdx;
    }

    void setComment(uint32_t eltIdx)
    {
        assert(getType() != UNKNOWN);
        assert(eltIdx != 0);
        if (getType() == COMMENT) {
            typed.comment.commentIdx = eltIdx;
        } else if (getType() == KEY) {
            typed.key.commentIdx = eltIdx;
        } else if (getType() == VALUE) {
            typed.value.commentIdx = eltIdx;
        } else if (getType() == MAP || getType() == SEQUENCE) {
            add(eltIdx);  // No dedicated field, so we just add the comment node
        }
    }

    uint32_t getNextCommentIndex() const
    {
        if (getType() == COMMENT) {
            return typed.comment.commentIdx;
        } else if (getType() == KEY) {
            return typed.key.commentIdx;
        } else if (getType() == VALUE) {
            return typed.value.commentIdx;
        }
        return 0;  // In containers, no need to piggyback as we already have a container
    }

    void setStandalone()
    {
        assert(getType() == COMMENT);
        typed.comment.isStandalone = 1;
    }

    bool isStandalone() const
    {
        assert(getType() == COMMENT);
        return (typed.comment.isStandalone == 1);
    }

    NodeType getType() const { return (NodeType)(d >> TypeShift); }

    uint32_t getStringSize() const
    {
        assert(getType() == KEY || getType() == VALUE || getType() == COMMENT);
        return getCompound();
    }
    uint32_t getStringIdx() const
    {
        assert(getType() == KEY || getType() == VALUE || getType() == COMMENT);
        return typed.key.stringIdx;  // Works also for value
    }
    uint32_t getSubQty() const
    {
        if (getType() == KEY) { return (typed.key.eltIdx == 0) ? 0 : 1; }
        assert(getType() == MAP || getType() == SEQUENCE);
        return typed.container.subQty;
    }

    uint32_t* getSubs() const
    {
        assert(getType() == MAP || getType() == SEQUENCE);
        return typed.container.subs;
    }
    uint32_t getSub(uint32_t idx) const
    {
        assert(getType() == MAP || getType() == SEQUENCE);
        assert(idx < typed.container.subQty);
        return typed.container.subs[idx];
    }

   private:
    void ensureSpaceForOne()
    {
        if (typed.container.subQty >= getCompound()) {
            uint32_t subCapacity = std::max((uint32_t)1, 2 * getCompound());
            setCompound(subCapacity);
            uint32_t* newSubs = new uint32_t[subCapacity];
            if (typed.container.subQty) { memcpy(newSubs, typed.container.subs, typed.container.subQty * sizeof(uint32_t)); }
            delete[] typed.container.subs;
            typed.container.subs = newSubs;
        }
    }

    // Untyped structures
    uint32_t getCompound() const { return (d & CompoundMask); }  // Semantic depends on the type
    void     setCompound(uint32_t value) { d = (d & (~CompoundMask)) | (value & CompoundMask); }

    struct TypeUnknown {
        uint32_t reserved1;
        uint32_t reserved2;
        uint32_t reserved3;
    };
    struct TypeKey {
        // Compound is stringSize
        uint32_t stringIdx;
        uint32_t eltIdx;
        uint32_t commentIdx;  // 0 means None
    };
    struct TypeValue {
        // Compound is stringSize
        uint32_t stringIdx;
        uint32_t commentIdx;  // 0 means None
    };
    struct TypeContainer {
        // Compound is subCapacity
        uint32_t  subQty;
        uint32_t* subs;
    };
    struct TypeComment {
        // Compound is stringSize
        uint32_t stringIdx;
        uint32_t isStandalone;  // Zero means to display with the previous Element. Else standalone line comment.
        uint32_t commentIdx;    // Optional next comment element idx
    };

    // Fields
    uint32_t d;  // Compound data type and data, depending on the coded type
    union {
        TypeUnknown   unknown;
        TypeKey       key;
        TypeValue     value;
        TypeContainer container;
        TypeComment   comment;
    } typed;
};
#pragma pack(pop)

// ==========================================================================================
// Wyhash https://github.com/wangyi-fudan/wyhash/tree/master (18a25157b modified)
// This is free and unencumbered software released into the public domain under The Unlicense
// (http://unlicense.org/)
// ==========================================================================================

static inline void
_wymum(uint64_t* A, uint64_t* B)
{
#if defined(_MSC_VER)
    *A = _umul128(*A, *B, B);
#else
    __uint128_t r = *A;
    r *= *B;
    *A = (uint64_t)r;
    *B = (uint64_t)(r >> 64);
#endif
}

static inline uint64_t
_wymix(uint64_t A, uint64_t B)
{
    _wymum(&A, &B);
    return A ^ B;
}
static inline uint64_t
_wyr8(const uint8_t* p)
{
    uint64_t v;  // NOLINT(cppcoreguidelines-init-variables)
    memcpy(&v, p, 8);
    return v;
}
static inline uint64_t
_wyr4(const uint8_t* p)
{
    uint32_t v;  // NOLINT(cppcoreguidelines-init-variables)
    memcpy(&v, p, 4);
    return v;
}
static inline uint64_t
_wyr3(const uint8_t* p, size_t k)
{
    return (((uint64_t)p[0]) << 16) | (((uint64_t)p[k >> 1]) << 8) | p[k - 1];
}

static inline uint64_t
wyhash(const void* key, size_t len)
{
    constexpr uint64_t secret0 = 0x2d358dccaa6c78a5ull;
    constexpr uint64_t secret1 = 0x8bb84b93962eacc9ull;
    constexpr uint64_t secret2 = 0x4b33a62ed433d4a3ull;
    constexpr uint64_t secret3 = 0x4d5a2da51de1aa47ull;
    const uint8_t*     p       = (const uint8_t*)key;
    uint64_t           seed    = 0xca813bf4c7abf0a9ull;  // seed ^= _wymix(seed ^ secret0, secret1);  with fixed seed = 0
    uint64_t           a = 0, b = 0;

    if (STYML_LIKELY(len <= 16)) {
        if (STYML_LIKELY(len >= 4)) {
            a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
            b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
        } else if (STYML_LIKELY(len > 0)) {
            a = _wyr3(p, len);
            b = 0;
        } else {
            a = b = 0;
        }
    } else {
        size_t i = len;
        if (STYML_UNLIKELY(i >= 48)) {
            uint64_t see1 = seed, see2 = seed;
            do {
                seed = _wymix(_wyr8(p) ^ secret1, _wyr8(p + 8) ^ seed);
                see1 = _wymix(_wyr8(p + 16) ^ secret2, _wyr8(p + 24) ^ see1);
                see2 = _wymix(_wyr8(p + 32) ^ secret3, _wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (STYML_LIKELY(i >= 48));
            seed ^= see1 ^ see2;
        }
        while (STYML_UNLIKELY(i > 16)) {
            seed = _wymix(_wyr8(p) ^ secret1, _wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = _wyr8(p + i - 16);
        b = _wyr8(p + i - 8);
    }
    a ^= secret1;
    b ^= seed;
    _wymum(&a, &b);
    return _wymix(a ^ secret0 ^ len, b ^ secret1);
}

// This structure contains the internal context of a document
class Context
{
    static constexpr uint64_t Empty         = 0;
    static constexpr uint64_t Tombstone     = 1;
    static constexpr uint64_t FirstValid    = 2;
    static constexpr uint64_t _maxLoad128th = (uint64_t)(0.90 * 128);  // 90% load factor with 8-associativity is ok
    static constexpr uint64_t CacheLineSize = 64;

    // Such associativity allows a table load up to 90% without compromising the performances.
    // Also, it makes it fit the cache line (KeyDirAssocQty*sizeof(Entry) = 64 = (usual) CacheLineSize)
    static constexpr uint32_t KeyDirAssocQty = 8;

   public:
    Context(size_t arenaStartReserveSize = 1024)
    {
        constexpr uint32_t InitMapSize = 16;
        arena.reserve(arenaStartReserveSize);
        resize(InitMapSize);
    }

    ~Context() { delete[] _alignedAlloc; }

    // String building
    // ===============

    void addString(const char* text, uint32_t textSize, uint32_t& stringIdx, uint32_t& stringSize)
    {
        stringIdx  = (uint32_t)arena.size();
        stringSize = textSize + 1;  // +1 for zero termination of the string
        arena.resize(arena.size() + stringSize);
        memcpy(arena.data() + stringIdx, text, textSize * sizeof(char));
        arena.back() = 0;  // So that using as 'const char*' works properly
    }

    void addString(const char* text, uint32_t textSize, Element* elt)
    {
        uint32_t stringIdx  = (int)arena.size();
        uint32_t stringSize = textSize + 1;
        arena.resize(arena.size() + stringSize);
        memcpy(arena.data() + stringIdx, text, textSize * sizeof(char));
        arena.back() = 0;
        elt->setString(stringIdx, stringSize);
    }

    void startStringSession() { sessionStartIdx = (uint32_t)arena.size(); }

    void addToSession(const char* text, uint32_t textSize)
    {
        uint32_t startChunkIdx = (int)arena.size();
        arena.resize(startChunkIdx + textSize);
        memcpy(arena.data() + startChunkIdx, text, textSize * sizeof(char));
    }

    void commitSession(uint32_t& stringIdx, uint32_t& stringSize)
    {
        arena.resize(arena.size() + 1);
        arena.back() = 0;
        stringIdx    = sessionStartIdx;
        stringSize   = (uint32_t)arena.size() - sessionStartIdx;
    }

    const char* getString(int stringIdx) const { return (const char*)(arena.data() + stringIdx); }

    // Accelerated map access
    // ======================

    uint32_t getMapChildIndex(uint32_t parentEltIdx, const char* key, uint32_t keySize, Element* parentElt)
    {
        // Important: This definition of keyHash ensures that there is no ambiguity on the retrieved value.
        // Indeed, value presence implies that both the hash and the key string match.
        // Matching hash and keys mathematically implies (due to XOR) that parentEltIdx matches too, so
        // the retrieved couple (parentEltIdx, childIndex) is unique.
        // In short, the parentEltIdx is implicitely stored in the hash, without extra storage.
        uint32_t keyHash = parentEltIdx ^ (uint32_t)wyhash(key, keySize);
        if (keyHash < FirstValid) keyHash += FirstValid;  // Infinitesimal pessimisation of a few first values of hash. Worth it.

        uint32_t mask      = (_maxEntryQty - 1) & (~(KeyDirAssocQty - 1));
        int      idx       = keyHash & mask;
        uint32_t probeIncr = 1;

        while (true) {
            uint32_t cellId = 0;
            for (; cellId < KeyDirAssocQty && _entries[idx + cellId].hash >= Tombstone; ++cellId) {
                if (_entries[idx + cellId].hash != keyHash || _entries[idx + cellId].childIndex >= parentElt->getSubQty()) continue;
                detail::Element* childElt = &elements[parentElt->getSub(_entries[idx + cellId].childIndex)];
                if (childElt->getType() == KEY && childElt->getStringSize() == keySize + 1 &&  // +1 due to zero termination included
                    strncmp(getString(childElt->getStringIdx()), key, keySize) == 0) {
                    return _entries[idx + cellId].childIndex;
                }
            }

            if (cellId < KeyDirAssocQty) { break; }  // Empty space spotted on this cache line, so key has not been found
            idx = (idx + (probeIncr * KeyDirAssocQty)) & mask;
            ++probeIncr;  // Between linear and quadratic probing
        }

        // Not found
        return UINT_MAX;
    }

    bool addMapChildIndex(uint32_t parentEltIdx, const char* key, uint32_t keySize, Element* parentElt, uint32_t childIndex)
    {
        uint32_t keyHash = parentEltIdx ^ (uint32_t)wyhash(key, keySize);
        if (keyHash < FirstValid) keyHash += FirstValid;

        uint32_t mask      = (_maxEntryQty - 1) & (~(KeyDirAssocQty - 1));
        int      idx       = keyHash & mask;
        uint32_t probeIncr = 1;
        uint32_t cellId    = 0;

        while (true) {
            for (cellId = 0; cellId < KeyDirAssocQty && _entries[idx + cellId].hash >= FirstValid; ++cellId) {
                if (_entries[idx + cellId].hash != keyHash || _entries[idx + cellId].childIndex >= parentElt->getSubQty()) continue;
                detail::Element* childElt = &elements[parentElt->getSub(_entries[idx + cellId].childIndex)];
                if (childElt->getType() == KEY && childElt->getStringSize() == keySize + 1 &&  // +1 due to zero termination included
                    strncmp(getString(childElt->getStringIdx()), key, keySize) == 0) {
                    _entries[idx + cellId].childIndex = childIndex;
                    return false;  // Replace previous value
                }
            }

            if (cellId < KeyDirAssocQty) { break; }  // Empty space spotted on this cache line, so key has not been found
            idx = (idx + (probeIncr * KeyDirAssocQty)) & mask;
            ++probeIncr;  // Between linear and quadratic probing
        }

        // Key not present: add a new entry
        _entries[idx + cellId] = {keyHash, childIndex};
        _entryQty += 1;
        if ((uint64_t)128 * _entryQty > _maxLoad128th * _maxEntryQty) { resize(2 * _maxEntryQty); }
        return true;  // New value added
    }

    uint32_t removeMapChildIndex(uint32_t parentEltIdx, const char* key, uint32_t keySize, Element* parentElt)
    {
        uint32_t keyHash = parentEltIdx ^ (uint32_t)wyhash(key, keySize);
        if (keyHash < FirstValid) keyHash += FirstValid;

        uint32_t mask      = (_maxEntryQty - 1) & (~(KeyDirAssocQty - 1));
        int      idx       = keyHash & mask;
        uint32_t probeIncr = 1;

        while (true) {
            uint32_t cellId = 0;
            for (; cellId < KeyDirAssocQty && _entries[idx + cellId].hash >= Tombstone; ++cellId) {
                if (_entries[idx + cellId].hash != keyHash || _entries[idx + cellId].childIndex >= parentElt->getSubQty()) continue;
                detail::Element* childElt = &elements[parentElt->getSub(_entries[idx + cellId].childIndex)];
                if (childElt->getType() == KEY && childElt->getStringSize() == keySize + 1 &&  // +1 due to zero termination included
                    strncmp(getString(childElt->getStringIdx()), key, keySize) == 0) {
                    int oldChildIndex      = _entries[idx + cellId].childIndex;
                    _entries[idx + cellId] = {Tombstone, UINT_MAX};
                    return oldChildIndex;
                }
            }

            if (cellId < KeyDirAssocQty) { break; }  // Empty space spotted on this cache line, so key has not been found
            idx = (idx + (probeIncr * KeyDirAssocQty)) & mask;
            ++probeIncr;  // Between linear and quadratic probing
        }

        // Key not present (weird in current project)
        assert(false && "Key not present");
        return UINT_MAX;
    }

    // Public fields
    std::vector<Element> elements;
    std::vector<uint8_t> arena;

   private:
    void resize(uint32_t newMaxSize)
    {
        // Allocate the new table
        uint8_t* newAlignedAlloc = new uint8_t[newMaxSize * sizeof(Entry) + CacheLineSize];
        Entry*   newArray        = (Entry*)(((uintptr_t)newAlignedAlloc + CacheLineSize - 1) & ~(CacheLineSize - 1));  // NOLINT
        memset(newArray, 0, newMaxSize * sizeof(Entry));

        // Transfer the data
        uint32_t newMask = (newMaxSize - 1) & (~(KeyDirAssocQty - 1));
        for (uint32_t oldIdx = 0; oldIdx < _maxEntryQty; ++oldIdx) {
            if (_entries[oldIdx].hash < FirstValid) continue;

            uint32_t newIdx    = _entries[oldIdx].hash & newMask;
            uint32_t probeIncr = 1;
            uint32_t cellId    = 0;
            while (true) {
                cellId = 0;
                while (cellId < KeyDirAssocQty && newArray[newIdx + cellId].hash >= FirstValid) ++cellId;
                if (cellId < KeyDirAssocQty) { break; }  // Empty space spotted on this cache line
                newIdx = (newIdx + (probeIncr * KeyDirAssocQty)) & newMask;
                ++probeIncr;
            }
            assert(cellId < KeyDirAssocQty && newArray[newIdx + cellId].hash < FirstValid);
            newArray[newIdx + cellId] = _entries[oldIdx];
        }

        // Replace the old array
        delete[] _alignedAlloc;
        _alignedAlloc = newAlignedAlloc;
        _entries      = newArray;
        _maxEntryQty  = newMaxSize;
    }

    // String helper
    uint32_t sessionStartIdx = 0;
    // Children access
    struct Entry {
        uint32_t hash;
        uint32_t childIndex;
    };
    uint8_t* _alignedAlloc = nullptr;  // Not easy to aligned allocate in a portable way (MSVC does not like std::align_val_t)...
    Entry*   _entries      = nullptr;
    uint32_t _entryQty     = 0;
    uint32_t _maxEntryQty  = 0;
};

struct StringHelper {
    template<int N>  // Character search size is templatized so that compiler can unroll the loop
    static uint32_t findFirstOf(const char* text, uint32_t textSize, const char* Nchars, uint32_t startPos)
    {
        uint32_t idx = startPos;
        while (idx < textSize) {
            char c = text[idx];
            for (int i = 0; i < N; ++i) {
                if (c == Nchars[i]) { return idx; }
            }
            ++idx;
        }
        return UINT_MAX;
    }

    template<int N>
    static uint32_t findFirstNotOf(const char* text, uint32_t textSize, const char* Nchars, uint32_t startPos)
    {
        uint32_t idx = startPos;
        while (idx < textSize) {
            char c    = text[idx];
            bool isOf = false;
            for (int i = 0; i < N; ++i) {
                if (c == Nchars[i]) {
                    isOf = true;
                    break;
                }
            }
            if (!isOf) return idx;
            ++idx;
        }
        return UINT_MAX;
    }

    void startSession()
    {
        arena.clear();
        chunks.clear();
        startLineIdx = 0;
    }

    void addLine(const char* text, uint32_t textSize)
    {
        // No terminating zero, as it is a line chunk representation
        uint32_t stringIdx = (uint32_t)arena.size();
        arena.resize(arena.size() + textSize);
        if (textSize > 0) { memcpy(arena.data() + stringIdx, text, textSize * sizeof(char)); }
        chunks.push_back({stringIdx, textSize});
        startLineIdx = (uint32_t)arena.size();
    }

    void addChar(const char c) { arena.push_back(c); }

    void addChunk(const char* text, uint32_t textSize)
    {
        assert(textSize < (1U << 31));
        uint32_t startIdx = (int)arena.size();
        arena.resize(startIdx + textSize);
        if (textSize > 0) { memcpy(arena.data() + startIdx, text, textSize * sizeof(char)); }
    }

    void addChunkNoTrail(const char* text, uint32_t textSize)
    {
        // Adjust the size
        while (textSize > 0 && (text[textSize - 1] == ' ' || text[textSize - 1] == '\t')) { --textSize; }

        uint32_t startIdx = (int)arena.size();
        arena.resize(startIdx + textSize);
        if (textSize > 0) { memcpy(arena.data() + startIdx, text, textSize * sizeof(char)); }
    }

    void endLine()
    {
        uint32_t newStartLineIdx = (uint32_t)arena.size();
        chunks.push_back({startLineIdx, newStartLineIdx - startLineIdx});
        startLineIdx = newStartLineIdx;
    }

    void removeTrailingLines()
    {
        while (!chunks.empty()) {
            const LineChunk& lc = chunks.back();
            if (findFirstNotOf<4>(arena.data(), lc.startIdx + lc.size, " \t\r\n", lc.startIdx) != UINT_MAX) { break; }
            chunks.pop_back();  // Empty line
        }
    }

    bool empty() const { return chunks.empty(); }

    struct LineChunk {
        uint32_t startIdx;
        uint32_t size;
    };
    std::vector<char>      arena;
    std::vector<LineChunk> chunks;
    uint32_t               startLineIdx = 0;
};

inline std::string
dumpAsPyStruct(Context* context, bool withIndent)
{
    if (!context) return "";

    constexpr uint32_t    indentSize = 2;
    constexpr const char* indentStr  = "  ";
    struct DumpItem {
        DumpItem() {}
        DumpItem(Element* node, int indent, bool isEnd, bool withPrefix, bool isLast)
            : node(node), indent(indent), isEnd(isEnd), withPrefix(withPrefix), isLast(isLast)
        {
        }
        Element* node;
        int      indent;
        bool     isEnd;
        bool     withPrefix;
        bool     isLast;
    };

    StringHelper sh;
    sh.arena.reserve(16384);
    sh.startSession();

    std::vector<DumpItem> stack{{&context->elements[0], 0, false, false, true}};
    while (!stack.empty()) {
        // Get next node to display
        Element* v          = stack.back().node;
        int      indent     = stack.back().indent;
        bool     isEnd      = stack.back().isEnd;
        bool     withPrefix = withIndent && stack.back().withPrefix;
        bool     isLast     = stack.back().isLast;
        stack.pop_back();
        assert(v);

        if (v->getType() == KEY) {
            if (v->getStringSize() > 1) {  // All cases except root
                if (withPrefix) {
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChar('\'');
                sh.addChunk(context->getString(v->getStringIdx()), v->getStringSize() - 1);
                sh.addChunk("' : ", 4);
            }
            if (v->getSubQty()) {
                stack.emplace_back(&context->elements[v->getKeyValue()], indent, false, false, isLast);
            } else {
                sh.addChunk("None", 4);
                if (!isLast) sh.addChar(',');
            }
        }

        else if (v->getType() == SEQUENCE) {
            if (isEnd) {
                if (withPrefix) {
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChar(']');
                if (!isLast) sh.addChar(',');
            } else {
                bool isOneLiner = (v->getSubQty() <= 1);
                stack.emplace_back(v, indent, true, !isOneLiner, isLast);
                if (withPrefix) {
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChar('[');
                for (int i = v->getSubQty() - 1; i >= 0; --i) {  // Reverse order
                    stack.emplace_back(&context->elements[v->getSub(i)], indent + 1, false, !isOneLiner, i == (int)(v->getSubQty() - 1));
                }
            }
        }

        else if (v->getType() == MAP) {
            if (isEnd) {
                if (withPrefix) {
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChar('}');
                if (!isLast) sh.addChar(',');
            } else {
                bool isOneLiner = (v->getSubQty() <= 1);
                stack.emplace_back(v, indent, true, !isOneLiner, isLast);
                if (withPrefix) {
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChar('{');
                for (int i = v->getSubQty() - 1; i >= 0; --i) {  // Reverse order
                    stack.emplace_back(&context->elements[v->getSub(i)], indent + 1, false, !isOneLiner, i == (int)(v->getSubQty() - 1));
                }
            }
        }

        else if (v->getType() == VALUE) {
            if (withPrefix) {
                sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
            }
            if (v->getStringSize() <= 1) {
                sh.addChunk("None", 4);
                if (!isLast) sh.addChar(',');
            } else {
                sh.addChar('"');
                // Escaping single quotes (') by replacing with double quote ('')
                const char* text     = context->getString(v->getStringIdx());
                uint32_t    textSize = v->getStringSize() - 1;  // Remove terminating zero
                uint32_t    lastPos  = 0;
                uint32_t    findPos  = 0;
                while ((findPos = StringHelper::findFirstOf<5>(text, textSize, "\\\n\r\t\"", lastPos)) != UINT_MAX) {
                    sh.addChunk(text + lastPos, (uint32_t)(findPos - lastPos));
                    if (text[findPos] == '\"') {
                        sh.addChar('\\');
                        sh.addChar('"');
                    } else if (text[findPos] == '\n') {
                        sh.addChar('\\');
                        sh.addChar('n');
                    } else if (text[findPos] == '\r') {
                        sh.addChar('\\');
                        sh.addChar('r');
                    } else if (text[findPos] == '\t') {
                        sh.addChar('\\');
                        sh.addChar('t');
                    } else if (text[findPos] == '\\') {
                        if (findPos + 1 >= textSize || (text[findPos + 1] != 'u' && text[findPos + 1] != 'U' && text[findPos + 1] != 'x')) {
                            sh.addChar('\\');
                        }
                        sh.addChar('\\');
                    }
                    lastPos = findPos + 1;
                }
                sh.addChunk(text + lastPos, textSize - lastPos);
                sh.addChar('"');
                if (!isLast) sh.addChar(',');
            }
        }

        else if (v->getType() == COMMENT) {
            // No way to emit comments in python structure
        } else if (v->getType() == UNKNOWN) {
            if (withPrefix) {
                sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
            }
            sh.addChunk("None", 4);
            if (!isLast) sh.addChar(',');
        }

        else {
            assert(false && "Undefined value type (corrupted)");
        }

    }  // End of loop on stack

    if (!sh.arena.empty() && sh.arena.back() == ',') sh.arena.pop_back();

    sh.addChar('\0');
    return std::string(sh.arena.data(), sh.arena.size());
}

inline std::string
dumpAsYaml(Context* context)
{
    if (!context) return "";

    constexpr uint32_t    indentSize = 2;
    constexpr const char* indentStr  = "  ";
    struct DumpItem {
        DumpItem() {}
        DumpItem(Element* node, int indent, NodeType parentType) : node(node), indent(indent), parentType(parentType) {}
        Element* node;
        int      indent;
        NodeType parentType;
    };

    bool         isFirst       = true;
    bool         lastIsComment = false;
    bool         lastIsKey     = false;
    StringHelper sh;
    sh.arena.reserve(16384);
    sh.startSession();

    std::vector<DumpItem> stack{{&context->elements[0], 0, context->elements[0].getType()}};
    while (!stack.empty()) {
        // Get next node to display
        Element* v          = stack.back().node;
        int      indent     = stack.back().indent;
        NodeType parentType = stack.back().parentType;
        stack.pop_back();
        assert(v);

        if (v->getType() == KEY) {
            if (v->getStringSize() > 1) {  // All cases except root
                if (parentType == SEQUENCE) {
                    ++indent;
                } else {
                    if (!isFirst) sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                }
                sh.addChunk(context->getString(v->getStringIdx()), v->getStringSize() - 1);
                sh.addChar(':');
                ++indent;
                isFirst = false;
            }
            if (v->getSubQty()) { stack.emplace_back(&context->elements[v->getKeyValue()], indent, v->getType()); }
            lastIsKey = true;
        }

        else if (v->getType() == SEQUENCE) {
            if (parentType == SEQUENCE) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                sh.addChar('-');
                sh.addChar(' ');
                ++indent;
            }
            for (int i = v->getSubQty() - 1; i >= 0; --i) {  // Reverse order
                stack.emplace_back(&context->elements[v->getSub(i)], indent, SEQUENCE);
            }
            isFirst = false;
        }

        else if (v->getType() == MAP) {
            if (parentType == SEQUENCE) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                sh.addChar('-');
                sh.addChar(' ');
                ++indent;
            }
            for (int i = v->getSubQty() - 1; i >= 0; --i) {  // Reverse order
                stack.emplace_back(&context->elements[v->getSub(i)], indent, MAP);
            }
            if (parentType == SEQUENCE) {
                stack.back().indent -= 1;
                stack.back().parentType = parentType;
            }
        }

        else if (v->getType() == VALUE) {
            if (parentType != KEY || lastIsComment) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                if (parentType == SEQUENCE) {
                    sh.addChar('-');
                    sh.addChar(' ');
                }
            }
            if (parentType == SEQUENCE) ++indent;
            if (v->getStringSize() > 1) {
                // Analyze the string for special characters
                const char* text     = context->getString(v->getStringIdx());
                uint32_t    textSize = v->getStringSize() - 1;  // Remove terminating zero
                uint32_t    idx      = 0;
                // Select the kind of emitted string: plain, single quote, double quote, literal
                // In this order:
                // 1) plain scalar, if does not start with " >|\"\'", does not end with " ", and does not contain ": " or " #", or any of
                // "\t\r\n".
                // 2) else single quote if no \n
                // 3) else double quote (arbitrary choice) if no \n in the middle (but accepted at the end as it does not help the visual.
                // Literal code is there but not used at the moment, as it may make the text harder to read
                bool isPlain      = (text[idx] != ' ' && text[idx] != '>' && text[idx] != '|' && text[idx] != '\'' && text[idx] != '\"' &&
                                text[textSize - 1] != ' ');
                int  newLineCount = 0;
                while (idx < textSize) {
                    char c = text[idx];
                    if (c == '\n') ++newLineCount;
                    if (isPlain && c == ':' && idx + 1 < textSize &&
                        (text[idx + 1] == ' ' || text[idx + 1] == '\r' || text[idx + 1] == '\n'))
                        isPlain = false;                                                             // No key-like text
                    if (isPlain && c == '#' && (idx == 0 || text[idx - 1] == ' ')) isPlain = false;  // No comment-like text
                    ++idx;
                }
                bool isSingleQuote = (newLineCount == 0);
                if (!isPlain && newLineCount > 0) {
                    // Select between case 3 and case 4 by removing the count of trailing newlines
                    int lastIdx = textSize - 1;
                    while (text[lastIdx] == '\n') {
                        --newLineCount;
                        --lastIdx;
                        if (text[lastIdx] == '\r') { --lastIdx; }
                    }
                }

                // Output the string in the right format
                if (isPlain && newLineCount == 0) {
                    if (lastIsKey) { sh.addChar(' '); }
                    sh.addChunk(text, textSize);
                } else if (isSingleQuote) {
                    if (lastIsKey) { sh.addChar(' '); }
                    sh.addChar('\'');
                    // Escaping single quotes (') by replacing with double quote ('')
                    uint32_t lastPos = 0;
                    uint32_t findPos = 0;
                    while ((findPos = StringHelper::findFirstOf<1>(text, textSize, "\'", lastPos)) != UINT_MAX) {
                        sh.addChunk(text + lastPos, findPos - lastPos);
                        sh.addChar('\'');
                        sh.addChar('\'');
                        lastPos = findPos + 1;
                    }
                    sh.addChunk(text + lastPos, textSize - lastPos);
                    sh.addChar('\'');
                } else if (true || newLineCount == 0) {  // No new line in the middle of the string?
                    if (lastIsKey) { sh.addChar(' '); }
                    sh.addChar('"');
                    uint32_t lastPos = 0;
                    uint32_t findPos = 0;
                    while ((findPos = StringHelper::findFirstOf<5>(text, textSize, "\\\n\r\t\"", lastPos)) != UINT_MAX) {
                        sh.addChunk(text + lastPos, (uint32_t)(findPos - lastPos));
                        if (text[findPos] == '\"') {
                            sh.addChar('\\');
                            sh.addChar('"');
                        } else if (text[findPos] == '\n') {
                            sh.addChar('\\');
                            sh.addChar('n');
                        } else if (text[findPos] == '\r') {
                            sh.addChar('\\');
                            sh.addChar('r');
                        } else if (text[findPos] == '\t') {
                            sh.addChar('\\');
                            sh.addChar('t');
                        } else if (text[findPos] == '\\') {
                            if (findPos + 1 >= textSize ||
                                (text[findPos + 1] != 'u' && text[findPos + 1] != 'U' && text[findPos + 1] != 'x')) {
                                sh.addChar('\\');
                            }
                            sh.addChar('\\');
                        }
                        lastPos = findPos + 1;
                    }
                    sh.addChunk(text + lastPos, textSize - lastPos);
                    sh.addChar('"');
                } else {  // Literal
                    if (textSize > 0) {
                        sh.addChar(' ');
                        sh.addChar('|');
                        sh.addChar('2');
                        if (text[textSize - 1] != '\n') {
                            sh.addChar('-');
                        } else {
                            sh.addChar('+');
                            textSize -= 1;  // Remove the last line feed
                        }
                    }
                    // Copy line by line, inserting the prefix
                    uint32_t lastPos = 0;
                    uint32_t findPos = 0;
                    while ((findPos = StringHelper::findFirstOf<1>(text, textSize, "\n", lastPos)) != UINT_MAX) {
                        sh.addChar('\n');
                        for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                        sh.addChunk(text + lastPos, (uint32_t)(findPos - lastPos));
                        lastPos = findPos + 1;  // Skip final \n
                    }
                    sh.addChar('\n');
                    for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                    sh.addChunk(text + lastPos, (uint32_t)(textSize - lastPos));
                }
                isFirst = false;
            }
        }

        else if (v->getType() == COMMENT) {
            if (v->isStandalone()) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
            } else {
                sh.addChar(' ');
            }
            sh.addChar('#');
            sh.addChunk(context->getString(v->getStringIdx()), v->getStringSize() - 1);
            lastIsComment = true;
            isFirst       = false;
        }

        else if (v->getType() == UNKNOWN) {
            if (parentType != KEY) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
                if (parentType == SEQUENCE) {
                    sh.addChar('-');
                    sh.addChar(' ');
                    ++indent;
                }
            }
        }

        else {
            assert(false && "Undefined value type");
        }

        if (v->getType() != COMMENT) { lastIsComment = false; }
        if (v->getType() != KEY) { lastIsKey = false; }

        // Comment piggybacking
        int nextCommentEltIdx = v->getNextCommentIndex();
        while (nextCommentEltIdx) {
            const Element& elt = context->elements[nextCommentEltIdx];

            if (lastIsComment || elt.isStandalone()) {
                if (!isFirst) sh.addChar('\n');
                for (int i = 0; i < indent; ++i) sh.addChunk(indentStr, indentSize);
            } else {
                sh.addChar(' ');
            }
            sh.addChar('#');
            sh.addChunk(context->getString(elt.getStringIdx()), elt.getStringSize() - 1);
            nextCommentEltIdx = elt.getNextCommentIndex();
            lastIsComment     = true;
            isFirst           = false;
        }

    }  // End of loop on stack

    sh.addChar('\0');
    return std::string(sh.arena.data(), sh.arena.size());
}

}  // namespace detail

// ==========================================================================================
// Public manipulation API
// ==========================================================================================

class Node
{
   public:
    // Constructor / destructor / copy / move
    // ======================================

    explicit Node() {}
    explicit Node(int eltIdx, detail::Context* context) : _eltIdx(eltIdx), _context(context) {}
    explicit Node(int eltIdx, detail::Context* context, std::string nonExistingKey)
        : _eltIdx(eltIdx), _context(context), _nonExistingKey(std::move(nonExistingKey))
    {
    }
    Node(Node&& rhs) noexcept
    {
        std::swap(_eltIdx, rhs._eltIdx);
        std::swap(_context, rhs._context);
        std::swap(_nonExistingKey, rhs._nonExistingKey);
    }
    Node& operator=(const Node& rhs)
    {
        _eltIdx         = rhs._eltIdx;
        _context        = rhs._context;
        _nonExistingKey = rhs._nonExistingKey;
        return *this;
    }
    Node& operator=(Node&& rhs) noexcept
    {
        std::swap(_eltIdx, rhs._eltIdx);
        std::swap(_context, rhs._context);
        std::swap(_nonExistingKey, rhs._nonExistingKey);
        return *this;
    }
    Node(const Node& rhs) : _eltIdx(rhs._eltIdx), _context(rhs._context), _nonExistingKey(rhs._nonExistingKey) {}
    ~Node() = default;

    // Generic
    // =======

    explicit operator bool() const
    {
        return (_context && _eltIdx < (uint32_t)_context->elements.size() &&
                (_context->elements[_eltIdx].getType() != MAP || _nonExistingKey.empty()));
    }

    template<class T>
    T as() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() == MAP && !_nonExistingKey.empty()) {
            throwMessage<AccessException>("Access error: unable to cast this node into (mangle) type '%s'  as the key '%s' does not exist",
                                          typeid(T).name(), _nonExistingKey.c_str());
        }
        if (elt->getType() != VALUE && elt->getType() != UNKNOWN) {
            throwMessage<AccessException>("Access error: unable to cast this node as it is not of type 'Value' but %s",
                                          to_string().c_str());
        }
        T typedValue;
        try {
            convert<T>::decode((elt->getType() == VALUE) ? _context->getString(elt->getStringIdx()) : "", typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: decoding error when accessing '%s' with 'as()':\n  %s", to_string().c_str(),
                                          e.what());
        }
        return typedValue;
    }

    template<class T>
    T as(const T& defaultValue) const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() == MAP && !_nonExistingKey.empty()) { return defaultValue; }
        if (elt->getType() != VALUE && elt->getType() != UNKNOWN) {
            throwMessage<AccessException>("Access error: unable to cast this node as it is not of type 'Value' but %s",
                                          to_string().c_str());
        }
        T typedValue;
        try {
            convert<T>::decode((elt->getType() == VALUE) ? _context->getString(elt->getStringIdx()) : "", typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: decoding error when accessing '%s' with 'as()':\n  %s", to_string().c_str(),
                                          e.what());
        }
        return typedValue;
    }

    template<class T>
    Node& operator=(const T& typedValue)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];
        std::string      encodedValue;
        try {
            encodedValue = convert<T>::encode(typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: encoding error when assigning to '%s':\n  %s", to_string().c_str(), e.what());
        }
        if (elt->getType() == VALUE) {
            _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), elt);
        } else if (!_nonExistingKey.empty()) {
            if (_context->getMapChildIndex(_eltIdx, _nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), elt) != UINT_MAX) {
                throwMessage<AccessException>("Access error: duplicated key are forbidden and the key '%s' is already present",
                                              _nonExistingKey.c_str());
            }
            assert(elt->getType() == MAP);
            uint32_t stringIdx = 0, stringSize = 0;
            _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), stringIdx, stringSize);
            uint32_t eltIdx = (uint32_t)_context->elements.size();
            _context->elements.emplace_back(VALUE, stringIdx, stringSize);  // Create the value element
            _context->addString(_nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), stringIdx, stringSize);
            _context->elements.emplace_back(KEY, stringIdx, stringSize, eltIdx);  // Create the key referring to the created value element
            _context->elements[_eltIdx].add(eltIdx + 1);                          // Add the key to the parent

            // Update the access acceleration hashtable
            _context->addMapChildIndex(_eltIdx, _nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), &_context->elements[_eltIdx],
                                       _context->elements[_eltIdx].getSubQty() - 1);
            // Clear the non existing key flag
            _nonExistingKey.clear();
        } else {
            // Turn the node into a string value
            assert(elt->getType() != KEY);
            elt->reset(VALUE);
            _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), elt);
        }
        return *this;
    }

    Node& operator=(const NodeType newKind)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (newKind != MAP && newKind != SEQUENCE) {
            throwMessage<AccessException>("Access error: only the structural elements MAP and SEQUENCE can be created, not '%s'",
                                          styml::to_string(newKind));
        }

        if (elt->getType() == MAP && !_nonExistingKey.empty()) {
            if (_context->getMapChildIndex(_eltIdx, _nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), elt) != UINT_MAX) {
                throwMessage<AccessException>("Access error: the key '%s' has already been added in the map", _nonExistingKey.c_str());
            }

            uint32_t stringIdx = 0, stringSize = 0;
            uint32_t eltIdx = (uint32_t)_context->elements.size();
            _context->elements.emplace_back(newKind);
            _context->addString(_nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), stringIdx, stringSize);
            _context->elements.emplace_back(KEY, stringIdx, stringSize, eltIdx);
            _context->elements[_eltIdx].add(eltIdx + 1);

            // Update the access acceleration hashtable
            _context->addMapChildIndex(_eltIdx, _nonExistingKey.data(), (uint32_t)_nonExistingKey.size(), &_context->elements[_eltIdx],
                                       _context->elements[_eltIdx].getSubQty() - 1);
            _nonExistingKey.clear();
        } else {
            elt->reset(newKind);  // Turn the node into an new empty structural node (array or table)
        }
        return *this;
    }

    size_t size() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP && elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'size()' can only be used on the structural elements MAP and SEQUENCE, not '%s'",
                                          to_string().c_str());
        }
        return elt->getSubQty();
    }

    NodeType type() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        NodeType t = _context->elements[_eltIdx].getType();
        return (t == UNKNOWN) ? VALUE : t;
    }

    bool isValue() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        NodeType t = _context->elements[_eltIdx].getType();
        return (t == VALUE || t == UNKNOWN);
    }

    bool isKey() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        return (_context->elements[_eltIdx].getType() == KEY);
    }

    bool isSequence() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        return (_context->elements[_eltIdx].getType() == SEQUENCE);
    }

    bool isMap() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        return (_context->elements[_eltIdx].getType() == MAP);
    }

    bool isComment() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        return (_context->elements[_eltIdx].getType() == COMMENT);
    }

    std::string keyName() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != KEY) {
            throwMessage<AccessException>("Access error: 'keyName()' can only be used on KEY elements, not '%s'", to_string().c_str());
        }
        return _context->getString(elt->getStringIdx());
    }

    Node value() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() == KEY) {
            // The value of a key is the sub element
            assert(elt->getSubQty() == 1);  // Always the case for keys
            return Node(elt->getKeyValue(), _context);
        }
        return Node(_eltIdx, _context);
    }

    // SEQUENCE specific
    // =================

    Node operator[](uint32_t idx) const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: Access by '[%d]' can only be used on SEQUENCE elements, not '%s'", idx,
                                          to_string().c_str());
        }
        if (idx >= elt->getSubQty()) {
            throwMessage<AccessException>("Access error: Access by '[%d]' is out of array bounds for '%s'", idx, to_string().c_str());
        }
        return Node(elt->getSub(idx), _context);
    }

    template<class T>
    void push_back(const T& typedValue)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'push_back(...)' can only be used on SEQUENCE elements, not '%s'",
                                          to_string().c_str());
        }
        uint32_t    stringIdx = 0, stringSize = 0;
        std::string encodedValue;
        try {
            encodedValue = convert<T>::encode(typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: encoding error when accessing '%s' with 'push_back(...)':\n  %s",
                                          to_string().c_str(), e.what());
        }
        _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), stringIdx, stringSize);
        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(VALUE, stringIdx, stringSize);
        _context->elements[_eltIdx].add(eltIdx);
    }

    void push_back(const NodeType newKind)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (newKind != MAP && newKind != SEQUENCE) {
            throwMessage<AccessException>(
                "Access error: only the structural elements MAP and SEQUENCE can be created-push_backed, not '%s'",
                styml::to_string(newKind));
        }
        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'push_back(...)' can only be used on SEQUENCE elements, not '%s'",
                                          to_string().c_str());
        }

        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(newKind);
        _context->elements[_eltIdx].add(eltIdx);
    }

    template<class T>
    void insert(uint32_t idx, const T& typedValue)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'insert()' can only be used on SEQUENCE elements, not '%s'", to_string().c_str());
        }
        if (idx > elt->getSubQty()) {
            throwMessage<AccessException>("Access error: Access by 'insert(%d, ...)' is out of array bounds for '%s'", idx,
                                          to_string().c_str());
        }
        uint32_t    stringIdx = 0, stringSize = 0;
        std::string encodedValue;
        try {
            encodedValue = convert<T>::encode(typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: encoding error when accessing '%s' with 'insert(%d, ...)':\n  %s",
                                          to_string().c_str(), idx, e.what());
        }
        _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), stringIdx, stringSize);
        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(VALUE, stringIdx, stringSize);
        _context->elements[_eltIdx].insert(idx, eltIdx);
    }

    void insert(uint32_t idx, const NodeType newKind)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (newKind != MAP && newKind != SEQUENCE) {
            throwMessage<AccessException>("Access error: only the structural elements MAP and SEQUENCE can be created-inserted, not '%s'",
                                          styml::to_string(newKind));
        }
        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'insert(...)' can only be used on SEQUENCE elements, not '%s'",
                                          to_string().c_str());
        }
        if (idx > elt->getSubQty()) {
            throwMessage<AccessException>("Access error: Access by 'insert(%d, ...)' is out of array bounds for '%s'", idx,
                                          to_string().c_str());
        }
        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(newKind);
        _context->elements[_eltIdx].insert(idx, eltIdx);
    }

    void remove(uint32_t idx)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'remove(%d)' can only be used on SEQUENCE elements, not '%s'", idx,
                                          to_string().c_str());
        }
        if (idx >= elt->getSubQty()) {
            throwMessage<AccessException>("Access error: Access by 'remove(%d, ...)' is out of array bounds for '%s'", idx,
                                          to_string().c_str());
        }
        elt->erase(idx);
    }

    void pop_back()
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: 'pop_back()' can only be used on SEQUENCE elements, not '%s'",
                                          to_string().c_str());
        }
        if (elt->getSubQty() == 0) { throwMessage<AccessException>("Access error: cannot 'pop_back' because array is empty"); }
        elt->erase(elt->getSubQty() - 1);
    }

    // Map specific
    // ============

    bool hasKey(const std::string& key) const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP) {
            throwMessage<AccessException>("Access error: 'hasKey(%s)' can only be used on MAP elements, not '%s'", key.c_str(),
                                          to_string().c_str());
        }
        if (key.empty()) { throwMessage<AccessException>("Access error: empty key is not allowed to access a MAP element"); }

        return (_context->getMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), elt) != UINT_MAX);
    }

    Node operator[](const std::string& key) const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP) {
            throwMessage<AccessException>("Access error: Access by '[%s]' can only be used on MAP elements, not '%s'", key.c_str(),
                                          to_string().c_str());
        }
        if (key.empty()) { throwMessage<AccessException>("Access error: empty key is not allowed to access a MAP element"); }
        if (!_nonExistingKey.empty()) {
            throwMessage<AccessException>("Access error: '%s' is a non-existent key in this MAP elements'", _nonExistingKey.c_str());
        }

        // Search for the key in the table. If present, return a node pointing on the string value
        uint32_t childIndex = _context->getMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), elt);
        if (childIndex == UINT_MAX) {
            // Key is not present, return a node pointing on the table associated with a non-empty key
            return Node(_eltIdx, _context, key);
        }
        assert(childIndex < elt->getSubQty());
        return Node(_context->elements[elt->getSub(childIndex)].getKeyValue(), _context);
    }

    template<class T>
    void insert(const std::string& key, const T& typedValue)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP) {
            throwMessage<AccessException>("Access error: Access by '[%s]' can only be used on MAP elements, not '%s'", key.c_str(),
                                          to_string().c_str());
        }
        if (key.empty()) { throwMessage<AccessException>("Access error: empty key is not allowed to access a MAP element"); }
        if (!_nonExistingKey.empty()) {
            throwMessage<AccessException>("Access error: '%s' is a non-existent key in this MAP elements'", _nonExistingKey.c_str());
        }
        if (_context->getMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), elt) != UINT_MAX) {
            throwMessage<AccessException>("Access error: duplicated key are forbidden and the key '%s' is already present", key.c_str());
        }

        uint32_t    stringIdx = 0, stringSize = 0;
        std::string encodedValue;
        try {
            encodedValue = convert<T>::encode(typedValue);
        } catch (ConvertException& e) {
            throwMessage<AccessException>("Access error: encoding error when accessing '%s' with 'insert('%s', ...)':\n  %s",
                                          to_string().c_str(), key.c_str(), e.what());
        }

        _context->addString(encodedValue.data(), (uint32_t)encodedValue.size(), stringIdx, stringSize);
        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(VALUE, stringIdx, stringSize);  // Create the value element
        _context->addString(key.data(), (uint32_t)key.size(), stringIdx, stringSize);
        _context->elements.emplace_back(KEY, stringIdx, stringSize, eltIdx);  // Create the key referring to the created value element
        _context->elements[_eltIdx].add(eltIdx + 1);                          // Add the key to the parent

        // Update the access acceleration hashtable
        _context->addMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), &_context->elements[_eltIdx],
                                   _context->elements[_eltIdx].getSubQty() - 1);
    }

    void insert(const std::string& key, const NodeType newKind)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP) {
            throwMessage<AccessException>("Access error: Access by '[%s]' can only be used on MAP elements, not '%s'", key.c_str(),
                                          to_string().c_str());
        }
        if (key.empty()) { throwMessage<AccessException>("Access error: empty key is not allowed to access a MAP element"); }
        if (!_nonExistingKey.empty()) {
            throwMessage<AccessException>("Access error: '%s' is a non-existent key in this MAP elements'", _nonExistingKey.c_str());
        }
        if (newKind != MAP && newKind != SEQUENCE) {
            throwMessage<AccessException>("Access error: only the structural elements MAP and SEQUENCE can be created-inserted, not '%s'",
                                          styml::to_string(newKind));
        }
        if (_context->getMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), elt) != UINT_MAX) {
            throwMessage<AccessException>("Access error: duplicated key are forbidden and the key '%s' is already present", key.c_str());
        }

        uint32_t eltIdx = (uint32_t)_context->elements.size();
        _context->elements.emplace_back(newKind);
        _context->elements[_eltIdx].add(eltIdx);

        // Update the access acceleration hashtable
        _context->addMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), &_context->elements[_eltIdx],
                                   _context->elements[_eltIdx].getSubQty() - 1);
    }

    bool remove(const std::string& key)
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];

        if (elt->getType() != MAP) {
            throwMessage<AccessException>("Access error: 'remove(%s)' can only be used on MAP elements, not '%s'", key.c_str(),
                                          to_string().c_str());
        }

        uint32_t childIndex = _context->removeMapChildIndex(_eltIdx, key.data(), (uint32_t)key.size(), elt);
        if (childIndex == UINT_MAX) { return false; }
        assert(childIndex < elt->getSubQty());

        if (childIndex < elt->getSubQty() - 1) {
            // As the indexes of valid children are immutable (because used in the acceleration hashtable), the last element of the array
            // is swapped with the removed one, to limit the modifications to only 1 more lookup.
            detail::Element* lastElt = &_context->elements[elt->getSub(elt->getSubQty() - 1)];
            _context->removeMapChildIndex(_eltIdx, _context->getString(lastElt->getStringIdx()), lastElt->getStringSize() - 1, elt);
            elt->replace(childIndex, elt->getSub(elt->getSubQty() - 1));
            _context->addMapChildIndex(_eltIdx, _context->getString(lastElt->getStringIdx()), lastElt->getStringSize() - 1, elt,
                                       childIndex);
        }
        elt->erase(elt->getSubQty() - 1);  // Pop back
        return true;
    }

    std::string to_string() const
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        const detail::Element* elt = &_context->elements[_eltIdx];
        switch (elt->getType()) {
            case UNKNOWN:
                return "[ Unknown ]";
            case KEY:
                if (elt->getStringSize() <= 1) { return "[ Root ]"; }
                return std::string("[ Key '") + std::string(_context->getString(elt->getStringIdx())) + "' ]";
            case VALUE:
                return std::string("[ Value string '") + std::string(_context->getString(elt->getStringIdx())) + "' ]";
            case SEQUENCE:
                return std::string("[ Sequence of ") + std::to_string(elt->getSubQty()) + " elements ]";
            case MAP:
                return std::string("[ Map of ") + std::to_string(elt->getSubQty()) + " elements ]";
            case COMMENT:
                return std::string("[ Comment '") + std::string(_context->getString(elt->getStringIdx())) + "' ]";
        };
        return "[ Weird ]";
    }

    // Iterators
    // =========

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = ptrdiff_t;
        using value_type        = Node;
        using pointer           = Node*;
        using reference         = Node&;

        iterator(uint32_t* ptr, detail::Context* context) : _ptr(ptr), _context(context) {}

        value_type operator*() const { return Node(*_ptr, _context); }
        value_type operator->() { return Node(*_ptr, _context); }
        iterator&  operator++()
        {
            _ptr += 1;
            return *this;
        }
        iterator operator++(int)
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        friend bool operator==(const iterator& a, const iterator& b) { return a._ptr == b._ptr; };
        friend bool operator!=(const iterator& a, const iterator& b) { return a._ptr != b._ptr; };

       private:
        uint32_t*        _ptr     = nullptr;
        detail::Context* _context = nullptr;
    };

    iterator begin()
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];
        if (elt->getType() != MAP && elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: only the structural elements MAP and SEQUENCE can be iterated, not type '%s'.",
                                          styml::to_string(elt->getType()));
        }
        return iterator(elt->getSubs(), _context);
    }
    iterator end()
    {
        assert(_context && _eltIdx < (uint32_t)_context->elements.size());
        detail::Element* elt = &_context->elements[_eltIdx];
        if (elt->getType() != MAP && elt->getType() != SEQUENCE) {
            throwMessage<AccessException>("Access error: only the structural elements MAP and SEQUENCE can be iterated, not type '%s'.",
                                          styml::to_string(elt->getType()));
        }
        return iterator(elt->getSubs() + elt->getSubQty(), _context);
    }

    Node* operator->() { return this; }

   protected:
    uint32_t         _eltIdx  = 0xFFFFFFFF;
    detail::Context* _context = nullptr;
    std::string      _nonExistingKey;  // If not empty and node is a table, indicates a non-existing key in the table
};

// A document is a node with extra capabilities: dump and delete
class Document : public Node
{
   public:
    explicit Document() : Node(0, new detail::Context())
    {
        _context->elements.emplace_back(KEY);
        _context->addString("", 0, &_context->elements.back());  // Empty key name for root
        initFromContext();
    }
    Document(detail::Context* context) : Node(0, context) { initFromContext(); }
    Document(Document&& rhs) noexcept : Node(std::move(static_cast<Node&>(rhs))) {}
    Document& operator=(Document&& rhs) noexcept
    {
        Node::operator=(std::move(static_cast<Node&>(rhs)));
        return *this;
    }
    Document(const Document& rhs) = delete;
    ~Document() { delete _context; }

    Node& operator=(const NodeType newKind) { return Node::operator=(newKind); }

    std::string asPyStruct(bool withIndent = false) const { return dumpAsPyStruct(_context, withIndent); }
    std::string asYaml() const { return dumpAsYaml(_context); }

   private:
    void initFromContext()
    {
        if (!_context->elements.empty()) {
            detail::Element* root = &_context->elements[0];
            _eltIdx               = 0;
            if (root->getSubQty() > 0) {
                _eltIdx = root->getKeyValue();  // Take the value node instead
            }
        }
    }
};

// ==========================================================================================
// Parsing
// ==========================================================================================

namespace detail
{

enum class TokenType : uint16_t { Key, StringValue, Newline, Caret, Comment, Eos };

struct TokenParser {
    bool      isValid     = false;
    TokenType type        = TokenType::Eos;
    int       startColNbr = -1;
    uint32_t  stringIdx   = 0;
    uint32_t  stringSize  = 0;
    explicit  operator bool() const { return isValid; }
};

inline TokenParser
getToken(const char* text, uint32_t endIdx, int parentIndent, Context* context, StringHelper& sh, int& colNbr, int& lineNbr, uint32_t& idx)
{
    bool isNewLine = (colNbr == 0);
    int  initIdx   = idx;

    // Go to first non space
    uint32_t idxFnp = idx;
    if (isNewLine) {
        while (idxFnp < endIdx && text[idxFnp] == ' ') ++idxFnp;
        if (text[idxFnp] == '\t') { throwParsing(lineNbr, text + idx, "Parse error: using tabulation is not accepted for indentation"); }
    } else {
        while (idxFnp < endIdx && (text[idxFnp] == ' ' || text[idxFnp] == '\t')) ++idxFnp;
    }
    colNbr += idxFnp - idx;
    idx             = idxFnp;
    int startColNbr = colNbr;

    // Case end of input
    if (idx >= endIdx) { return {true, TokenType::Eos, startColNbr, 0, 0}; }
    char firstChar = text[idx];

    // Case end of line
    if (firstChar == '\n' || firstChar == '\r') {
        if (idx + 1 < endIdx && firstChar == '\r' && text[idx + 1] == '\n') ++idx;
        ++lineNbr;
        colNbr = 0;
        ++idx;
        return {true, TokenType::Newline, startColNbr, 0, 0};
    }

    // Case "-" as first on the line (followed by space or equivalent)
    if (firstChar == '-' && (idx + 1 == endIdx || text[idx + 1] == ' ' || text[idx + 1] == '\r' || text[idx + 1] == '\n')) {
        ++colNbr;
        ++idx;
        return {true, TokenType::Caret, startColNbr, 0, 0};
    }

    // Case "#" (comment) @FIX should be preceded by space or equivalent
    if (firstChar == '#') {
        // Find the end of the line and return the comment. The newline will be handled in another call
        uint32_t startIdx = idx + 1;
        while (idx < endIdx && text[idx] != '\r' && text[idx] != '\n') {
            ++idx;
            ++colNbr;
        }
        uint32_t stringIdx = 0, stringSize = 0;
        context->addString(text + startIdx, idx - startIdx, stringIdx, stringSize);
        return {true, TokenType::Comment, startColNbr, stringIdx, stringSize};
    }

    sh.startSession();
    // Find the type of string to deal with (YAML is creative on this point...)
    char mlType       = ' ';
    char chomp        = ' ';
    int  deltaIndent  = -1;
    int  targetIndent = -1;
    if (firstChar == '\'' || firstChar == '"') {
        mlType = firstChar;
        ++idx;
        ++colNbr;
        targetIndent = 0;  // Termination is not indent-based but quote based
        // Add the leading spaces
        uint32_t nonSpaceIdx = idx;
        while (nonSpaceIdx < endIdx && text[nonSpaceIdx] == ' ') {
            ++nonSpaceIdx;
            sh.addChar(' ');
        }
        colNbr += nonSpaceIdx - idx;
    } else if (firstChar == '|' || firstChar == '>') {
        mlType = firstChar;
        ++idx;
        ++colNbr;
        // @TODO Add check for unwanted characters
        for (int i = 0; i < 2; ++i) {  // Two passes as we shall extract 2 infos (chomp & indent) in any order
            if (idx >= endIdx) { break; }
            if (text[idx] == '+' || text[idx] == '-') {
                if (chomp != ' ') throwParsing(lineNbr, text + initIdx, "Parse error: chomp cannot be provided more than once");
                chomp = text[idx];
                ++idx;
                ++colNbr;
            } else if (text[idx] >= '1' && text[idx] <= '9') {
                if (deltaIndent >= 0)
                    throwParsing(lineNbr, text + initIdx, "Parse error: explicit indentation cannot be provided more than once");
                deltaIndent = text[idx] - '0';
                ++idx;
                ++colNbr;
            }
        }
        // @TODO: Go to endline (and check that there is only blanks except comment (to skip)...) and generate errors
        //        Current behavior is just ignoring the rest of the line, which allows things like "2+ +++2222++"
        while (idx < endIdx && text[idx] != '\n' && text[idx] != '\r') { ++idx; }
        if (idx + 1 < endIdx && text[idx] == '\r' && text[idx + 1] == '\n') { ++idx; }
        ++idx;
        ++lineNbr;
        colNbr    = 0;
        isNewLine = true;
        if (deltaIndent >= 0) { targetIndent = parentIndent + deltaIndent; }
    } else {
        // For plain string, indent does not matter as text is stripped, only the parent indent matters
        // We shall check however that it is indeed a child. Else the accept the current indent "as is"
        targetIndent = (colNbr > parentIndent) ? parentIndent + 1 : colNbr;
    }

    bool isKey              = false;
    bool indentedFoldedLine = false;

    // Analyse the string line per line
    while (idx < endIdx) {
        // Find first non-space character
        uint32_t nonSpaceIdx = idx;
        while (nonSpaceIdx < endIdx && text[nonSpaceIdx] == ' ') ++nonSpaceIdx;
        colNbr += nonSpaceIdx - idx;
        if (isNewLine && nonSpaceIdx < endIdx && text[nonSpaceIdx] == '\t') {
            throwParsing(lineNbr, text + initIdx, "Parse error: using tabulation is not accepted for indentation");
        }
        // Compute the expected indent (folded or literal strings case only)
        int effectiveIndent = (uint32_t)(nonSpaceIdx - idx);
        if (targetIndent < 0) {
            // Initial empty line case
            if (text[nonSpaceIdx] == '\n' || text[nonSpaceIdx] == '\r') {  // @BUG Need to handle comments too
                if (sh.empty()) {
                    sh.addLine("", 0);
                } else {
                    sh.addLine("\n", 1);
                }
                idx = nonSpaceIdx + ((nonSpaceIdx + 1 < endIdx && text[nonSpaceIdx] == '\r' && text[nonSpaceIdx + 1] == '\n') ? 2 : 1);
                indentedFoldedLine = true;
                ++lineNbr;
                colNbr = 0;
                continue;
            }
            // Set the expected indent
            targetIndent = colNbr;
        }

        uint32_t lineEndIdx           = nonSpaceIdx;
        bool     isEndOfStringReached = false;

        // Single quote case: process one line. Focus on double single quote
        if (mlType == '\'') {
            bool isFirstAdd = !sh.empty();
            ;
            uint32_t chunkStartIdx = lineEndIdx;
            while (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r') {
                // Easy non-single quote case
                if (text[lineEndIdx] != '\'') {
                    ++lineEndIdx;
                    continue;
                }
                // Escaped (=double) single quote ?
                if (lineEndIdx + 1 < endIdx && text[lineEndIdx + 1] == '\'') {
                    if (isFirstAdd) {
                        if (sh.arena.back() != '\n') { sh.addChar(' '); }
                        isFirstAdd = false;
                    }
                    sh.addChunk(text + chunkStartIdx, lineEndIdx + 1 - chunkStartIdx);  // Keep one single quote
                    lineEndIdx += 2;
                    chunkStartIdx = lineEndIdx;
                    continue;
                }
                isEndOfStringReached = true;
                break;
            }
            if (isFirstAdd && sh.arena.back() != '\n') { sh.addChar(' '); }
            if (lineEndIdx > chunkStartIdx) { sh.addChunk(text + chunkStartIdx, lineEndIdx - chunkStartIdx); }
            if (lineEndIdx >= endIdx) { throwParsing(lineNbr, text + initIdx, "Parse error: unfinished single-quote string"); }
            if (text[lineEndIdx] == '\'') {
                isEndOfStringReached = true;
                ++lineEndIdx;  // Skip double quote
                while (text[lineEndIdx] == ' ' || text[lineEndIdx] == '\t') { ++lineEndIdx; }
            }
            if (!isEndOfStringReached && nonSpaceIdx == lineEndIdx) { sh.addLine("\n", 1); }
        }  // End of single quote case

        // Double quote case: process one line. Focus on backslash escape
        else if (mlType == '\"') {
            bool isFirstAdd = !sh.empty();
            ;
            uint32_t chunkStartIdx = lineEndIdx;
            while (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r' && text[lineEndIdx] != '\"') {
                if (text[lineEndIdx] != '\\') {
                    ++lineEndIdx;
                    continue;
                }
                // Skip this backslash
                if (isFirstAdd && sh.arena.back() != '\n') {
                    sh.addChar(' ');
                    isFirstAdd = false;
                }
                if (lineEndIdx > chunkStartIdx) { sh.addChunk(text + chunkStartIdx, lineEndIdx - chunkStartIdx); }
                chunkStartIdx = ++lineEndIdx;
                // Process next character
                if (lineEndIdx < endIdx) {
                    if (text[lineEndIdx] == 'n') {
                        sh.addChar('\n');
                    } else if (text[lineEndIdx] == 'r') {
                        sh.addChar('\r');
                    } else if (text[lineEndIdx] == 't') {
                        sh.addChar('\t');
                    } else if (text[lineEndIdx] == '"') {
                        sh.addChar('"');
                    } else if (text[lineEndIdx] == '\\') {
                        sh.addChar('\\');
                    } else if (text[lineEndIdx] == '\r' || text[lineEndIdx] == '\n') {
                        // Case of escaping the end of line to join it with the next one
                        if (text[lineEndIdx] == '\r') ++lineEndIdx;
                        if (text[lineEndIdx] == '\n') ++lineEndIdx;
                        // Skip next line spaces
                        while (text[lineEndIdx] == ' ') ++lineEndIdx;
                        --lineEndIdx;
                    } else {  // Fallback @TODO Handle \x, \u and \U
                        sh.addChar('\\');
                        sh.addChar(text[lineEndIdx]);
                    }
                    chunkStartIdx = ++lineEndIdx;
                }
            }
            if (isFirstAdd && sh.arena.back() != '\n') { sh.addChar(' '); }
            if (lineEndIdx > chunkStartIdx) { sh.addChunk(text + chunkStartIdx, lineEndIdx - chunkStartIdx); }
            if (lineEndIdx >= endIdx) throwParsing(lineNbr, text + initIdx, "Parse error: unfinished double-quote string");
            if (text[lineEndIdx] == '\"') {
                isEndOfStringReached = true;
                ++lineEndIdx;  // Skip double quote
                while (text[lineEndIdx] == ' ' || text[lineEndIdx] == '\t') { ++lineEndIdx; }
            }
            if (!isEndOfStringReached && nonSpaceIdx == lineEndIdx) { sh.addLine("\n", 1); }
        }  // End of double quote case

        // Literal
        else if (mlType == '|') {
            uint32_t rollbackLineEndIdx = lineEndIdx;
            while (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r') { ++lineEndIdx; }
            if (lineEndIdx != nonSpaceIdx && colNbr < targetIndent) {
                isEndOfStringReached = true;
                lineEndIdx           = rollbackLineEndIdx;
            } else {
                if (!sh.empty()) { sh.addChar('\n'); }
                if (lineEndIdx >= idx + targetIndent) { sh.addChunk(text + idx + targetIndent, lineEndIdx - (idx + targetIndent)); }
            }
        }

        // Folded
        else if (mlType == '>') {
            uint32_t rollbackLineEndIdx = lineEndIdx;
            while (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r') { ++lineEndIdx; }
            if (lineEndIdx != nonSpaceIdx && colNbr < targetIndent) {
                isEndOfStringReached = true;
                lineEndIdx           = rollbackLineEndIdx;
            } else {
                // Indentation behavior
                bool isIndented = (lineEndIdx <= idx + targetIndent || (!sh.empty() && text[idx + targetIndent] == ' '));
                if (isIndented || indentedFoldedLine) {
                    sh.addChar('\n');
                } else if (lineEndIdx > idx + targetIndent && !sh.empty() && sh.arena.back() != '\n') {
                    sh.addChar(' ');
                }

                indentedFoldedLine = (lineEndIdx > idx + targetIndent && text[idx + targetIndent] == ' ');
                if (lineEndIdx > idx + targetIndent) { sh.addChunk(text + idx + targetIndent, lineEndIdx - (idx + targetIndent)); }
            }
        }

        // Plain string
        else {
            uint32_t rollbackLineEndIdx = lineEndIdx;
            while (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r' &&
                   (text[lineEndIdx] != '#' || lineEndIdx == idx || text[lineEndIdx - 1] != ' ') &&
                   (text[lineEndIdx] != ':' || (lineEndIdx + 1 != endIdx && text[lineEndIdx + 1] != ' ' && text[lineEndIdx + 1] != '\n' &&
                                                text[lineEndIdx + 1] != '\r'))) {
                ++lineEndIdx;
            }
            isEndOfStringReached = (lineEndIdx < endIdx && text[lineEndIdx] != '\n' && text[lineEndIdx] != '\r');
            if (lineEndIdx != nonSpaceIdx && colNbr < targetIndent) {
                isEndOfStringReached = true;
                lineEndIdx           = rollbackLineEndIdx;
            } else {
                if (!sh.empty() && sh.arena.back() != '\n') { sh.addChar(' '); }
                sh.addChunkNoTrail(text + idx + effectiveIndent, lineEndIdx - (idx + effectiveIndent));
            }
            if (!isEndOfStringReached && nonSpaceIdx == lineEndIdx) { sh.addLine("\n", 1); }
        }

        // Finalize the line
        uint32_t nextLineStartIdx =
            lineEndIdx + ((lineEndIdx + 1 < endIdx && text[lineEndIdx] == '\r' && text[lineEndIdx + 1] == '\n') ? 2 : 1);
        sh.endLine();

        if (isEndOfStringReached && text[lineEndIdx] == ':' &&
            (lineEndIdx + 1 == endIdx || text[lineEndIdx + 1] == ' ' || text[lineEndIdx + 1] == '\n' || text[lineEndIdx + 1] == '\r')) {
            isKey = true;
            ++lineEndIdx;  // Skip colon
        }

        // Next line
        if (isEndOfStringReached) {
            idx = lineEndIdx;
            colNbr += (int)(lineEndIdx - nonSpaceIdx);
            break;
        }

        idx    = nextLineStartIdx;
        colNbr = 0;
        ++lineNbr;

        if (idx >= endIdx && !isEndOfStringReached) {
            if (mlType == '"') { throwParsing(lineNbr, text + initIdx, "Parse error: unfinished double-quote string"); }
            if (mlType == '\'') { throwParsing(lineNbr, text + initIdx, "Parse error: unfinished single-quote string"); }
        }
    }  // End of text line parsing

    // Apply the chomp
    if (mlType != '\'' && mlType != '"' && (chomp == '-' || chomp == ' ')) { sh.removeTrailingLines(); }

    // Build the final string from the different lines
    context->startStringSession();
    for (StringHelper::LineChunk& lc : sh.chunks) { context->addToSession(sh.arena.data() + lc.startIdx, lc.size); }
    if ((mlType == '|' || mlType == '>') && (chomp == ' ' || chomp == '+')) { context->addToSession("\n", 1); }

    // Store
    uint32_t stringIdx = 0, stringSize = 0;
    context->commitSession(stringIdx, stringSize);
    return {true, isKey ? TokenType::Key : TokenType::StringValue, startColNbr, stringIdx, stringSize};
}

}  // namespace detail

inline Document
parse(const char* text, uint32_t textSize)
{
    //#define DEBUG_PARSING
#ifdef DEBUG_PARSING
#define dbgPrintf(...) printf(__VA_ARGS__)
    const char* tokenName[6] = {"KEY", "STRINGVALUE", "NEWLINE", "CARET", "COMMENT", "EOS"};
#else
#define dbgPrintf(...) \
    do {               \
    } while (0)
#endif

    using namespace detail;
    uint32_t     startIdx     = 0;
    int          lineNbr      = 1;
    bool         isEndOfInput = false;
    StringHelper sh;  // Utility for string parsing, to limit memory allocation

    struct ParseItem {
        ParseItem() {}
        ParseItem(uint32_t eltIdx, int indent, int childIndent) : eltIdx(eltIdx), indent(indent), childIndent(childIndent) {}
        uint32_t eltIdx      = 0;
        int      indent      = 0;
        int      childIndent = -1;
    };

    // To prevent memory leaks when parsing encounters an error:
    // - unique_ptr is used to hold the root node, which recursively owns all nodes, and the global context
    // - no exception shall be thrown between an Element creation and its addition into the tree
    std::unique_ptr<Context> context(new Context(textSize));
    std::vector<Element>&    elements = context->elements;

    elements.emplace_back(KEY);                   // Root is a KEY type, index 0  @TEST Check when root's child is directly modified
    context->addString("", 0, &elements.back());  // Empty key name for root

    // Parse context: the stack, the parent (often equal to stack.back()), the parent indent,
    std::vector<ParseItem> stack{{0, -1, -1}};
    ParseItem              parent               = stack.back();
    int                    mlStringParentIndent = -1;
    int                    indexColNbr          = 0;
    int                    tokenLineNbr         = 1;
    int                    tokenIdx             = 0;

    while (!isEndOfInput && !stack.empty()) {
#ifdef DEBUG_PARSING
        dbgPrintf("line %d) STACK:\n", lineNbr);
        for (const auto s : stack) {
            dbgPrintf("   * %9s  parent indent %2d / %2d  . prev indent %2d", to_string(elements[s.eltIdx].getType()), s.indent,
                      s.childIndent, mlStringParentIndent);
            if (elements[s.eltIdx].getType() == NodeType::KEY) dbgPrintf(" [%s]", context->getString(elements[s.eltIdx].getStringIdx()));
            dbgPrintf("\n");
        }
#endif

        // Get the next token
        bool        isStartingWithNewLine = (indexColNbr == 0);
        TokenParser token = getToken(text, textSize, mlStringParentIndent, context.get(), sh, indexColNbr, lineNbr, startIdx);
        dbgPrintf("  GetToken: %s baseIndent=%2d  newline=%d  String: [%s]\n", tokenName[(int)token.type], mlStringParentIndent,
                  isStartingWithNewLine, token.stringSize ? context->getString(token.stringIdx) : "");

        switch (token.type) {
            case TokenType::Comment: {
                int eltIdx = (int)elements.size();
                elements.emplace_back(COMMENT, token.stringIdx, token.stringSize);
                if (isStartingWithNewLine) { elements.back().setStandalone(); }

                int parentCommentEltIdx = parent.eltIdx;
                if (elements[parentCommentEltIdx].getType() == UNKNOWN && stack.size() >= 2) {
                    parentCommentEltIdx =
                        stack[stack.size() - 2].eltIdx;  // If the last is unknown, then the for-last must be a key or sequence
                }

                if (elements[parentCommentEltIdx].getType() != UNKNOWN) {
                    int tmpIdx = 0;
                    while ((tmpIdx = elements[parentCommentEltIdx].getNextCommentIndex()) != 0) { parentCommentEltIdx = tmpIdx; }
                    elements[parentCommentEltIdx].setComment(eltIdx);
                }
            } break;

            case TokenType::Caret: {
                mlStringParentIndent = token.startColNbr;
                const int colNbr     = token.startColNbr;

                // Pop stack until caret has the same or higher indent than the parent
                while (
                    !stack.empty() &&
                    (elements[parent.eltIdx].getType() != KEY || colNbr != parent.indent) &&  // Case a caret just below a key ("a:\n- b")
                    (elements[parent.eltIdx].getType() != UNKNOWN || stack.size() < 2 ||
                     elements[stack[stack.size() - 2].eltIdx].getType() != KEY ||
                     colNbr != stack[stack.size() - 2].indent) &&  // Case a caret just below a key ("a:\n- b")
                    colNbr <= parent.indent &&
                    (parent.childIndent < 0 || colNbr < parent.childIndent)) {
                    stack.pop_back();
                    parent = stack.back();
                }

                // Checks
                if (stack.empty()) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the caret (=%d) does not match any parent (=%d)", colNbr,
                                 parent.childIndent);  // Reachable state?
                }
                if (parent.childIndent >= 0 && colNbr != parent.childIndent) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the caret (=%d) is not aligned with other child elements (=%d)", colNbr,
                                 parent.childIndent);
                }

                // Insert a sequence if the parent isn't one
                if (elements[parent.eltIdx].getType() != SEQUENCE) {
                    Element& parentElt = elements[parent.eltIdx];
                    if (parentElt.getType() == UNKNOWN) {
                        parentElt.reset(SEQUENCE);
                        stack.back().indent      = colNbr;
                        stack.back().childIndent = colNbr;
                        parent                   = stack.back();
                        if (stack[stack.size() - 2].childIndent < 0) stack[stack.size() - 2].childIndent = colNbr;
                    } else {
                        assert(parentElt.getType() != VALUE);
                        if (parentElt.getType() == KEY && parentElt.getSubQty() > 0) {
                            throwParsing(tokenLineNbr, text + tokenIdx,
                                         "Parse error: probably bad indentation with caret, as the parent ('%s') already has a value",
                                         to_string(KEY));  // Reachable state?
                        }
                        int eltIdx = (int)elements.size();
                        elements.emplace_back(SEQUENCE);
                        stack.emplace_back(eltIdx, colNbr, colNbr);
                        elements[parent.eltIdx].add(eltIdx);
                        parent = stack.back();
                    }
                }

                // Create the next node, untyped. This is required to handle the case of empty values in sequences.
                assert(elements[parent.eltIdx].getType() == SEQUENCE);
                int eltIdx = (int)elements.size();
                elements.emplace_back(UNKNOWN);
                stack.emplace_back(eltIdx, colNbr, -1);
                elements[parent.eltIdx].add(eltIdx);
                parent = stack.back();

            } break;

            case TokenType::Key: {
                mlStringParentIndent = token.startColNbr;
                const int colNbr     = token.startColNbr;

                // Pop stack until the key indent matches the parent's
                while (!stack.empty() && colNbr <= parent.indent) {
                    stack.pop_back();
                    parent = stack.back();
                }

                // Checks
                if (stack.empty()) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the key (=%d) does not match any parent (=%d)", colNbr,
                                 parent.childIndent);  // Reachable state?
                }
                if (parent.childIndent >= 0 && colNbr < parent.childIndent) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the key (=%d) is not aligned with other child elements (=%d)", colNbr,
                                 parent.childIndent);
                }
                if (parent.childIndent < 0) {
                    stack.back().childIndent = colNbr;
                    parent                   = stack.back();
                }

                // Insert a map if the parent isn't one
                // This is required due to the idiomatic syntax "- a:" which implies a 'map' between the caret and the "a:"
                if (elements[parent.eltIdx].getType() != MAP) {
                    Element& parentElt = elements[parent.eltIdx];
                    if (parentElt.getType() == UNKNOWN) {
                        parentElt.reset(MAP);
                    } else {
                        assert(parentElt.getType() != VALUE);
                        if (parentElt.getType() == KEY && parentElt.getSubQty() > 0) {
                            throwParsing(tokenLineNbr, text + tokenIdx,
                                         "Parse error: probably bad indentation, as the parent ('%s') already has a value",
                                         to_string(parentElt.getType()));
                        }

                        int eltIdx = (int)elements.size();
                        elements.emplace_back(MAP);
                        stack.emplace_back(eltIdx, parent.indent, -1);
                        elements[parent.eltIdx].add(eltIdx);
                        parent = stack.back();
                    }
                }

                // Add key
                if (parent.childIndent < 0) { stack.back().childIndent = colNbr; }
                int eltIdx = (int)elements.size();
                elements.emplace_back(KEY, token.stringIdx, token.stringSize);
                stack.emplace_back(eltIdx, colNbr, -1);
                assert(elements[parent.eltIdx].getType() != KEY || elements[parent.eltIdx].getSubQty() == 0);
                elements[parent.eltIdx].add(eltIdx);
                if (!context->addMapChildIndex(parent.eltIdx, context->getString(token.stringIdx), token.stringSize - 1,
                                               &elements[parent.eltIdx], elements[parent.eltIdx].getSubQty() - 1)) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: duplicated key are forbidden and the key '%s' is already present.",
                                 context->getString(token.stringIdx));
                }
                parent = stack.back();

                // Create the next node, untyped. This is required to handle the case of empty values in sequences.
                assert(elements[parent.eltIdx].getType() == KEY);
                eltIdx = (int)elements.size();
                elements.emplace_back(UNKNOWN);
                stack.emplace_back(eltIdx, colNbr, -1);
                elements[parent.eltIdx].add(eltIdx);
                parent = stack.back();
            } break;

            case TokenType::StringValue: {
                const int colNbr = token.startColNbr;

                // Checks
                if (colNbr <= parent.indent) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the value (=%d) is not compatible with the parent indentation (=%d)",
                                 colNbr, parent.indent);
                }
                if (parent.childIndent >= 0 && colNbr < parent.childIndent) {
                    throwParsing(tokenLineNbr, text + tokenIdx,
                                 "Parse error: the indentation of the value (=%d) is not aligned with other child elements (=%d)", colNbr,
                                 parent.childIndent);
                }
                if (elements[parent.eltIdx].getType() == MAP) {
                    throwParsing(tokenLineNbr, text + tokenIdx, "Parse error: in a map, a value without a key is forbidden");
                }
                if (parent.childIndent < 0) {
                    stack.back().childIndent = colNbr;
                    parent                   = stack.back();
                }

                // Create the string value, either by typing an untyped node or creating a new one
                Element& parentElt = elements[parent.eltIdx];
                if (parentElt.getType() == UNKNOWN) {
                    parentElt.reset(VALUE);
                    parentElt.setString(token.stringIdx, token.stringSize);
                    stack.pop_back();
                    parent = stack.back();  // Now the parent is the upper container
                } else {
                    assert(parentElt.getType() != KEY || parentElt.getSubQty() == 0);  // Container or not a key already with value
                    uint32_t eltIdx = (uint32_t)elements.size();
                    elements.emplace_back(VALUE, token.stringIdx, token.stringSize);
                    elements[parent.eltIdx].add(eltIdx);
                }

                // If the parent is a key, pop it from the stack (container with only 1 child)
                if (elements[parent.eltIdx].getType() == KEY) {
                    stack.pop_back();
                    parent = stack.back();
                }
            } break;

            case TokenType::Newline:
                mlStringParentIndent = parent.indent;
                indexColNbr          = 0;
                break;

            case TokenType::Eos:
                isEndOfInput = true;
                break;

            default:
                assert(false && "Bug");
        };

        tokenLineNbr = lineNbr;
        tokenIdx     = startIdx;
    }  // End of input

    return Document(context.release());
}

inline Document
parse(const char* text)
{
    return parse(text, (uint32_t)strlen(text));
}

inline Document
parse(const std::string& text)
{
    return parse(text.data(), (uint32_t)text.size());
}

}  // Namespace styml
