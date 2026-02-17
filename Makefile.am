bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = test_save_settings
check_PROGRAMS = test_save_settings

test_save_settings_SOURCES = tests/test_save_settings.c tests/mock_disasterparty.c motifgpt.c
test_save_settings_CPPFLAGS = @MOTIFGPT_CPPFLAGS@ -DTEST -I$(top_srcdir)/tests
test_save_settings_LDADD = @MOTIFGPT_LIBS@
