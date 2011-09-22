/*** durst.c -- the daily urs tool
 *
 * LICENCE here
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "urs.h"
#include "urs_fut.h"
#include "urs_cash.h"

#include "iso4217.h"
#include "iso4217.c"

#if defined DEBUG_FLAG
# define URS_DEBUG(args...)	fprintf(stderr, "[urs] " args);
#else  /* !DEBUG_FLAG */
# define URS_DEBUG(args...)
#endif	/* DEBUG_FLAG */
#if !defined UNUSED
# define UNUSED(x)	__attribute__((unused)) x
#endif	/* !UNUSED */

#define countof(x)	(sizeof(x) / sizeof(*x))

typedef struct pos_s *pos_t;
typedef struct pf_s *pf_t;

typedef struct __nav_pos_s *urs_nav_pos_t;

/* specific guys */
struct fx_pos_s {
	struct __hdr_s base;
	struct __hdr_s term;

	struct __mkt_s rate;
};

struct __nav_pos_s {
	/* ident stuff */
	struct __hdr_s hdr;

	/* this currency */
	const_pfack_4217_t tccy;

	/* characteristics, track soft and hard positions,
	 * we distinguish forex positions here as well because we must
	 * not introduce instruments ourselves, as long as we don't do
	 * FX positions anyway.
	 * a forex long position would mean, buy the currency in question,
	 * debitting the base currency position, short vice versa */
	struct __val_s base;

	double soft_ini;
	double hard_ini;
};

typedef enum {
	POSTY_UNK,
	POSTY_FUT,
	POSTY_CASH,
	POSTY_FX,
	POSTY_FXFW,
	POSTY_STK,
	POSTY_NAV,
} posty_t;

struct pos_s {
	posty_t ty;
	union {
		struct __fut_pos_s fut;
		struct __cash_pos_s cash;
		struct __nav_pos_s nav;
	};
};

struct pf_s {
	/* hard */
	struct __val_s val;
	struct __val_s val_ini;
	const_pfack_4217_t bccy;

	size_t nposs;
	struct pos_s poss[];
};


/* posty specific accessors */
static double
pos_soft_val(pos_t p)
{
	switch (p->ty) {
	case POSTY_FUT:
		return p->fut.base.soft = p->fut.term.soft = 0.0;
	case POSTY_CASH: {
		double b_s = p->cash.term.soft / p->cash.s_mkt.stl;
		double b_fx = p->cash.forex / p->cash.s_mkt.stl;
		return (p->cash.base.soft = b_s) + b_fx;
	}
	default:
		return 0.0;
	}
}

static double
pos_hard_val(pos_t p)
{
	switch (p->ty) {
	case POSTY_FUT:
		return p->fut.base.hard =
			p->fut.term.hard / p->fut.val_fac;
	case POSTY_CASH:
		return p->cash.base.hard =
			p->cash.term.hard / p->cash.s_mkt.stl;
	default:
		return 0.0;
	}
}

static double
compute_pf_val(pf_t pf)
{
	pf->val = pf->val_ini;

	for (size_t i = 0; i < pf->nposs; i ++) {
		if (pf->val_ini.hard == 0.0 || pf->poss[i].ty != POSTY_CASH) {
			/* only add stuff up if the portfolio hasn't
			 * had a hard-set NAV */
			pf->val.soft += pos_soft_val(pf->poss + i);
			pf->val.hard += pos_hard_val(pf->poss + i);
		}
	}
	URS_DEBUG("pf_val() soft %.6f hard %.6f\n", pf->val.soft, pf->val.hard);
	return pf->val.soft + pf->val.hard;
}

static urs_cash_pos_t
find_cash_pos(pf_t pf, pos_t pos)
{
	switch (pos->ty) {
	case POSTY_CASH:
		return &pos->cash;
	case POSTY_FUT:
		for (size_t i = 0; i < pf->nposs; i++) {
			if (pf->poss[i].ty == POSTY_CASH &&
			    pf->poss[i].cash.tccy == pos->fut.ccy) {
				return &pf->poss[i].cash;
			}
		}
	default:
		return NULL;
	}
}

