// EXPECT: 4
int main(void)
{
    int i = 0;
    for (;;) {
        if (i == 4)
            return i;
        i = i + 1;
    }
    return 0;
}
