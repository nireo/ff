#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    T_EOF,
    T_IDENT,
    T_NUM,
    T_PUNCT,
};

enum {
    N_NUM,
    N_IDENT,
    N_ADD,
    N_SUB,
    N_MUL,
    N_ASSIGN,
    N_EXPR_STMT,
};

enum {
    D_FUNC,
};

typedef struct token token;
struct token {
    char* pt;
    int len;
    int ty;
    int val;
    token* next;
};

typedef struct node node;
struct node {
    int ty;
    int val;
    token* tok;
    node* lhs;
    node* rhs;
};

typedef struct param param;
struct param {
    token* ty;
    token* name;
    param* next;
};

typedef struct decl decl;
struct decl {
    int ty;
    node* value; // expr for global vars, stmt for functions
    param* param;
};

token* tokens;
token* cur;

int is_digit(char c)
{
    return '0' <= c && c <= '9';
}

int is_alpha(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

int is_ident2(char c)
{
    return is_alpha(c) || is_digit(c);
}

int is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

int is_punct(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '=' || c == ';' || c == '(' || c == ')';
}

token* new_token(int ty, char* pt, int len)
{
    token* t = malloc(sizeof(token));
    t->ty = ty;
    t->pt = pt;
    t->len = len;
    t->val = 0;
    t->next = NULL;
    return t;
}

void lex(char* p)
{
    token head;
    token* tail = &head;
    head.next = NULL;

    while (*p) {
        if (is_space(*p)) {
            p++;
            continue;
        }

        if (is_digit(*p)) {
            char* start = p;
            int val = 0;

            while (is_digit(*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }

            token* t = new_token(T_NUM, start, p - start);
            t->val = val;
            tail->next = t;
            tail = t;
            continue;
        }

        if (is_alpha(*p)) {
            char* start = p;

            while (is_ident2(*p))
                p++;

            token* t = new_token(T_IDENT, start, p - start);
            tail->next = t;
            tail = t;
            continue;
        }

        if (is_punct(*p)) {
            token* t = new_token(T_PUNCT, p, 1);
            tail->next = t;
            tail = t;
            p++;
            continue;
        }

        fprintf(stderr, "unknown char: %c\n", *p);
        exit(1);
    }

    tail->next = new_token(T_EOF, p, 0);
    tokens = head.next;
    cur = tokens;
}

int equal(char* s)
{
    return cur->ty == T_PUNCT && cur->len == (int)strlen(s) && strncmp(cur->pt, s, cur->len) == 0;
}

int consume(char* s)
{
    if (equal(s)) {
        cur = cur->next;
        return 1;
    }
    return 0;
}

void expect(char* s)
{
    if (!consume(s)) {
        fprintf(stderr, "expected '%s'\n", s);
        exit(1);
    }
}

node* new_node(int ty, node* lhs, node* rhs)
{
    node* n = malloc(sizeof(node));
    n->ty = ty;
    n->val = 0;
    n->tok = NULL;
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

node* new_num(int val)
{
    node* n = new_node(N_NUM, NULL, NULL);
    n->val = val;
    return n;
}

node* new_ident(token* tok)
{
    node* n = new_node(N_IDENT, NULL, NULL);
    n->tok = tok;
    return n;
}

/*
Grammar:

program     = stmt*
stmt        = expr ";"
expr        = assign
assign      = add ("=" assign)?
add         = mul ("+" mul | "-" mul)*
mul         = primary ("*" primary)*
primary     = num | ident | "(" expr ")"
*/

node* expr();

node* primary()
{
    if (consume("(")) {
        node* n = expr();
        expect(")");
        return n;
    }

    if (cur->ty == T_NUM) {
        int val = cur->val;
        cur = cur->next;
        return new_num(val);
    }

    if (cur->ty == T_IDENT) {
        token* tok = cur;
        cur = cur->next;
        return new_ident(tok);
    }

    fprintf(stderr, "expected expression\n");
    exit(1);
}

node* mul()
{
    node* n = primary();

    for (;;) {
        if (consume("*")) {
            n = new_node(N_MUL, n, primary());
            continue;
        }

        return n;
    }
}

node* add()
{
    node* n = mul();

    for (;;) {
        if (consume("+")) {
            n = new_node(N_ADD, n, mul());
            continue;
        }

        if (consume("-")) {
            n = new_node(N_SUB, n, mul());
            continue;
        }

        return n;
    }
}

node* assign()
{
    node* n = add();
    if (consume("="))
        n = new_node(N_ASSIGN, n, assign());

    return n;
}

node* expr()
{
    return assign();
}

node* stmt()
{
    node* n = expr();
    expect(";");
    return new_node(N_EXPR_STMT, n, NULL);
}

void print_node(node* n)
{
    if (!n)
        return;

    switch (n->ty) {
    case N_NUM:
        printf("%d", n->val);
        return;

    case N_IDENT:
        printf("%.*s", n->tok->len, n->tok->pt);
        return;

    case N_ADD:
        printf("(");
        print_node(n->lhs);
        printf(" + ");
        print_node(n->rhs);
        printf(")");
        return;

    case N_SUB:
        printf("(");
        print_node(n->lhs);
        printf(" - ");
        print_node(n->rhs);
        printf(")");
        return;

    case N_MUL:
        printf("(");
        print_node(n->lhs);
        printf(" * ");
        print_node(n->rhs);
        printf(")");
        return;

    case N_ASSIGN:
        printf("(");
        print_node(n->lhs);
        printf(" = ");
        print_node(n->rhs);
        printf(")");
        return;

    case N_EXPR_STMT:
        print_node(n->lhs);
        printf(";\n");
        return;
    }
}

int main(void)
{
    char buf[8096];

    size_t n = fread(buf, 1, sizeof(buf) - 1, stdin);

    if (ferror(stdin)) {
        perror("fread");
        return 1;
    }

    buf[n] = 0;
    lex(buf);

    while (cur->ty != T_EOF) {
        node* n = stmt();
        print_node(n);
    }

    return 0;
}
