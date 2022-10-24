#ifndef RPLSTRING_H
#define RPLSTRING_H
// ****************************************************************************
//  rplstring.h                                                  DB48X project
// ****************************************************************************
//
//   File Description:
//
//      The RPL string object type
//
//      Operations on rplstring values
//
//
//
//
//
//
// ****************************************************************************
//   (C) 2022 Christophe de Dinechin <christophe@dinechin.org>
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

#include "object.h"
#include "runtime.h"

struct string : object
// ----------------------------------------------------------------------------
//    Represent string objects
// ----------------------------------------------------------------------------
{
    string(gcstring source, size_t len, id type = ID_string): object(type)
    {
        cstring s = (cstring) source;
        char *p = (char *) payload();
        p = leb128(p, len);
        while (len--)
            *p++ = *s++;
    }

    static size_t required_memory(id i, gcstring UNUSED str, size_t len)
    {
        return leb128size(i) + leb128size(len) + len;
    }

    static string *make(cstring str, size_t len)
    {
        gcstring gcstr = str;
        return RT.make<string>(gcstr, len);
    }

    static string *make(cstring str)
    {
        return make(str, strlen(str));
    }

    size_t length() const
    {
        byte *p = payload();
        return leb128<size_t>(p);
    }

    utf8 text(size_t *size = nullptr) const
    {
        byte  *p   = payload();
        size_t len = leb128<size_t>(p);
        if (size)
            *size = len;
        return (utf8) p;
    }

    OBJECT_HANDLER(string);
    OBJECT_PARSER(string);
    OBJECT_RENDERER(string);
};

typedef const string *string_p;

#endif // STRING_H
