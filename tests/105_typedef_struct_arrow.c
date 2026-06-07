// EXPECT: 6
typedef struct item item;

struct item {
    int value;
};

int main(void)
{
    item x;
    item* p = &x;
    p->value = 6;
    return p->value;
}
