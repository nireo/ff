// EXPECT: 5
typedef struct type type;

struct type {
    int size;
};

int main(void)
{
    if (1) {
        type x;
        type* ty = &x;
        if (1)
            ty->size = 5;
        if (ty->size == 0)
            return 1;
        return ty->size;
    }
    return 0;
}
