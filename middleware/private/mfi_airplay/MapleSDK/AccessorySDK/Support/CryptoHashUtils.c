/*
	Copyright (C) 2015-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "CryptoHashUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================

static void _BLAKE2bInit(CryptoHashContext* ctx);
static void _BLAKE2bUpdate(CryptoHashContext* ctx, const void* inData, size_t inLen);
static void _BLAKE2bFinal(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

static void _MD5Init(CryptoHashContext* ctx);
static void _MD5Update(CryptoHashContext* ctx, const void* inData, size_t inLen);
static void _MD5Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

static void _SHA1Init(CryptoHashContext* ctx);
static void _SHA1Update(CryptoHashContext* ctx, const void* inData, size_t inLen);
static void _SHA1Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

static void _SHA512Init(CryptoHashContext* ctx);
static void _SHA512Update(CryptoHashContext* ctx, const void* inData, size_t inLen);
static void _SHA512Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

static void _SHA3Init(CryptoHashContext* ctx);
static void _SHA3Update(CryptoHashContext* ctx, const void* inData, size_t inLen);
static void _SHA3Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

//===========================================================================================================================

typedef void (*CryptoHashInit_f)(CryptoHashContext* ctx);
typedef void (*CryptoHashUpdate_f)(CryptoHashContext* ctx, const void* inData, size_t inLen);
typedef void (*CryptoHashFinal_f)(CryptoHashContext* ctx, uint8_t* inDigestBuffer);

struct CryptoHashDescriptorPrivate {
    CryptoHashInit_f init_f;
    CryptoHashUpdate_f update_f;
    CryptoHashFinal_f final_f;
    size_t digestLen;
    size_t blockLen;
};

// BLAKE2b

const struct CryptoHashDescriptorPrivate _kCryptoHashDescriptor_BLAKE2b = {
    _BLAKE2bInit, _BLAKE2bUpdate, _BLAKE2bFinal, kBLAKE2bDigestSize, kBLAKE2bBlockSize
};
CryptoHashDescriptorRef kCryptoHashDescriptor_BLAKE2b = &_kCryptoHashDescriptor_BLAKE2b;

// MD5

const struct CryptoHashDescriptorPrivate _kCryptoHashDescriptor_MD5 = {
    _MD5Init, _MD5Update, _MD5Final, 16, 64
};
CryptoHashDescriptorRef kCryptoHashDescriptor_MD5 = &_kCryptoHashDescriptor_MD5;

// SHA-1

const struct CryptoHashDescriptorPrivate _kCryptoHashDescriptor_SHA1 = {
    _SHA1Init, _SHA1Update, _SHA1Final, SHA_DIGEST_LENGTH, 64
};
CryptoHashDescriptorRef kCryptoHashDescriptor_SHA1 = &_kCryptoHashDescriptor_SHA1;

// SHA-512

const struct CryptoHashDescriptorPrivate _kCryptoHashDescriptor_SHA512 = {
    _SHA512Init, _SHA512Update, _SHA512Final, SHA512_DIGEST_LENGTH, 128
};
CryptoHashDescriptorRef kCryptoHashDescriptor_SHA512 = &_kCryptoHashDescriptor_SHA512;

// SHA-3

const struct CryptoHashDescriptorPrivate _kCryptoHashDescriptor_SHA3 = {
    _SHA3Init, _SHA3Update, _SHA3Final, SHA3_DIGEST_LENGTH, SHA3_BLOCK_SIZE
};
CryptoHashDescriptorRef kCryptoHashDescriptor_SHA3 = &_kCryptoHashDescriptor_SHA3;

#if 0
#pragma mark -
#endif

//===========================================================================================================================

size_t CryptoHashDescriptorGetBlockSize(CryptoHashDescriptorRef inDesc)
{
    return (inDesc->blockLen);
}

//===========================================================================================================================

size_t CryptoHashDescriptorGetDigestSize(CryptoHashDescriptorRef inDesc)
{
    return (inDesc->digestLen);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

void CryptoHashInit(CryptoHashContext* ctx, CryptoHashDescriptorRef inDesc)
{
    ctx->desc = inDesc;
    inDesc->init_f(ctx);
}

//===========================================================================================================================

void CryptoHashUpdate(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    ctx->desc->update_f(ctx, inData, inLen);
}

//===========================================================================================================================

void CryptoHashFinal(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    ctx->desc->final_f(ctx, inDigestBuffer);
}

//===========================================================================================================================

void CryptoHashOneShot(CryptoHashDescriptorRef inDesc, const void* inData, size_t inLen, uint8_t* inDigestBuffer)
{
    CryptoHashContext ctx;

    ctx.desc = inDesc;
    inDesc->init_f(&ctx);
    inDesc->update_f(&ctx, inData, inLen);
    inDesc->final_f(&ctx, inDigestBuffer);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

void CryptoHMACInit(CryptoHMACContext* ctx, CryptoHashDescriptorRef inDesc, const void* inKeyPtr, size_t inKeyLen)
{
    const uint8_t* keyPtr = (const uint8_t*)inKeyPtr;
    uint8_t ipad[kCryptoHashMaxBlockLen];
    uint8_t tempKey[kCryptoHashMaxDigestLen];
    size_t i;
    uint8_t b;

    check(inDesc->blockLen <= kCryptoHashMaxBlockLen);
    check(inDesc->digestLen <= kCryptoHashMaxDigestLen);

    if (inKeyLen > inDesc->blockLen) {
        CryptoHashInit(&ctx->hashCtx, inDesc);
        CryptoHashUpdate(&ctx->hashCtx, inKeyPtr, inKeyLen);
        CryptoHashFinal(&ctx->hashCtx, tempKey);
        keyPtr = tempKey;
        inKeyLen = inDesc->digestLen;
    }
    for (i = 0; i < inKeyLen; ++i) {
        b = keyPtr[i];
        ipad[i] = b ^ 0x36;
        ctx->opad[i] = b ^ 0x5C;
    }
    for (; i < inDesc->blockLen; ++i) {
        ipad[i] = 0x36;
        ctx->opad[i] = 0x5C;
    }
    CryptoHashInit(&ctx->hashCtx, inDesc);
    CryptoHashUpdate(&ctx->hashCtx, ipad, inDesc->blockLen);
}

//===========================================================================================================================

void CryptoHMACUpdate(CryptoHMACContext* ctx, const void* inPtr, size_t inLen)
{
    CryptoHashUpdate(&ctx->hashCtx, inPtr, inLen);
}

//===========================================================================================================================

void CryptoHMACFinal(CryptoHMACContext* ctx, uint8_t* outDigest)
{
    CryptoHashDescriptorRef desc;

    desc = ctx->hashCtx.desc;
    CryptoHashFinal(&ctx->hashCtx, outDigest);
    CryptoHashInit(&ctx->hashCtx, desc);
    CryptoHashUpdate(&ctx->hashCtx, ctx->opad, desc->blockLen);
    CryptoHashUpdate(&ctx->hashCtx, outDigest, desc->digestLen);
    CryptoHashFinal(&ctx->hashCtx, outDigest);
}

//===========================================================================================================================

void CryptoHMACOneShot(
    CryptoHashDescriptorRef inDesc,
    const void* inKeyPtr,
    size_t inKeyLen,
    const void* inMsgPtr,
    size_t inMsgLen,
    uint8_t* outDigest)
{
    CryptoHMACContext ctx;

    CryptoHMACInit(&ctx, inDesc, inKeyPtr, inKeyLen);
    CryptoHMACUpdate(&ctx, inMsgPtr, inMsgLen);
    CryptoHMACFinal(&ctx, outDigest);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

void CryptoHKDF(
    CryptoHashDescriptorRef inDesc,
    const void* inInputKeyPtr, size_t inInputKeyLen,
    const void* inSaltPtr, size_t inSaltLen,
    const void* inInfoPtr, size_t inInfoLen,
    size_t inOutputLen, void* outKey)
{
    uint8_t* const keyPtr = (uint8_t*)outKey;
    uint8_t nullSalt[kCryptoHashMaxDigestLen];
    uint8_t key[kCryptoHashMaxDigestLen];
    CryptoHMACContext hmacCtx;
    size_t i, n, offset, Tlen;
    uint8_t T[kCryptoHashMaxDigestLen];
    uint8_t b;

    check(inDesc->digestLen <= kCryptoHashMaxDigestLen);

    // Extract phase.

    if (inSaltLen == 0) {
        memset(nullSalt, 0, inDesc->digestLen);
        inSaltPtr = nullSalt;
        inSaltLen = inDesc->digestLen;
    }
    CryptoHMACOneShot(inDesc, inSaltPtr, inSaltLen, inInputKeyPtr, inInputKeyLen, key);

    // Expand phase.

    n = (inOutputLen / inDesc->digestLen) + ((inOutputLen % inDesc->digestLen) ? 1 : 0);
    check(n <= 255);
    Tlen = 0;
    offset = 0;
    for (i = 1; i <= n; ++i) {
        CryptoHMACInit(&hmacCtx, inDesc, key, inDesc->digestLen);
        CryptoHMACUpdate(&hmacCtx, T, Tlen);
        CryptoHMACUpdate(&hmacCtx, inInfoPtr, inInfoLen);
        b = (uint8_t)i;
        CryptoHMACUpdate(&hmacCtx, &b, 1);
        CryptoHMACFinal(&hmacCtx, T);
        memcpy(&keyPtr[offset], T, (i != n) ? inDesc->digestLen : (inOutputLen - offset));
        offset += inDesc->digestLen;
        Tlen = inDesc->digestLen;
    }
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static void _BLAKE2bInit(CryptoHashContext* ctx)
{
    check(ctx->desc == kCryptoHashDescriptor_BLAKE2b);
    BLAKE2bInit(&ctx->state.blake2b, NULL, 0, kBLAKE2bDigestSize);
}

static void _BLAKE2bUpdate(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    check(ctx->desc == kCryptoHashDescriptor_BLAKE2b);
    BLAKE2bUpdate(&ctx->state.blake2b, inData, inLen);
}

static void _BLAKE2bFinal(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    check(ctx->desc == kCryptoHashDescriptor_BLAKE2b);
    BLAKE2bFinal(&ctx->state.blake2b, inDigestBuffer);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static void _MD5Init(CryptoHashContext* ctx)
{
    check(ctx->desc == kCryptoHashDescriptor_MD5);
    MD5_Init(&ctx->state.md5);
}

static void _MD5Update(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    check(ctx->desc == kCryptoHashDescriptor_MD5);
    MD5_Update(&ctx->state.md5, inData, inLen);
}

static void _MD5Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    check(ctx->desc == kCryptoHashDescriptor_MD5);
    MD5_Final(inDigestBuffer, &ctx->state.md5);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static void _SHA1Init(CryptoHashContext* ctx)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA1);
    SHA1_Init(&ctx->state.sha1);
}

static void _SHA1Update(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA1);
    SHA1_Update(&ctx->state.sha1, inData, inLen);
}

static void _SHA1Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA1);
    SHA1_Final(inDigestBuffer, &ctx->state.sha1);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static void _SHA512Init(CryptoHashContext* ctx)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA512);
    SHA512_Init(&ctx->state.sha512);
}

static void _SHA512Update(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA512);
    SHA512_Update(&ctx->state.sha512, inData, inLen);
}

static void _SHA512Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA512);
    SHA512_Final(inDigestBuffer, &ctx->state.sha512);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static void _SHA3Init(CryptoHashContext* ctx)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA3);
    SHA3_Init(&ctx->state.sha3);
}

static void _SHA3Update(CryptoHashContext* ctx, const void* inData, size_t inLen)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA3);
    SHA3_Update(&ctx->state.sha3, inData, inLen);
}

static void _SHA3Final(CryptoHashContext* ctx, uint8_t* inDigestBuffer)
{
    check(ctx->desc == kCryptoHashDescriptor_SHA3);
    SHA3_Final(inDigestBuffer, &ctx->state.sha3);
}
