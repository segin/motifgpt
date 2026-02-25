bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c buffer_utils.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = tests/test_buffer_utils tests/test_base64
TESTS = $(check_PROGRAMS)

tests_test_buffer_utils_SOURCES = tests/test_buffer_utils.c buffer_utils.c
tests_test_buffer_utils_CPPFLAGS = -I$(srcdir)
tests_test_buffer_utils_LDADD =

tests_test_base64_SOURCES = tests/test_base64.c buffer_utils.c
tests_test_base64_CPPFLAGS = -I$(srcdir)
tests_test_base64_LDADD =
