// EXPECT: 17
int ignored(int x)
{
    return x;
}

int main(void)
{
    // Function calls and comments should compile without affecting the result.
    ignored(0);
    /* Block comments should be skipped by the lexer. */
    return 17;
}
