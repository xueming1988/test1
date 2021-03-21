/*
	BLAKE2 Cryptographic Hash and Message Authentication Code (MAC)
	Based on RFC 7693.
	
	Copyright (C) 2015-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "BLAKEUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	BLAKE2b internals
//===========================================================================================================================

#define B2B_G(a, b, c, d, x, y)         \
    {                                   \
        v[a] = v[a] + v[b] + x;         \
        v[d] = ROTR64(v[d] ^ v[a], 32); \
        v[c] = v[c] + v[d];             \
        v[b] = ROTR64(v[b] ^ v[c], 24); \
        v[a] = v[a] + v[b] + y;         \
        v[d] = ROTR64(v[d] ^ v[a], 16); \
        v[c] = v[c] + v[d];             \
        v[b] = ROTR64(v[b] ^ v[c], 63); \
    }

static const uint64_t kBLAKE2bIV[8] = {
    0x6A09E667F3BCC908ull, 0xBB67AE8584CAA73Bull,
    0x3C6EF372FE94F82Bull, 0xA54FF53A5F1D36F1ull,
    0x510E527FADE682D1ull, 0x9B05688C2B3E6C1Full,
    0x1F83D9ABFB41BD6Bull, 0x5BE0CD19137E2179ull
};

static void _BLAKE2bCompress(BLAKE2bContext* ctx, int last);

//===========================================================================================================================
//	BLAKE2bInit
//===========================================================================================================================

void BLAKE2bInit(BLAKE2bContext* ctx, const void* inKeyPtr, size_t inKeyLen, size_t inDigestLen)
{
    size_t i;

    require_string((inDigestLen > 0) && (inDigestLen <= 64), exit, "Bad digest length");
    require_string(inKeyLen <= 64, exit, "Bad key length");

    for (i = 0; i < 8; ++i) {
        ctx->h[i] = kBLAKE2bIV[i];
    }
    ctx->h[0] ^= (0x01010000 ^ (inKeyLen << 8) ^ inDigestLen);
    ctx->t[0] = 0;
    ctx->t[1] = 0;
    ctx->c = 0;
    ctx->digestLen = inDigestLen;
    for (i = inKeyLen; i < 128; ++i) {
        ctx->b[i] = 0;
    }
    if (inKeyLen > 0) {
        BLAKE2bUpdate(ctx, inKeyPtr, inKeyLen);
        ctx->c = 128;
    }
exit:
    return;
}

//===========================================================================================================================
//	BLAKE2bInit
//===========================================================================================================================

void BLAKE2bUpdate(BLAKE2bContext* ctx, const void* inDataPtr, size_t inDataLen)
{
    size_t i;

    for (i = 0; i < inDataLen; ++i) {
        if (ctx->c == 128) {
            ctx->t[0] += ctx->c;
            if (ctx->t[0] < ctx->c)
                ++ctx->t[1];
            _BLAKE2bCompress(ctx, 0);
            ctx->c = 0;
        }
        ctx->b[ctx->c++] = ((const uint8_t*)inDataPtr)[i];
    }
}

//===========================================================================================================================
//	BLAKE2bFinal
//===========================================================================================================================

void BLAKE2bFinal(BLAKE2bContext* ctx, void* inDigestBuffer)
{
    size_t i;

    ctx->t[0] += ctx->c;
    if (ctx->t[0] < ctx->c)
        ++ctx->t[1];
    while (ctx->c < 128)
        ctx->b[ctx->c++] = 0;
    _BLAKE2bCompress(ctx, 1);
    for (i = 0; i < ctx->digestLen; ++i) {
        ((uint8_t*)inDigestBuffer)[i] = (ctx->h[i >> 3] >> (8 * (i & 7))) & 0xFF;
    }
}

//===========================================================================================================================
//	BLAKE2bOneShot
//===========================================================================================================================

void BLAKE2bOneShot(
    const void* inKeyPtr,
    size_t inKeyLen,
    const void* inDataPtr,
    size_t inDataLen,
    size_t inDigestLen,
    void* inDigestBuffer)
{
    BLAKE2bContext ctx;

    BLAKE2bInit(&ctx, inKeyPtr, inKeyLen, inDigestLen);
    BLAKE2bUpdate(&ctx, inDataPtr, inDataLen);
    BLAKE2bFinal(&ctx, inDigestBuffer);
}

//===========================================================================================================================
//	_BLAKE2bCompress
//===========================================================================================================================

static void _BLAKE2bCompress(BLAKE2bContext* ctx, int inLast)
{
    static const uint8_t kSigma[12][16] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
        { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
        { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
        { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
        { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
        { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
        { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
        { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
        { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
        { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
        { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 }
    };
    int i;
    uint64_t v[16], m[16];

    for (i = 0; i < 8; ++i) {
        v[i] = ctx->h[i];
        v[i + 8] = kBLAKE2bIV[i];
    }
    v[12] ^= ctx->t[0];
    v[13] ^= ctx->t[1];
    if (inLast)
        v[14] = ~v[14];
    for (i = 0; i < 16; ++i) {
        m[i] = ReadLittle64(&ctx->b[8 * i]);
    }
    for (i = 0; i < 12; ++i) {
        B2B_G(0, 4, 8, 12, m[kSigma[i][0]], m[kSigma[i][1]]);
        B2B_G(1, 5, 9, 13, m[kSigma[i][2]], m[kSigma[i][3]]);
        B2B_G(2, 6, 10, 14, m[kSigma[i][4]], m[kSigma[i][5]]);
        B2B_G(3, 7, 11, 15, m[kSigma[i][6]], m[kSigma[i][7]]);
        B2B_G(0, 5, 10, 15, m[kSigma[i][8]], m[kSigma[i][9]]);
        B2B_G(1, 6, 11, 12, m[kSigma[i][10]], m[kSigma[i][11]]);
        B2B_G(2, 7, 8, 13, m[kSigma[i][12]], m[kSigma[i][13]]);
        B2B_G(3, 4, 9, 14, m[kSigma[i][14]], m[kSigma[i][15]]);
    }
    for (i = 0; i < 8; ++i) {
        ctx->h[i] ^= (v[i] ^ v[i + 8]);
    }
}
