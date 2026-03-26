#include "net_utils.h"

#include <string.h>
#include <sys/socket.h>

/*
 * send puede enviar menos bytes de los solicitados.
 * Este loop garantiza envio completo del buffer.
 */
int send_all(int sockfd, const void *buf, int len) {
    int total = 0;
    const char *p = (const char *)buf;

    while (total < len) {
        int n = (int)send(sockfd, p + total, (size_t)(len - total), 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }

    return 0;
}

/*
 * recv puede devolver lecturas parciales.
 * Este loop bloquea hasta completar len bytes o detectar cierre/error.
 */
int recv_all(int sockfd, void *buf, int len) {
    int total = 0;
    char *p = (char *)buf;

    while (total < len) {
        int n = (int)recv(sockfd, p + total, (size_t)(len - total), 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }

    return 0;
}

/* Envia el paquete binario completo definido en protocolo.h. */
int send_packet(int sockfd, const ChatPacket *pkt) {
    return send_all(sockfd, pkt, (int)sizeof(ChatPacket));
}

/* Limpia la estructura y luego llena los 1024 bytes desde el socket. */
int recv_packet(int sockfd, ChatPacket *pkt) {
    memset(pkt, 0, sizeof(ChatPacket));
    return recv_all(sockfd, pkt, (int)sizeof(ChatPacket));
}
