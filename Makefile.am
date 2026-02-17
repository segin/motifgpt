bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c config_utils.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = tests/test_config
check_PROGRAMS = tests/test_config

tests_test_config_SOURCES = tests/test_config.c config_utils.c
tests_test_config_CPPFLAGS = -I$(top_srcdir)