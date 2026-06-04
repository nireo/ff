// EXPECT: 21
int double_it(int x)
{
    return x * 2;
}

int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    return add(double_it(8), 5);
}
