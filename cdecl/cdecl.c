#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#define MAX_TOKEN_LEN 64
#define MAX_TOKENS 128
#define STRUCT_ENUM_ADDENDUM 7

enum token_type_t {
    TYPE,
    SPECIFIER,
    IDENTIFIER,
    ARRAY,
    POINTER,
    LBRACE,
    RBRACE,
    END,
};

const char *token_to_str(enum token_type_t type) {
    static char *strs[] = {"TYPE",
                           "SPECIFIER",
                           "IDENTIFIER",
                           "ARRAY",
                           "POINTER",
                           "LBRACE",
                           "RBRACE",
                           "END"
                          };
    return strs[type];
}

struct token_t {
    enum token_type_t type;

    union {
        char name[STRUCT_ENUM_ADDENDUM + MAX_TOKEN_LEN + 1];
        int size;
    } info;
};

static int line = 1;
static int position = 0;
static struct token_t token;
static struct token_t stack[MAX_TOKENS];
static int head = 0;
static bool verbose = false;

void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

#define fatal_pos(format, ...) \
  fatal("Line: %d, Position: %d: " format, line, position, #__VA_ARGS__)

static void stack_push(const struct token_t *token) {
    if (head >= MAX_TOKENS) {
        fatal("Too long declaration. Can't proceed.\n");
    }
    stack[head++] = *token;
}

static struct token_t stack_pop(void) {
    if (head == 0) {
        fatal("Stack underflow. Invald declaration.\n");
    }
    return stack[--head];
}

static bool
stack_is_empty(void) {
    return head == 0;
}

int _getchar() {
    int c = getchar();
    if (c != EOF) {
        ++position;
        if (c == '\n') {
            ++line;
            position = 0;
        }
    }
    return c;
}

void _ungetchar(int c) {
    if (ungetc(c, stdin) != c) {
        fatal("Unrecoverable IO error occurred.\n");
    }
    --position;
}

void skip_spaces(void) {
    int c;

    while ((c = getchar()) != EOF) {
        if (!isspace(c)) {
            _ungetchar(c);
            return;
        }
    }

    fatal_pos("Unexpected end of file\n");
}

char *get_id(char *id) {
    int i = 0;
    char *p = id;

    while (1) {
        int c = _getchar();
        if (c == EOF) {
            fatal_pos("Unexpected end of file.\n");
        } else if (isalnum(c) || c == '_') {
            if (i++ >= MAX_TOKEN_LEN) {
                fatal_pos("Too long token occured. Can't proceed.\n");
            }
            *p++ = c;
        } else {
            _ungetchar(c);
            *p = '\0';
            return id;
        }
    }
}

void skip_to_char(char end) {
    while (1) {
        int c = _getchar();
        if (c == EOF) {
            fatal_pos("Unexpected end of file\n");
        } else if (c == end) {
            return;
        }
    }
}

static void dump_token(void) {
    if (verbose) {
        fprintf(stderr, "token: %s", token_to_str(token.type));
        switch (token.type) {
        case TYPE:
        case SPECIFIER:
        case IDENTIFIER:
            fprintf(stderr, ", %s\n", token.info.name);
            break;
        default:
            fprintf(stderr, "\n");
        }
    }
}

void get_token() {
    skip_spaces();
    int c = getchar();
    ++position;

    if (c == EOF) {
        fatal_pos("Unexpected end of file\n");
    } else if (c == ';') {
        token.type = END;
    } else if (isalpha(c)) {
        char *name = token.info.name;
        _ungetchar(c);
        get_id(name);
        if (strcmp(name, "int") == 0 ||
                strcmp(name, "char") == 0 ||
                strcmp(name, "void") == 0 ||
                strcmp(name, "signed") == 0 ||
                strcmp(name, "unsigned") == 0 ||
                strcmp(name, "short") == 0 ||
                strcmp(name, "long") == 0 ||
                strcmp(name, "float") == 0 ||
                strcmp(name, "double") == 0) {
            token.type = TYPE;
        } else if (strcmp(name, "struct") == 0 ||
                   strcmp(name, "enum") == 0) {
            token.type = TYPE;
            skip_spaces();
            int tmp = _getchar();
            if (tmp == '{') {
                skip_to_char('}');
            } else {
                size_t len = strlen(name);
                name[len] = ' ';
                _ungetchar(tmp);
                get_id(name + len + 1);
            }
        } else if (strcmp(name, "const") == 0 ||
                   strcmp(name, "volatile") == 0) {
            token.type = SPECIFIER;
        } else {
            token.type = IDENTIFIER;
        }
    } else if (c == '[') {
        skip_to_char(']');
        token.type = ARRAY;
        token.info.size = -1;
    } else if (c == '(') {
        token.type = LBRACE;
    } else if (c == '*') {
        token.type = POINTER;
    } else {
        fatal_pos("Unexpected token encountered\n");
    }

    dump_token();
}

static void pronounce_token(const struct token_t *token) {
    switch (token->type) {
    case TYPE:
        printf("%s ", token->info.name);
        break;
        ;
    case SPECIFIER:
        if (strcmp(token->info.name, "const") == 0) {
            printf("read-only ");
        } else {
            printf("%s ", token->info.name);
        }
        break;
    case ARRAY:
        printf("array of ");
        break;
    case POINTER:
        printf("pointer to ");
        break;
    case LBRACE:
        printf("function returning ");
        break;
    case RBRACE:
    case END:
        break;
    default:
        assert(0);
    }
}

static void pronounce(void) {
    do {
        get_token();
        stack_push(&token);
    } while (token.type != IDENTIFIER);
    stack_pop();
    printf("%s is ", token.info.name);

    bool right_finished = false;
    bool left_finished = false;

    while (true) {
        if (!right_finished) {
            do {
                get_token();
                pronounce_token(&token);

                if (token.type == LBRACE) {
                    do {
                        get_token();
                    } while (token.type != RBRACE);
                    token.type = LBRACE;
                }
            } while (token.type != END && token.type != RBRACE);
            if (token.type == END) {
                right_finished = true;
            }
        }
        if (!left_finished) {
            struct token_t left_token;
            do {
                left_token = stack_pop();
                if (left_token.type != LBRACE) {
                    pronounce_token(&left_token);
                }
            } while (!stack_is_empty() && left_token.type != LBRACE);

            if (stack_is_empty()) {
                left_finished = true;
            }
        }
        if (left_finished && right_finished) {
            break;
        }
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fatal("%s: wrong number of arguments given\n", argv[0]);
    }
    if (argc == 2) {
        if (strcmp(argv[1], "--verbose") == 0 ||
                strcmp(argv[1], "-v") == 0) {
            verbose = true;
        } else {
            fatal("%s: invalid argument (%s)\n", argv[0], argv[1]);
        }
    }

    pronounce();

    return EXIT_SUCCESS;
}