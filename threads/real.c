#define F (1<<14)

int int_to_float(int n){
    return n*F;
}

int float_to_int_zero(int x){
    return x/F;
}

int float_to_int_near(int x){
    if (x>=0){
        return (x+F/2)/F;
    }
    else{
        return (x-F/2)/F;
    }
}

int add_float(int x, int y){
    return x+y;
}

int sub_float(int x, int y){
    return x-y;
}

int add_comb(int x, int n){
    return x+n*F;
}

int sub_comb(int x, int n){
    return x-n*F;
}

int mul_float(int x, int y){
    return ((int64_t)x)*y/F;
}

int mul_comb(int x, int n){
    return x*n;
}

int div_float(int x, int y){
    return ((int64_t)x)*F/y;
}

int div_comb(int x, int n){
    return x/n;
}