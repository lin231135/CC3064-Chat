#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#include "../protocolo.h"

#define MAX_CLIENTS 100

/* Representa una sesion conectada en memoria del servidor. */
typedef struct {
    char username[32];
    char ip[INET_ADDRSTRLEN];
    char status[16];
    int sockfd;
    int active;
    time_t last_activity;
} Client;

/* Evento producido por el watchdog cuando un usuario pasa a INACTIVE. */
typedef struct {
    int sockfd;
    char username[32];
} InactiveEvent;

/* Inicializa el almacenamiento y contador de clientes. */
void cm_init(void);
/* Agrega un cliente validando duplicados por username e IP. */
int cm_add_client(const char *username, const char *ip, int sockfd);
/* Elimina por username. */
int cm_remove_client(const char *username);
/* Elimina por socket y opcionalmente retorna username removido. */
int cm_remove_by_sockfd(int sockfd, char *out_username, int out_len);
/* Busca cliente y devuelve copia de la estructura. */
int cm_find_client(const char *username, Client *out);
/* Actualiza status de un usuario. */
int cm_set_status(const char *username, const char *status);
/* Renueva marca de actividad del usuario. */
int cm_update_activity(const char *username);
/* Obtiene sockets activos, excluyendo uno (util para broadcast). */
int cm_get_sockets_except(int exclude_sockfd, int *out_sockets, int max_sockets);
/* Obtiene socket por username para mensajes directos. */
int cm_get_socket_by_username(const char *username);
/* Construye CSV de usuarios y status: user,status;... */
int cm_build_user_list(char *buffer, int buffer_len);
/* Construye info puntual: ip,status. */
int cm_build_user_info(const char *username, char *buffer, int buffer_len);
/* Marca usuarios inactivos segun timeout y produce lista de eventos. */
int cm_mark_inactive_clients(int timeout_sec, InactiveEvent *events, int max_events);

#endif
