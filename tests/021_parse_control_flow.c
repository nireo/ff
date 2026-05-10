// EXPECT: 12
int main(void)
{
    if (1)
        return 99;

    while (0)
        return 88;

    for (int i = 0; i < 3; i = i + 1)
        return 77;

    return 12;
}
