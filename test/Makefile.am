# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

TESTS =
TESTS += cash-reba.durst
TESTS += fut-reba.durst
TESTS += futcash-reba.durst
TESTS += fut-no-ccy.durst
TESTS += miss-quo.durst

TEST_EXTENSIONS = .durst .truffle
DURST_LOG_COMPILER = $(top_builddir)/src/durst
AM_DURST_LOG_FLAGS = <
TRUFFLE_LOG_COMPILER = $(top_builddir)/src/truffle
AM_TRUFFLE_LOG_FLAGS = <
LOG_COMPILER = echo

