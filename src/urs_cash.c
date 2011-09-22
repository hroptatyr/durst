#include <stdio.h>
#include <math.h>
#include "urs.h"
#include "urs_cash.h"

#if defined DEBUG_FLAG
# define URS_DEBUG(args...)	fprintf(stderr, "[urs] " args);
#else  /* !DEBUG_FLAG */
# define URS_DEBUG(args...)
#endif	/* DEBUG_FLAG */
#if !defined UNUSED
# define UNUSED(x)	__attribute__((unused)) x
#endif	/* !UNUSED */
#define FEE_AWARE	1

static double
term_to_base(urs_cash_pos_t cp, double amt)
{
	return amt / cp->s_mkt.ask;
}

static double
term_in_base(urs_cash_pos_t cp, double amt)
{
	return amt / cp->s_mkt.stl;
}

static double __attribute__((unused))
base_to_term(urs_cash_pos_t cp, double amt)
{
	return amt * cp->s_mkt.bid;
}

DEFUN double
urs_cash_value(urs_cash_pos_t cp)
{
	const double tamt = cp->term.soft + cp->term.hard;
	const double fxamt = cp->forex;
	return term_in_base(cp, tamt + fxamt);
}

static double
cash_cost(urs_cash_pos_t cp, double amt)
{
/* compute the cost to exchange AMT currency units to base */
	double res;

	/* fees */
	res = fabs(amt) * cp->soft_fee;

	if (res < cp->hard_fee) {
		return cp->hard_fee;
	}
	return res;
}

DEFUN void
urs_cash_relanav(urs_cash_pos_t cp, const double nav)
{
/* nav is given in terms, convert to base and rebalance to meet the band */
	double tgt;
	double tamt;
	double dv_b, dv_t;
	double cost;
	double err;

	if (cp->s_mkt.stl <= 0.0) {
		return;
	} else if (cp->band.med < 0.0) {
		return;
	}

	/* start the actual rebalancing */
	tgt = cp->band.med * nav;
	tamt = cp->term.hard + cp->term.soft + cp->forex;
	URS_DEBUG("%s reba %.6f  %.6f\n", cp->tccy->sym, nav, tamt);
	dv_t = tgt - tamt;
	dv_b = term_to_base(cp, dv_t);
	cost = cash_cost(cp, dv_t);
	err = term_in_base(cp, tgt) / (term_in_base(cp, nav) - cost);

	URS_DEBUG("-> %.6f %.6f v %.6f\n", tgt, err, cp->band.med);
	if (err > cp->band.lo && err < cp->band.hi || 1) {
		cp->forex += dv_t;
		cp->bp->forex -= dv_b + cost;
	}
	return;
}

DEFUN double
urs_cash_setl(urs_cash_pos_t UNUSED(cp))
{
	return 0.0;
}

/* urs_cash.c ends here */
