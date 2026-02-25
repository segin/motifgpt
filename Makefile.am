bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c history.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_history
test_history_SOURCES = tests/test_history_truncation.c history.c
test_history_CPPFLAGS = -I$(srcdir) @MOTIFGPT_CPPFLAGS@
TESTS = test_history
