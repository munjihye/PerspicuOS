# $FreeBSD: release/9.0.0/usr.sbin/ntp/libparse/Makefile 182008 2008-08-22 16:00:48Z roberto $

.PATH: ${.CURDIR}/../../../contrib/ntp/libparse

LIB= parse
INTERNALLIB=

SRCS=	clk_computime.c	clk_dcf7000.c	clk_hopf6021.c	clk_meinberg.c \
	clk_rawdcf.c	clk_rcc8000.c	clk_schmid.c	clk_trimtaip.c \
	clk_trimtsip.c	clk_varitext.c	clk_wharton.c	data_mbg.c \
	info_trimble.c	parse.c		parse_conf.c	trim_info.c \
	binio.c gpstolfp.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

.include <bsd.lib.mk>
