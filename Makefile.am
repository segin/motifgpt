bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c config_utils.c config_utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

AM_CPPFLAGS = -I$(srcdir)

check_PROGRAMS = test_config_utils
test_config_utils_SOURCES = tests/test_config_utils.c config_utils.c
TESTS = test_config_utils