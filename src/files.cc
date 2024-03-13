// ****************************************************************************
//  files.cc                                                      DB48X project
// ****************************************************************************
//
//   File Description:
//
//
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

#include "files.h"

#include "array.h"
#include "dmcp.h"
#include "file.h"
#include "list.h"
#include "program.h"
#include "runtime.h"
#include "settings.h"


// ============================================================================
//
//   Storing an object to disk
//
// ============================================================================

bool files::store(text_p name, object_p value, cstring defext) const
// ----------------------------------------------------------------------------
//   Check if the name ends in 'B' for binary, otherwise use source format
// ----------------------------------------------------------------------------
{
    size_t len  = 0;
    utf8   path = name->value(&len);
    if (!len || !path)
    {
        rt.invalid_file_name_error();
        return false;
    }

    // Select format based on extension
    size_t extpos = len;
    while (extpos > 0 && path[extpos-1] != '.')
        extpos--;
    if (!extpos)
    {
        text_g nmg = text_g(name)
            + text_g(text::make(".")) + text_g(text::make(defext));
        return store(nmg, value, "48s");
    }

    // Check how to save
    cstring ext = cstring(path + extpos);

    // Save as binary?
    if (strncasecmp(ext, "48b", 3) == 0)
        return store_binary(name, value);

    // Save as text?
    if (strncasecmp(ext, "txt", 3) == 0)
    {
        if (text_p txt = value->as<text>())
            return store_text(name, txt);
        return store_text(name, value->as_text(true, false));
    }

    // Save as comma-separated value
    if (strncasecmp(ext, "csv", 3) == 0)
    {
        id ty = value->type();
        if (ty == ID_array || ty == ID_list)
            return store_list(name, list_p(value));
    }

    // Default to saving as source
    return store_source(name, value);
}


// The checksum we use for all commands
static uint32_t computed_checksum = 0;
static byte     file_magic[]      = FILE_MAGIC;


static uint32_t id_checksum()
// ----------------------------------------------------------------------------
//   A checksum of all ID names, used to identify changes in binary format
// ----------------------------------------------------------------------------
{
    if (computed_checksum == 0)
    {
        for (uint i = 0; i < object::NUM_IDS; i++)
            for (utf8 p = object::fancy(object::id(i)); *p; p++)
                computed_checksum = 0x1081 * computed_checksum ^ *p;
    }
    return computed_checksum;
}


bool files::store_binary(text_p name, object_p value) const
// ----------------------------------------------------------------------------
//  Store object in binary format
// ----------------------------------------------------------------------------
{
    if (value)
    {
        file f(filename(name, true), true);
        if (f.valid())
        {
            uint32_t checksum = id_checksum();
            if (f.write(cstring(file_magic), sizeof(file_magic))    &&
                f.write(cstring(&checksum), sizeof(checksum))       &&
                f.write(cstring(value), value->size()))
                return true;
        }
        rt.error(f.error());
    }
    return false;
}


bool files::store_source(text_p name, object_p value) const
// ----------------------------------------------------------------------------
//   Store object in source format
// ----------------------------------------------------------------------------
{
    if (value)
    {
        file f(filename(name, true), true);
        if (f.valid())
        {
            renderer r(f);
            value->render(r);
            return true;
        }
        rt.error(f.error());
    }
    return false;
}


bool files::store_text(text_p name, text_p value) const
// ----------------------------------------------------------------------------
//   Stores a text value directly
// ----------------------------------------------------------------------------
{
    if (value)
    {
        file f(filename(name, true), true);
        if (f.valid())
        {
            size_t len = 0;
            utf8   txt    = value->value(&len);
            if (f.write(cstring(txt), len))
                return true;
        }
        rt.error(f.error());
    }
    return false;
}


bool files::store_list(text_p name, list_p value) const
// ----------------------------------------------------------------------------
//   Store a list or array in CSV format, using ';' as separator
// ----------------------------------------------------------------------------
{
    if (value)
    {
        file f(filename(name, true), true);
        if (f.valid())
        {
            renderer r(f);
            bool ok = true;
            for (object_p row : *value)
            {
                id     type = row->type();
                list_p li   = nullptr;
                if (type == ID_list || type == ID_array)
                    li = list_p(row);;
                if (li)
                {
                    bool first = true;
                    for (object_p col : *li)
                    {
                        if (!first)
                            ok = f.write(";", 1);
                        if (ok)
                            col->render(r);
                        first = false;
                        if (!ok)
                            break;
                    }
                    if (ok)
                        ok = f.write("\n", 1);
                }
                else
                {
                    row->render(r);
                    if (ok)
                        ok = f.write("\n", 1);
                }
                if (!ok)
                    break;
            }
            if (ok)
                return true;
        }
        rt.error(f.error());
    }
    return false;

}


