CFLAGS=-Wall -g -O0 -lcurl -Wl,-Bsymbolic-functions

all: bot

bot: bot_cmd_http.o

clean:
	rm -f *.o
	rm -f bot
