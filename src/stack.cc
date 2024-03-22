// ****************************************************************************
//  stack.cc                                                      DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Rendering of the objects on the stack
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

#include "stack.h"

#include "blitter.h"
#include "dmcp.h"
#include "grob.h"
#include "renderer.h"
#include "runtime.h"
#include "settings.h"
#include "target.h"
#include "user_interface.h"
#include "utf8.h"


stack    Stack;

using coord = blitter::coord;
using size  = blitter::size;


RECORDER_DECLARE(tests);

stack::stack()
// ----------------------------------------------------------------------------
//   Constructor does nothing at the moment
// ----------------------------------------------------------------------------
#if SIMULATOR
    : history(), writer(0), reader(0)
#endif  // SIMULATOR
{
}


static inline uint countDigits(uint value)
// ----------------------------------------------------------------------------
//   Count how many digits we need to display a value
// ----------------------------------------------------------------------------
{
    uint result = 1;
    while (value /= 10)
        result++;
    return result;
}


void stack::draw_stack()
// ----------------------------------------------------------------------------
//   Draw the stack on screen
// ----------------------------------------------------------------------------
{
    font_p font       = Settings.result_font();
    font_p hdrfont    = HeaderFont;
    font_p idxfont    = HelpFont;
    size   lineHeight = font->height();
    size   idxHeight  = idxfont->height();
    size   idxOffset  = (lineHeight - idxHeight) / 2 - 2;
    coord  top        = hdrfont->height() + 2;
    coord  bottom     = ui.stack_screen_bottom();
    uint   depth      = rt.depth();
    uint   digits     = countDigits(depth);
    coord  hdrx       = idxfont->width('0') * digits + 2;
    size   avail      = LCD_W - hdrx - 5;

    Screen.fill(0, top, LCD_W, bottom, pattern::white);
    if (!depth)
        return;

    rect clip      = Screen.clip();

    Screen.fill(hdrx, top, hdrx, bottom, pattern::gray50);
    if (rt.editing())
    {
        bottom--;
        Screen.fill(0, bottom, LCD_W, bottom, pattern::gray50);
    }

#ifdef SIMULATOR
    extern int last_key;
#endif // SIMULATOR

    char buf[16];
    coord y = bottom;
    for (uint level = 0; level < depth; level++)
    {
        if (coord(y) <= top)
            break;

        grob_g   graph = nullptr;
        object_g obj   = rt.stack(level);
        size     w = 0;
        if (level ? Settings.GraphicStackDisplay()
                  : Settings.GraphicResultDisplay())
        {
            auto    fid = !level ? Settings.ResultFont() : Settings.StackFont();
            grapher g(avail - 2,
                      bottom - top,
                      fid,
                      pattern::black,
                      pattern::white,
                      true);
            do
            {
                graph = obj->graph(g);
            } while (!graph && Settings.AutoScaleStack() && g.reduce_font());

            if (graph)
            {
                size gh = graph->height();
                if (lineHeight < gh)
                    lineHeight = gh;
                w = graph->width();

#ifdef SIMULATOR
                if (level == 0)
                {
                    extern int last_key;
                    bool     ml = (level ? Settings.MultiLineStack()
                                   : Settings.MultiLineResult());
                    renderer r(nullptr, ~0U, true, ml);
                    size_t   len = obj->render(r);
                    utf8     out = r.text();
                    int      key = last_key;
                    output(key, obj->type(), out, len);
                    record(tests,
                           "Key %d X-reg %+s size %u %s",
                           key, object::name(obj->type()), len, out);
                }
#endif // SIMULATOR
            }
        }

        y -= lineHeight;
        coord ytop = y < top ? top : y;
        coord yb   = y + lineHeight-1;
        Screen.clip(0, ytop, LCD_W, yb);

        if (graph)
        {
            surface s = graph->pixels();
            rect r = s.area();
            r.offset(LCD_W - 2 - w, y);
            Screen.copy(s, r);
        }
        else
        {
            // Text rendering
            bool     ml = (level ? Settings.MultiLineStack()
                                 : Settings.MultiLineResult());
            renderer r(nullptr, ~0U, true, ml);
            size_t   len = obj->render(r);
            utf8     out = r.text();
#ifdef SIMULATOR
            if (level == 0)
            {
                extern int last_key;
                int key = last_key;
                output(key, obj->type(), out, len);
                record(tests,
                       "Stack key %d X-reg %+s size %u %s",
                       key, object::name(obj->type()), len, out);
            }
#endif
            w = font->width(out, len);

            if (w >= avail || memchr(out, '\n', len))
            {
                uint availRows = (y + lineHeight - 1 - top) / lineHeight;
                bool dots      = !ml || w >= avail * availRows;

                if (!dots)
                {
                    // Try to split into lines
                    size_t rlen[16];
                    uint   rows = 0;
                    utf8   end  = out + len;
                    utf8   rs   = out;
                    size   rw   = 0;
                    size   rx   = 0;
                    for (utf8 p = out; p < end; p = utf8_next(p))
                    {
                        unicode c = utf8_codepoint(p);
                        bool cr = c == '\n';
                        size cw = cr ? 0 : font->width(c);
                        rw += cw;
                        if (cr || rw >= avail)
                        {
                            if (rows >= availRows)
                            {
                                dots = true;
                                break;
                            }
                            rlen[rows++] = p - rs;
                            rs = p;
                            if (rx < rw - cw)
                                rx = rw - cw;
                            rw = cw;
                        }
                    }
                    if (rx < rw)
                        rx = rw;

                    if (!dots)
                    {
                        if (end > rs)
                            rlen[rows++] = end - rs;
                        y -= (rows - 1) * lineHeight;
                        ytop = y < top ? top : y;
                        Screen.clip(0, ytop, LCD_W, yb);
                        rs = out;
                        for (uint r = 0; r < rows; r++)
                        {
                            Screen.text(LCD_W - 2 - rx,
                                        y + r * lineHeight,
                                        rs, rlen[r], font);
                            rs += rlen[r];
                        }
                    }
                }

                if (dots)
                {
                    unicode sep   = L'…';
                    coord   x     = hdrx + 5;
                    coord   split = 200;
                    coord   skip  = font->width(sep) * 3 / 2;
                    size    offs  = lineHeight / 5;

                    Screen.clip(x, ytop, split, yb);
                    Screen.text(x, y, out, len, font);
                    Screen.clip(split, ytop, split + skip, yb);
                    Screen.glyph(split + skip/8, y - offs, sep, font,
                                 pattern::gray50);
                    Screen.clip(split+skip, y, LCD_W, yb);
                    Screen.text(LCD_W - 2 - w, y, out, len, font);
                }
            }
            else
            {
                Screen.text(LCD_W - 2 - w, y, out, len, font);
            }

            font = Settings.stack_font();
        }

        // Draw index
        Screen.clip(clip);
        snprintf(buf, sizeof(buf), "%u", level + 1);
        size hw = idxfont->width(utf8(buf));
        Screen.text(hdrx - hw, y + idxOffset, utf8(buf), idxfont);

        lineHeight = font->height();
    }
    Screen.clip(clip);
}
