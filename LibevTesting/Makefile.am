AUTOMAKE_OPTIONS = foreign

VERSION_INFO = 4:0:0

EXTRA_DIST = LICENSE Changes libev.m4 autogen.sh \
	     ev_vars.h ev_wrap.h \
	     ev_epoll.c ev_select.c ev_poll.c ev_kqueue.c ev_port.c ev_win32.c

include_HEADERS = ev.h

noinst_LTLIBRARIES = libev.la

libev_la_SOURCES = ev.c
libev_la_LDFLAGS = -version-info $(VERSION_INFO)


noinst_PROGRAMS = LibevExample

LibevExample_SOURCES = libevtesting.c
LibevExample_LDFLAGS = libev.la
LibevExample_CPPFLAGS = -I.
