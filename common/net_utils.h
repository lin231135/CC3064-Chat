#ifndef NET_UTILS_H
#define NET_UTILS_H

#include "../protocolo.h"

/* Envia exactamente len bytes o retorna error. */
int send_all(int sockfd, const void *buf, int len);
/* Recibe exactamente len bytes o retorna error/desconexion. */
int recv_all(int sockfd, void *buf, int len);
/* Wrapper para enviar un ChatPacket completo (1024 bytes). */
int send_packet(int sockfd, const ChatPacket *pkt);
/* Wrapper para recibir un ChatPacket completo (1024 bytes). */
int recv_packet(int sockfd, ChatPacket *pkt);

#endif
