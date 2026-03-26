#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

/* Tipo logico de comando reconocido desde stdin del cliente. */
typedef enum {
    PARSE_INVALID = 0,
    PARSE_HELP,
    PARSE_EXIT,
    PARSE_BROADCAST,
    PARSE_DIRECT,
    PARSE_STATUS,
    PARSE_LIST,
    PARSE_INFO
} ParseType;

/* Resultado del parseo: tipo + argumentos normalizados. */
typedef struct {
    ParseType type;
    char arg1[32];
    char arg2[957];
} ParsedCommand;

/* Convierte una linea de texto en un comando del protocolo cliente. */
int parse_input_line(const char *line, ParsedCommand *out);

#endif
