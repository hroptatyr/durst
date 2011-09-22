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

#define countof(x)	(sizeof(x) / sizeof(*x))

typedef struct pos_s *pos_t;
typedef struct pf_s *pf_t;

/* specific guys */
struct fx_pos_s {
	struct __hdr_s base;
	struct __hdr_s term;

	struct __mkt_s rate;
};

typedef enum {
	POSTY_UNK,
	POSTY_FUT,
	POSTY_CASH,
	POSTY_FX,
	POSTY_FXFW,
} posty_t;

struct pos_s {
	posty_t ty;
	union {
		struct __fut_pos_s fut;
		struct __cash_pos_s cash;
	};
};

struct pf_s {
	/* hard */
	struct __val_s val;

	size_t nposs;
	struct pos_s poss[];
};


/* posty specific accessors */
static double
pos_soft_val(pos_t p)
{
	switch (p->ty) {
	case POSTY_FUT:
		if (p->fut.term.soft == 0.0) {
			/* we need a soft val first */
			p->fut.term.soft = urs_fut_value(&p->fut);
		}
		return p->fut.base.soft =
			p->fut.term.soft * p->fut.val_fac;
	case POSTY_CASH:
		return p->cash.base.soft =
			p->cash.term.soft * p->cash.s_mkt.stl;
	default:
		return 0.0;
	}
}

static double
pos_hard_val(pos_t p)
{
	switch (p->ty) {
	case POSTY_FUT:
		p->fut.base.hard =
			p->fut.term.hard * p->fut.val_fac;
		/* we assume reconciliation later */
		return 0.0;
	case POSTY_CASH:
		return p->cash.base.hard =
			p->cash.term.hard * p->cash.s_mkt.stl;
	default:
		return 0.0;
	}
}

static double
compute_pf_val(pf_t pf)
{
	pf->val.soft = 0.0;
	pf->val.hard = 0.0;

	for (size_t i = 0; i < pf->nposs; i ++) {
		pf->val.soft += pos_soft_val(pf->poss + i);
		pf->val.hard += pos_hard_val(pf->poss + i);
	}
	return pf->val.soft + pf->val.hard;
}

