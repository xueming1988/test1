/*
	Portions Copyright (C) 2007-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/
/*
 * Copyright (c) 1997-2007  The Stanford SRP Authentication Project
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF
 * THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Redistributions in source or binary form must retain an intact copy
 * of this copyright notice.
 */
#include <assert.h>

#include "config.h"
#include "srp.h"

/*
 * SRP-6/6a has two minor refinements relative to SRP-3/RFC2945:
 * 1. The "g^x" value is multipled by three in the client's
 *    calculation of its session key.
 *    SRP-6a: The "g^x" value is multiplied by the hash of
 *            N and g in the client's session key calculation.
 * 2. The value of u is taken as the hash of A and B,
 *    instead of the top 32 bits of the hash of B.
 *    This eliminates the old restriction where the
 *    server had to receive A before it could send B.
 */

/*
 * The RFC2945 client keeps track of the running hash
 * state via SRPHashCtx structures pointed to by the
 * meth_data pointer.  The "hash" member is the hash value that
 * will be sent to the other side; the "ckhash" member is the
 * hash value expected from the other side.
 */
struct client_meth_st {
  SRPHashCtx hash;
  SRPHashCtx ckhash;
  unsigned char k[SRP_MAX_DIGEST_SIZE];
};

#define CLIENT_CTXP(srp)    ((struct client_meth_st *)(srp)->meth_data)

static SRP_RESULT
srp6a_client_init(SRP * srp)
{
  srp->magic = SRP_MAGIC_CLIENT;
  srp->flags = SRP_FLAG_MOD_ACCEL | SRP_FLAG_LEFT_PAD;
  srp->meth_data = malloc(sizeof(struct client_meth_st));
  srp->hashDesc->init_f(&CLIENT_CTXP(srp)->hash);
  srp->hashDesc->init_f(&CLIENT_CTXP(srp)->ckhash);
  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_client_finish(SRP * srp)
{
  if(srp->meth_data) {
    memset(srp->meth_data, 0, sizeof(struct client_meth_st));
    free(srp->meth_data);
  }
  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_client_params(SRP * srp, const unsigned char * modulus, int modlen,
		   const unsigned char * generator, int genlen,
		   const unsigned char * salt, int saltlen)
{
  int i;
  unsigned char buf1[SRP_MAX_DIGEST_SIZE], buf2[SRP_MAX_DIGEST_SIZE];
  SRPHashCtx ctxt;

  /* Fields set by SRP_set_params */

  /* Update hash state */
  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, modulus, (size_t) modlen);
  srp->hashDesc->final_f(buf1, &ctxt);	/* buf1 = H(modulus) */

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, generator, (size_t) genlen);
  srp->hashDesc->final_f(buf2, &ctxt);	/* buf2 = H(generator) */

  for(i = 0; i < (int) srp->hashDesc->digestLen; ++i)
    buf1[i] ^= buf2[i];		/* buf1 = H(modulus) xor H(generator) */

  /* hash: H(N) xor H(g) */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, buf1, srp->hashDesc->digestLen);

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->username->data, (size_t) srp->username->length);
  srp->hashDesc->final_f(buf1, &ctxt);	/* buf1 = H(user) */

  /* hash: (H(N) xor H(g)) | H(U) */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, buf1, srp->hashDesc->digestLen);

  /* hash: (H(N) xor H(g)) | H(U) | s */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, salt, (size_t) saltlen);

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_client_auth(SRP * srp, const unsigned char * a, int alen)
{
  /* On the client, the authenticator is the raw password-derived hash */
  srp->password = BigIntegerFromBytes(a, alen);

  /* verifier = g^x mod N */
  srp->verifier = BigIntegerFromInt(0);
  BigIntegerModExp(srp->verifier, srp->generator, srp->password, srp->modulus, srp->bctx, srp->accel);

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_client_passwd(SRP * srp, const unsigned char * p, int plen)
{
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];
  int r;

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->username->data, (size_t) srp->username->length);
  srp->hashDesc->update_f(&ctxt, ":", 1);
  srp->hashDesc->update_f(&ctxt, p, (size_t) plen);
  srp->hashDesc->final_f(dig, &ctxt);	/* dig = H(U | ":" | P) */

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->salt->data, (size_t) srp->salt->length);
  srp->hashDesc->update_f(&ctxt, dig, srp->hashDesc->digestLen);
  srp->hashDesc->final_f(dig, &ctxt);	/* dig = H(s | H(U | ":" | P)) */
  memset(&ctxt, 0, sizeof(ctxt));

  r = SRP_set_authenticator(srp, dig, (int) srp->hashDesc->digestLen);
  memset(dig, 0, srp->hashDesc->digestLen);

  return r;
}

