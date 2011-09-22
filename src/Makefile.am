### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

bin_PROGRAMS += durst
durst_SOURCES = durst.c
durst_SOURCES += urs.h
durst_SOURCES += urs_fut.c urs_fut.h
durst_SOURCES += urs_cash.c urs_cash.h
durst_CPPFLAGS = $(AM_CPPFLAGS)
durst_LDADD = -lm
BUILT_SOURCES += durst-clo.c durst-clo.h
EXTRA_durst_SOURCES = iso4217.c iso4217.h

## ggo rule
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

## ggo rule
%.c: %.ggo
	gengetopt -l -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