/* future rebalancing relative to the NAV of the portfolio */
static int
reba_relanav_pos(pos_t pos, double nav)
{
	switch (pos->ty) {
		double new;
	default:
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
	for (size_t i = 0; i < pf->nposs; i++) {
		double soft, hard;
		double r_s, r_h;

		switch (pf->poss[i].ty) {
		default:
			continue;

		case POSTY_FUT:
			soft = pos_soft_val(pf->poss + i);
			hard = pos_hard_val(pf->poss + i);

			r_s = soft / nav;
			URS_DEBUG("soft %2.4f  nav %2.4f\n", soft, nav);
			r_h = hard / nav;

			URS_DEBUG("r_s %2.4f  r_h %2.4f\n", r_s, r_h);

			if (r_s < pf->poss[i].fut.band.lo ||
			    r_s > pf->poss[i].fut.band.hi) {
				return false;
			}
		}
	}
	return true;
}

static void
reba_relanav(pf_t pf)
{
	double nav;

	/* cash assets constitute the nav as well, option? */
	nav = compute_pf_val(pf);

	if (reba_relanav_check(pf, nav)) {
		return;
	}

	for (size_t i = 0; i < pf->nposs; i++) {
		reba_relanav_pos(pf->poss + i, nav);
	}
	return;
}


/* test setup */
#if 0
FUT	GI	USD	400	190.80	191.20	190.90	0	0	0	0.49	0.50	0.51	1.8
FUT	CL2	USD	2900	22.64	22.66	22.66	0	0	0	0.48	0.50	0.51	1.8
FUT	XAU	USD	100	1533.20	1533.40	1532.0	0	0	0	0.00	0.01	0.1	3.0
CASH	USD	USD	0.0	440000.0	1.41025	1.41035	1.41020	-1	-1	-1	0.0	0.0001
#endif

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
fprint_pos(pos_t pos, double nav, FILE *whither)
{
	switch (pos->ty) {
	case POSTY_UNK:
	default:
		break;

	case POSTY_CASH:
		fprintf(whither, "CASH %s\tsoft %2.4f\thard %2.4f\n",
			pos->cash.hdr.sym,
			pos->cash.term.soft,
			pos->cash.term.hard);
		break;

	case POSTY_FUT: {
		double ex = pos->fut.base.soft / nav;

		fprintf(whither, "FUT %s\t\
%.4f (%.4f)\t* %.4f\t@ %.4f/%.4f\t\
soft %.4f\thard %.4f\tctr_soft %.4f\t%.6f v %.6f\n",
			pos->fut.hdr.sym,
			pos->fut.pos.hard,
			pos->fut.pos.soft,
			pos->fut.mult,
			pos->fut.f_mkt.bid,
			pos->fut.f_mkt.ask,
			pos->fut.term.soft,
			pos->fut.term.hard,
			pos->fut.reba_soft,
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
		fprint_pos(pf->poss + i, nav, whither);
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
			if (d > 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			} else if (d < 0.0) {
				fprintf(whither, "CLEAR\t%.4f\t%s\n",
					d, p->cash.hdr.sym);
			}
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

static void
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
		if (p->ty != POSTY_FUT || p->fut.ccy->cod != ccy->cod) {
			continue;
		}
		sum += p->fut.reba_soft;
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
		if (p->ty != POSTY_FUT || p->fut.ccy->cod != ccy->cod) {
			continue;
		}
		sum += p->fut.term.hard;
	}
	return sum;
}

static void
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
	/* start out by setting all CCY future position factors to 1.0 */
	set_base_ccy_fut(pf, ccy, 1.0);

	/* find the currency in question, otherwise create a position */
	for (size_t i = 0; i < pf->nposs; i++) {
		pos_t p = pf->poss + i;
		if (p->ty == POSTY_CASH && p->cash.tccy != ccy) {
			set_base_ccy_fut(pf, p->cash.tccy, p->cash.s_mkt.stl);
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
		}
	}
	return;
}

static posty_t
__parse_posty(const char *s)
{
	static const char c[] = "CASH";
	static const char f[] = "FUT";
	static const char fx[] = "FX";
	if (strncmp(s, c, sizeof(c) - 1) == 0) {
		return POSTY_CASH;
	} else if (strncmp(s, f, sizeof(f) - 1) == 0) {
		return POSTY_FUT;
	} else if (strncmp(s, fx, sizeof(fx) - 1) == 0) {
		return POSTY_FX;
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

	/* frob pos */
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
	cp->tccy = __find_4217(line);

	/* frob soft */
	line = p;
	cp->soft_ini = cp->term.soft = read_tab_double(p = line);

	/* frob hard */
	line = __skip_behind_tab(p);
	cp->hard_ini = cp->term.hard = read_tab_double(p = line);

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

static pf_t
read_pf(FILE *whence)
{
	pf_t res = NULL;
	char *line = NULL;
	size_t len;
	ssize_t nrd;
	size_t csz = 0;

	/* read line by line */
	if (whence == NULL) {
		return NULL;
	}

	res = calloc(1, sizeof(*res) + 4 * sizeof(*res->poss));

	while ((nrd = getline(&line, &len, whence)) != -1) {
		posty_t pty = __parse_posty(line);
		pos_t p = res->poss + res->nposs;

		switch (pty) {
		case POSTY_CASH:
			if (__parse_cash(&p->cash, line) == 0) {
				p->ty = pty;
				res->nposs++;
			}
			break;
		case POSTY_FUT:
			if (__parse_fut(&p->fut, line) == 0) {
				p->ty = pty;
				res->nposs++;
			}
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
int
main(int argc, char *argv[])
{
	pf_t inpf;
	int no_reba = 0;

	/* parse command line and shite, preliminary */
	if (argv[1] && strcmp(argv[1], "-n") == 0) {
		no_reba = 1;
	}

	inpf = read_pf(stdin);
	/* establish base currency */
	set_base_currency(inpf, PFACK_4217_USD);

	if (!data_complete_p(inpf)) {
		/* just refuse to do stuff*/
		fprintf(stderr, "DATA INCOMPLETE ... CUNT OFF\n");
	} else if (no_reba) {
		fprint_poss(inpf, stdout);
	} else {
		URS_DEBUG("rebalancing ...\n");
		reba_relanav(inpf);
		/* reconciliation, could be a CLI option */
		reco_poss(inpf);
		fprint_poss(inpf, stderr);

		/* print a list of trades so we can settle this crap */
		fprint_trades(inpf, stdout);
		/* for the moment we need info too */
		fprint_info(inpf, stdout);
	}

	free_pf(inpf);
	return 0;
}

/* durst.c ends here */
