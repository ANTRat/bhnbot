CFLAGS=-Wall -g -O0 -lcurl -Wl,-Bsymbolic-functions

all: bot

bot: botcmd_youtube.o

clean:
	rm -f *.o
	rm -f bot