object_p files::recall(text_p name, cstring defext) const
// ----------------------------------------------------------------------------
//    Recall an object from disk
// ----------------------------------------------------------------------------
{
    size_t len  = 0;
    utf8   path = name->value(&len);
    if (!len || !path)
    {
        rt.invalid_file_name_error();
        return nullptr;
    }

    // Select format based on extension
    size_t extpos = len;
    while (extpos > 0 && path[extpos-1] != '.')
        extpos--;
    if (!extpos)
    {
        text_g nmg = text_g(name)
            + text_g(text::make(".")) + text_g(text::make(defext));
        return recall(nmg, "48s");
    }

    // Check the format of the file
    cstring ext = cstring(path + extpos);

    // Load from binary?
    if (strncasecmp(ext, "48b", 3) == 0)
        return recall_binary(name);

    // Load from text?
    if (strncasecmp(ext, "txt", 3) == 0)
        return recall_text(name);

    // Load from comma-separated value
    if (strncasecmp(ext, "csv", 3) == 0)
        return recall_list(name, true);

    // Default to loading from source
    return recall_source(name);
}


object_p files::recall_binary(text_p name) const
// ----------------------------------------------------------------------------
//   Recall an object from binary file
// ----------------------------------------------------------------------------
{
    if (rt.allocated())
    {
        rt.unable_to_allocate_error();
        return nullptr;
    }

    file f(filename(name), false);
    if (f.valid())
    {
        scribble scr;
        uint32_t checksum = id_checksum();
        char     buf[sizeof(file_magic)];
        uint32_t check = 0;
        char     c;
        size_t   sz;
        object_p result;
        if (!f.read(buf, sizeof(file_magic)))
            goto err;
        if (memcmp(buf, file_magic, sizeof(file_magic)) != 0)
        {
            rt.invalid_magic_number_error();
            return nullptr;
        }
        if (!f.read((char *) &check, sizeof(checksum)))
            goto err;
        if (check != checksum)
        {
            rt.incompatible_binary_error();
            return nullptr;
        }

        while (true)
        {
            if (!f.read(&c, 1))
                break;
            byte *ptr = rt.allocate(1);
            *ptr = c;
        }

        sz = rt.allocated();
        result = rt.temporary();
        if (result->type() >= NUM_IDS || result->size() != sz)
        {
            rt.invalid_object_in_file_error();
            return nullptr;
        }

        return result;

    err:
        rt.error(f.error());
    }
    return nullptr;
}


object_p files::recall_source(text_p name) const
// ----------------------------------------------------------------------------
//   Recall an object from source file
// ----------------------------------------------------------------------------
{
    file prog(filename(name), false);
    if (!prog.valid())
    {
        rt.error(prog.error());
        return nullptr;
    }

    // Loop on the input file and process it as if it was being typed
    uint bytes = 0;
    rt.clear();

    for (unicode c = prog.get(); c; c = prog.get())
    {
        byte buffer[4];
        size_t count = utf8_encode(c, buffer);
        rt.insert(bytes, buffer, count);
        bytes += count;
    }

    // End of file: execute the command we typed
    size_t edlen = rt.editing();
    if (edlen)
    {
        text_g edstr = rt.close_editor(true);
        if (edstr)
        {
            gcutf8 editor = edstr->value();
            bool dc = Settings.DecimalComma();
            Settings.DecimalComma(false);
            object_p obj = object::parse(editor, edlen);
            Settings.DecimalComma(dc);
            return obj;
        }
    }

    rt.invalid_object_error();
    return nullptr;
}


text_p files::recall_text(text_p name) const
// ----------------------------------------------------------------------------
//   Recall text from a text file
// ----------------------------------------------------------------------------
{
    file f(filename(name), false);
    if (!f.valid())
    {
        rt.error(f.error());
        return nullptr;
    }

    // Loop on the input file and process it as if it was being typed
    size_t bytes = 0;
    rt.clear();
    for (unicode c = f.get(); c; c = f.get())
    {
        byte buffer[4];
        size_t count = utf8_encode(c, buffer);
        rt.insert(bytes, buffer, count);
        bytes += count;
    }

    // End of file: execute the command we typed
    return rt.close_editor(true, false);
}


