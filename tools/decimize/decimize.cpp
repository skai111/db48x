// ****************************************************************************
//  decimize.cpp                                                  DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Encode a mantissa (from a file) into the binary format
//
//
//
//
//
//
//
//
// ****************************************************************************
//   (C) 2023 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the terms outlined in LICENSE.txt
// ****************************************************************************
//   This file is part of DB48X.
//
//   DB48X is free software: you can redistribute it and/or modify
//   it under the terms outlined in the LICENSE.txt file
//
//   DB48X is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ****************************************************************************

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>


typedef uint16_t      kint;
typedef unsigned char byte;


static void kigit_encode(byte *base, size_t index, kint value)
// ----------------------------------------------------------------------------
//    Write the given kigit (base-1000 digit)
// ----------------------------------------------------------------------------
{
    base += (index * 10) / 8;
    index = (index % 4) * 2 + 2;
    base[0] = (base[0] & (0xFF << (10 - index)))   | (value >> index);
    index = (8 - index) % 8;
    base[1] = (base[1] & ~(0xFF << index)) | byte(value << index);
}


static kint kigit_decode(byte_p base, size_t index)
// ----------------------------------------------------------------------------
//    Find the given kigit (base-1000 digit)
// ----------------------------------------------------------------------------
{
    base += (index * 10) / 8;
    index = (index % 4) * 2 + 2;
    return ((kint(base[0]) << index) | (base[1] >> (8 - index))) & 1023;
}


int main(int argc, char **argv)
// ----------------------------------------------------------------------------
//   Parse the arguments and run the tool
// ----------------------------------------------------------------------------
{
    byte   *buffer = nullptr;
    size_t  bufsz  = 32;
    size_t  digits = 0;
    size_t  kigits = 0;
    size_t  kigc   = 0;
    uint    kigit  = 0;
    bool    done   = false;
    cstring name   = "table";

    if (argc < 2)
    {
        fprintf(stderr, "%s: Missing table name\n", argv[0]);
        exit(1);
    }
    name = argv[1];
    bool debug = argc > 2;

    printf("// Decimal table for:\n");
    printf("// ");

    while (!done)
    {
        int c = getchar();
        if (c == '.' || isspace(c))
            continue;
        done = c < '0' || c > '9';
        if (!done)
        {
            kigit = 10 * kigit + c - '0';
            putchar(c);
            digits++;
        }
        else if (kigc)
        {
            for (size_t k = kigc; k < 3; k++)
                kigit *= 10;
            kigc = 3;
        }

        if (++kigc >= 3)
        {
            size_t bits  = ++kigits * 10;
            size_t bytes = (bits + 7) / 8;
            if (!buffer || bytes >= bufsz)
            {
                size_t oldsz = bufsz;
                bufsz *= 2;
                byte *alloc = (byte *) calloc(bufsz, 1);
                if (!alloc)
                {
                    perror("Out of memory");
                    exit(1);
                }
                if (buffer)
                {
                    memcpy(alloc, buffer, oldsz);
                    free(buffer);
                }
                buffer = alloc;
            }

            if (!done && digits % 60 != 0)
                putchar(' ');
            else
                printf("\n// ");

            kigit_encode(buffer, kigits - 1, kigit);
            kigc = 0;
            kigit = 0;
        }
    }

    size_t bits   = kigits * 10;
    size_t bytes  = (bits + 7) / 8;

    printf("%zu digits, %zu kigits, %zu bytes\n", digits, kigits, bytes);
    printf("\n");
    printf("static const byte %s[%zu] =\n{", name, bytes);
    for (size_t b = 0; b < bytes; b++)
        printf("%s0x%02X%s",
               b % 16 ? " " : "\n    ",
               buffer[b],
               b + 1 < bytes ? "," : "");
    printf("\n};\n\n");

    if (debug)
    {
        printf("// Reconstructed digits: ");
        for (size_t k = 0; k < kigits; k++)
            printf("%03d ", kigit_decode(buffer, k));
        printf("\n");
    }
}
