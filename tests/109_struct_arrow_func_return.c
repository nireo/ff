// EXPECT: 13
typedef struct item item;

struct item {
    int value;
};

item* id(item* p)
{
    return p;
}

int main(void)
{
    item x;
    id(&x)->value = 13;
    return x.value;
}
