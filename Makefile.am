bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c config_store.c config_store.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_config_store
test_config_store_SOURCES = tests/test_config_store.c config_store.c config_store.h
test_config_store_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
test_config_store_LDADD = @MOTIFGPT_LIBS@

TESTS = test_config_store
