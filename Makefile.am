bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c history.c history.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_history
test_history_SOURCES = tests/test_history.c history.c tests/mock_disasterparty.c tests/disasterparty.h history.h
test_history_CPPFLAGS = -I$(srcdir)/tests -I$(srcdir)
TESTS = test_history
