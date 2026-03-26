#ifndef SESSION_H
#define SESSION_H

/* Contexto minimo que se transfiere a cada thread de sesion. */
typedef struct {
    int sockfd;
    char ip[64];
} SessionArgs;

/* Atiende comandos de un cliente conectado hasta su salida. */
void *client_session_thread(void *arg);
/* Monitorea inactividad y aplica transicion a INACTIVE. */
void *inactivity_watchdog_thread(void *arg);

#endif
