bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c config_utils.c config_utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = tests/test_config
tests_test_config_SOURCES = tests/test_config.c config_utils.c config_utils.h
tests_test_config_CPPFLAGS = -I$(srcdir)
TESTS = tests/test_config
