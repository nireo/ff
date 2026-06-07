#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Shared syntax model
 */

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
    ND_STR,
    ND_MEMBER,
};

enum {
    TY_CHAR,
    TY_INT,
    TY_LONG,
    TY_VOID,
    TY_PTR,
    TY_ARRAY,
    TY_STRUCT,
};

typedef struct token token;
struct token {
    char* pt;
    int len;
    int ty;
    int val;
    token* next;
};

void err(char* s);
void err_int(int n);
void err_token(token* tok);

typedef struct member member;
typedef struct type type;
struct type {
    int kind;
    int size;
    int array_len;
    type* base;
    member* members;
};

struct member {
    char* name;
    type* ty;
    int offset;
    member* next;
};

typedef struct obj obj;
struct obj {
    char* name;
    type* ty;
    int is_local;
    int offset;
    obj* next;
    obj* param_next;
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
    int str_label;
    token* str_tok;
    member* mem;
};

typedef struct function function;
struct function {
    token* name;
    node* body;
    obj* params;
    obj* locals;
    int stack_size;
    function* next;
};

typedef struct name name;
struct name {
    char* pt;
    int len;
    int val;
    type* ty;
    name* next;
};

typedef struct string_lit string_lit;
struct string_lit {
    int label;
    token* tok;
    string_lit* next;
};

/*
 * Shared utilities
 */

int startswith(char* p, char* s)
{
    return strncmp(p, s, strlen(s)) == 0;
}

/*
 * Lexer
 */

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

    return strchr("+-*/%=;(){}[],.&!<>?:", *p) != NULL;
}

void lex(char* p)
{
    token head;
    token* tail = &head;
    head.next = NULL;

    while (*p) {
        if (is_space(*p)) {
            p = p + 1;
        } else if (*p == '#') {
            while (*p && *p != '\n')
                p = p + 1;
        } else if (startswith(p, "//")) {
            p = p + 2;
            while (*p && *p != '\n')
                p = p + 1;
        } else if (startswith(p, "/*")) {
            p = p + 2;
            while (*p && !startswith(p, "*/"))
                p = p + 1;
            if (*p)
                p = p + 2;
        } else if (is_digit(*p)) {
            char* start = p;
            int val = 0;

            while (is_digit(*p)) {
                val = val * 10 + (*p - '0');
                p = p + 1;
            }

            token* t = new_token(T_NUM, start, p - start);
            t->val = val;
            tail->next = t;
            tail = t;
        } else if (*p == '"') {
            char* start = p;
            p = p + 1;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1])
                    p = p + 1;
                p = p + 1;
            }
            if (*p)
                p = p + 1;

            tail->next = new_token(T_STR, start, p - start);
            tail = tail->next;
        } else if (*p == '\'') {
            char* start = p;
            p = p + 1;
            while (*p && *p != '\'') {
                if (*p == '\\' && p[1])
                    p = p + 1;
                p = p + 1;
            }
            if (*p)
                p = p + 1;

            tail->next = new_token(T_CHAR, start, p - start);
            tail = tail->next;
        } else if (is_alpha(*p)) {
            char* start = p;

            while (is_ident2(*p))
                p = p + 1;

            token* t = new_token(T_IDENT, start, p - start);
            tail->next = t;
            tail = t;
        } else {
            int len = punct_len(p);
            if (len) {
                token* t = new_token(T_PUNCT, p, len);
                tail->next = t;
                tail = t;
                p = p + len;
            } else {
                err("unknown char: ");
                fputc(*p, stderr);
                err("\n");
                exit(1);
            }
        }
    }

    tail->next = new_token(T_EOF, p, 0);
    tokens = head.next;
    cur = tokens;
}

/*
 * Parser
 */

name* type_names;
name* struct_tags;
name* enum_consts;
obj* globals;
obj* funcs;
obj* locals;
int stack_offset;
string_lit* strings;
int string_labelseq;
type* ty_char;
type* ty_int;
type* ty_long;
type* ty_void;

int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

type* new_type(int kind, int size)
{
    type* ty = malloc(sizeof(type));
    memset(ty, 0, sizeof(type));
    ty->kind = kind;
    ty->size = size;
    return ty;
}

type* pointer_to(type* base)
{
    type* ty = new_type(TY_PTR, 8);
    ty->base = base;
    return ty;
}

type* array_of(type* base, int len)
{
    type* ty = new_type(TY_ARRAY, base->size * len);
    ty->base = base;
    ty->array_len = len;
    return ty;
}

int is_ptrlike(type* ty)
{
    return ty && (ty->kind == TY_PTR || ty->kind == TY_ARRAY);
}

