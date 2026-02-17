bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c utils.c utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_utils
test_utils_SOURCES = tests/test_utils.c utils.c utils.h
test_utils_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
test_utils_LDADD = @MOTIFGPT_LIBS@
TESTS = test_utils
