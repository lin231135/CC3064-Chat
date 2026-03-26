#include "command_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Avanza el puntero hasta el siguiente caracter no-espacio. */
static const char *skip_spaces(const char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/* Copia segura de tokens parseados a buffers destino. */
static void copy_token(char *dst, int dst_len, const char *src) {
    snprintf(dst, (size_t)dst_len, "%s", src);
}

/*
 * Parser de comandos del cliente.
 * Soporta /help, /exit, /broadcast, /msg, /status, /list, /info.
 */
int parse_input_line(const char *line, ParsedCommand *out) {
    char cmd[64] = {0};
    const char *p;

    memset(out, 0, sizeof(ParsedCommand));
    out->type = PARSE_INVALID;

    if (line == NULL) {
        return -1;
    }

    p = skip_spaces(line);
    if (*p == '\0') {
        return -1;
    }

    if (sscanf(p, "%63s", cmd) != 1) {
        return -1;
    }

    if (strcmp(cmd, "/help") == 0) {
        out->type = PARSE_HELP;
        return 0;
    }

    if (strcmp(cmd, "/exit") == 0) {
        out->type = PARSE_EXIT;
        return 0;
    }

    if (strcmp(cmd, "/list") == 0) {
        out->type = PARSE_LIST;
        return 0;
    }

    if (strcmp(cmd, "/broadcast") == 0) {
        /* Todo lo que sigue al primer espacio se toma como mensaje. */
        const char *msg = strstr(p, " ");
        if (msg == NULL) {
            return -1;
        }
        msg = skip_spaces(msg);
        if (*msg == '\0') {
            return -1;
        }
        out->type = PARSE_BROADCAST;
        copy_token(out->arg2, (int)sizeof(out->arg2), msg);
        return 0;
    }

    if (strcmp(cmd, "/msg") == 0) {
        /* Formato esperado: /msg <usuario> <mensaje>. */
        char user[32] = {0};
        char message[957] = {0};
        if (sscanf(p, "/msg %31s %956[^\n]", user, message) < 2) {
            return -1;
        }
        out->type = PARSE_DIRECT;
        copy_token(out->arg1, (int)sizeof(out->arg1), user);
        copy_token(out->arg2, (int)sizeof(out->arg2), message);
        return 0;
    }

    if (strcmp(cmd, "/status") == 0) {
        char status[16] = {0};
        if (sscanf(p, "/status %15s", status) != 1) {
            return -1;
        }
        out->type = PARSE_STATUS;
        copy_token(out->arg1, (int)sizeof(out->arg1), status);
        return 0;
    }

    if (strcmp(cmd, "/info") == 0) {
        char user[32] = {0};
        if (sscanf(p, "/info %31s", user) != 1) {
            return -1;
        }
        out->type = PARSE_INFO;
        copy_token(out->arg1, (int)sizeof(out->arg1), user);
        return 0;
    }

    return -1;
}
