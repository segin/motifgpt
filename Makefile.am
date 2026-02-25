bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c config.c config.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = test_config
check_PROGRAMS = test_config
test_config_SOURCES = tests/test_config.c config.c config.h
test_config_CPPFLAGS = -I$(srcdir) -I$(srcdir)/tests
