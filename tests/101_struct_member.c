// EXPECT: 7
struct pair {
    int x;
    int y;
};

int main(void)
{
    struct pair p;
    p.x = 3;
    p.y = 4;
    return p.x + p.y;
}
