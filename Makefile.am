bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c buffer_utils.c buffer_utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_base64
TESTS = test_base64

test_base64_SOURCES = tests/test_base64.c buffer_utils.c buffer_utils.h
test_base64_CPPFLAGS = -I$(srcdir)
test_base64_LDADD =