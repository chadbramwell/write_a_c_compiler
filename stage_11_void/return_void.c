int global;

void set_g()
{
    global = 1337;
    return;
}

void double_g()
{
    global = global * global;
}

int main()
{
    set_g();
    double_g();
    return global;
}