static SRP_RESULT
srp6_client_genpub(SRP * srp, cstr ** result)
{
  cstr * astr;
  int slen = (SRP_get_secret_bits(BigIntegerBitLen(srp->modulus)) + 7) / 8;

  if(result == NULL)
    astr = cstr_new();
  else {
    if(*result == NULL)
      *result = cstr_new();
    astr = *result;
  }

  cstr_set_length(astr, BigIntegerByteLen(srp->modulus));
#if SRP_TESTS
  if (srp->test_random) {
    assert(srp->test_random->length == slen);
    memcpy(astr->data, srp->test_random->data, slen);
  } else
#endif
    t_random((unsigned char *) astr->data, (size_t)slen);
  srp->secret = BigIntegerFromBytes((const unsigned char *) astr->data, slen);
#if !SRP_TESTS // Disable this for tests because we need an exact match of the public key.
  /* Force g^a mod n to "wrap around" by adding log[2](n) to "a". */
  BigIntegerAddInt(srp->secret, srp->secret, (unsigned int)BigIntegerBitLen(srp->modulus));
#endif
  /* A = g^a mod n */
  srp->pubkey = BigIntegerFromInt(0);
  BigIntegerModExp(srp->pubkey, srp->generator, srp->secret, srp->modulus, srp->bctx, srp->accel);
  BigIntegerToCstr(srp->pubkey, astr);

  /* hash: (H(N) xor H(g)) | H(U) | s | A */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, astr->data, (size_t) astr->length);
  /* ckhash: A */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->ckhash, astr->data, (size_t) astr->length);

  if(result == NULL)	/* astr was a temporary */
    cstr_clear_free(astr);

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_client_key_ex(SRP * srp, cstr ** result,
		   const unsigned char * pubkey, int pubkeylen, BigInteger k)
{
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];
  BigInteger gb, e;
  cstr * s;
  int modlen;

  modlen = BigIntegerByteLen(srp->modulus);
  if(pubkeylen > modlen)
    return SRP_ERROR;

  /* Compute u from client's and server's values */
  srp->hashDesc->init_f(&ctxt);
  /* Use s as a temporary to store client's value */
  s = cstr_new();
  if(srp->flags & SRP_FLAG_LEFT_PAD) {
    BigIntegerToCstrEx(srp->pubkey, s, modlen);
    srp->hashDesc->update_f(&ctxt, s->data, (size_t) s->length);
    if(pubkeylen < modlen) {
      memcpy(s->data + (modlen - pubkeylen), pubkey, pubkeylen);
      memset(s->data, 0, modlen - pubkeylen);
      srp->hashDesc->update_f(&ctxt, s->data, (size_t) modlen);
    }
    else
      srp->hashDesc->update_f(&ctxt, pubkey, (size_t) pubkeylen);
  }
  else {
    BigIntegerToCstr(srp->pubkey, s);
    srp->hashDesc->update_f(&ctxt, s->data, (size_t) s->length);
    srp->hashDesc->update_f(&ctxt, pubkey, (size_t) pubkeylen);
  }
  srp->hashDesc->final_f(dig, &ctxt);
  srp->u = BigIntegerFromBytes(dig, (int) srp->hashDesc->digestLen);

  /* hash: (H(N) xor H(g)) | H(U) | s | A | B */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, pubkey, (size_t) pubkeylen);

  gb = BigIntegerFromBytes(pubkey, pubkeylen);
  /* reject B == 0, B >= modulus */
  if(BigIntegerCmp(gb, srp->modulus) >= 0 || BigIntegerCmpInt(gb, 0) == 0) {
    BigIntegerFree(gb);
    cstr_clear_free(s);
    return SRP_ERROR;
  }
  e = BigIntegerFromInt(0);
  srp->key = BigIntegerFromInt(0);
  /* unblind g^b (mod N) */
  BigIntegerSub(srp->key, srp->modulus, srp->verifier);
  /* use e as temporary, e == -k*v (mod N) */
  BigIntegerMul(e, k, srp->key, srp->bctx);
  BigIntegerAdd(e, e, gb);
  BigIntegerMod(gb, e, srp->modulus, srp->bctx);

  /* compute gb^(a + ux) (mod N) */
  BigIntegerMul(e, srp->password, srp->u, srp->bctx);
  BigIntegerAdd(e, e, srp->secret);	/* e = a + ux */

  BigIntegerModExp(srp->key, gb, e, srp->modulus, srp->bctx, srp->accel);
  BigIntegerClearFree(e);
  BigIntegerClearFree(gb);

  /* convert srp->key into a session key, update hash states */
  BigIntegerToCstr(srp->key, s);
  if(srp->hashDesc->keyLen == 40) {
    t_mgf1(CLIENT_CTXP(srp)->k, (unsigned int)srp->hashDesc->keyLen, (const unsigned char *) s->data, (unsigned int)s->length); /* Interleaved hash */
  } else {
    srp->hashDesc->init_f(&ctxt);
    srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
    srp->hashDesc->final_f(CLIENT_CTXP(srp)->k, &ctxt);
  }
  cstr_clear_free(s);

  /* hash: (H(N) xor H(g)) | H(U) | s | A | B | K */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, CLIENT_CTXP(srp)->k, srp->hashDesc->keyLen);
  /* hash: (H(N) xor H(g)) | H(U) | s | A | B | K | ex_data */
  if(srp->ex_data->length > 0)
    srp->hashDesc->update_f(&CLIENT_CTXP(srp)->hash, srp->ex_data->data, (size_t) srp->ex_data->length);

  if(result) {
    if(*result == NULL)
      *result = cstr_new();
    cstr_setn(*result, (const char *) CLIENT_CTXP(srp)->k, (int) srp->hashDesc->keyLen);
  }

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6a_client_key(SRP * srp, cstr ** result,
		 const unsigned char * pubkey, int pubkeylen)
{
  SRP_RESULT ret;
  BigInteger k;
  cstr * s;
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];

  srp->hashDesc->init_f(&ctxt);
  s = cstr_new();
  BigIntegerToCstr(srp->modulus, s);
  srp->hashDesc->update_f(&ctxt, s->data, (size_t) s->length);
  if(srp->flags & SRP_FLAG_LEFT_PAD)
    BigIntegerToCstrEx(srp->generator, s, s->length);
  else
    BigIntegerToCstr(srp->generator, s);
  srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
  srp->hashDesc->final_f(dig, &ctxt);
  cstr_free(s);

  k = BigIntegerFromBytes(dig, (int) srp->hashDesc->digestLen);
  if(BigIntegerCmpInt(k, 0) == 0)
    ret = SRP_ERROR;
  else
    ret = srp6_client_key_ex(srp, result, pubkey, pubkeylen, k);
  BigIntegerClearFree(k);
  return ret;
}

