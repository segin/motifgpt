bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c buffer_utils.c buffer_utils.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_buffer_utils
test_buffer_utils_SOURCES = tests/test_buffer_utils.c buffer_utils.c buffer_utils.h
test_buffer_utils_CPPFLAGS =
test_buffer_utils_LDADD =

TESTS = test_buffer_utils