type* ptr_base(type* ty)
{
    if (ty && ty->base)
        return ty->base;
    return ty_char;
}

int same(token* tok, char* s);
void error_at(token* tok, char* msg);

name* find_name(name* list, token* tok)
{
    name* n = list;

    while (n) {
        if (tok->len == n->len && strncmp(tok->pt, n->pt, tok->len) == 0)
            return n;
        n = n->next;
    }

    return NULL;
}

member* find_member(type* ty, token* tok)
{
    if (!ty || ty->kind != TY_STRUCT)
        error_at(tok, "not a struct");

    member* mem = ty->members;
    while (mem) {
        if (same(tok, mem->name))
            return mem;
        mem = mem->next;
    }

    error_at(tok, "no such member");
    return NULL;
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
            line = line + 1;
        p = p + 1;
    }

    err(msg);
    err(" at line ");
    err_int(line);
    err(" near '");
    err_token(tok);
    err("'\n");
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

void out(char* s)
{
    fputs(s, stdout);
}

void out_long(long n)
{
    if (n < 0) {
        putchar('-');
        n = 0 - n;
    }

    if (n >= 10)
        out_long(n / 10);

    putchar(n % 10 + '0');
}

void out_int(int n)
{
    out_long(n);
}

void out_token(token* tok)
{
    int i = 0;
    while (i < tok->len) {
        putchar(tok->pt[i]);
        i = i + 1;
    }
}

void err(char* s)
{
    fputs(s, stderr);
}

void err_int(int n)
{
    if (n < 0) {
        fputc('-', stderr);
        n = 0 - n;
    }

    if (n >= 10)
        err_int(n / 10);

    fputc(n % 10 + '0', stderr);
}

void err_token(token* tok)
{
    int i = 0;
    while (i < tok->len) {
        fputc(tok->pt[i], stderr);
        i = i + 1;
    }
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
    n->ty = ty_int;
    if ((kind == ND_ADD || kind == ND_SUB) && is_ptrlike(lhs->ty))
        n->ty = lhs->ty;
    if (kind == ND_ADD && is_ptrlike(rhs->ty))
        n->ty = rhs->ty;
    if (kind == ND_ASSIGN)
        n->ty = lhs->ty;
    return n;
}

node* new_unary(int kind, node* lhs)
{
    node* n = new_node(kind);
    n->lhs = lhs;
    n->ty = ty_int;
    if (lhs)
        n->ty = lhs->ty;
    if (kind == ND_ADDR)
        n->ty = pointer_to(lhs->ty);
    if (kind == ND_DEREF)
        n->ty = ptr_base(lhs->ty);
    return n;
}

node* new_num(long val)
{
    node* n = new_node(ND_NUM);
    n->val = val;
    n->ty = ty_int;
    return n;
}

node* new_string(token* tok)
{
    string_lit* s = malloc(sizeof(string_lit));
    s->label = string_labelseq;
    string_labelseq = string_labelseq + 1;
    s->tok = tok;
    s->next = strings;
    strings = s;

    node* n = new_node(ND_STR);
    n->ty = pointer_to(ty_char);
    n->str_label = s->label;
    n->str_tok = tok;
    return n;
}

int decode_escape(char c)
{
    if (c == 'n')
        return '\n';
    if (c == 't')
        return '\t';
    if (c == 'r')
        return '\r';
    if (c == '0')
        return 0;
    return c;
}

int char_val(token* tok)
{
    char* p = tok->pt + 1;

    if (*p == '\\')
        return decode_escape(p[1]);

    return *p;
}

node* new_member(node* lhs, token* name)
{
    node* n = new_unary(ND_MEMBER, lhs);
    n->mem = find_member(lhs->ty, name);
    n->ty = n->mem->ty;
    return n;
}

node* new_var(token* tok)
{
    node* n = new_node(ND_VAR);
    char* name = token_to_str(tok);

    for (obj* var = locals; var; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            n->var = var;
            n->ty = var->ty;
            return n;
        }
    }

    for (obj* var = globals; var; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            n->var = var;
            n->ty = var->ty;
            return n;
        }
    }

    obj* var = malloc(sizeof(obj));
    memset(var, 0, sizeof(obj));
    var->name = name;
    var->ty = ty_long;
    n->var = var;
    n->ty = var->ty;
    return n;
}

obj* new_gvar(token* tok, type* ty)
{
    obj* var = malloc(sizeof(obj));
    memset(var, 0, sizeof(obj));
    var->name = token_to_str(tok);
    var->ty = ty;
    var->next = globals;
    globals = var;
    return var;
}

