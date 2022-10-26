#ifndef INPUT_H
#define INPUT_H
// ****************************************************************************
//  input.h                                                       DB48X project
// ****************************************************************************
//
//   File Description:
//
//    Calculator input
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

#include "graphics.h"
#include "object.h"
#include "types.h"
#include "dmcp.h"

#include <string>
#include <vector>

struct runtime;

struct input
// ----------------------------------------------------------------------------
//    Calculator input state
// ----------------------------------------------------------------------------
{
    input();

    enum modes
    // ------------------------------------------------------------------------
    //   Current input mode
    // ------------------------------------------------------------------------
    {
        STACK,                  // Showing the stack, not editing
        DIRECT,                 // Keys like 'sin' evaluate directly
        TEXT,                   // Alphanumeric entry, e.g. in strings
        PROGRAM,                // Keys like 'sin' show as 'sin' in the editor
        ALGEBRAIC,              // Keys like 'sin' show as 'sin()'
        MATRIX,                 // Matrix/vector mode
    };

    enum
    // ------------------------------------------------------------------------
    //   Dimensioning constants
    // ------------------------------------------------------------------------
    {
        NUM_PLANES      = 3,    // NONE, Shift and "extended" shift
        NUM_KEYS        = 46,   // Including SCREENSHOT, SH_UP and SH_DN
        NUM_SOFTKEYS    = 6,    // Number of softkeys
        NUM_LABEL_CHARS = 12,   // Number of characters per menu label
        NUM_MENUS = NUM_PLANES * NUM_SOFTKEYS,
    };


    bool        key(int key);
    bool        repeating()     { return repeat; }
    void        assign(int key, uint plane, object_p code);
    object_p    assigned(int key, uint plane);
    void        menus(cstring labels[NUM_MENUS], object_p function[NUM_MENUS]);
    void        menu(uint index, cstring labels, object_p function);
    void        draw_menus();
    void        draw_annunciators();
    void        draw_editor();
    int         draw_cursor();
    void        draw_error();
    bool        draw_help();
    void        draw_command();
    int         stack_screen_bottom()   { return stack; }
    bool        showingHelp()           { return help + 1 != 0; }

    uint        shift_plane()   { return xshift ? 2 : shift ? 1 : 0; }
    void        clear_help();
    object_p    object_for_key(int key);
    bool        end_edit();
    void        clear_editor();
    void        load_help(utf8 topic);

protected:
    bool        handle_shifts(int key);
    bool        handle_help(int &key);
    bool        handle_editing(int key);
    bool        handle_alpha(int key);
    bool        handle_functions(int key);
    bool        handle_digits(int key);

    bool        handle_enter(int key);
    bool        handle_backspace(int key);


protected:
    typedef graphics::coord     coord;
    typedef graphics::size      size;

protected:
    utf8     command;       // Command being executed
    uint     help;          // Offset of help being displayed in help file
    uint     line;          // Line offset in the help display
    uint     topic;         // Offset of topic being highlighted
    uint     history;       // History depth
    uint     topics[8];     // Topics history
    uint     cursor;        // Cursor position in buffer
    coord    xoffset;       // Offset of the cursor
    modes    mode;          // Current editing mode
    int      last;          // Last key
    int      stack;         // Vertical bottom of the stack
    coord    cx, cy;        // Cursor position on screen
    utf8code cchar;         // Character under the cursor
    bool     shift     : 1; // Normal shift active
    bool     xshift    : 1; // Extended shift active (simulate Right)
    bool     alpha     : 1; // Alpha mode active
    bool     lowercase : 1; // Lowercase
    bool     hideMenu  : 1; // Hide the menu
    bool     down      : 1; // Move one line down
    bool     up        : 1; // Move one line up
    bool     repeat    : 1; // Repeat the key
    bool     longpress : 1; // We had a long press of the key
    bool     blink     : 1; // Cursor blink indicator
    bool     follow    : 1; // Follow a help topic

protected:
    // Key mappings
    object_p function[NUM_PLANES][NUM_KEYS];
    char     menu_label[NUM_PLANES][NUM_SOFTKEYS][NUM_LABEL_CHARS];
    static runtime &RT;
    friend struct tests;
    friend struct runtime;

protected:
    struct file
    // ------------------------------------------------------------------------
    //   Direct access to the help file
    // ------------------------------------------------------------------------
    {
        file();
        ~file();

        void     open(cstring path);
        bool     valid();
        void     close();
        utf8code get();
        utf8code get(uint offset);
        void     seek(uint offset);
        utf8code peek();
        uint     position();
        uint     find(utf8code cp);
        uint     rfind(utf8code cp);

    protected:
#ifdef SIMULATOR
        FILE    *data;
#else
        FIL      data;
#endif
    } helpfile;
};


enum { TIMER0, TIMER1, TIMER2, TIMER3 };

extern input Input;


#endif // INPUT_H
