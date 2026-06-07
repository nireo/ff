// EXPECT: 68
struct mixed {
    char c;
    int x;
    long y;
};

int main(void)
{
    struct mixed m;
    m.c = 'A';
    m.x = 2;
    m.y = 1;
    return m.c + m.x + m.y;
}