obj* new_func_symbol(token* tok, type* ty)
{
    obj* fn = malloc(sizeof(obj));
    memset(fn, 0, sizeof(obj));
    fn->name = token_to_str(tok);
    fn->ty = ty;
    fn->next = funcs;
    funcs = fn;
    return fn;
}

type* find_func_type(char* name)
{
    for (obj* fn = funcs; fn; fn = fn->next) {
        if (strcmp(fn->name, name) == 0)
            return fn->ty;
    }

    return ty_long;
}

type* find_func_type_token(token* tok)
{
    char* name = token_to_str(tok);
    return find_func_type(name);
}

obj* new_lvar(token* tok, type* ty)
{
    obj* var = malloc(sizeof(obj));
    memset(var, 0, sizeof(obj));
    var->name = token_to_str(tok);
    var->ty = ty;
    var->is_local = 1;
    stack_offset = align_to(stack_offset, 8);
    stack_offset = stack_offset + ty->size;
    var->offset = stack_offset;
    var->next = locals;
    locals = var;
    return var;
}

node* new_null(void)
{
    return new_node(ND_NULL);
}

function* new_function(token* name, obj* params, node* body)
{
    function* fn = malloc(sizeof(function));
    memset(fn, 0, sizeof(function));
    fn->name = name;
    fn->body = body;
    fn->params = params;
    fn->locals = locals;
    fn->stack_size = (stack_offset + 15) / 16 * 16;
    return fn;
}

void add_type_name(token* tok, type* ty)
{
    name* n = malloc(sizeof(name));
    n->pt = tok->pt;
    n->len = tok->len;
    n->val = 0;
    n->ty = ty;
    n->next = type_names;
    type_names = n;
}

void add_struct_tag(token* tok, type* ty)
{
    name* n = find_name(struct_tags, tok);
    if (!n) {
        n = malloc(sizeof(name));
        n->pt = tok->pt;
        n->len = tok->len;
        n->val = 0;
        n->next = struct_tags;
        struct_tags = n;
    }
    n->ty = ty;
}

void add_enum_const(token* tok, int val)
{
    name* n = malloc(sizeof(name));
    n->pt = tok->pt;
    n->len = tok->len;
    n->val = val;
    n->ty = ty_int;
    n->next = enum_consts;
    enum_consts = n;
}

name* find_enum_const(token* tok)
{
    return find_name(enum_consts, tok);
}

void add_builtin_type(char* s, type* ty)
{
    token tok;
    tok.pt = s;
    tok.len = strlen(s);
    add_type_name(&tok, ty);
}

int is_type_name(token* tok)
{
    return find_name(type_names, tok) != NULL;
}

int is_type_start(void)
{
    return cur->ty == T_IDENT && (is_type_name(cur) || equal("struct") || equal("enum"));
}

node* expr(void);
node* declaration_rest(int is_typedef, int make_lvars);
node* statement(void);
type* declarator(type* ty, token** name);

void enum_spec(void)
{
    expect("enum");

    if (cur->ty == T_IDENT)
        cur = cur->next;

    if (!consume("{"))
        return;

    int val = 0;
    while (!consume("}")) {
        token* name = expect_ident();
        if (consume("=")) {
            if (cur->ty == T_NUM)
                val = cur->val;
            expr();
        }
        add_enum_const(name, val);
        val = val + 1;
        consume(",");
    }
}

type* type_spec(void)
{
    if (consume("struct")) {
        token* tag = NULL;
        type* ty = NULL;

        if (cur->ty == T_IDENT) {
            tag = cur;
            name* n = find_name(struct_tags, tag);
            if (n)
                ty = n->ty;
            cur = cur->next;
        }

        if (!ty)
            ty = new_type(TY_STRUCT, 0);

        if (tag)
            add_struct_tag(tag, ty);

        if (consume("{")) {
            member head;
            member* tail = &head;
            int offset = 0;
            head.next = NULL;

            while (!consume("}")) {
                type* base_ty = type_spec();
                token* name;
                type* mem_ty = declarator(base_ty, &name);

                int align = mem_ty->size;
                if (align > 8)
                    align = 8;
                offset = align_to(offset, align);
                member* mem = malloc(sizeof(member));
                memset(mem, 0, sizeof(member));
                mem->name = token_to_str(name);
                mem->ty = mem_ty;
                mem->offset = offset;
                tail->next = mem;
                tail = mem;
                offset = offset + mem_ty->size;

                while (consume(",")) {
                    mem_ty = declarator(base_ty, &name);
                    align = mem_ty->size;
                    if (align > 8)
                        align = 8;
                    offset = align_to(offset, align);
                    mem = malloc(sizeof(member));
                    memset(mem, 0, sizeof(member));
                    mem->name = token_to_str(name);
                    mem->ty = mem_ty;
                    mem->offset = offset;
                    tail->next = mem;
                    tail = mem;
                    offset = offset + mem_ty->size;
                }

                expect(";");
            }

            ty->members = head.next;
            ty->size = align_to(offset, 8);
        }

        return ty;
    }

    if (equal("enum")) {
        enum_spec();
        return ty_int;
    }

    if (cur->ty == T_IDENT && is_type_name(cur)) {
        type* ty = find_name(type_names, cur)->ty;
        cur = cur->next;
        return ty;
    }

    error_at(cur, "expected type");
    return ty_int;
}

