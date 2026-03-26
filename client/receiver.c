#include "receiver.h"

#include <stdio.h>
#include <string.h>

#include "../common/net_utils.h"

/*
 * Consume respuestas/eventos del servidor en paralelo al hilo principal.
 * Esto evita bloquear la entrada del usuario mientras llegan mensajes.
 */
void *receiver_thread(void *arg) {
    int sockfd = *(int *)arg;

    while (1) {
        ChatPacket pkt;

        if (recv_packet(sockfd, &pkt) != 0) {
            printf("\n[conexion cerrada por el servidor]\n");
            break;
        }

        /* Formatea salida segun tipo de comando recibido. */
        if (pkt.command == CMD_MSG) {
            printf("\n[%s -> %s] %s\n", pkt.sender, pkt.target[0] ? pkt.target : "ALL", pkt.payload);
        } else if (pkt.command == CMD_OK) {
            printf("\n[OK] %s\n", pkt.payload);
        } else if (pkt.command == CMD_ERROR) {
            printf("\n[ERROR] %s\n", pkt.payload);
        } else if (pkt.command == CMD_USER_LIST) {
            printf("\n[USUARIOS] %s\n", pkt.payload);
        } else if (pkt.command == CMD_USER_INFO) {
            printf("\n[INFO] %s\n", pkt.payload);
        } else if (pkt.command == CMD_DISCONNECTED) {
            printf("\n[DESCONECTADO] %s\n", pkt.payload);
        } else {
            printf("\n[CMD %u] %s\n", pkt.command, pkt.payload);
        }

        printf("> ");
        fflush(stdout);
    }

    return NULL;
}
