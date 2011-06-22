#include <stdio.h>
#include <math.h>
#include "urs.h"
#include "urs_fut.h"

#if defined DEBUG_FLAG
# define URS_DEBUG(args...)	fprintf(stderr, "[urs] " args);
#else  /* !DEBUG_FLAG */
# define URS_DEBUG(args...)
#endif	/* DEBUG_FLAG */

#define FEE_AWARE	1
#define ROLAND_EXP	1

/* We employ newton's method, as follows:
 * We assume last day's rebalancing was successful and the following holds
 *   (F0^i - S^i) * p^i * m = D0^i  (= beta_i N0)
 * where F0 is yesterday's future price, S is the (expected) spot price at
 * maturity, p is the contact position in our portfolio and
 * D0 is (yday's) value (PV) of the future position.
 * N0 is \sum_i D0^i
 * m is the multiplier
 *
 * Now today's situation is different, assume the future price has changed
 * by dp, everything reads now
 *   (F0^i + dp^i - S^i) * p^i * m = D0^i + dp^i * p^i * m
 *   (Fn^i - S^i) * p^i * m = Dn^i
 *
 * Summing up all these new position values yields
 *   Nn := \sum_i Dn^i
 * and without rebalancing this shifts the weights to
 *   (Fn^i - S^i) * p^i * m = (beta_i + eps_i) * Nn
 *
 * Now buying another v contracts gets us closer to a weight of beta
 *   (Fn^i - S^i) * (p^i + v^i) * m = beta_i * Nn - |v^i| * f
 * where |v^i| * f is the total due for this transaction.
 * Juggling the both together gives:
 *   (beta_i + eps_i) * Nn + (Fn^i - S^i) * v^i * m + |v^i| * f = beta_i * Nn
 * and hence
 *   (Fn^i - S^i) * v^i * m + |v^i| * f = -eps_i * Nn
 * using the identity eps_i = Dn^i / Nn - beta_i  we yield
 *   (Fn^i - S^i) * v^i * m + |v^i| * f = beta_i * Nn - Dn^i
 *
 * Fee-aware rebalancing would instead give us:
 *   (Fn^i - S^i) * (p^i + v^i) * m = beta_i * (Nn - |v^i| * f)
 * whose fee has to be split amongst the rebalanced positions.
 * Juggling the both together gives:
 *   (beta_i + eps_i) Nn + (Fn^i - S^i) v^i m + beta_i |v^i| f = beta_i Nn
 * and hence
 *   (Fn^i - S^i) v^i m + beta_i |v^i| f = -eps_i Nn
 * using the identity eps_i = Dn^i / Nn - beta_i  we yield
 *   (Fn^i - S^i) v^i m + beta_i |v^i| f = beta_i Nn - Dn^i
 *
 * And here we can start Newton or Newton-Raphson since ...
 * well, the other way around, we can't rebalance futures whose quotes
 * are closely similar to the spot prices, as the derivative might vanish */

static double
__asgn(double x, double v)
{
	if (x == 0.0) {
		return 0.0;
	} else if (x > 0.0) {
		return v;
	} else if (x < 0.0) {
		return -v;
	} else {
		return NAN;
	}
}

static double
fut_value_fun(urs_fut_pos_t fp, double contracts)
{
	return (fp->f_mkt.stl - fp->s_mkt.stl) * contracts * fp->mult;
}

static double
fut_value(urs_fut_pos_t fp)
{
	return fut_value_fun(fp, fp->pos.soft + fp->pos.hard);
}

static double
fut_deriv(urs_fut_pos_t fp, double dpos)
{
	double onev = fut_value_fun(fp, 1);
#if defined FEE_AWARE
	double beta = fp->band.med;
	return onev + __asgn(dpos, beta * fp->fee);
#else  /* !FEE_AWARE */
	return onev + __asgn(dpos, fp->fee);
#endif	/* FEE_AWARE */
}