type* declarator(type* ty, token** name)
{
    while (consume("*"))
        ty = pointer_to(ty);

    *name = expect_ident();

    while (consume("[")) {
        int len = 0;
        if (!consume("]")) {
            if (cur->ty == T_NUM)
                len = cur->val;
            expr();
            expect("]");
        }
        ty = array_of(ty, len);
    }

    return ty;
}

node* initializer(void)
{
    if (consume("{")) {
        if (!consume("}")) {
            initializer();
            while (consume(",")) {
                if (consume("}"))
                    return new_null();
                initializer();
            }
            expect("}");
        }
        return new_null();
    }

    return expr();
}

node* declaration_rest(int is_typedef, int make_lvars)
{
    node head;
    node* tail = &head;
    head.next = NULL;

    type* base_ty = type_spec();

    if (consume(";"))
        return new_null();

    token* decl_name;
    type* ty = declarator(base_ty, &decl_name);
    if (is_typedef)
        add_type_name(decl_name, ty);
    obj* var = NULL;
    if (make_lvars)
        var = new_lvar(decl_name, ty);

    if (consume("=")) {
        node* init = initializer();
        if (var && init->kind != ND_NULL) {
            node* lhs = new_node(ND_VAR);
            lhs->var = var;
            lhs->ty = var->ty;
            tail->next = new_unary(ND_EXPR_STMT, new_binary(ND_ASSIGN, lhs, init));
            tail = tail->next;
        }
    }

    while (consume(",")) {
        ty = declarator(base_ty, &decl_name);
        if (is_typedef)
            add_type_name(decl_name, ty);
        var = NULL;
        if (make_lvars)
            var = new_lvar(decl_name, ty);
        if (consume("=")) {
            node* init = initializer();
            if (var && init->kind != ND_NULL) {
                node* lhs = new_node(ND_VAR);
                lhs->var = var;
                lhs->ty = var->ty;
                tail->next = new_unary(ND_EXPR_STMT, new_binary(ND_ASSIGN, lhs, init));
                tail = tail->next;
            }
        }
    }

    expect(";");
    if (!head.next)
        return new_null();

    node* n = new_node(ND_BLOCK);
    n->body = head.next;
    return n;
}

obj* param_list(void)
{
    obj head;
    obj* tail = &head;
    head.next = NULL;
    head.param_next = NULL;

    if (consume(")"))
        return NULL;

    if (equal("void") && same(cur->next, ")")) {
        cur = cur->next;
        expect(")");
        return NULL;
    }

    for (;;) {
        type* base_ty = type_spec();
        if (!equal(",") && !equal(")")) {
            token* name;
            type* ty = declarator(base_ty, &name);
            tail->param_next = new_lvar(name, ty);
            tail = tail->param_next;
        }

        if (consume(")"))
            return head.param_next;
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
            tail->next = declaration_rest(1, 0);
            tail = tail->next;
        } else if (is_type_start()) {
            tail->next = declaration_rest(0, 1);
            tail = tail->next;
        } else {
            tail->next = statement();
            tail = tail->next;
        }
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
            n->init = declaration_rest(0, 1);
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

    if (equal("switch") || equal("case") || equal("default") || equal("break") || equal("continue"))
        error_at(cur, "unsupported statement");

    if (consume(";"))
        return new_null();

    node* n = new_unary(ND_EXPR_STMT, expr());
    expect(";");
    return n;
}