/* future rebalancing relative to the NAV of the portfolio */
static int
reba_relanav_pos(pos_t pos, double nav)
{
	switch (pos->ty) {
	default:
		break;

	case POSTY_CASH:
		urs_cash_relanav(&pos->cash, nav);
		break;

	case POSTY_FUT:
		urs_fut_relanav(&pos->fut, nav);
		break;
	}
	return 0;
}

static bool
reba_relanav_check(pf_t pf, double nav)
{
	bool res = true;

	for (size_t i = 0; i < pf->nposs; i++) {
		double lo, hi;
		double ratio;
		/* the nav we give here is relative to the ccy of the pos */
		urs_cash_pos_t cp = find_cash_pos(pf, pf->poss + i);
		double tnav = cp ? nav * cp->s_mkt.stl : 0.0;

		switch (pf->poss[i].ty) {
			double hard, soft;
		default:
			continue;

		case POSTY_CASH:
			/* convert nav to term_nav, needed for the rolandique
			 * definition of exposures */
			soft = pf->poss[i].cash.base.soft;
			hard = pf->poss[i].cash.base.hard;

			ratio = (soft + hard) / tnav;
			if ((lo = pf->poss[i].cash.band.lo) < 0.0 ||
			    (hi = pf->poss[i].cash.band.hi) < 0.0) {
				continue;
			}
			break;

		case POSTY_FUT:
			soft = pf->poss[i].fut.pos.soft;
			hard = pf->poss[i].fut.pos.hard;

			ratio = (soft + hard) / tnav;
			lo = pf->poss[i].fut.band.lo;
			hi = pf->poss[i].fut.band.hi;
			break;
		}

		if (ratio < lo || ratio > hi) {
			URS_DEBUG("NEED REBA %s: %.8g < %.8g < %.8g NOT\n",
				  pf->poss[i].fut.hdr.sym, lo, ratio, hi);
			res = false;
#if !defined DEBUG_FLAG
			break;
#endif	/* DEBUG_FLAG */
		}
	}
	return res;
}

static void
reba_relanav(pf_t pf, double nav)
{
	if (reba_relanav_check(pf, nav)) {
		return;
	}

	for (size_t i = 0; i < pf->nposs; i++) {
		/* the nav we give here is relative to the ccy of the pos */
		urs_cash_pos_t cp = find_cash_pos(pf, pf->poss + i);
		double tnav = cp ? nav * cp->s_mkt.stl : 0.0;
		reba_relanav_pos(pf->poss + i, tnav);
	}
	return;
}


/* test setup */
static size_t nmy4217 = 0;
static struct pfack_4217_s my4217[16];

static void
free_pf(pf_t pf)
{
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		switch (p->ty) {
		case POSTY_CASH:
			free(p->cash.hdr.sym);
			break;
		case POSTY_FUT:
			free(p->fut.hdr.sym);
			break;
		default:
			break;
		}
	}
	free(pf);
	return;
}

static void
fprint_pos(pf_t pf, pos_t pos, double nav, FILE *whither)
{
	switch (pos->ty) {
	case POSTY_UNK:
	default:
		break;

	case POSTY_CASH: {
		double tcv = urs_cash_value(&pos->cash);
		double nav = pf->val.soft + pf->val.hard;

		fprintf(whither, "CASH %s\t\
soft %.4f\thard %.4f\tfx %.4f\t%.6e v %.6e\n",
			pos->cash.hdr.sym,
			pos->cash.term.soft,
			pos->cash.term.hard,
			pos->cash.forex,
			tcv / nav, pos->cash.band.med);
		break;
	}

	case POSTY_FUT: {
		urs_cash_pos_t cp = find_cash_pos(pf, pos);
		double tnav = cp ? nav * cp->s_mkt.stl : 0.0;
		double ex = cp
			? (pos->fut.pos.hard + pos->fut.pos.soft) / tnav : 0.0;

		fprintf(whither, "FUT %s\t\
%.4f (%.4f)\t* %.4f\t@ %.4f/%.4f\t\
soft %.4f\thard %.4f\t%.6e v %.6e\n",
			pos->fut.hdr.sym,
			pos->fut.pos.hard,
			pos->fut.pos.soft,
			pos->fut.mult,
			pos->fut.f_mkt.bid,
			pos->fut.f_mkt.ask,
			pos->fut.term.soft,
			pos->fut.term.hard,
			ex, pos->fut.band.med);
		break;
	}
	}
	return;
}

