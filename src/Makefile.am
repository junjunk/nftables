sbin_PROGRAMS = nft

CLEANFILES = scanner.c parser_bison.c

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CPPFLAGS += -DDEFAULT_INCLUDE_PATH="\"${sysconfdir}\"" \
		${LIBMNL_CFLAGS} ${LIBNFTNL_CFLAGS}
if BUILD_DEBUG
AM_CPPFLAGS += -g -DDEBUG
endif
if BUILD_XTABLES
AM_CPPFLAGS += ${XTABLES_CFLAGS}
endif

AM_CFLAGS = -Wall								\
	    -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations	\
	    -Wdeclaration-after-statement -Wsign-compare -Winit-self		\
	    -Wformat-nonliteral -Wformat-security -Wmissing-format-attribute	\
	    -Wcast-align -Wundef -Wbad-function-cast				\
	    -Waggregate-return -Wunused -Wwrite-strings


AM_YFLAGS = -d

BUILT_SOURCES = parser_bison.h

lib_LTLIBRARIES = libnftables.la

libnftables_la_SOURCES =			\
		rule.c				\
		statement.c			\
		datatype.c			\
		expression.c			\
		evaluate.c			\
		proto.c				\
		payload.c			\
		exthdr.c			\
		fib.c				\
		hash.c				\
		meta.c				\
		rt.c				\
		numgen.c			\
		ct.c				\
		netlink.c			\
		netlink_linearize.c		\
		netlink_delinearize.c		\
		monitor.c			\
		segtree.c			\
		rbtree.c			\
		gmputil.c			\
		utils.c				\
		erec.c				\
		mnl.c				\
		iface.c				\
		services.c			\
		mergesort.c			\
		tcpopt.c			\
		socket.c			\
		libnftables.c

# yacc and lex generate dirty code
noinst_LTLIBRARIES = libparser.la
libparser_la_SOURCES = parser_bison.y scanner.l
libparser_la_CFLAGS = ${AM_CFLAGS} \
		      -Wno-missing-prototypes \
		      -Wno-missing-declarations \
		      -Wno-implicit-function-declaration \
		      -Wno-nested-externs \
		      -Wno-undef \
		      -Wno-redundant-decls

libnftables_la_LIBADD = ${LIBMNL_LIBS} ${LIBNFTNL_LIBS} libparser.la

if BUILD_MINIGMP
noinst_LTLIBRARIES += libminigmp.la
libminigmp_la_SOURCES = mini-gmp.c
libminigmp_la_CFLAGS = ${AM_CFLAGS} -Wno-sign-compare
libnftables_la_LIBADD += libminigmp.la
endif

if BUILD_XTABLES
libnftables_la_SOURCES += xt.c
libnftables_la_LIBADD += ${XTABLES_LIBS}
endif

nft_SOURCES = main.c

if BUILD_CLI
nft_SOURCES += cli.c
endif

if BUILD_JSON
libnftables_la_SOURCES += json.c parser_json.c
libnftables_la_LIBADD += ${JANSSON_LIBS}
endif

nft_LDADD = libnftables.la
