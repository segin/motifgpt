bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c chat_logic.c chat_logic.h

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

check_PROGRAMS = test_stream_handler
test_stream_handler_SOURCES = tests/test_stream_handler.c chat_logic.c chat_logic.h
test_stream_handler_CPPFLAGS = -I$(top_srcdir) @MOTIFGPT_CPPFLAGS@
test_stream_handler_LDADD = @MOTIFGPT_LIBS@
TESTS = test_stream_handler