//
// Created by ubuntu on 23-4-10.
//
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "RTOS_BE.h"
char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int get_bits(int num, int start_bit, int len){
    return (num >> start_bit) & ((1 << len) - 1);
}

void test_main(int seed){
    int result = seed % 0x1000000;
    char out[4] = {alphabet[get_bits(result, 0, 6)],
                   alphabet[get_bits(result, 6, 6)],
                   alphabet[get_bits(result, 12, 6)],
                   alphabet[get_bits(result, 18, 6)]};
    char in[5] = {0};
    printf("THIS IS A TEST\n");
    scanf("%4s", in);
    int x = 10;
    if(!memcmp(in, out, 4))
        in[x] = 'X';
}