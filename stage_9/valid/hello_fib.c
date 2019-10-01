int putchar(int c);

int fib(int x)
{
    if(x == 0 || x == 1)
    {
        return x;
    }
    return fib(x-1) + fib(x-2);
}
void print_num(int x)
{
    while(x / 10)
    {
        putchar('0' + x/10);
        x = x - (x/10) * 10;
    }
    putchar('0' + x);
}

// 1 1 2 3 5 8 13
int main() {
    int f = fib(7);
    print_num(f);
    return f;
}