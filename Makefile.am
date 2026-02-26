bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c utils.c motifgpt_config.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_utils test_config
test_utils_SOURCES = tests/test_utils.c utils.c
test_utils_CPPFLAGS = -I$(top_srcdir)

test_config_SOURCES = tests/test_config.c motifgpt_config.c
test_config_CPPFLAGS = -I$(top_srcdir)

TESTS = test_utils test_config
