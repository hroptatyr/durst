#if !defined INCLUDED_urs_h_
#define INCLUDED_urs_h_

struct __hdr_s {
	char *sym;
};

struct __val_s {
	/* softs and hards in native currency */
	double soft;
	double hard;
};

struct __mkt_s {
	double stl;
	double bid;
	double ask;
};

struct __wei_s {
	double lo;
	double med;
	double hi;
};

#define DECLF	extern
#define DEFUN

#endif	/* INCLUDED_urs_h_ */
