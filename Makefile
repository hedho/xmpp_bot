CC = gcc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags libstrophe)
LDFLAGS = $(shell pkg-config --libs libstrophe)

xmpp_bot: xmpp_bot.c
	$(CC) $(CFLAGS) -o xmpp_bot xmpp_bot.c $(LDFLAGS)

clean:
	rm -f xmpp_bot
