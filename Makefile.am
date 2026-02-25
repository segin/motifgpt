bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c buffer_utils.c buffer_utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = tests/test_base64
check_PROGRAMS = tests/test_base64
tests_test_base64_SOURCES = tests/test_base64.c buffer_utils.c buffer_utils.h
tests_test_base64_CPPFLAGS = -I$(srcdir)
tests_test_base64_LDADD =
