int putchar(int c);
int print_num(int x);

int fib(int x)
{
    if(x == 0 || x == 1)
    {
        return x;
    }
    return fib(x-1) + fib(x-2);
}
int print_num(int x)
{
    while(x / 10)
    {
        putchar('0' + x/10);
        x = x - (x/10) * 10;
    }
    putchar('0' + x);
    return 0;
}

int main() {
    int fibsize = 8;
    for(int i = 0; i <= fibsize; i=i+1)
    {
        int f = fib(i);
        putchar('F');
        putchar('i');
        putchar('b');
        putchar('(');
        print_num(i);
        putchar(')');
        putchar(':');
        putchar(' ');
        print_num(f);
        putchar('\n');
    }
    putchar('\n');
    return fib(fibsize);
}