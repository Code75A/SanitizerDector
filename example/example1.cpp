//https://godbolt.org/z/YTG7q7vhx

int a, c;
short b;
long d;
int m[10];
int mark;
int main() { 
    mark=m[0-1];
    a = (short)(d == c | b > 9) / 0; 
    return a;
}

//y/x or y%x => x!=0

//array_len=max_absdatetype
//a[|x|-1] => 0<=|x|-1<max_absdatetype 
//|x|-1<max_absdatetype is true
//a[x] => 1<=|x| => x!=0