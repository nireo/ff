#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    T_EOF,
    T_IDENT,
    T_NUM,
    T_STR,
    T_CHAR,
    T_PUNCT,
};

enum {
    ND_NULL,
    ND_NUM,
    ND_VAR,
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_ASSIGN,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_LOGAND,
    ND_LOGOR,
    ND_FUNCALL,
    ND_EXPR_STMT,
    ND_RETURN,
    ND_BLOCK,
    ND_IF,
    ND_FOR,
    ND_ADDR,
    ND_DEREF,
    ND_NOT,
};

typedef struct token token;
struct token {
    char* pt;
    int len;
    int ty;
    int val;
    token* next;
};

typedef struct type type;
struct type {
    int kind;
};

typedef struct obj obj;
struct obj {
    char* name;
    type* ty;
    int is_local;
    int offset;
    obj* next;
};

typedef struct node node;
struct node {
    int kind;
    node* next;

    type* ty;

    node* lhs;
    node* rhs;

    node* cond;
    node* then;
    node* els;

    node* init;
    node* inc;

    node* body;

    node* args;

    obj* var;

    long val;

    char* funcname;
};

typedef struct function function;
struct function {
    token* name;
    node* body;
    function* next;
};

typedef struct name name;
struct name {
    char* pt;
    int len;
    name* next;
};

token* tokens;
token* cur;
name* type_names;

int labelseq;
int current_return_label;

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

int startswith(char* p, char* s)
{
    return strncmp(p, s, strlen(s)) == 0;
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

int punct_len(char* p)
{
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">="))
        return 2;

    if (startswith(p, "&&") || startswith(p, "||") || startswith(p, "->"))
        return 2;

    if (startswith(p, "++") || startswith(p, "--") || startswith(p, "+=") || startswith(p, "-="))
        return 2;

    if (startswith(p, "*=") || startswith(p, "/=") || startswith(p, "%="))
        return 2;

    return strchr("+-*/%=;(){}[],.&!<>?:", *p) != NULL;
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

        if (*p == '#') {
            while (*p && *p != '\n')
                p++;
            continue;
        }

        if (startswith(p, "//")) {
            p += 2;
            while (*p && *p != '\n')
                p++;
            continue;
        }

        if (startswith(p, "/*")) {
            p += 2;
            while (*p && !startswith(p, "*/"))
                p++;
            if (*p)
                p += 2;
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

        if (*p == '"') {
            char* start = p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1])
                    p++;
                p++;
            }
            if (*p)
                p++;

            tail->next = new_token(T_STR, start, p - start);
            tail = tail->next;
            continue;
        }

        if (*p == '\'') {
            char* start = p++;
            while (*p && *p != '\'') {
                if (*p == '\\' && p[1])
                    p++;
                p++;
            }
            if (*p)
                p++;

            tail->next = new_token(T_CHAR, start, p - start);
            tail = tail->next;
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

        int len = punct_len(p);
        if (len) {
            token* t = new_token(T_PUNCT, p, len);
            tail->next = t;
            tail = t;
            p += len;
            continue;
        }

        fprintf(stderr, "unknown char: %c\n", *p);
        exit(1);
    }

    tail->next = new_token(T_EOF, p, 0);
    tokens = head.next;
    cur = tokens;
}

int same(token* tok, char* s)
{
    return tok->len == (int)strlen(s) && strncmp(tok->pt, s, tok->len) == 0;
}

int equal(char* s)
{
    return same(cur, s);
}

int consume(char* s)
{
    if (equal(s)) {
        cur = cur->next;
        return 1;
    }
    return 0;
}

void error_at(token* tok, char* msg)
{
    int line = 1;
    char* p = tokens->pt;

    while (p < tok->pt) {
        if (*p == '\n')
            line++;
        p++;
    }

    fprintf(stderr, "%s at line %d near '%.*s'\n", msg, line, tok->len, tok->pt);
    exit(1);
}

void expect(char* s)
{
    if (!consume(s))
        error_at(cur, "unexpected token");
}

token* expect_ident(void)
{
    if (cur->ty != T_IDENT)
        error_at(cur, "expected identifier");

    token* tok = cur;
    cur = cur->next;
    return tok;
}

