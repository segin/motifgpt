bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c buffer_utils.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_base64
test_base64_SOURCES = tests/test_base64.c buffer_utils.c
test_base64_CFLAGS = -I$(top_srcdir)

TESTS = $(check_PROGRAMS)