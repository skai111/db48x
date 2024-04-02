// ****************************************************************************
//  renderer.cc                                                   DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Structure used to render objects
//
//
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

#include "renderer.h"

#include "file.h"
#include "runtime.h"
#include "settings.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <wctype.h>

renderer::~renderer()
// ----------------------------------------------------------------------------
//   When we used the scratchpad, free memory used
// ----------------------------------------------------------------------------
{
    if (!target)
        rt.free(written);
}


bool renderer::put(unicode code)
// ----------------------------------------------------------------------------
//   Render a unicode character
// ----------------------------------------------------------------------------
{
    byte buffer[4];
    size_t rendered = utf8_encode(code, buffer);
    return put(buffer, rendered);
}


bool renderer::put(cstring s)
// ----------------------------------------------------------------------------
//   Put a null-terminated string
// ----------------------------------------------------------------------------
{
    for (char c = *s++; c; c = *s++)
        if (!put(c))
            return false;
    return true;
}


bool renderer::put(cstring s, size_t len)
// ----------------------------------------------------------------------------
//   Put a length-based string
// ----------------------------------------------------------------------------
{
    for (size_t i = 0; i < len; i++)
        if (!put(s[i]))
            return false;
    return true;
}


bool renderer::put(char c)
// ----------------------------------------------------------------------------
//   Write a single character
// ----------------------------------------------------------------------------
{
    if (written >= length)
        return false;

    // Check if this is a space or \n
    bool spc = isspace(c);
    bool cr  = c == '\n';

    // If not inside a text, check whitespace formatting
    if (!txt)
    {
        // Render flat for stack display: collect all spaces in one
        if (stk && !mlstk)
        {
            if (spc)
            {
                if (gotSpace || gotCR)
                    return true;
                c = ' ';
            }
            gotSpace = spc;
        }

        if (spc && !cr && edit)
            if (uint maxcol = Settings.EditorWrapColumn())
                if (column > maxcol)
                    needCR = true;;

        // Check if we need ot emit a CR
        if (needCR)
        {
            needCR = false;
            if (!put('\n'))
                return false;

            // Do not emit a space right after a \n
            if (spc)
                return true;
        }

        // Check if we need to emit a space
        if (needSpace)
        {
            if (spc && !cr)
                return true;

            needSpace = false;
            if (!cr && !put(' '))
                return false;
        }
    }

    // Actually write the target character
    if (saving)
    {
        saving->put(c);
        written++;
    }
    else if (target)
    {
        target[written++] = c;
    }
    else
    {
        byte *p = rt.allocate(1);
        if (!p)
            return false;
        *p = c;
        written++;
    }

    if (cr)
    {
        needCR = false;
        needSpace = false;
        column = 0;
        if (!txt)
        {
            for (uint i = 0; i < tabs; i++)
                if (!put('\t'))
                    return false;
        }
    }
    else
    {
        column++;
    }
    gotCR = cr;
    gotSpace = spc;

    if (c == '"')
        txt = !txt;
    return true;
}


static unicode db48x_to_lower(unicode cp)
// ----------------------------------------------------------------------------
//   Case conversion to lowercase ignoring special DB48X characters
// ----------------------------------------------------------------------------
{
    if (cp == L'Σ' || cp == L'∏' || cp == L'∆')
        return cp;
    return towlower(cp);
}


static unicode db48x_to_upper(unicode cp)
// ----------------------------------------------------------------------------
//   Case conversion to uppercase ignoring special DB48X characters
// ----------------------------------------------------------------------------
{
    if (cp == L'∂' || cp ==  L'ρ' || cp == L'π' ||
        cp == L'μ' || cp == L'θ' || cp == L'ε')
        return cp;
    return towupper(cp);
}


bool renderer::put(object::id format, utf8 text, size_t len)
// ----------------------------------------------------------------------------
//   Render a command with proper capitalization
// ----------------------------------------------------------------------------
{
    bool result = true;

    if (edit)
        if (utf8_codepoint(text) == settings::SPACE_UNIT)
            return put('_');

    switch(format)
    {
    case object::ID_LowerCaseNames:
    case object::ID_LowerCase:
        for (utf8 s = text; size_t(s - text) < len &&  *s; s = utf8_next(s))
            result = put(unicode(db48x_to_lower(utf8_codepoint(s))));
        break;

    case object::ID_UpperCaseNames:
    case object::ID_UpperCase:

        for (utf8 s = text; size_t(s - text) < len && *s; s = utf8_next(s))
            result = put(unicode(db48x_to_upper(utf8_codepoint(s))));
        break;

    case object::ID_CapitalizedNames:
    case object::ID_Capitalized:
        for (utf8 s = text; size_t(s - text) < len &&  *s; s = utf8_next(s))
            result = put(unicode(s == text
                                 ? db48x_to_upper(utf8_codepoint(s))
                                 : utf8_codepoint(s)));
        break;

    default:
    case object::ID_LongFormNames:
    case object::ID_LongForm:
        for (cstring p = cstring(text); size_t(utf8(p) - text) < len && *p; p++)
            result = put(*p);
        break;
    }

    return result;
}


size_t renderer::printf(const char *format, ...)
// ----------------------------------------------------------------------------
//   Write a formatted string
// ----------------------------------------------------------------------------
{
    if (saving)
    {
        va_list va;
        va_start(va, format);
        char buf[80];
        size_t remaining = length - written;
        if (remaining > sizeof(buf))
            remaining = sizeof(buf);
        int size = vsnprintf(buf, remaining, format, va);
        va_end(va);
        if (size > 0)
            if (saving->write(buf, size))
                written += size;
        return size;
    }
    else if (target)
    {
        // Fixed target: write directly there
        if (written >= length)
            return 0;

        va_list va;
        va_start(va, format);
        size_t remaining = length - written;
        size_t size = vsnprintf(target + written, remaining, format, va);
        va_end(va);
        written += size;
        return size;
    }
    else
    {
        // Write in the scratchpad
        char buf[32];
        va_list va;
        va_start(va, format);
        size_t size = vsnprintf(buf, sizeof(buf), format, va);
        va_end(va);

        // Check if we can allocate enough for the output
        byte *p = rt.allocate(size);
        if (!p)
            return 0;

        if (size < sizeof(buf))
        {
            // Common case: it fits in 32-bytes buffer, allocate directly
            memcpy(p, buf, size);
        }
        else
        {
            // Uncommon case: re-run vsnprintf in the allocated buffer
            va_list va;
            va_start(va, format);
            size = vsnprintf((char *) p, size, format, va);
            va_end(va);
        }
        written += size;
        return size;
    }
}


utf8 renderer::text() const
// ----------------------------------------------------------------------------
//   Return the buffer of what was written in the renderer
// ----------------------------------------------------------------------------
{
    if (target)
        return (utf8) target;
    if (saving)
        return nullptr;
#ifdef SIMULATOR
    *rt.scratchpad() = 0;
#endif // SIMULATOR
    return (utf8) rt.scratchpad() - written;
}