static void
fprint_poss(pf_t pf, FILE *whither)
{
	double nav = compute_pf_val(pf);

	fprintf(whither, "PORTFOLIO\tsoft %2.4f\thard %2.4f\tnav %.4f\n",
		pf->val.soft, pf->val.hard, nav);
	for (size_t i = 0; i < pf->nposs; i++) {
		if (pf->poss[i].ty == POSTY_CASH) {
			fprintf(whither, "\
TERM\t%s\tsoft %.4f\thard %.4f\tnav %.4f\n",
				pf->poss[i].cash.tccy->sym,
				pf->val.soft * pf->poss[i].cash.s_mkt.stl,
				pf->val.hard * pf->poss[i].cash.s_mkt.stl,
				nav * pf->poss[i].cash.s_mkt.stl);
		}
	}
	for (size_t i = 0; i < pf->nposs; i++) {
		fprint_pos(pf, pf->poss + i, nav, whither);
	}
	return;
}

static void
fprint_trades(pf_t pf, FILE *whither)
{
	/* traverse the soft pos's to emit trades */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;

		switch (p->ty) {
		case POSTY_CASH: {
			double d = p->cash.term.hard - p->cash.hard_ini;
			double dfx = p->cash.forex;

			if (d > 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			} else if (d < 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			}

			/* do not buy or sell base currency?
			 * at the moment we use the criterion, if it's not
			 * balanced do fuckall */
			if (p->cash.band.med < 0.0) {
				break;
			}

			if (dfx > 0.0) {
				fprintf(whither, "BUY\t%.4f\t%s\n",
					dfx, p->cash.tccy->sym);
			} else if (dfx < 0.0) {
				fprintf(whither, "SELL\t%.4f\t%s\n",
					-dfx, p->cash.tccy->sym);
			}
			break;
		}
		case POSTY_FUT:
			if (p->fut.pos.soft > 0.0 &&
			    p->fut.pos.hard < 0.0) {
				fprintf(whither, "SHORT_BUY\t%.4f\t%s\n",
					p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft > 0.0) {
				fprintf(whither, "BUY\t%.4f\t%s\n",
					p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft < 0.0 &&
				   p->fut.pos.hard > 0.0) {
				fprintf(whither, "SELL\t%.4f\t%s\n",
					-p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft < 0.0) {
				fprintf(whither, "SHORT_SELL\t%.4f\t%s\n",
					-p->fut.pos.soft, p->fut.hdr.sym);
			}

			if (p->fut.term.hard != 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					p->fut.term.hard, p->fut.ccy->sym);
			}
			if (p->fut.term.soft != 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					p->fut.term.soft, p->fut.ccy->sym);
			}
			break;
		default:
			break;
		}
	}
	return;
}

