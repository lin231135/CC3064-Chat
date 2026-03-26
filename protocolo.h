#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

/*
 * Contrato comun de red para cliente y servidor.
 * Todas las implementaciones deben compartir este archivo sin cambios de layout.
 */

/* Comandos Cliente -> Servidor */
#define CMD_REGISTER 1
#define CMD_BROADCAST 2
#define CMD_DIRECT 3
#define CMD_LIST 4
#define CMD_INFO 5
#define CMD_STATUS 6
#define CMD_LOGOUT 7

/* Respuestas Servidor -> Cliente */
#define CMD_OK 8
#define CMD_ERROR 9
#define CMD_MSG 10
#define CMD_USER_LIST 11
#define CMD_USER_INFO 12
#define CMD_DISCONNECTED 13

/* Valores de status */
#define STATUS_ACTIVO "ACTIVE"
#define STATUS_OCUPADO "BUSY"
#define STATUS_INACTIVO "INACTIVE"

/* Timeouts */
#define INACTIVITY_TIMEOUT 60

/*
 * Paquete binario fijo de 1024 bytes.
 * El servidor y cliente leen/escriben siempre sizeof(ChatPacket).
 */
typedef struct {
    uint8_t command;
    uint16_t payload_len;
    char sender[32];
    char target[32];
    char payload[957];
} __attribute__((packed)) ChatPacket;

/* Falla compilacion si el tamano deja de ser 1024 bytes. */
typedef char ChatPacketSizeMustBe1024[(sizeof(ChatPacket) == 1024) ? 1 : -1];

#endif