node* primary(void)
{
    if (cur->ty == T_NUM) {
        node* n = new_num(cur->val);
        cur = cur->next;
        return n;
    }

    if (cur->ty == T_CHAR) {
        node* n = new_num(char_val(cur));
        cur = cur->next;
        return n;
    }

    if (cur->ty == T_STR) {
        node* n = new_string(cur);
        cur = cur->next;
        return n;
    }

    if (cur->ty == T_IDENT) {
        if (same(cur, "NULL")) {
            cur = cur->next;
            return new_num(0);
        }

        name* ec = find_enum_const(cur);
        if (ec) {
            cur = cur->next;
            return new_num(ec->val);
        }

        node* n = new_var(cur);
        cur = cur->next;
        return n;
    }

    if (consume("(")) {
        if (is_type_start()) {
            type* ty = type_spec();
            while (consume("*"))
                ty = pointer_to(ty);
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
    int done = 0;

    while (!done) {
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
            call->ty = ty_long;
            if (n->kind == ND_VAR) {
                call->funcname = n->var->name;
                call->ty = find_func_type(n->var->name);
            }
            n = call;
        } else if (consume("[")) {
            n = new_binary(ND_ADD, n, expr());
            expect("]");
            n = new_unary(ND_DEREF, n);
        } else if (consume(".")) {
            n = new_member(n, expect_ident());
        } else if (consume("->")) {
            n = new_unary(ND_DEREF, n);
            n = new_member(n, expect_ident());
        } else {
            done = 1;
        }
    }

    return n;
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

    if (consume("sizeof")) {
        if (consume("(")) {
            type* ty = NULL;
            if (is_type_start()) {
                ty = type_spec();
                while (consume("*"))
                    ty = pointer_to(ty);
            } else {
                node* n = expr();
                expect(")");
                return new_num(n->ty->size);
            }
            expect(")");
            return new_num(ty->size);
        }
        node* n = unary();
        return new_num(n->ty->size);
    }

    return postfix();
}

node* mul(void)
{
    node* n = unary();

    for (;;) {
        if (consume("*")) {
            n = new_binary(ND_MUL, n, unary());
        } else if (consume("/")) {
            n = new_binary(ND_DIV, n, unary());
        } else if (consume("%")) {
            n = new_binary(ND_MOD, n, unary());
        } else {
            return n;
        }
    }
}

node* add(void)
{
    node* n = mul();

    for (;;) {
        if (consume("+")) {
            n = new_binary(ND_ADD, n, mul());
        } else if (consume("-")) {
            n = new_binary(ND_SUB, n, mul());
        } else {
            return n;
        }
    }
}

node* relational(void)
{
    node* n = add();

    for (;;) {
        if (consume("<")) {
            n = new_binary(ND_LT, n, add());
        } else if (consume(">")) {
            n = new_binary(ND_LT, add(), n);
        } else if (consume("<=")) {
            n = new_binary(ND_LE, n, add());
        } else if (consume(">=")) {
            n = new_binary(ND_LE, add(), n);
        } else {
            return n;
        }
    }
}

node* equality(void)
{
    node* n = relational();

    for (;;) {
        if (consume("==")) {
            n = new_binary(ND_EQ, n, relational());
        } else if (consume("!=")) {
            n = new_binary(ND_NE, n, relational());
        } else {
            return n;
        }
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

    if (consume("="))
        n = new_binary(ND_ASSIGN, n, assign());

    return n;
}

node* expr(void)
{
    return assign();
}

function* external(void)
{
    if (consume("typedef")) {
        declaration_rest(1, 0);
        return NULL;
    }

    type* base_ty = type_spec();

    if (consume(";"))
        return NULL;

    token* name;
    type* ty = declarator(base_ty, &name);

    if (consume("(")) {
        new_func_symbol(name, ty);
        locals = NULL;
        stack_offset = 0;
        obj* params = param_list();
        if (consume(";"))
            return NULL;
        node* body = compound();
        return new_function(name, params, body);
    }

    if (consume("="))
        initializer();
    new_gvar(name, ty);

    while (consume(",")) {
        token* gname;
        type* gty = declarator(base_ty, &gname);
        if (consume("="))
            initializer();
        new_gvar(gname, gty);
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
        } else {
            statement();
        }
    }

    return head.next;
}

/*
 * Code generation
 */

int labelseq;
int current_return_label = -1;

void gen_expr(node* n);

void print_sym(token* tok)
{
    out("_");
    out_token(tok);
}

void print_name(char* name)
{
    if (strcmp(name, "stdin") == 0) {
        out("___stdinp");
        return;
    }

    if (strcmp(name, "stdout") == 0) {
        out("___stdoutp");
        return;
    }

    if (strcmp(name, "stderr") == 0) {
        out("___stderrp");
        return;
    }

    out("_");
    out(name);
}

void emit_text(void)
{
    out(".text\n");
}

void emit_data(void)
{
    out(".data\n");
}

int is_std_stream(char* name)
{
    return strcmp(name, "stdin") == 0 || strcmp(name, "stdout") == 0 || strcmp(name, "stderr") == 0;
}

void emit_global(token* name)
{
    out(".globl ");
    print_sym(name);
    out("\n");
}

void emit_label(token* name)
{
    print_sym(name);
    out(":\n");
}

int new_label(void)
{
    int label = labelseq;
    labelseq = labelseq + 1;
    return label;
}

void mov_imm(char* reg, int val)
{
    int lo = val % 65536;
    int hi = val / 65536;
    out("    movz ");
    out(reg);
    out(", #");
    out_int(lo);
    out("\n");
    if (val >= 65536) {
        out("    movk ");
        out(reg);
        out(", #");
        out_int(hi);
        out(", lsl #16\n");
    }
}

void emit_func_start(token* name, int stack_size, obj* params)
{
    emit_global(name);
    emit_label(name);
    out("    stp x29, x30, [sp, #-16]!    // save frame pointer and link register\n");
    out("    mov x29, sp                  // establish stack frame\n");
    if (stack_size) {
        mov_imm("x10", stack_size);
        out("    sub sp, sp, x10              // allocate locals\n");
    }
    int i = 0;
    for (obj* var = params; var; var = var->param_next) {
        if (i >= 8) {
            err("too many parameters\n");
            exit(1);
        }
        mov_imm("x10", var->offset);
        out("    sub x9, x29, x10             // address of parameter ");
        out(var->name);
        out("\n");
        if (var->ty->size == 1) {
            out("    strb w");
            out_int(i);
            out(", [x9]               // save char parameter\n");
        } else if (var->ty->size == 4) {
            out("    str w");
            out_int(i);
            out(", [x9]                // save int parameter\n");
        } else {
            out("    str x");
            out_int(i);
            out(", [x9]                // save parameter\n");
        }
        i = i + 1;
    }
    out("    mov w0, #0                   // default return value\n");
}

void emit_return(void)
{
    if (current_return_label < 0)
        return;

    out("    b .L.return.");
    out_int(current_return_label);
    out("               // return\n");
}

void emit_imm(long val)
{
    out("    mov x0, #");
    out_long(val);
    out("                 // load immediate\n");
}

void emit_func_end(int label)
{
    out(".L.return.");
    out_int(label);
    out(":\n");
    out("    mov sp, x29                  // release locals\n");
    out("    ldp x29, x30, [sp], #16      // restore frame pointer and link register\n");
    out("    ret                          // return to caller\n");
}

void push(void)
{
    out("    str x0, [sp, #-16]!          // push x0\n");
}

void pop(char* reg)
{
    out("    ldr ");
    out(reg);
    out(", [sp], #16            // pop into ");
    out(reg);
    out("\n");
}

void load(type* ty)
{
    if (ty->kind == TY_ARRAY)
        return;
    if (ty->size == 1) {
        out("    ldrsb x0, [x0]               // load char\n");
        return;
    }
    if (ty->size == 4) {
        out("    ldrsw x0, [x0]               // load int\n");
        return;
    }
    out("    ldr x0, [x0]                 // load value\n");
}

void store(type* ty)
{
    if (ty->size == 1) {
        out("    strb w0, [x1]                // store char\n");
        return;
    }
    if (ty->size == 4) {
        out("    str w0, [x1]                 // store int\n");
        return;
    }
    out("    str x0, [x1]                 // store value\n");
}

void gen_binary(node* n)
{
    gen_expr(n->lhs);
    push();
    gen_expr(n->rhs);
    pop("x1");

    if (n->kind == ND_ADD) {
        if (is_ptrlike(n->lhs->ty) && ptr_base(n->lhs->ty)->size != 1) {
            out("    mov x2, #");
            out_int(ptr_base(n->lhs->ty)->size);
            out("                  // pointer scale\n");
            out("    mul x0, x0, x2\n");
        }
        if (is_ptrlike(n->rhs->ty) && ptr_base(n->rhs->ty)->size != 1) {
            out("    mov x2, #");
            out_int(ptr_base(n->rhs->ty)->size);
            out("                  // pointer scale\n");
            out("    mul x1, x1, x2\n");
        }
        out("    add x0, x1, x0               // x0 = lhs + rhs\n");
        return;
    }

    if (n->kind == ND_SUB) {
        if (is_ptrlike(n->lhs->ty) && ptr_base(n->lhs->ty)->size != 1) {
            out("    mov x2, #");
            out_int(ptr_base(n->lhs->ty)->size);
            out("                  // pointer scale\n");
            out("    mul x0, x0, x2\n");
        }
        out("    sub x0, x1, x0               // x0 = lhs - rhs\n");
        return;
    }

    if (n->kind == ND_MUL) {
        out("    mul x0, x1, x0               // x0 = lhs * rhs\n");
        return;
    }

    if (n->kind == ND_DIV) {
        out("    sdiv x0, x1, x0              // x0 = lhs / rhs\n");
        return;
    }

    if (n->kind == ND_MOD) {
        out("    sdiv x2, x1, x0              // x2 = lhs / rhs\n");
        out("    msub x0, x2, x0, x1          // x0 = lhs % rhs\n");
        return;
    }

    if (n->kind == ND_EQ) {
        out("    cmp x1, x0                   // compare lhs == rhs\n");
        out("    cset w0, eq                  // x0 = comparison result\n");
        return;
    }

    if (n->kind == ND_NE) {
        out("    cmp x1, x0                   // compare lhs != rhs\n");
        out("    cset w0, ne                  // x0 = comparison result\n");
        return;
    }

    if (n->kind == ND_LT) {
        out("    cmp x1, x0                   // compare lhs < rhs\n");
        out("    cset w0, lt                  // x0 = comparison result\n");
        return;
    }

    if (n->kind == ND_LE) {
        out("    cmp x1, x0                   // compare lhs <= rhs\n");
        out("    cset w0, le                  // x0 = comparison result\n");
        return;
    }

    if (n->kind == ND_LOGAND) {
        out("    cmp x1, #0                   // lhs != 0\n");
        out("    cset w1, ne\n");
        out("    cmp x0, #0                   // rhs != 0\n");
        out("    cset w0, ne\n");
        out("    and w0, w1, w0               // x0 = lhs && rhs\n");
        return;
    }

    if (n->kind == ND_LOGOR) {
        out("    cmp x1, #0                   // lhs != 0\n");
        out("    cset w1, ne\n");
        out("    cmp x0, #0                   // rhs != 0\n");
        out("    cset w0, ne\n");
        out("    orr w0, w1, w0               // x0 = lhs || rhs\n");
        return;
    }
}

void gen_funcall(node* n)
{
    int nargs = 0;

    for (node* arg = n->args; arg; arg = arg->next) {
        if (nargs >= 8) {
            err("too many arguments\n");
            exit(1);
        }
        gen_expr(arg);
        push();
        nargs = nargs + 1;
    }

    for (int i = nargs - 1; i >= 0; i = i - 1) {
        char reg[3];
        reg[0] = 'x';
        reg[1] = '0' + i;
        reg[2] = 0;
        pop(reg);
    }

    out("    bl _");
    out(n->funcname);
    out("                       // call function\n");
}

void gen_addr(node* n)
{
    if (n->kind == ND_VAR) {
        if (n->var->is_local) {
            mov_imm("x10", n->var->offset);
            out("    sub x0, x29, x10             // address of ");
            out(n->var->name);
            out("\n");
            return;
        }
        if (is_std_stream(n->var->name)) {
            out("    adrp x0, ");
            print_name(n->var->name);
            out("@GOTPAGE          // address of external global ");
            out(n->var->name);
            out("\n");
            out("    ldr x0, [x0, ");
            print_name(n->var->name);
            out("@GOTPAGEOFF]\n");
            return;
        }
        out("    adrp x0, ");
        print_name(n->var->name);
        out("@PAGE              // address of global ");
        out(n->var->name);
        out("\n");
        out("    add x0, x0, ");
        print_name(n->var->name);
        out("@PAGEOFF\n");
        return;
    }

    if (n->kind == ND_DEREF) {
        gen_expr(n->lhs);
        return;
    }

    if (n->kind == ND_MEMBER) {
        gen_addr(n->lhs);
        if (n->mem->offset) {
            mov_imm("x10", n->mem->offset);
            out("    add x0, x0, x10             // address of member ");
            out(n->mem->name);
            out("\n");
        }
        return;
    }

    err("not an lvalue\n");
    exit(1);
}

void gen_expr(node* n)
{
    if (!n)
        return;

    if (n->kind == ND_NUM) {
        emit_imm(n->val);
        return;
    }

    if (n->kind == ND_STR) {
        out("    adrp x0, .L.str.");
        out_int(n->str_label);
        out("@PAGE      // address of string literal\n");
        out("    add x0, x0, .L.str.");
        out_int(n->str_label);
        out("@PAGEOFF\n");
        return;
    }

    if (n->kind == ND_VAR) {
        gen_addr(n);
        if (n->ty->kind != TY_ARRAY)
            load(n->ty);
        return;
    }

    if (n->kind == ND_MEMBER) {
        gen_addr(n);
        load(n->ty);
        return;
    }

    if (n->kind == ND_ASSIGN) {
        gen_addr(n->lhs);
        push();
        gen_expr(n->rhs);
        pop("x1");
        store(n->lhs->ty);
        return;
    }

    if (n->kind == ND_ADDR) {
        gen_addr(n->lhs);
        return;
    }

    if (n->kind == ND_DEREF) {
        gen_expr(n->lhs);
        load(n->ty);
        return;
    }

    if (n->kind == ND_FUNCALL) {
        gen_funcall(n);
        return;
    }

    if (n->kind == ND_ADD || n->kind == ND_SUB || n->kind == ND_MUL || n->kind == ND_DIV || n->kind == ND_MOD || n->kind == ND_EQ || n->kind == ND_NE || n->kind == ND_LT || n->kind == ND_LE || n->kind == ND_LOGAND || n->kind == ND_LOGOR) {
        gen_binary(n);
        return;
    }

    if (n->kind == ND_NOT) {
        gen_expr(n->lhs);
        out("    cmp x0, #0                   // logical not\n");
        out("    cset w0, eq                  // x0 = !x0\n");
        return;
    }
}

void gen_stmt(node* n)
{
    if (!n)
        return;

    if (n->kind == ND_NULL)
        return;

    if (n->kind == ND_BLOCK) {
        for (node* stmt = n->body; stmt; stmt = stmt->next)
            gen_stmt(stmt);
        return;
    }

    if (n->kind == ND_RETURN) {
        gen_expr(n->lhs);
        emit_return();
        return;
    }

    if (n->kind == ND_EXPR_STMT) {
        gen_expr(n->lhs);
        return;
    }

    if (n->kind == ND_IF) {
        int label = new_label();
        gen_expr(n->cond);
        out("    cmp x0, #0                   // if condition\n");
        if (n->els) {
            out("    beq .L.else.");
            out_int(label);
            out("\n");
            gen_stmt(n->then);
            out("    b .L.end.");
            out_int(label);
            out("\n");
            out(".L.else.");
            out_int(label);
            out(":\n");
            gen_stmt(n->els);
            out(".L.end.");
            out_int(label);
            out(":\n");
            return;
        }
        out("    beq .L.end.");
        out_int(label);
        out("\n");
        gen_stmt(n->then);
        out(".L.end.");
        out_int(label);
        out(":\n");
        return;
    }

    if (n->kind == ND_FOR) {
        int label = new_label();
        gen_stmt(n->init);
        out(".L.begin.");
        out_int(label);
        out(":\n");
        if (n->cond) {
            gen_expr(n->cond);
            out("    cmp x0, #0                   // loop condition\n");
            out("    beq .L.end.");
            out_int(label);
            out("\n");
        }
        gen_stmt(n->then);
        gen_expr(n->inc);
        out("    b .L.begin.");
        out_int(label);
        out("\n");
        out(".L.end.");
        out_int(label);
        out(":\n");
        return;
    }

    gen_expr(n);
}

void gen_function(function* fn)
{
    int return_label = new_label();
    current_return_label = return_label;
    emit_func_start(fn->name, fn->stack_size, fn->params);
    gen_stmt(fn->body);
    emit_func_end(return_label);
    current_return_label = -1;
}

void gen_program(function* prog)
{
    emit_data();

    for (string_lit* s = strings; s; s = s->next) {
        out(".L.str.");
        out_int(s->label);
        out(":\n");
        out("    .asciz ");
        out_token(s->tok);
        out("\n");
    }

    for (obj* var = globals; var; var = var->next) {
        out(".globl ");
        print_name(var->name);
        out("\n");
        print_name(var->name);
        out(":\n");
        out("    .zero ");
        out_int(var->ty->size);
        out("\n");
    }

    emit_text();

    for (function* fn = prog; fn; fn = fn->next)
        gen_function(fn);
}

int main(void)
{
    char buf[65536];

    ty_char = new_type(TY_CHAR, 1);
    ty_int = new_type(TY_INT, 4);
    ty_long = new_type(TY_LONG, 8);
    ty_void = new_type(TY_VOID, 1);

    add_builtin_type("char", ty_char);
    add_builtin_type("int", ty_int);
    add_builtin_type("long", ty_long);
    add_builtin_type("void", ty_void);
    add_builtin_type("size_t", ty_long);

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