static void
fprint_trades_fixml(pf_t pf, FILE *whither)
{
	/* traverse the soft pos's to emit trades */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;

		switch (p->ty) {
		case POSTY_CASH: {
			double d = p->cash.term.hard - p->cash.hard_ini;
			double dfx = p->cash.forex;

			if (d > 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			} else if (d < 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			}

			if (dfx > 0.0) {
				fprintf(whither, "BUY\t%.4f\t%s\n",
					dfx, p->cash.tccy->sym);
			} else if (dfx < 0.0) {
				fprintf(whither, "SELL\t%.4f\t%s\n",
					-dfx, p->cash.tccy->sym);
			}
			break;
		}
		case POSTY_FUT: {
			if (p->fut.pos.soft > 0.0 &&
			    p->fut.pos.hard < 0.0) {
				fprintf(whither, "SHORT_BUY\t%.4f\t%s\n",
					p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft > 0.0) {
				fprintf(whither, "BUY\t%.4f\t%s\n",
					p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft < 0.0 &&
				   p->fut.pos.hard > 0.0) {
				fprintf(whither, "SELL\t%.4f\t%s\n",
					-p->fut.pos.soft, p->fut.hdr.sym);
			} else if (p->fut.pos.soft < 0.0) {
				fprintf(whither, "SHORT_SELL\t%.4f\t%s\n",
					-p->fut.pos.soft, p->fut.hdr.sym);
			}
		}
		default:
			break;
		}
	}
	return;
}

static void __attribute__((unused))
fprint_info(pf_t pf, FILE *whither)
{
	/* traverse the soft pos's to emit trades */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;

		switch (p->ty) {
		case POSTY_CASH: {
			double d = p->cash.term.soft - p->cash.soft_ini;
			fprintf(whither, "INFO\t%.4f\t%s\n",
				d, p->cash.hdr.sym);
		}
		default:
			break;
		}
	}
	return;
}

static double
reco_poss_ccy_s(pf_t pf, const_pfack_4217_t ccy)
{
/* go through all future positions and sum up their counter positions,
 * then find a cash position to book this to. */
	double sum = 0.0;

	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;

		switch (p->ty) {
		case POSTY_FUT:
			if (p->fut.ccy->cod == ccy->cod) {
				/* futures account for nothing */
			}
		default:
			break;
		}
	}
	return sum;
}

static double
reco_poss_ccy_h(pf_t pf, const_pfack_4217_t ccy)
{
/* go through all future positions and sum up their counter positions,
 * then find a cash position to book this to. */
	double sum = 0.0;

	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;

		switch (p->ty) {
		case POSTY_FUT:
			if (p->fut.ccy->cod == ccy->cod) {
				sum += p->fut.term.hard;
			}
		default:
			break;
		}
	}
	return sum;
}

static void
reco_poss_freeze(pf_t pf)
{
/* go through all cash positions and gather any softs left over from
 * rebalancing, book it into the soft account of the cash position. */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != NULL) {
			p->cash.soft_ini = p->cash.term.soft;
			p->cash.hard_ini = p->cash.term.hard;
			p->cash.forex_ini = p->cash.forex;
		}
	}
	return;
}

static void
reco_poss_thaw(pf_t pf)
{
/* go through all cash positions and gather any softs left over from
 * rebalancing, book it into the soft account of the cash position. */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != NULL) {
			p->cash.term.soft = p->cash.soft_ini;
			p->cash.term.hard = p->cash.hard_ini;
			p->cash.forex = p->cash.forex_ini;
		}
	}
	return;
}

static void
reco_poss_reset(pf_t pf)
{
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != NULL) {
			p->cash.term.soft = p->cash.soft_ini;
			p->cash.term.hard = p->cash.hard_ini;
			p->cash.forex = 0.0;
		}
	}
	return;
}

static void __attribute__((unused))
reco_poss(pf_t pf)
{
/* go through all cash positions and gather any softs left over from
 * rebalancing, book it into the soft account of the cash position. */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != NULL) {
			p->cash.term.soft += reco_poss_ccy_s(pf, p->cash.tccy);
			p->cash.term.hard += reco_poss_ccy_h(pf, p->cash.tccy);
		}
	}
	return;
}

static void
set_base_ccy_fut(pf_t pf, const_pfack_4217_t ccy, double val_fac)
{
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_FUT && p->fut.ccy == ccy) {
			p->fut.val_fac = val_fac;
		}
	}
	return;	
}

