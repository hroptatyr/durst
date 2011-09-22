/*** iso4217.h -- currency symbols
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <hroptatyr@gna.org>
 *
 * This file is part of libpfack.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#if !defined INCLUDED_iso4217_h_
#define INCLUDED_iso4217_h_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pfack_4217_s *pfack_4217_t;
typedef const struct pfack_4217_s *const_pfack_4217_t;
/* for code where pointers are inapt provide an index */
typedef unsigned int pfack_4217_id_t;
/* the official currency code */
typedef short unsigned int pfack_4217_code_t;
/* the official currency abbreviated name, iso 3166 + one character */
typedef const char pfack_4217_sym_t[4];

/**
 * Structure for iso4217 symbols and codes.
 * This thing is a pun really, cast to char* to use it as symbol directly.
 * The symbol strings are nul terminated, so occupy exactly 4 bytes.
 * The code numbers should be padded with naughts to the left if used in
 * official contexts. */
struct pfack_4217_s {
	/** 4217 symbol, this is an iso3166 country code plus one character */
	const char sym[4];
	/** 4217 code */
	const pfack_4217_code_t cod:10;
	/* exponent (of 10) to get to the minor currency, 2 = 100, 3 = 1000 */
	const signed char exp:4;
	/* official name, VLA(!), not really though see alignment */
	const char *name;
} __attribute__((aligned(64)));

/**
 * ISO4217 symbols and code. */
extern const struct pfack_4217_s pfack_4217[];


#define PFACK_4217(_x)		((const_pfack_4217_t)&pfack_4217[_x])

#define PFACK_4217_SYM(_x)	((const char*)&pfack_4217[_x].sym)

static inline const char __attribute__((always_inline))*
pfack_4217_sym(pfack_4217_id_t slot)
{
	return PFACK_4217_SYM(slot);
}

#define PFACK_4217_COD(_x)	((short unsigned int)pfack_4217[_x].cod)

static inline pfack_4217_code_t __attribute__((always_inline))
pfack_4217_cod(pfack_4217_id_t slot)
{
	return PFACK_4217_COD(slot);
}

#define PFACK_4217_EXP(_x)	((signed char)pfack_4217[_x].exp)

static inline signed char __attribute__((always_inline))
pfack_4217_exp(pfack_4217_id_t slot)
{
	return PFACK_4217_EXP(slot);
}

#define PFACK_4217_NAME(_x)	((const char*)pfack_4217[_x].name)

static inline const char __attribute__((always_inline))*
pfack_4217_name(pfack_4217_id_t slot)
{
	return PFACK_4217_NAME(slot);
}

/**
 * Return the id (the index into the global 4217 array) of PTR. */
static inline pfack_4217_id_t __attribute__((always_inline))
pfack_4217_id(const_pfack_4217_t ptr)
{
	return (pfack_4217_id_t)(ptr - pfack_4217);
}


