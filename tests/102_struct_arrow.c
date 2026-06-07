// EXPECT: 9
struct box {
    int value;
};

int main(void)
{
    struct box b;
    struct box* p = &b;
    p->value = 9;
    return b.value;
}
