bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c utils.c utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = test_base64
check_PROGRAMS = test_base64
test_base64_SOURCES = tests/test_base64.c utils.c
test_base64_CPPFLAGS = -I.