char* token_to_str(token* tok)
{
    char* s = malloc(tok->len + 1);
    memcpy(s, tok->pt, tok->len);
    s[tok->len] = 0;
    return s;
}

node* new_node(int kind)
{
    node* n = malloc(sizeof(node));
    memset(n, 0, sizeof(node));
    n->kind = kind;
    return n;
}

node* new_binary(int kind, node* lhs, node* rhs)
{
    node* n = new_node(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

node* new_unary(int kind, node* lhs)
{
    node* n = new_node(kind);
    n->lhs = lhs;
    return n;
}

node* new_num(long val)
{
    node* n = new_node(ND_NUM);
    n->val = val;
    return n;
}

node* new_var(token* tok)
{
    node* n = new_node(ND_VAR);
    obj* var = malloc(sizeof(obj));
    memset(var, 0, sizeof(obj));
    var->name = token_to_str(tok);
    n->var = var;
    return n;
}

node* new_null(void)
{
    return new_node(ND_NULL);
}

function* new_function(token* name, node* body)
{
    function* fn = malloc(sizeof(function));
    memset(fn, 0, sizeof(function));
    fn->name = name;
    fn->body = body;
    return fn;
}

void add_type_name(token* tok)
{
    name* n = malloc(sizeof(name));
    n->pt = tok->pt;
    n->len = tok->len;
    n->next = type_names;
    type_names = n;
}

void add_builtin_type(char* s)
{
    token tok;
    tok.pt = s;
    tok.len = strlen(s);
    add_type_name(&tok);
}

void print_sym(token* tok)
{
    printf("_");
    printf("%.*s", tok->len, tok->pt);
}

void emit_text(void)
{
    printf(".text\n");
}

void emit_global(token* name)
{
    printf(".globl ");
    print_sym(name);
    printf("\n");
}

void emit_label(token* name)
{
    print_sym(name);
    printf(":\n");
}

int new_label(void)
{
    return labelseq++;
}

void emit_func_start(token* name)
{
    emit_global(name);
    emit_label(name);
    printf("    stp x29, x30, [sp, #-16]!    // save frame pointer and link register\n");
    printf("    mov x29, sp                  // establish stack frame\n");
    printf("    mov w0, #0                   // default return value\n");
}

void emit_return(void)
{
    if (current_return_label < 0)
        return;

    printf("    b .L.return.%d               // return\n", current_return_label);
}

void emit_imm(long val)
{
    printf("    mov x0, #%ld                 // load immediate\n", val);
}

void emit_func_end(int label)
{
    printf(".L.return.%d:\n", label);
    printf("    ldp x29, x30, [sp], #16      // restore frame pointer and link register\n");
    printf("    ret                          // return to caller\n");
}

int is_type_name(token* tok)
{
    name* n = type_names;

    while (n) {
        if (tok->len == n->len && strncmp(tok->pt, n->pt, tok->len) == 0)
            return 1;
        n = n->next;
    }

    return 0;
}

int is_type_start(void)
{
    return cur->ty == T_IDENT && (is_type_name(cur) || equal("struct") || equal("enum"));
}

node* expr(void);
node* declaration_rest(int is_typedef);
node* statement(void);

void enum_spec(void)
{
    expect("enum");

    if (cur->ty == T_IDENT)
        cur = cur->next;

    if (!consume("{"))
        return;

    while (!consume("}")) {
        expect_ident();
        if (consume("="))
            expr();
        consume(",");
    }
}

void type_spec(void)
{
    if (consume("struct")) {
        if (cur->ty == T_IDENT)
            cur = cur->next;

        if (consume("{")) {
            while (!consume("}"))
                declaration_rest(0);
        }
        return;
    }

    if (equal("enum")) {
        enum_spec();
        return;
    }

    if (cur->ty == T_IDENT && is_type_name(cur)) {
        cur = cur->next;
        return;
    }

    error_at(cur, "expected type");
}

token* declarator(void)
{
    while (consume("*"))
        ;

    token* name = expect_ident();

    while (consume("[")) {
        if (!consume("]")) {
            expr();
            expect("]");
        }
    }

    return name;
}

void initializer(void)
{
    if (consume("{")) {
        if (!consume("}")) {
            initializer();
            while (consume(",")) {
                if (consume("}"))
                    return;
                initializer();
            }
            expect("}");
        }
        return;
    }

    expr();
}

node* declaration_rest(int is_typedef)
{
    type_spec();

    if (consume(";"))
        return new_null();

    token* decl_name = declarator();
    if (is_typedef)
        add_type_name(decl_name);

    if (consume("="))
        initializer();

    while (consume(",")) {
        decl_name = declarator();
        if (is_typedef)
            add_type_name(decl_name);
        if (consume("="))
            initializer();
    }

    expect(";");
    return new_null();
}

void param_list(void)
{
    if (consume(")"))
        return;

    if (equal("void") && same(cur->next, ")")) {
        cur = cur->next;
        expect(")");
        return;
    }

    for (;;) {
        type_spec();
        if (!equal(",") && !equal(")"))
            declarator();

        if (consume(")"))
            return;
        expect(",");
    }
}

node* compound_after_open(void)
{
    node head;
    node* tail = &head;
    head.next = NULL;

    while (!consume("}")) {
        if (cur->ty == T_EOF)
            error_at(cur, "expected '}'");

        if (consume("typedef")) {
            tail->next = declaration_rest(1);
            tail = tail->next;
            continue;
        }

        if (is_type_start()) {
            tail->next = declaration_rest(0);
            tail = tail->next;
            continue;
        }

        tail->next = statement();
        tail = tail->next;
    }

    node* n = new_node(ND_BLOCK);
    n->body = head.next;
    return n;
}

node* compound(void)
{
    expect("{");
    return compound_after_open();
}

node* statement(void)
{
    if (consume("{")) {
        return compound_after_open();
    }

    if (consume("return")) {
        node* n = new_node(ND_RETURN);
        if (!consume(";")) {
            n->lhs = expr();
            expect(";");
        }
        return n;
    }

    if (consume("if")) {
        node* n = new_node(ND_IF);
        expect("(");
        n->cond = expr();
        expect(")");
        n->then = statement();
        if (consume("else"))
            n->els = statement();
        return n;
    }

    if (consume("while")) {
        node* n = new_node(ND_FOR);
        expect("(");
        n->cond = expr();
        expect(")");
        n->then = statement();
        return n;
    }

    if (consume("for")) {
        node* n = new_node(ND_FOR);
        expect("(");
        if (is_type_start()) {
            n->init = declaration_rest(0);
        } else {
            if (!consume(";")) {
                n->init = expr();
                expect(";");
            }
        }

        if (!consume(";")) {
            n->cond = expr();
            expect(";");
        }

        if (!consume(")")) {
            n->inc = expr();
            expect(")");
        }

        n->then = statement();
        return n;
    }

    if (consume("switch")) {
        expect("(");
        expr();
        expect(")");
        return statement();
    }

    if (consume("case")) {
        expr();
        expect(":");
        return statement();
    }

    if (consume("default")) {
        expect(":");
        return statement();
    }

    if (consume("break") || consume("continue")) {
        expect(";");
        return new_null();
    }

    if (consume(";"))
        return new_null();

    node* n = new_unary(ND_EXPR_STMT, expr());
    expect(";");
    return n;
}

node* primary(void)
{
    if (cur->ty == T_NUM || cur->ty == T_STR || cur->ty == T_CHAR) {
        node* n = new_num(cur->val);
        cur = cur->next;
        return n;
    }

    if (cur->ty == T_IDENT) {
        node* n = new_var(cur);
        cur = cur->next;
        return n;
    }

    if (consume("(")) {
        if (is_type_start()) {
            type_spec();
            while (consume("*"))
                ;
            expect(")");
            return primary();
        }

        node* n = expr();
        expect(")");
        return n;
    }

    error_at(cur, "expected expression");
    return new_null();
}

node* postfix(void)
{
    node* n = primary();

    for (;;) {
        if (consume("(")) {
            node head;
            node* tail = &head;
            head.next = NULL;

            if (!consume(")")) {
                tail->next = expr();
                tail = tail->next;
                while (consume(",")) {
                    tail->next = expr();
                    tail = tail->next;
                }
                expect(")");
            }

            node* call = new_node(ND_FUNCALL);
            call->args = head.next;
            if (n->kind == ND_VAR)
                call->funcname = n->var->name;
            n = call;
            continue;
        }

        if (consume("[")) {
            n = new_binary(ND_ADD, n, expr());
            expect("]");
            n = new_unary(ND_DEREF, n);
            continue;
        }

        if (consume(".") || consume("->")) {
            expect_ident();
            continue;
        }

        if (consume("++") || consume("--"))
            continue;

        return n;
    }
}

node* unary(void)
{
    if (consume("+"))
        return unary();

    if (consume("-"))
        return new_binary(ND_SUB, new_num(0), unary());

    if (consume("!"))
        return new_unary(ND_NOT, unary());

    if (consume("*"))
        return new_unary(ND_DEREF, unary());

    if (consume("&"))
        return new_unary(ND_ADDR, unary());

    if (consume("++") || consume("--")) {
        return unary();
    }

    if (consume("sizeof")) {
        if (consume("(")) {
            if (is_type_start()) {
                type_spec();
                while (consume("*"))
                    ;
            } else {
                expr();
            }
            expect(")");
            return new_num(8);
        }
        unary();
        return new_num(8);
    }

    return postfix();
}

node* mul(void)
{
    node* n = unary();

    for (;;) {
        if (consume("*")) {
            n = new_binary(ND_MUL, n, unary());
            continue;
        }
        if (consume("/")) {
            n = new_binary(ND_DIV, n, unary());
            continue;
        }
        if (consume("%")) {
            n = new_binary(ND_MOD, n, unary());
            continue;
        }
        return n;
    }
}

node* add(void)
{
    node* n = mul();

    for (;;) {
        if (consume("+")) {
            n = new_binary(ND_ADD, n, mul());
            continue;
        }
        if (consume("-")) {
            n = new_binary(ND_SUB, n, mul());
            continue;
        }
        return n;
    }
}

node* relational(void)
{
    node* n = add();

    for (;;) {
        if (consume("<")) {
            n = new_binary(ND_LT, n, add());
            continue;
        }
        if (consume(">")) {
            n = new_binary(ND_LT, add(), n);
            continue;
        }
        if (consume("<=")) {
            n = new_binary(ND_LE, n, add());
            continue;
        }
        if (consume(">=")) {
            n = new_binary(ND_LE, add(), n);
            continue;
        }
        return n;
    }
}

node* equality(void)
{
    node* n = relational();

    for (;;) {
        if (consume("==")) {
            n = new_binary(ND_EQ, n, relational());
            continue;
        }
        if (consume("!=")) {
            n = new_binary(ND_NE, n, relational());
            continue;
        }
        return n;
    }
}

node* logical_and(void)
{
    node* n = equality();
    while (consume("&&"))
        n = new_binary(ND_LOGAND, n, equality());
    return n;
}

node* logical_or(void)
{
    node* n = logical_and();
    while (consume("||"))
        n = new_binary(ND_LOGOR, n, logical_and());
    return n;
}

node* assign(void)
{
    node* n = logical_or();

    if (consume("=") || consume("+=") || consume("-=") || consume("*=") || consume("/=") || consume("%="))
        n = new_binary(ND_ASSIGN, n, assign());

    return n;
}

node* expr(void)
{
    return assign();
}

void gen_expr(node* n);

void push(void)
{
    printf("    str x0, [sp, #-16]!          // push x0\n");
}

void pop(char* reg)
{
    printf("    ldr %s, [sp], #16            // pop into %s\n", reg, reg);
}

void gen_binary(node* n)
{
    gen_expr(n->lhs);
    push();
    gen_expr(n->rhs);
    pop("x1");

    switch (n->kind) {
    case ND_ADD:
        printf("    add x0, x1, x0               // x0 = lhs + rhs\n");
        return;
    case ND_SUB:
        printf("    sub x0, x1, x0               // x0 = lhs - rhs\n");
        return;
    case ND_MUL:
        printf("    mul x0, x1, x0               // x0 = lhs * rhs\n");
        return;
    case ND_DIV:
        printf("    sdiv x0, x1, x0              // x0 = lhs / rhs\n");
        return;
    case ND_MOD:
        printf("    sdiv x2, x1, x0              // x2 = lhs / rhs\n");
        printf("    msub x0, x2, x0, x1          // x0 = lhs %% rhs\n");
        return;
    case ND_EQ:
        printf("    cmp x1, x0                   // compare lhs == rhs\n");
        printf("    cset w0, eq                  // x0 = comparison result\n");
        return;
    case ND_NE:
        printf("    cmp x1, x0                   // compare lhs != rhs\n");
        printf("    cset w0, ne                  // x0 = comparison result\n");
        return;
    case ND_LT:
        printf("    cmp x1, x0                   // compare lhs < rhs\n");
        printf("    cset w0, lt                  // x0 = comparison result\n");
        return;
    case ND_LE:
        printf("    cmp x1, x0                   // compare lhs <= rhs\n");
        printf("    cset w0, le                  // x0 = comparison result\n");
        return;
    case ND_LOGAND:
        printf("    cmp x1, #0                   // lhs != 0\n");
        printf("    cset w1, ne\n");
        printf("    cmp x0, #0                   // rhs != 0\n");
        printf("    cset w0, ne\n");
        printf("    and w0, w1, w0               // x0 = lhs && rhs\n");
        return;
    case ND_LOGOR:
        printf("    cmp x1, #0                   // lhs != 0\n");
        printf("    cset w1, ne\n");
        printf("    cmp x0, #0                   // rhs != 0\n");
        printf("    cset w0, ne\n");
        printf("    orr w0, w1, w0               // x0 = lhs || rhs\n");
        return;
    }
}

void gen_expr(node* n)
{
    if (!n)
        return;

    switch (n->kind) {
    case ND_NUM:
        emit_imm(n->val);
        return;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_LOGAND:
    case ND_LOGOR:
        gen_binary(n);
        return;
    case ND_NOT:
        gen_expr(n->lhs);
        printf("    cmp x0, #0                   // logical not\n");
        printf("    cset w0, eq                  // x0 = !x0\n");
        return;

    default:
        return;
    }
}

void gen_stmt(node* n)
{
    if (!n)
        return;

    switch (n->kind) {
    case ND_NULL:
        return;

    case ND_BLOCK:
        for (node* stmt = n->body; stmt; stmt = stmt->next)
            gen_stmt(stmt);
        return;

    case ND_RETURN:
        gen_expr(n->lhs);
        emit_return();
        return;

    case ND_EXPR_STMT:
        gen_expr(n->lhs);
        return;

    case ND_IF:
        return;

    case ND_FOR:
        return;

    default:
        gen_expr(n);
        return;
    }
}

void gen_function(function* fn)
{
    int return_label = new_label();
    current_return_label = return_label;
    emit_func_start(fn->name);
    gen_stmt(fn->body);
    emit_func_end(return_label);
    current_return_label = -1;
}

void gen_program(function* prog)
{
    emit_text();

    for (function* fn = prog; fn; fn = fn->next)
        gen_function(fn);
}

function* external(void)
{
    if (consume("typedef")) {
        declaration_rest(1);
        return NULL;
    }

    type_spec();

    if (consume(";"))
        return NULL;

    token* name = declarator();

    if (consume("(")) {
        param_list();
        if (consume(";"))
            return NULL;
        node* body = compound();
        return new_function(name, body);
    }

    if (consume("="))
        initializer();

    while (consume(",")) {
        declarator();
        if (consume("="))
            initializer();
    }

    expect(";");
    return NULL;
}

function* program(void)
{
    function head;
    function* tail = &head;
    head.next = NULL;

    while (cur->ty != T_EOF) {
        if (equal("typedef") || is_type_start()) {
            function* fn = external();
            if (fn) {
                tail->next = fn;
                tail = tail->next;
            }
            continue;
        }

        statement();
    }

    return head.next;
}

int main(void)
{
    char buf[65536];

    add_builtin_type("char");
    add_builtin_type("int");
    add_builtin_type("long");
    add_builtin_type("void");
    add_builtin_type("size_t");
    current_return_label = -1;

    size_t n = fread(buf, 1, sizeof(buf) - 1, stdin);

    if (ferror(stdin)) {
        perror("fread");
        return 1;
    }

    buf[n] = 0;
    lex(buf);
    function* prog = program();
    gen_program(prog);

    return 0;
}
