// EXPECT: 17
int puts(char* s);

int main(void)
{
    // Function calls and strings are parsed, but call codegen is not implemented yet.
    puts("ignored");
    /* Block comments should be skipped by the lexer. */
    return 17;
}
