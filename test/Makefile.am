# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(DT_LOG_COMPILER)
TESTS =

TEST_EXTENSIONS = .dt
DT_LOG_COMPILER = $(srcdir)/dt-test.sh
AM_DT_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum
LOG_COMPILER = echo

TESTS =
TESTS += cash-reba.dt
EXTRA_DIST += cash-reba.dt cash-reba.durst

TESTS += fut-reba.dt
EXTRA_DIST += fut-reba.dt fut-reba.durst

TESTS += futcash-reba.dt
EXTRA_DIST += futcash-reba.dt futcash-reba.durst

TESTS += fut-no-ccy.dt
EXTRA_DIST += fut-no-ccy.dt fut-no-ccy.durst

TESTS += miss-quo.1.dt
EXTRA_DIST += miss-quo.1.dt miss-quo.1.durst

TESTS += miss-quo.2.dt
EXTRA_DIST += miss-quo.2.dt miss-quo.2.durst
