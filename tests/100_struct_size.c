// EXPECT: 16
struct pair {
    int x;
    int y;
    long z;
};

int main(void)
{
    return sizeof(struct pair);
}
