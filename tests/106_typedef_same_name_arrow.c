// EXPECT: 4
typedef struct type type;

struct type {
    int size;
};

int main(void)
{
    type x;
    type* ty = &x;
    ty->size = 4;
    return ty->size;
}
