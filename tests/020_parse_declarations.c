// EXPECT: 5
typedef struct item item;

struct item {
    int value;
    item* next;
};

int global_count;

int main(void)
{
    int x = 1 + 2 * 3;
    char* name = "ff";
    return 5;
}
