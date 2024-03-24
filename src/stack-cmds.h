#ifndef STACK_CMDS_H
#define STACK_CMDS_H
// ****************************************************************************
//  stack-cmds.h                                                  DB48X project
// ****************************************************************************
//
//   File Description:
//
//     RPL Stack commands
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

#include "command.h"
#include "dmcp.h"

COMMAND(Dup)
// ----------------------------------------------------------------------------
//   Implement the RPL "dup" command, duplicate top of stack
// ----------------------------------------------------------------------------
{
    if (object_g top = rt.top())
        if (rt.push(top))
            return OK;
    return ERROR;
}


COMMAND(Dup2)
// ----------------------------------------------------------------------------
//   Implement the RPL "dup2" command, duplicate two elements at top of stack
// ----------------------------------------------------------------------------
{
    if (object_g y = rt.stack(1))
        if (object_g x = rt.stack(0))
            if (rt.push(y))
                if (rt.push(x))
                    return OK;
    return ERROR;
}


COMMAND(DupN)
// ----------------------------------------------------------------------------
//   Implement the RPL "dupN" command, duplicate two elements at top of stack
// ----------------------------------------------------------------------------
{
    uint32_t depth = uint32_arg();
    if (!rt.error() && rt.pop())
    {
        for (uint i = 0; i < depth; i++)
            if (object_p obj = rt.stack(depth-1))
                if (!rt.push(obj))
                    return ERROR;
        return OK;
    }
    return ERROR;
}


COMMAND(Drop)
// ----------------------------------------------------------------------------
//   Implement the RPL "drop" command, remove top of stack
// ----------------------------------------------------------------------------
{
    if (rt.drop())
        return OK;
    return ERROR;
}


COMMAND(Drop2)
// ----------------------------------------------------------------------------
//   Implement the Drop2 command, remove two elements from the stack
// ----------------------------------------------------------------------------
{
    if (rt.drop(2))
        return OK;
    return ERROR;
}


COMMAND(DropN)
// ----------------------------------------------------------------------------
//   Implement the DropN command, remove N elements from the stack
// ----------------------------------------------------------------------------
{
    uint32_t depth = uint32_arg();
    if (!rt.error())
        if (rt.pop())
            if (rt.drop(depth))
                return OK;
    return ERROR;
}


COMMAND(Over)
// ----------------------------------------------------------------------------
//   Implement the Over command, getting object from level 2
// ----------------------------------------------------------------------------
{
    if (object_p o = rt.stack(1))
        if (rt.push(o))
            return OK;
    return ERROR;
}


COMMAND(Pick)
// ----------------------------------------------------------------------------
//   Implement the Pick command, getting from level N
// ----------------------------------------------------------------------------
{
    uint32_t depth = uint32_arg();
    if (!rt.error())
        if (object_p obj = rt.stack(depth))
            if (rt.top(obj))
                return OK;
    return ERROR;
}


COMMAND(Roll)
// ----------------------------------------------------------------------------
//   Implement the Roll command, moving objects from high stack level down
// ----------------------------------------------------------------------------
{
    uint32_t depth = uint32_arg();
    if (!rt.error())
        if (rt.pop())
            if (rt.roll(depth))
                return OK;
    return ERROR;
}


COMMAND(RollD)
// ----------------------------------------------------------------------------
//   Implement the RollD command, moving objects from first level up
// ----------------------------------------------------------------------------
{
    uint32_t depth = uint32_arg();
    if (!rt.error())
        if (rt.pop())
            if (rt.rolld(depth))
                return OK;
    return ERROR;
}


COMMAND(Rot)
// ----------------------------------------------------------------------------
//   Implement the RollD command, moving objects from first level up
// ----------------------------------------------------------------------------
{
    if (rt.roll(3))
        return OK;
    return ERROR;
}


COMMAND(Swap)
// ----------------------------------------------------------------------------
//   Implement the RPL "swap" command, swap the two top elements
// ----------------------------------------------------------------------------
{
    object_p x = rt.stack(0);
    object_p y = rt.stack(1);
    if (x && y)
    {
        rt.stack(0, y);
        rt.stack(1, x);
        return OK;
    }
    return ERROR;
}


COMMAND(Nip)
// ----------------------------------------------------------------------------
//   Implement the RPL "nip" command, remove level 2 of the stack
// ----------------------------------------------------------------------------
{
    if (!rt.args(2))
        return ERROR;
    if (object_p x = rt.stack(0))
        if (rt.stack(1, x))
            if (rt.drop())
                return OK;
    return ERROR;
}


COMMAND(Depth)
// ----------------------------------------------------------------------------
//   Return the depth of the stack
// ----------------------------------------------------------------------------
{
    uint ticks = rt.depth();
    if (integer_p ti = rt.make<integer>(ID_integer, ticks))
        if (rt.push(ti))
            return OK;
    return ERROR;
}


COMMAND(ClearStack)
// ----------------------------------------------------------------------------
//   Clear the stack
// ----------------------------------------------------------------------------
{
    return rt.drop(rt.depth()) ? OK : ERROR;
}

#endif // STACK_CMDS_H
