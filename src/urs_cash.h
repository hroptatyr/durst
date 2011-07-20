#if !defined INCLUDED_urs_cash_h_
#define INCLUDED_urs_cash_h_

#include "urs.h"
#include "iso4217.h"

typedef struct __cash_pos_s *urs_cash_pos_t;

struct __cash_pos_s {
	/* ident stuff */
	struct __hdr_s hdr;

	/* this currency */
	const_pfack_4217_t tccy;

	/* bid and ask to base ccy, spot */
	struct __mkt_s s_mkt;
	/* >0 if s_mkt is quoted in this currency,
	 * <0 if this s_mkt's base is this currency,
	 * 0 otherwise */
	int dir;

	/* characteristics, track soft and hard positions */
	struct __val_s term;
	struct __val_s base;
	double soft_fee;
	double hard_fee;

	double soft_ini;
	double hard_ini;

	/* keep track of navs */
	double term_nav;

	/* band, if regarded as asset, use -1 if not */
	struct __wei_s band;

	/* pointer to the base currency, for bookings */
	urs_cash_pos_t bp;
};

/* compute cash values (in base currency units) */
DECLF double urs_cash_value(urs_cash_pos_t fp);
/* rebalance cash positions */
DECLF void urs_cash_relanav(urs_cash_pos_t cp, const double nav);

/* in terms */
DECLF double urs_cash_setl(urs_cash_pos_t fp);

#endif	/* INCLUDED_urs_cash_h_ */
