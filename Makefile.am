bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c stream_handler.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

TESTS = test_stream_handler
check_PROGRAMS = test_stream_handler
test_stream_handler_SOURCES = tests/test_stream_handler.c stream_handler.c
test_stream_handler_CPPFLAGS = -I.