/* frequently used currencies */
/* EUR */
#define PFACK_4217_EUR_IDX	((pfack_4217_id_t)48)
#define PFACK_4217_EUR		(PFACK_4217(PFACK_4217_EUR_IDX))
#define PFACK_4217_EUR_SYM	(PFACK_4217_SYM(PFACK_4217_EUR_IDX))
/* USD */
#define PFACK_4217_USD_IDX	((pfack_4217_id_t)149)
#define PFACK_4217_USD		(PFACK_4217(PFACK_4217_USD_IDX))
#define PFACK_4217_USD_SYM	(PFACK_4217_SYM(PFACK_4217_USD_IDX))
/* GBP */
#define PFACK_4217_GBP_IDX	((pfack_4217_id_t)51)
#define PFACK_4217_GBP		(PFACK_4217(PFACK_4217_GBP_IDX))
#define PFACK_4217_GBP_SYM	(PFACK_4217_SYM(PFACK_4217_GBP_IDX))
/* CAD */
#define PFACK_4217_CAD_IDX	((pfack_4217_id_t)26)
#define PFACK_4217_CAD		(PFACK_4217(PFACK_4217_CAD_IDX))
#define PFACK_4217_CAD_SYM	(PFACK_4217_SYM(PFACK_4217_CAD_IDX))
/* AUD */
#define PFACK_4217_AUD_IDX	((pfack_4217_id_t)7)
#define PFACK_4217_AUD		(PFACK_4217(PFACK_4217_AUD_IDX))
#define PFACK_4217_AUD_SYM	(PFACK_4217_SYM(PFACK_4217_AUD_IDX))
/* NZD */
#define PFACK_4217_NZD_IDX	((pfack_4217_id_t)110)
#define PFACK_4217_NZD		(PFACK_4217(PFACK_4217_NZD_IDX))
#define PFACK_4217_NZD_SYM	(PFACK_4217_SYM(PFACK_4217_NZD_IDX))
/* KRW */
#define PFACK_4217_KRW_IDX	((pfack_4217_id_t)78)
#define PFACK_4217_KRW		(PFACK_4217(PFACK_4217_KRW_IDX))
#define PFACK_4217_KRW_SYM	(PFACK_4217_SYM(PFACK_4217_KRW_IDX))
/* JPY */
#define PFACK_4217_JPY_IDX	((pfack_4217_id_t)72)
#define PFACK_4217_JPY		(PFACK_4217(PFACK_4217_JPY_IDX))
#define PFACK_4217_JPY_SYM	(PFACK_4217_SYM(PFACK_4217_JPY_IDX))
/* INR */
#define PFACK_4217_INR_IDX	((pfack_4217_id_t)66)
#define PFACK_4217_INR		(PFACK_4217(PFACK_4217_INR_IDX))
#define PFACK_4217_INR_SYM	(PFACK_4217_SYM(PFACK_4217_INR_IDX))
/* HKD */
#define PFACK_4217_HKD_IDX	((pfack_4217_id_t)59)
#define PFACK_4217_HKD		(PFACK_4217(PFACK_4217_HKD_IDX))
#define PFACK_4217_HKD_SYM	(PFACK_4217_SYM(PFACK_4217_HKD_IDX))
/* CNY */
#define PFACK_4217_CNY_IDX	((pfack_4217_id_t)33)
#define PFACK_4217_CNY		(PFACK_4217(PFACK_4217_CNY_IDX))
#define PFACK_4217_CNY_SYM	(PFACK_4217_SYM(PFACK_4217_CNY_IDX))
/* RUB */
#define PFACK_4217_RUB_IDX	((pfack_4217_id_t)122)
#define PFACK_4217_RUB		(PFACK_4217(PFACK_4217_RUB_IDX))
#define PFACK_4217_RUB_SYM	(PFACK_4217_SYM(PFACK_4217_RUB_IDX))
/* SEK */
#define PFACK_4217_SEK_IDX	((pfack_4217_id_t)128)
#define PFACK_4217_SEK		(PFACK_4217(PFACK_4217_SEK_IDX))
#define PFACK_4217_SEK_SYM	(PFACK_4217_SYM(PFACK_4217_SEK_IDX))
/* NOK */
#define PFACK_4217_NOK_IDX	((pfack_4217_id_t)108)
#define PFACK_4217_NOK		(PFACK_4217(PFACK_4217_NOK_IDX))
#define PFACK_4217_NOK_SYM	(PFACK_4217_SYM(PFACK_4217_NOK_IDX))
/* DKK */
#define PFACK_4217_DKK_IDX	((pfack_4217_id_t)41)
#define PFACK_4217_DKK		(PFACK_4217(PFACK_4217_DKK_IDX))
#define PFACK_4217_DKK_SYM	(PFACK_4217_SYM(PFACK_4217_DKK_IDX))
/* ISK */
#define PFACK_4217_ISK_IDX	((pfack_4217_id_t)69)
#define PFACK_4217_ISK		(PFACK_4217(PFACK_4217_ISK_IDX))
#define PFACK_4217_ISK_SYM	(PFACK_4217_SYM(PFACK_4217_ISK_IDX))
/* EEK */
#define PFACK_4217_EEK_IDX	((pfack_4217_id_t)44)
#define PFACK_4217_EEK		(PFACK_4217(PFACK_4217_EEK_IDX))
#define PFACK_4217_EEK_SYM	(PFACK_4217_SYM(PFACK_4217_EEK_IDX))
/* HUF */
#define PFACK_4217_HUF_IDX	((pfack_4217_id_t)63)
#define PFACK_4217_HUF		(PFACK_4217(PFACK_4217_HUF_IDX))
#define PFACK_4217_HUF_SYM	(PFACK_4217_SYM(PFACK_4217_HUF_IDX))
/* MXN */
#define PFACK_4217_MXN_IDX	((pfack_4217_id_t)101)
#define PFACK_4217_MXN		(PFACK_4217(PFACK_4217_MXN_IDX))
#define PFACK_4217_MXN_SYM	(PFACK_4217_SYM(PFACK_4217_MXN_IDX))
/* BRL */
#define PFACK_4217_BRL_IDX	((pfack_4217_id_t)20)
#define PFACK_4217_BRL		(PFACK_4217(PFACK_4217_BRL_IDX))
#define PFACK_4217_BRL_SYM	(PFACK_4217_SYM(PFACK_4217_BRL_IDX))
/* CLP */
#define PFACK_4217_CLP_IDX	((pfack_4217_id_t)32)
#define PFACK_4217_CLP		(PFACK_4217(PFACK_4217_CLP_IDX))
#define PFACK_4217_CLP_SYM	(PFACK_4217_SYM(PFACK_4217_CLP_IDX))
/* CHF */
#define PFACK_4217_CHF_IDX	((pfack_4217_id_t)29)
#define PFACK_4217_CHF		(PFACK_4217(PFACK_4217_CHF_IDX))
#define PFACK_4217_CHF_SYM	(PFACK_4217_SYM(PFACK_4217_CHF_IDX))

/* precious metals */
/* gold */
#define PFACK_4217_XAU_IDX	((pfack_4217_id_t)154)
#define PFACK_4217_XAU		(PFACK_4217(PFACK_4217_XAU_IDX))
#define PFACK_4217_XAU_SYM	(PFACK_4217_SYM(PFACK_4217_XAU_IDX))
/* silver */
#define PFACK_4217_XAG_IDX	((pfack_4217_id_t)153)
#define PFACK_4217_XAG		(PFACK_4217(PFACK_4217_XAG_IDX))
#define PFACK_4217_XAG_SYM	(PFACK_4217_SYM(PFACK_4217_XAG_IDX))
/* platinum */
#define PFACK_4217_XPT_IDX	((pfack_4217_id_t)165)
#define PFACK_4217_XPT		(PFACK_4217(PFACK_4217_XPT_IDX))
#define PFACK_4217_XPT_SYM	(PFACK_4217_SYM(PFACK_4217_XPT_IDX))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_iso4217_h_ */
