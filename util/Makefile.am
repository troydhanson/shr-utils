srcdir = @srcdir@

bin_PROGRAMS = shr-socket

shr_socket_SOURCES = shr-socket.c
shr_socket_CPPFLAGS =
shr_socket_LDADD = -lshr

if HAVE_NANOMSG
bin_PROGRAMS += shr-npub shr-nsub
shr_npub_LDADD = -lshr -lnanomsg 
shr_nsub_LDADD = -lshr -lnanomsg 
endif

#
noinst_PROGRAMS = shr-read-template
shr_read_template_SOURCES = shr-read-template.c
shr_read_template_CPPFLAGS = 
shr_read_template_LDADD = -lshr

bin_PROGRAMS += shr-pub 
shr_pub_SOURCES = shr-pub.c
shr_pub_CPPFLAGS = 
shr_pub_LDADD = -lshr
