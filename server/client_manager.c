#include "client_manager.h"

#include <stdio.h>
#include <string.h>

/* Estado global del servidor protegido por mutex. */
static Client g_clients[MAX_CLIENTS];
static int g_client_count = 0;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Reinicia el estado de clientes al arrancar el servidor. */
void cm_init(void) {
    pthread_mutex_lock(&g_clients_mutex);
    memset(g_clients, 0, sizeof(g_clients));
    g_client_count = 0;
    pthread_mutex_unlock(&g_clients_mutex);
}

/* Verifica que el status solicitado sea uno permitido por protocolo. */
static int is_valid_status(const char *status) {
    return strcmp(status, STATUS_ACTIVO) == 0 || strcmp(status, STATUS_OCUPADO) == 0 || strcmp(status, STATUS_INACTIVO) == 0;
}

/* Inserta un cliente nuevo si no hay duplicados y existe cupo. */
int cm_add_client(const char *username, const char *ip, int sockfd) {
    int result = 0;

    pthread_mutex_lock(&g_clients_mutex);

    if (g_client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&g_clients_mutex);
        return -3;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].active) {
            continue;
        }
        if (strcmp(g_clients[i].username, username) == 0) {
            pthread_mutex_unlock(&g_clients_mutex);
            return -1;
        }
        if (strcmp(g_clients[i].ip, ip) == 0) {
            pthread_mutex_unlock(&g_clients_mutex);
            return -2;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active) {
            continue;
        }

        memset(&g_clients[i], 0, sizeof(Client));
        strncpy(g_clients[i].username, username, sizeof(g_clients[i].username) - 1);
        strncpy(g_clients[i].ip, ip, sizeof(g_clients[i].ip) - 1);
        strncpy(g_clients[i].status, STATUS_ACTIVO, sizeof(g_clients[i].status) - 1);
        g_clients[i].sockfd = sockfd;
        g_clients[i].active = 1;
        g_clients[i].last_activity = time(NULL);
        g_client_count++;
        result = 0;
        break;
    }

    pthread_mutex_unlock(&g_clients_mutex);
    return result;
}

/* Remueve un cliente por nombre de usuario. */
int cm_remove_client(const char *username) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            memset(&g_clients[i], 0, sizeof(Client));
            g_client_count--;
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/* Remueve un cliente por su socket de sesion (desconexion/logout). */
int cm_remove_by_sockfd(int sockfd, char *out_username, int out_len) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && g_clients[i].sockfd == sockfd) {
            if (out_username != NULL && out_len > 0) {
                strncpy(out_username, g_clients[i].username, (size_t)(out_len - 1));
                out_username[out_len - 1] = '\0';
            }
            memset(&g_clients[i], 0, sizeof(Client));
            g_client_count--;
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/* Busca un cliente activo y retorna sus datos. */
int cm_find_client(const char *username, Client *out) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            if (out != NULL) {
                *out = g_clients[i];
            }
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/* Cambia el status de un usuario y actualiza su actividad. */
int cm_set_status(const char *username, const char *status) {
    if (!is_valid_status(status)) {
        return -2;
    }

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            strncpy(g_clients[i].status, status, sizeof(g_clients[i].status) - 1);
            g_clients[i].status[sizeof(g_clients[i].status) - 1] = '\0';
            g_clients[i].last_activity = time(NULL);
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/* Renueva timestamp de actividad al recibir comandos del usuario. */
int cm_update_activity(const char *username) {
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            g_clients[i].last_activity = time(NULL);
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/* Recolecta sockets activos para difusion, excluyendo al emisor. */
int cm_get_sockets_except(int exclude_sockfd, int *out_sockets, int max_sockets) {
    int count = 0;

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS && count < max_sockets; i++) {
        if (!g_clients[i].active) {
            continue;
        }
        if (g_clients[i].sockfd == exclude_sockfd) {
            continue;
        }
        out_sockets[count++] = g_clients[i].sockfd;
    }
    pthread_mutex_unlock(&g_clients_mutex);

    return count;
}

/* Retorna el socket asociado a un username para envio directo. */
int cm_get_socket_by_username(const char *username) {
    int found_sock = -1;

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            found_sock = g_clients[i].sockfd;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);

    return found_sock;
}

/* Construye el payload de CMD_USER_LIST en formato CSV simple. */
int cm_build_user_list(char *buffer, int buffer_len) {
    int written = 0;

    if (buffer_len <= 0) {
        return -1;
    }

    buffer[0] = '\0';

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int needed;

        if (!g_clients[i].active) {
            continue;
        }

        needed = snprintf(buffer + written, (size_t)(buffer_len - written), "%s,%s;", g_clients[i].username, g_clients[i].status);
        if (needed < 0 || written + needed >= buffer_len) {
            pthread_mutex_unlock(&g_clients_mutex);
            return -1;
        }
        written += needed;
    }
    pthread_mutex_unlock(&g_clients_mutex);

    return 0;
}

/* Construye el payload de CMD_USER_INFO para un usuario puntual. */
int cm_build_user_info(const char *username, char *buffer, int buffer_len) {
    if (buffer_len <= 0) {
        return -1;
    }

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active && strcmp(g_clients[i].username, username) == 0) {
            snprintf(buffer, (size_t)buffer_len, "%s,%s", g_clients[i].ip, g_clients[i].status);
            pthread_mutex_unlock(&g_clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return -1;
}

/*
 * Revisa usuarios activos y marca INACTIVE cuando exceden timeout.
 * Devuelve eventos para que otra capa envie notificaciones de red.
 */
int cm_mark_inactive_clients(int timeout_sec, InactiveEvent *events, int max_events) {
    time_t now = time(NULL);
    int count = 0;

    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS && count < max_events; i++) {
        if (!g_clients[i].active) {
            continue;
        }

        if (strcmp(g_clients[i].status, STATUS_INACTIVO) == 0) {
            continue;
        }

        if ((int)(now - g_clients[i].last_activity) > timeout_sec) {
            strncpy(g_clients[i].status, STATUS_INACTIVO, sizeof(g_clients[i].status) - 1);
            g_clients[i].status[sizeof(g_clients[i].status) - 1] = '\0';

            events[count].sockfd = g_clients[i].sockfd;
            strncpy(events[count].username, g_clients[i].username, sizeof(events[count].username) - 1);
            events[count].username[sizeof(events[count].username) - 1] = '\0';
            count++;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);

    return count;
}
