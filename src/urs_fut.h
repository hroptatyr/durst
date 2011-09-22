#if !defined INCLUDED_urs_fut_h_
#define INCLUDED_urs_fut_h_

#include "urs.h"
#include "iso4217.h"

typedef struct __fut_pos_s *urs_fut_pos_t;

struct __fut_pos_s {
	struct __hdr_s hdr;
	/* position in our portfolio */
	struct __val_s pos;
	/* future and spot market info, spot market is used to determine
	 * the present value of a future */
	struct __mkt_s f_mkt;
	struct __mkt_s s_mkt;

	double mult;

	/* characteristics */
	struct __val_s term;
	struct __val_s base;

	/* parameters */
	/* bid is the lower bound, ask the upper bound and stl the target */
	struct __wei_s band;
	double fee;
	double val_fac;

	/* rebalancing result, error cash is now booked in
	 * term.hard and base.hard */
	const_pfack_4217_t ccy;
	double reba_soft;
};

DECLF double urs_fut_value(urs_fut_pos_t fp);
DECLF void urs_fut_relanav(urs_fut_pos_t fp, const double nav);

#endif	/* INCLUDED_urs_fut_h_ */
