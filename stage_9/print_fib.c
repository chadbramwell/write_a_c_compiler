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
        putchar(48 + x/10);
        x = x - (x/10) * 10;
    }
    putchar(48 + x);
    return 0;
}

int main() {
    int fibsize = 8;
    for(int i = 0; i <= fibsize; i=i+1)
    {
        int f = fib(i);
        putchar(70);
        putchar(105);
        putchar(98);
        putchar(40);
        print_num(i);
        putchar(41);
        putchar(58);
        putchar(32);
        print_num(f);
        putchar(10);
    }
    putchar(10);
    return fib(fibsize);
}