TESTS = test_settings
check_PROGRAMS = test_settings
test_settings_SOURCES = test_settings.c
test_settings_CFLAGS = @CHECK_CFLAGS@ @MOTIFGPT_CPPFLAGS@ -DUNIT_TESTING
test_settings_LDADD = @CHECK_LIBS@ @MOTIFGPT_LIBS@

noinst_LIBRARIES = libdisasterparty.a
libdisasterparty_a_SOURCES = disasterparty_mock.c
libdisasterparty_a_CFLAGS = @DISASTERPARTY_CFLAGS@
