// EXPECT: 21
int main(void)
{
    return sizeof(char) + sizeof(int) + sizeof(long) + sizeof(char*);
}
