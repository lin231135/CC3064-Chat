#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/net_utils.h"
#include "client_manager.h"

/* Construye un paquete consistente y calcula payload_len automaticamente. */
static void make_packet(ChatPacket *pkt, uint8_t command, const char *sender, const char *target, const char *payload) {
    memset(pkt, 0, sizeof(ChatPacket));
    pkt->command = command;

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

/* Envia una respuesta estandar de error al cliente. */
static void send_error(int sockfd, const char *target, const char *msg) {
    ChatPacket pkt;
    make_packet(&pkt, CMD_ERROR, "SERVER", target, msg);
    send_packet(sockfd, &pkt);
}

/* Envia una respuesta estandar de confirmacion al cliente. */
static void send_ok(int sockfd, const char *target, const char *msg) {
    ChatPacket pkt;
    make_packet(&pkt, CMD_OK, "SERVER", target, msg);
    send_packet(sockfd, &pkt);
}

/* Notifica a todos los demas clientes que un usuario se desconecto. */
static void broadcast_disconnected(const char *username, int exclude_sockfd) {
    int sockets[MAX_CLIENTS];
    int count = cm_get_sockets_except(exclude_sockfd, sockets, MAX_CLIENTS);

    ChatPacket pkt;
    make_packet(&pkt, CMD_DISCONNECTED, "SERVER", "ALL", username);

    for (int i = 0; i < count; i++) {
        send_packet(sockets[i], &pkt);
    }
}

/* Procesa el handshake de registro y deja asociada la sesion al username. */
static void handle_register(int sockfd, const char *ip, const ChatPacket *req, char *session_username, int session_username_len, int *registered) {
    int rc;

    if (req->sender[0] == '\0') {
        send_error(sockfd, "", "Username vacio");
        return;
    }

    rc = cm_add_client(req->sender, ip, sockfd);
    if (rc == -1) {
        send_error(sockfd, req->sender, "Usuario ya existe");
        return;
    }
    if (rc == -2) {
        send_error(sockfd, req->sender, "IP ya conectada");
        return;
    }
    if (rc == -3) {
        send_error(sockfd, req->sender, "Servidor lleno");
        return;
    }

    snprintf(session_username, (size_t)session_username_len, "%s", req->sender);
    *registered = 1;

    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Bienvenido %s", session_username);
        send_ok(sockfd, session_username, msg);
    }

    printf("[+] Usuario registrado: %s (%s)\n", session_username, ip);
}

/*
 * Loop principal de una sesion cliente:
 * - exige REGISTER antes de otros comandos
 * - enruta comandos del protocolo
 * - limpia recursos al salir o desconectarse
 */
void *client_session_thread(void *arg) {
    SessionArgs *args = (SessionArgs *)arg;
    int sockfd = args->sockfd;
    char ip[64];
    char username[32] = {0};
    int is_registered = 0;

    strncpy(ip, args->ip, sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0';
    free(args);

    while (1) {
        ChatPacket req;

        /* Si falla recv, se asume desconexion abrupta o error de red. */
        if (recv_packet(sockfd, &req) != 0) {
            break;
        }

        /* Primer requisito de sesion: registro exitoso. */
        if (!is_registered) {
            if (req.command != CMD_REGISTER) {
                send_error(sockfd, "", "Primero debes registrarte");
                continue;
            }
            handle_register(sockfd, ip, &req, username, (int)sizeof(username), &is_registered);
            continue;
        }

        cm_update_activity(username);

        /* Mensaje al chat general. */
        if (req.command == CMD_BROADCAST) {
            int sockets[MAX_CLIENTS];
            int count = cm_get_sockets_except(sockfd, sockets, MAX_CLIENTS);
            ChatPacket out;

            make_packet(&out, CMD_MSG, username, "ALL", req.payload);
            for (int i = 0; i < count; i++) {
                send_packet(sockets[i], &out);
            }
            send_ok(sockfd, username, "Broadcast enviado");
            continue;
        }

        /* Mensaje privado a un usuario especifico. */
        if (req.command == CMD_DIRECT) {
            int target_sock = cm_get_socket_by_username(req.target);
            if (target_sock < 0) {
                send_error(sockfd, username, "Destinatario no conectado");
                continue;
            }

            {
                ChatPacket out;
                make_packet(&out, CMD_MSG, username, req.target, req.payload);
                send_packet(target_sock, &out);
            }
            send_ok(sockfd, username, "Mensaje directo enviado");
            continue;
        }

        /* Retorna listado de usuarios conectados. */
        if (req.command == CMD_LIST) {
            char list_payload[957];
            ChatPacket out;
            if (cm_build_user_list(list_payload, sizeof(list_payload)) != 0) {
                send_error(sockfd, username, "No se pudo construir lista");
                continue;
            }
            make_packet(&out, CMD_USER_LIST, "SERVER", username, list_payload);
            send_packet(sockfd, &out);
            continue;
        }

        /* Retorna IP y status de un usuario puntual. */
        if (req.command == CMD_INFO) {
            char info_payload[957];
            ChatPacket out;
            if (cm_build_user_info(req.target, info_payload, sizeof(info_payload)) != 0) {
                send_error(sockfd, username, "Usuario no conectado");
                continue;
            }
            make_packet(&out, CMD_USER_INFO, "SERVER", username, info_payload);
            send_packet(sockfd, &out);
            continue;
        }

        /* Cambia status del usuario actual. */
        if (req.command == CMD_STATUS) {
            if (cm_set_status(username, req.payload) != 0) {
                send_error(sockfd, username, "Status invalido");
                continue;
            }
            send_ok(sockfd, username, req.payload);
            continue;
        }

        /* Cierre controlado de sesion. */
        if (req.command == CMD_LOGOUT) {
            send_ok(sockfd, username, "Logout exitoso");
            break;
        }

        send_error(sockfd, username, "Comando no soportado");
    }

    /* Limpieza de la sesion y aviso global de desconexion. */
    if (is_registered) {
        char removed[32] = {0};
        if (cm_remove_by_sockfd(sockfd, removed, (int)sizeof(removed)) == 0 && removed[0] != '\0') {
            printf("[-] Usuario desconectado: %s\n", removed);
            broadcast_disconnected(removed, sockfd);
        }
    }

    close(sockfd);
    return NULL;
}

/*
 * Hilo periodico de inactividad:
 * marca usuarios como INACTIVE y les envia aviso.
 */
void *inactivity_watchdog_thread(void *arg) {
    (void)arg;

    while (1) {
        InactiveEvent events[MAX_CLIENTS];
        int count = cm_mark_inactive_clients(INACTIVITY_TIMEOUT, events, MAX_CLIENTS);

        for (int i = 0; i < count; i++) {
            ChatPacket pkt;
            make_packet(&pkt, CMD_MSG, "SERVER", events[i].username, "Tu status cambio a INACTIVE");
            send_packet(events[i].sockfd, &pkt);
        }

        sleep(2);
    }

    return NULL;
}
