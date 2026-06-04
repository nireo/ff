// EXPECT: 19
int f(int x)
{
    return x + 3;
}

int main(void)
{
    return f(4) * 2 + 5;
}