static void
set_base_currency(pf_t pf, const_pfack_4217_t ccy)
{
	urs_cash_pos_t bp;

	/* start out by setting all CCY future position factors to 1.0 */
	set_base_ccy_fut(pf, ccy, 1.0);

	/* find the base currency cash position */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy == ccy) {
			bp = &p->cash;
			break;
		}
	}

	/* find the currency in question, otherwise create a position */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != ccy) {
			set_base_ccy_fut(pf, p->cash.tccy, p->cash.s_mkt.stl);
			p->cash.bp = bp;
		}
	}

	/* to avoid confusion, we nil out all the stuff that has no ccy
	 * val fac */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_FUT && p->fut.val_fac == 0.0) {
			p->fut.band.lo =
				p->fut.band.med =
				p->fut.band.hi = 0.0;
			p->fut.val_fac = -1.0;
		}
	}
	pf->bccy = ccy;
	return;
}

static posty_t
__parse_posty(const char *s)
{
	static const char c[] = "CASH";
	static const char f[] = "FUT";
	static const char fx[] = "FX";
	static const char stk[] = "STK";
	static const char nav[] = "NAV";

	if (strncmp(s, c, sizeof(c) - 1) == 0) {
		return POSTY_CASH;
	} else if (strncmp(s, f, sizeof(f) - 1) == 0) {
		return POSTY_FUT;
	} else if (strncmp(s, fx, sizeof(fx) - 1) == 0) {
		return POSTY_FX;
	} else if (strncmp(s, stk, sizeof(stk) - 1) == 0) {
		return POSTY_STK;
	} else if (strncmp(s, nav, sizeof(nav) - 1) == 0) {
		return POSTY_NAV;
	}
	return POSTY_UNK;
}

static const char*
__skip_behind_tab(const char *s)
{
	while (*s != '\0' && *s++ != '\t');
	return s;
}

static const_pfack_4217_t
__find_4217(const char *sym)
{
	for (size_t i = 0; i < countof(pfack_4217); i++) {
		if (strncmp(sym, pfack_4217_sym(i), 3) == 0) {
			return PFACK_4217(i);
		}
	}
	for (size_t j = 0; j < nmy4217; j++) {
		if (strncmp(sym, my4217[j].sym, 3) == 0) {
			return my4217 + j;
		}
	}
	return NULL;
}

static double
read_tab_double(const char *s)
{
	if (*s == '\0' || *s == '\t') {
		return 0.0;
	}
	return strtod(s, NULL);
}

static int
__parse_fut(urs_fut_pos_t fp, const char *line)
{
/* FUT name ccy pos fbid fask fstl rbid rask rstl lo tgt hi fee */
	const char *p;

	p = __skip_behind_tab(line);

	/* frob sym */
	line = p;
	p = __skip_behind_tab(line);
	fp->hdr.sym = strndup(line, p - line - 1);

	/* frob ccy */
	line = p;
	p = __skip_behind_tab(line);
	fp->ccy = __find_4217(line);

	/* frob pos */
	line = p;
	if ((fp->mult = read_tab_double(line)) == 0.0) {
		fp->mult = 1;
	}

	/* frob soft_pos */
	line = __skip_behind_tab(p);
	fp->pos.hard = read_tab_double(p = line);

	/* frob hard_pos */
	line = __skip_behind_tab(p);
	fp->pos.hard = read_tab_double(p = line);

	/* frob fbid */
	line = __skip_behind_tab(p);
	fp->f_mkt.bid = read_tab_double(p = line);

	/* frob fask */
	line = __skip_behind_tab(p);
	fp->f_mkt.ask = read_tab_double(p = line);

	/* frob fstl */
	line = __skip_behind_tab(p);
	fp->f_mkt.stl = read_tab_double(p = line);

	/* frob sbid */
	line = __skip_behind_tab(p);
	fp->s_mkt.bid = read_tab_double(p = line);

	/* frob sask */
	line = __skip_behind_tab(p);
	fp->s_mkt.ask = read_tab_double(p = line);

	/* frob sstl */
	line = __skip_behind_tab(p);
	fp->s_mkt.stl = read_tab_double(p = line);

	/* frob lo */
	line = __skip_behind_tab(p);
	fp->band.lo = read_tab_double(p = line);

	/* frob tgt */
	line = __skip_behind_tab(p);
	fp->band.med = read_tab_double(p = line);

	/* frob hi */
	line = __skip_behind_tab(p);
	fp->band.hi = read_tab_double(p = line);

	/* convenience check */
	if (fp->band.lo > fp->band.hi) {
		double tmp = fp->band.lo;
		fp->band.lo = fp->band.hi;
		fp->band.hi = tmp;
	}

	/* frob fee */
	line = __skip_behind_tab(p);
	fp->fee = read_tab_double(p = line);

	return 0;
}