static double
fut_weight(urs_fut_pos_t fp, double dpos, double nav)
{
	double beta = fp->band.med;
	double npv = fut_value_fun(fp, fp->pos.hard);
	double dpv = fut_value_fun(fp, dpos);
#if defined FEE_AWARE
	return dpv + beta * fabs(dpos) * fp->fee - beta * nav + npv;
#else  /* !FEE_AWARE */
	return dpv + fabs(dpos) * fp->fee - beta * nav + npv;
#endif	/* FEE_AWARE */
}

#if !defined ROLAND_EXP
static double
fut_newt_step(urs_fut_pos_t fp, double dpos, double nav)
{
	double fpv = fut_weight(fp, dpos, nav);
	double fpdv = fut_deriv(fp, dpos);
	return dpos - fpv / fpdv;
}
#else
static double
fut_newt_step(urs_fut_pos_t fp, double dpos, double nav)
{
	return fp->band.med * nav;
}
#endif	/* !ROLAND_EXP */

static double
fut_round(urs_fut_pos_t fp)
{
	double sr = round(fp->pos.soft);
	return sr;
}

struct __gross_cost_s {
	double fee;
	double spread;
};

static struct __gross_cost_s
fut_cost(urs_fut_pos_t fp)
{
	struct __gross_cost_s res;
	double spr;

	/* fees */
	res.fee = fabs(fp->pos.soft) * fp->fee;

	if (fp->pos.soft > 0.0) {
		spr = (fp->f_mkt.ask - fp->f_mkt.stl);
	} else {
		spr = (fp->f_mkt.bid - fp->f_mkt.stl);
	}
	res.spread = spr * fp->pos.soft * fp->mult;
	return res;
}

DEFUN void
urs_fut_relanav(urs_fut_pos_t fp, const double nav)
{
	const double tgt = fp->band.med * nav;

	if (fp->f_mkt.stl == 0.0) {
		return;
	}

	for (double dpos = 0.0, dpr = -1.0, opr = 0.0; dpr != opr;) {
		double v, nv;
		double err = 0.0;
		struct __gross_cost_s cost;

		dpos = fp->pos.soft = fut_newt_step(fp, dpos, nav + err);
		/* value in base currency */
		v = fut_value(fp) / fp->val_fac;
		/* round the whole shebang and compute trades */
		opr = dpr;
		dpr = fp->pos.soft = fut_round(fp);
		nv = (fp->term.soft = fut_value(fp)) / fp->val_fac;
		cost = fut_cost(fp);

		err = tgt - (nv + (cost.fee + cost.spread) * fp->val_fac);
		fp->term.hard = -cost.fee;
		URS_DEBUG("%.4f %.4f  r %.4f %.4f  f %.4f s %.4f  e %.4f\n",
			  dpos, v, dpr, nv, cost.fee, cost.spread, err);

		/* leave a note about total soft cash for this transaction */
		fp->reba_soft = -(fut_value_fun(fp, dpr) + cost.spread);
#if defined ROLAND_EXP
		break;
#endif	/* ROLAND_EXP */
	}
	return;
}

DEFUN double
urs_fut_value(urs_fut_pos_t fp)
{
	return fut_value(fp);
}

#if defined TEST
static struct __fut_pos_s GI = {
	.hdr = {.sym = "GI___CCS"},

	.pos = {
		 .hard = 400.0,
		 .soft = 0.0,
	 },

	.f_mkt = {
		 .stl = 190.90,
		 .bid = 190.80,
		 .ask = 191.20,
	 },

	.s_mkt = {
		 .stl = 0.0,
		 .bid = 0.0,
		 .ask = 0.0,
	 },

	.band = {
		 .lo = 0.48,
		 .med = 0.50,
		 .hi = 0.52,
	 },
	.fee = 1.80,
};

int
main(int argc, char *argv[])
{
	urs_fut_relanav(&GI, 81000.0);
	return 0;
}
#endif	/* TEST */

/* urs_fut.c ends here */