list_p files::recall_list(text_p name, bool as_array) const
// ----------------------------------------------------------------------------
//  Recall list from a CSV file
// ----------------------------------------------------------------------------
{
    file f(filename(name), false);
    if (!f.valid())
    {
        if (!rt.error())
            rt.error(f.error());
        return nullptr;
    }

    id       ty     = as_array ? ID_array : ID_list;
    list_g   result = list::make(ty, nullptr, 0);
    object_g item   = nullptr;
    list_g   row    = nullptr;
    int      cols   = 0;
    int      kcols  = -1;
    bool     intxt  = false;
    bool     ineqn  = false;
    uint     paren  = 0;
    uint     brack  = 0;
    uint     curly  = 0;
    uint     nonsp  = 0;

    // Loop on the input file and process it as if it was being typed
    size_t   bytes  = 0;
    rt.clear();
    for (unicode c = f.get(); c; c = f.get())
    {
        switch(c)
        {
        case '(':       paren++; break;
        case ')':       paren--; break;
        case '[':       brack++; break;
        case ']':       brack--; break;
        case '{':       curly++; break;
        case '}':       curly--; break;
        case '"':       intxt = !intxt; break;
        case '\'':      ineqn = !ineqn; break;
        }
        bool sepok = !paren && !brack && !curly && !intxt && !ineqn;

        if (!isspace(c))
            nonsp++;
        if (sepok && (c == ',' || c == ';' || c == '\n'))
        {
            text_p parsed = rt.close_editor(true);
            size_t len    = 0;
            utf8   txt    = parsed->value(&len);
            if (nonsp)
                item = object::parse(txt, len);
            else
                item = +symbol::make("");;
            nonsp = 0;
            if (!item)
                break;
            byte_p b  = byte_p(+item);
            size_t sz = item->size();
            list_g li = rt.make<list>(ty, b, sz);

            if (row || c == ';' || c == ',')
            {
                row = row ? row + li : li;
                if (c == ';' || c == ',')
                    cols++;
            }
            if (c == '\n')
            {
                // Check if we have a rectangular input
                if (kcols < 0)
                    kcols = cols;
                if (cols != kcols)
                {
                    if (ty != ID_list)
                    {
                        ty = ID_list;
                        list_g copy = list::make(ty, nullptr, 0);
                        for (object_p obj : *result)
                        {
                            list_g ci;
                            id     oty = obj->type();
                            bool   isl = oty == ID_list || oty == ID_array;
                            list_p li  = isl ? list_p(obj) : nullptr;
                            if (li)
                            {
                                b = byte_p(li->objects(&sz));
                                ci = list::make(b, sz);
                                obj = ci;
                            }
                            b = byte_p(obj);
                            ci = list::make(b, obj->size());
                            copy = copy + ci;
                        }
                        if (row)
                        {
                            b = byte_p(row->objects(&sz));
                            row = list::make(b, sz);
                        }
                        result = copy;
                    }
                }
                if (row)
                {
                    b = byte_p(+row);
                    sz = row->size();
                    li = rt.make<list>(ty, b, sz);
                }
                result = result + li;
                row = nullptr;
                cols = 0;
            }
            rt.clear();
            bytes = 0;
        }
        else
        {
            byte buffer[4];
            size_t count = utf8_encode(c, buffer);
            rt.insert(bytes, buffer, count);
            bytes += count;
        }
    }
    if (row)
        result = result + row;

    return result;
}


bool files::purge(text_p name) const
// ----------------------------------------------------------------------------
//   Purge a file (unlink it)
// ----------------------------------------------------------------------------
{
    text_p path = filename(name);
    return file::unlink(path);
}


static inline size_t find_colon(utf8 txt, size_t len)
// ----------------------------------------------------------------------------
//   Check if there is a colon after letters/digits, e.g. A: or SDCARD:
// ----------------------------------------------------------------------------
{
    utf8 start = txt;
    while (len--)
    {
        char c = *txt++;
        if (c == ':')
            return txt - start;
        if (!isalnum(c))
            break;
    }
    return 0;
}


static inline bool is_path_separator(char c)
// ----------------------------------------------------------------------------
//   Check path separator
// ----------------------------------------------------------------------------
{
    return c == '/' || c == '\\';
}


