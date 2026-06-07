// EXPECT: 12
typedef struct point point;

struct point {
    int x;
    int y;
};

int main(void)
{
    point p;
    p.x = 5;
    p.y = 7;
    return p.x + p.y;
}
