bin_PROGRAMS = bhnbot
bhnbot_SOURCES = src/bot.c src/bot_conf.c src/bot_cmd_echo.c src/bot_cmd_echo.h src/bot_cmd_http.c src/bot_cmd_http.h src/bot_cmd_imgur.c src/bot_cmd_imgur.h
bhnbot_CFLAGS = -Wall -Wextra -Werror

test:
	valgrind --track-origins=yes --leak-check=full --show-reachable=yes ./bhnbot |& less

dist-clean:
	rm -f bot
	rm -f bhnbot
	rm -f *.o
	rm -rf .deps/
	rm -f Makefile
	rm -f Makefile.in
	rm -f aclocal.m4
	rm -rf autom4te.cache/
	rm -f compile
	rm -f config.h
	rm -f config.h.in
	rm -f config.log
	rm -f config.status
	rm -f configure
	rm -f depcomp
	rm -f install-sh
	rm -f missing
	rm -f stamp-h1
	rm -f *~
