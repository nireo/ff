// EXPECT: 8
typedef struct member member;
typedef struct type type;

struct type {
    int kind;
    int size;
    member* members;
};

struct member {
    char* name;
    type* ty;
    int offset;
    member* next;
};

int main(void)
{
    type t;
    type* ty = &t;
    ty->size = 8;
    return ty->size;
}
