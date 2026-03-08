ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = motifgpt
motifgpt_SOURCES = motifgpt.c utils.c motifgpt_config.c motifgpt_history.c motifgpt_chat.c buffer_utils.c

motifgpt_CPPFLAGS = @MOTIFGPT_CPPFLAGS@
motifgpt_LDADD = @MOTIFGPT_LIBS@

all-local: plugins/plugin_filereader.so plugins/plugin_weather.so plugins/plugin_stock.so

plugins/plugin_filereader.so: plugins/plugin_filereader.c motifgpt_plugin.h
	mkdir -p plugins
	$(CC) $(CFLAGS) -fPIC -shared -o $@ plugins/plugin_filereader.c @LIBCJSON_LIBS@ -I.

plugins/plugin_weather.so: plugins/plugin_weather.c motifgpt_plugin.h
	mkdir -p plugins
	$(CC) $(CFLAGS) -fPIC -shared -o $@ plugins/plugin_weather.c @LIBCJSON_LIBS@ -I.

plugins/plugin_stock.so: plugins/plugin_stock.c motifgpt_plugin.h
	mkdir -p plugins
	$(CC) $(CFLAGS) -fPIC -shared -o $@ plugins/plugin_stock.c @LIBCJSON_LIBS@ -I.

clean-local:
	rm -f plugins/*.so

check_PROGRAMS = test_utils test_config test_history test_stream_handler test_buffer_utils
test_utils_SOURCES = tests/test_utils.c utils.c
test_utils_CPPFLAGS = -I$(top_srcdir)

test_config_SOURCES = tests/test_config.c motifgpt_config.c
test_config_CPPFLAGS = -I$(top_srcdir)

test_history_SOURCES = tests/test_history.c motifgpt_history.c
test_history_CPPFLAGS = -I$(top_srcdir)

test_stream_handler_SOURCES = tests/test_stream_handler.c motifgpt_chat.c
test_stream_handler_CPPFLAGS = -I$(top_srcdir)

test_buffer_utils_SOURCES = tests/test_buffer_utils.c buffer_utils.c
test_buffer_utils_CPPFLAGS = -I$(top_srcdir)

TESTS = test_utils test_config test_history test_stream_handler test_buffer_utils