text_p files::filename(text_p fname, bool writing) const
// ----------------------------------------------------------------------------
//   Build a filename from given input
// ----------------------------------------------------------------------------
// In order to work in the simulator case, absolute paths are treated as
// being relative to current working directory.
// We also accept "pools", like C: or SD:, which are turned to directories
{
    text_g path = this;
    text_g name = fname;

    // If name is an absolute path, use it directly
    size_t len  = 0;
    utf8   txt  = name->value(&len);

    // Check if we have C: or SDCARD:, if so, turn it to a base path
    bool   in_pool = false;
    if (size_t colon = find_colon(txt, len))
    {
        if (colon + 1 < len)
        {
            path = text::make(txt, colon - 1);
            txt += colon;
            len -= colon;
            in_pool = true;
            name = text::make(txt, len);
            txt = name->value(&len);
        }
    }

    // Check if we have an absolute path
    bool absolute = len > 0 && is_path_separator(*txt);
    if (absolute)
    {
        txt++;
        len--;

        // Turn CONFIG:/CONSTANTS.CSV into CONFIG/CONSTANTS.CSV
        if (in_pool)
            absolute = false;
        name = text::make(txt, len);
    }

    // Check if length of this one is zero or if it's just '/'
    size_t plen = 0;
    utf8   ptxt = path->value(&plen);
    if (plen == 0 || (plen == 1 && is_path_separator(*ptxt)))
        absolute = true;

    // Build the path if necessary
    if (!absolute)
    {
        text_g sep = text::make("/", 1);
        name = path + sep + name;
    }

    // Make sure that we do not evade the sandbox
    uint depth = 0;
    char last = 0;
    txt = name->value(&len);
    for (utf8 p = txt; len--; p++)
    {
        byte c = *p;
        if (c == '.')
        {
            if (last == '.')
            {
                if (!depth)
                {
                    rt.invalid_path_error();
                    return nullptr;
                }
                depth--;
            }
        }
        if (is_path_separator(c))
        {
            if (last != '.' && !is_path_separator(last))
            {
                depth++;

                // If we want to write, make sure all directories exist
                if (writing)
                {
                    // Hack: overwrite '/' with 0 to null-terminate string
                    byte *term = (byte *) p;
                    save<byte> ssep(*term, 0);
                    check_create_dir(cstring(txt));
                }
            }
        }
        last = c;
    }

    return name;
}


#if 0
// mimeType = "image/bmp";

unsigned char file[14] = {
    'B','M', // magic
    0,0,0,0, // size in bytes
    0,0, // app data
    0,0, // app data
    40+14,0,0,0 // start of data offset
};
unsigned char info[40] = {
    40,0,0,0, // info hd size
    0,0,0,0, // width
    0,0,0,0, // heigth
    1,0, // number color planes
    24,0, // bits per pixel
    0,0,0,0, // compression is none
    0,0,0,0, // image bits size
    0x13,0x0B,0,0, // horz resoluition in pixel / m
    0x13,0x0B,0,0, // vert resolutions (0x03C3 = 96 dpi, 0x0B13 = 72 dpi)
    0,0,0,0, // #colors in pallete
    0,0,0,0, // #important colors
    };

int w=waterfallWidth;
int h=waterfallHeight;

int padSize  = (4-(w*3)%4)%4;
int sizeData = w*h*3 + h*padSize;
int sizeAll  = sizeData + sizeof(file) + sizeof(info);

file[ 2] = (unsigned char)( sizeAll    );
file[ 3] = (unsigned char)( sizeAll>> 8);
file[ 4] = (unsigned char)( sizeAll>>16);
file[ 5] = (unsigned char)( sizeAll>>24);

info[ 4] = (unsigned char)( w   );
info[ 5] = (unsigned char)( w>> 8);
info[ 6] = (unsigned char)( w>>16);
info[ 7] = (unsigned char)( w>>24);

info[ 8] = (unsigned char)( h    );
info[ 9] = (unsigned char)( h>> 8);
info[10] = (unsigned char)( h>>16);
info[11] = (unsigned char)( h>>24);

info[20] = (unsigned char)( sizeData    );
info[21] = (unsigned char)( sizeData>> 8);
info[22] = (unsigned char)( sizeData>>16);
info[23] = (unsigned char)( sizeData>>24);

stream.write( (char*)file, sizeof(file) );
stream.write( (char*)info, sizeof(info) );

unsigned char pad[3] = {0,0,0};

for ( int y=0; y<h; y++ )
{
    for ( int x=0; x<w; x++ )
    {
        long red = lround( 255.0 * waterfall[x][y] );
        if ( red < 0 ) red=0;
        if ( red > 255 ) red=255;
        long green = red;
        long blue = red;

        unsigned char pixel[3];
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;

        stream.write( (char*)pixel, 3 );
    }
    stream.write( (char*)pad, padSize );
}
#endif
