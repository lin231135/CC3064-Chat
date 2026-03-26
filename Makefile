CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c11 -O2 -pthread

SERVER_SRC=server/server.c server/session.c server/client_manager.c common/net_utils.c
CLIENT_SRC=client/client.c client/receiver.c client/command_parser.c common/net_utils.c

all: servidor cliente

servidor: $(SERVER_SRC) protocolo.h
	$(CC) $(CFLAGS) -o servidor $(SERVER_SRC)

cliente: $(CLIENT_SRC) protocolo.h
	$(CC) $(CFLAGS) -o cliente $(CLIENT_SRC)

clean:
	rm -f servidor cliente

.PHONY: all clean