static SRP_RESULT
srp6_client_verify(SRP * srp, const unsigned char * proof, int prooflen)
{
  unsigned char expected[SRP_MAX_DIGEST_SIZE];

  srp->hashDesc->final_f(expected, &CLIENT_CTXP(srp)->ckhash);
  if(prooflen == (int) srp->hashDesc->digestLen && memcmp(expected, proof, (size_t) prooflen) == 0)
    return SRP_SUCCESS;
  else
    return SRP_ERROR;
}

static SRP_RESULT
srp6_client_respond(SRP * srp, cstr ** proof)
{
  if(proof == NULL)
    return SRP_ERROR;

  if(*proof == NULL)
    *proof = cstr_new();

  /* proof contains client's response */
  cstr_set_length(*proof, (int) srp->hashDesc->digestLen);
  srp->hashDesc->final_f((unsigned char *)(*proof)->data, &CLIENT_CTXP(srp)->hash);

  /* ckhash: A | M | K */
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->ckhash, (*proof)->data, (size_t)(*proof)->length);
  srp->hashDesc->update_f(&CLIENT_CTXP(srp)->ckhash, CLIENT_CTXP(srp)->k, srp->hashDesc->keyLen);
  return SRP_SUCCESS;
}

static SRP_METHOD srp6a_client_meth = {
  "SRP-6a client (tjw)",
  srp6a_client_init,
  srp6_client_finish,
  srp6_client_params,
  srp6_client_auth,
  srp6_client_passwd,
  srp6_client_genpub,
  srp6a_client_key,
  srp6_client_verify,
  srp6_client_respond,
  NULL
};

_TYPE( SRP_METHOD * )
SRP6a_client_method()
{
  return &srp6a_client_meth;
}
