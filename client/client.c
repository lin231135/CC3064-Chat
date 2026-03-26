#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/net_utils.h"
#include "../protocolo.h"
#include "command_parser.h"
#include "receiver.h"

/* Abre y conecta el socket TCP del cliente hacia el servidor. */
static int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "IP invalida\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* Construye un paquete de salida y calcula payload_len automaticamente. */
static void build_packet(ChatPacket *pkt, uint8_t cmd, const char *sender, const char *target, const char *payload) {
    memset(pkt, 0, sizeof(ChatPacket));
    pkt->command = cmd;

    if (sender != NULL) {
        strncpy(pkt->sender, sender, sizeof(pkt->sender) - 1);
    }
    if (target != NULL) {
        strncpy(pkt->target, target, sizeof(pkt->target) - 1);
    }
    if (payload != NULL) {
        strncpy(pkt->payload, payload, sizeof(pkt->payload) - 1);
    }

    pkt->payload_len = (uint16_t)strlen(pkt->payload);
}

/* Muestra ayuda de comandos soportados por la CLI actual. */
static void print_help(void) {
    printf("Comandos disponibles:\n");
    printf("  /broadcast <mensaje>\n");
    printf("  /msg <usuario> <mensaje>\n");
    printf("  /status <ACTIVE|BUSY|INACTIVE>\n");
    printf("  /list\n");
    printf("  /info <usuario>\n");
    printf("  /help\n");
    printf("  /exit\n");
}

/*
 * Flujo principal del cliente:
 * 1) conecta y registra usuario
 * 2) levanta thread receptor
 * 3) parsea comandos de stdin y los envia al servidor
 */
int main(int argc, char **argv) {
    int sockfd;
    int port;
    pthread_t rx_tid;
    char line[1200];

    if (argc != 4) {
        fprintf(stderr, "Uso: ./cliente <username> <IP_servidor> <puerto>\n");
        return 1;
    }

    port = atoi(argv[3]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto invalido\n");
        return 1;
    }

    sockfd = connect_to_server(argv[2], port);
    if (sockfd < 0) {
        return 1;
    }

    {
        /* Registro obligatorio antes de operar otros comandos. */
        ChatPacket reg;
        build_packet(&reg, CMD_REGISTER, argv[1], "", argv[1]);
        if (send_packet(sockfd, &reg) != 0) {
            fprintf(stderr, "No se pudo enviar registro\n");
            close(sockfd);
            return 1;
        }
    }

    if (pthread_create(&rx_tid, NULL, receiver_thread, &sockfd) != 0) {
        perror("pthread_create receiver");
        close(sockfd);
        return 1;
    }

    print_help();
    printf("> ");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        ParsedCommand cmd;
        ChatPacket pkt;

        if (parse_input_line(line, &cmd) != 0) {
            printf("Comando invalido. Usa /help\n> ");
            continue;
        }

        if (cmd.type == PARSE_HELP) {
            print_help();
            printf("> ");
            continue;
        }

        if (cmd.type == PARSE_EXIT) {
            /* Solicita cierre controlado de sesion. */
            build_packet(&pkt, CMD_LOGOUT, argv[1], "", "");
            send_packet(sockfd, &pkt);
            break;
        }

        if (cmd.type == PARSE_BROADCAST) {
            build_packet(&pkt, CMD_BROADCAST, argv[1], "", cmd.arg2);
            send_packet(sockfd, &pkt);
            printf("> ");
            continue;
        }

        if (cmd.type == PARSE_DIRECT) {
            build_packet(&pkt, CMD_DIRECT, argv[1], cmd.arg1, cmd.arg2);
            send_packet(sockfd, &pkt);
            printf("> ");
            continue;
        }

        if (cmd.type == PARSE_STATUS) {
            build_packet(&pkt, CMD_STATUS, argv[1], "", cmd.arg1);
            send_packet(sockfd, &pkt);
            printf("> ");
            continue;
        }

        if (cmd.type == PARSE_LIST) {
            build_packet(&pkt, CMD_LIST, argv[1], "", "");
            send_packet(sockfd, &pkt);
            printf("> ");
            continue;
        }

        if (cmd.type == PARSE_INFO) {
            build_packet(&pkt, CMD_INFO, argv[1], cmd.arg1, "");
            send_packet(sockfd, &pkt);
            printf("> ");
            continue;
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(rx_tid, NULL);

    return 0;
}