static int
__parse_cash(urs_cash_pos_t cp, const char *line)
{
/* CASH name soft_pos hard_pos bid ask stl lo med hi soft_fee hard_fee */
	const char *p;

	p = __skip_behind_tab(line);

	/* frob sym */
	line = p;
	p = __skip_behind_tab(line);
	cp->hdr.sym = strndup(line, p - line - 1);

	/* frob ccy */
	line = p;
	p = __skip_behind_tab(line);
	if ((cp->tccy = __find_4217(line)) == NULL) {
		const_pfack_4217_t res = my4217 + nmy4217++;
		memcpy((char*)res, line, 3);
		*((char*)res + 3) = '\0';
		cp->tccy = res;
	}

	/* frob soft */
	line = p;
	cp->soft_ini = cp->term.soft = read_tab_double(p = line);

	/* frob hard */
	line = __skip_behind_tab(p);
	cp->hard_ini = cp->term.hard = read_tab_double(p = line);

	/* set the fx slot for convenience */
	cp->forex_ini = 0.0;

	/* frob bid */
	line = __skip_behind_tab(p);
	cp->s_mkt.bid = read_tab_double(p = line);

	/* frob ask */
	line = __skip_behind_tab(p);
	cp->s_mkt.ask = read_tab_double(p = line);

	/* frob stl */
	line = __skip_behind_tab(p);
	cp->s_mkt.stl = read_tab_double(p = line);

	/* frob lo */
	line = __skip_behind_tab(p);
	cp->band.lo = read_tab_double(p = line);

	/* frob tgt */
	line = __skip_behind_tab(p);
	cp->band.med = read_tab_double(p = line);

	/* frob hi */
	line = __skip_behind_tab(p);
	cp->band.hi = read_tab_double(p = line);

	/* frob soft fee */
	line = __skip_behind_tab(p);
	cp->soft_fee = read_tab_double(p = line);

	/* frob fee */
	line = __skip_behind_tab(p);
	cp->hard_fee = read_tab_double(p = line);

	return 0;
}

static int
__parse_nav(urs_nav_pos_t np, const char *line)
{
/* CASH name soft_pos hard_pos bid ask stl lo med hi soft_fee hard_fee */
	const char *p;

	p = __skip_behind_tab(line);

	/* frob sym, we just set it to NAV */
	line = p;
	np->hdr.sym = strdup("NAV");

	/* frob ccy */
	p = __skip_behind_tab(line);
	if ((np->tccy = __find_4217(line)) == NULL) {
		const_pfack_4217_t res = my4217 + nmy4217++;
		memcpy((char*)res, line, 3);
		*((char*)res + 3) = '\0';
		np->tccy = res;
	}

	/* frob soft */
	line = p;
	np->soft_ini = np->base.soft = read_tab_double(p = line);

	/* frob hard */
	line = __skip_behind_tab(p);
	np->hard_ini = np->base.hard = read_tab_double(p = line);
	return 0;
}

static pf_t
read_pf(FILE *whence)
{
	pf_t res = NULL;
	char *line = NULL;
	size_t len;

	/* read line by line */
	if (whence == NULL) {
		return NULL;
	}

	res = calloc(1, sizeof(*res) + 4 * sizeof(*res->poss));

	while (getline(&line, &len, whence) != -1) {
		posty_t pty = __parse_posty(line);
		pos_t p = res->poss + res->nposs;

		switch ((p->ty = pty)) {
		case POSTY_CASH:
			if (__parse_cash(&p->cash, line) == 0) {
				res->nposs++;
			}
			break;
		case POSTY_FUT:
			if (__parse_fut(&p->fut, line) == 0) {
				res->nposs++;
			}
			break;
		case POSTY_NAV:
			if (__parse_nav(&p->nav, line) == 0) {
				res->nposs++;
				res->val_ini.soft = p->nav.base.soft;
				res->val_ini.hard = p->nav.base.hard;
			}
			break;
		default:
			break;
		}

		/* check whether to resize */
		if (res->nposs % 4 == 0) {
			res = realloc(
				res, sizeof(*res) +
				(res->nposs + 4) * sizeof(*res->poss));
		}
	}
	free(line);
	return res;
}

static int
data_complete_p(pf_t pf)
{
/* check if for all non-0 positions we have market data */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		switch (p->ty) {
		case POSTY_CASH:
			if ((p->cash.term.soft != 0.0 ||
			     p->cash.term.hard != 0.0) &&
			    p->cash.s_mkt.stl == 0.0) {
				return 0;
			}
			break;
		case POSTY_FUT:
			if ((p->fut.pos.soft != 0.0 ||
			     p->fut.pos.hard != 0.0) &&
			    p->fut.f_mkt.stl == 0.0) {
				return 0;
			}
			break;
		default:
			break;
		}
	}
	return 1;
}


/* we need:
 * FUT name ccy pos fbid fask fstl rbid rask rstl lo tgt hi fee
 * CASH name ccy soft_pos hard_pos bid ask stl lo tgt hi soft_fee hard_fee
 */
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "durst-clo.h"
#include "durst-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

static void
__work(pf_t pf, enum enum_outfmt of)
{
	double new_nav = 0.0;
	double old_nav;
	size_t step = 0;
	const size_t max_steps = 10;

	URS_DEBUG("rebalancing ...\n");

	reco_poss_freeze(pf);
	/* cash assets constitute the nav as well, option? */
	old_nav = new_nav = compute_pf_val(pf);
	reco_poss_reset(pf);

	do {
		old_nav = new_nav;
		reba_relanav(pf, old_nav);

		reco_poss_freeze(pf);
		/* cash assets constitute the nav as well, option? */
		new_nav = compute_pf_val(pf);
		reco_poss_reset(pf);

		URS_DEBUG("OLD v NEW %.4f v %.4f\n", old_nav, new_nav);
	} while (abs(old_nav - new_nav) > 0.01 && step++ < max_steps);

	/* reconciliation, could be a CLI option */
	reco_poss_thaw(pf);
	/* now after thawing iterate over the portfolio to get the cash
	 * balances right */
	reba_relanav(pf, new_nav);
	fprint_poss(pf, stderr);

	/* print a list of trades so we can settle this crap */
	switch (of) {
	case outfmt_arg_csv:
		fprint_trades(pf, stdout);
		break;
	case outfmt_arg_fixml:
		fprint_trades_fixml(pf, stdout);
		break;
	default:
		break;
	}
#if 0
	/* for the moment we need info too */
	fprint_info(pf, stdout);
#endif
	return;
}

int
main(int argc, char *argv[])
{
	pf_t inpf;
	struct gengetopt_args_info argi[1];

	/* parse command line and shite, preliminary */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	inpf = read_pf(stdin);
	/* establish base currency */
	set_base_currency(inpf, PFACK_4217_EUR);

	/* adapt levers */
	if (argi->lever_given) {
		for (size_t i = 0; i < inpf->nposs; i++) {
			if (inpf->poss[i].ty) {
				inpf->poss[i].fut.band.lo *= argi->lever_arg;
				inpf->poss[i].fut.band.med *= argi->lever_arg;
				inpf->poss[i].fut.band.hi *= argi->lever_arg;
			}
		}
	}

	if (!data_complete_p(inpf)) {
		/* just refuse to do stuff*/
		fprintf(stderr, "DATA INCOMPLETE ... CUNT OFF\n");
	} else if (argi->nav_only_given) {
		fprint_poss(inpf, stdout);
	} else {
		__work(inpf, argi->outfmt_arg);
	}

	free_pf(inpf);
	return 0;
}

/* durst.c ends here */
