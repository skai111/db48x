// ****************************************************************************
//  expression.cc                                                   DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of algebraic equations
//
//     Equations are simply programs that are rendered and parsed specially
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

#include "expression.h"

#include "arithmetic.h"
#include "functions.h"
#include "grob.h"
#include "integer.h"
#include "parser.h"
#include "precedence.h"
#include "renderer.h"
#include "unit.h"
#include "utf8.h"

RECORDER(equation, 16, "Processing of equations and algebraic objects");
RECORDER(equation_error, 16, "Errors with equations");


symbol_g *expression::independent = nullptr;
object_g *expression::independent_value = nullptr;
symbol_g *expression::dependent = nullptr;
object_g *expression::dependent_value = nullptr;



// ============================================================================
//
//    Equation
//
// ============================================================================

PARSE_BODY(expression)
// ----------------------------------------------------------------------------
//    Try to parse this as an equation
// ----------------------------------------------------------------------------
{
    // If already parsing an equation, let upper parser deal with quote
    if (p.precedence)
        return SKIP;

    p.precedence = 1;
    auto result = list_parse(ID_expression, p, '\'', '\'');
    p.precedence = 0;

    return result;
}


HELP_BODY(expression)
// ----------------------------------------------------------------------------
//   Help topic for equations
// ----------------------------------------------------------------------------
{
    return utf8("Equations");
}


symbol_p expression::parentheses(symbol_g arg)
// ----------------------------------------------------------------------------
//   Render, putting parentheses around an argument
// ----------------------------------------------------------------------------
{
    symbol_g open = symbol::make('(');
    symbol_g close = symbol::make(')');
    return open + arg + close;
}


symbol_p expression::space(symbol_g arg)
// ----------------------------------------------------------------------------
//   Render, putting parentheses around an argument
// ----------------------------------------------------------------------------
{
    return symbol::make(' ') + arg;
}


symbol_p expression::render(uint depth, int &precedence, bool editing)
// ----------------------------------------------------------------------------
//   Render an object as a symbol at a given precedence
// ----------------------------------------------------------------------------
{
    if (object_g obj = rt.pop())
    {
        int arity = obj->arity();
        switch(arity)
        {
        case 0:
            // Symbols and other non-algebraics, e.g. numbers
            precedence = obj->precedence();
            if (precedence == precedence::NONE)
                precedence = precedence::SYMBOL;
            if (obj->type() == ID_symbol)
                return symbol_p(object_p(obj));
            return obj->as_symbol(editing);

        case 1:
        {
            int      argp = 0;
            id       oid  = obj->type();
            symbol_g fn   = obj->as_symbol(editing);
            symbol_g arg  = render(depth, argp, editing);
            int      maxp =
                oid == ID_neg ? precedence::FUNCTION : precedence::SYMBOL;
            if (argp < maxp)
                arg = parentheses(arg);
            precedence = precedence::FUNCTION;
            switch(oid)
            {
            case ID_sq:
                precedence = precedence::FUNCTION_POWER;
                return arg + symbol::make("²");
            case ID_cubed:
                precedence = precedence::FUNCTION_POWER;
                return arg + symbol::make("³");
            case ID_neg:
                precedence = precedence::ADDITIVE;
                return symbol::make('-') + arg;
            case ID_fact:
                precedence = precedence::SYMBOL;
                return arg + symbol::make("!");
            case ID_inv:
                precedence = precedence::FUNCTION_POWER;
                return arg + symbol::make("⁻¹");
            default:
                break;
            }
            if (argp >= precedence::FUNCTION &&
                argp != precedence::FUNCTION_POWER)
                arg = space(arg);
            return fn + arg;
        }
        break;
        case 2:
        {
            int lprec = 0, rprec = 0;
            symbol_g op = obj->as_symbol(editing);
            symbol_g rtxt = render(depth, rprec, editing);
            symbol_g ltxt = render(depth, lprec, editing);
            int prec = obj->precedence();
            if (prec != precedence::FUNCTION)
            {
                if (lprec < prec)
                    ltxt = parentheses(ltxt);
                if (rprec <= prec)
                    rtxt = parentheses(rtxt);
                precedence = prec;
                return ltxt + op + rtxt;
            }
            else
            {
                symbol_g arg = ltxt + symbol::make(';') + rtxt;
                arg = parentheses(arg);
                precedence = precedence::FUNCTION;
                return op + arg;
            }
        }
        break;
        default:
        {
            symbol_g op = obj->as_symbol(editing);
            symbol_g args = nullptr;
            for (int a = 0; a < arity; a++)
            {
                int prec = 0;
                symbol_g arg = render(depth, prec, editing);
                if (a)
                    args = arg + symbol::make(';') + args;
                else
                    args = arg;
            }
            args = parentheses(args);
            precedence = precedence::FUNCTION;
            return op + args;
        }
        }
    }

    return nullptr;
}


RENDER_BODY(expression)
// ----------------------------------------------------------------------------
//   Render the equation
// ----------------------------------------------------------------------------
{
    return render(o, r, !r.expression());
}


size_t expression::render(const expression *o, renderer &r, bool quoted)
// ----------------------------------------------------------------------------
//  Internal rendering code
// ----------------------------------------------------------------------------
//   1 2 3 5 * + - 2 3 * +
{
    size_t depth   = rt.depth();
    bool   ok      = true;
    bool   funcall = o->type() == ID_funcall;

    // First push all things so that we have the outermost operators first
    for (object_p obj : *o)
    {
        ASSERT(obj);
        ok = rt.push(obj);
        if (!ok)
            break;
    }

    if (!ok)
    {
        // We ran out of memory pushing things
        if (size_t remove = rt.depth() - depth)
            rt.drop(remove);
        return 0;
    }

    int      prec   = 0;
    symbol_g result = render(depth, prec, r.editing());
    if (!result)
        return 0;
    if (quoted)
        r.put('\'');

    size_t len = 0;
    utf8   txt = result->value(&len);
    r.put(txt, len);
    bool more = rt.depth() > depth;
    if (more)
    {
        if (funcall)
        {
            symbol_g args = nullptr;
            symbol_g sep  = symbol::make(";");
            while (more)
            {
                int aprec = 0;
                symbol_g arg = render(depth, aprec, r.editing());
                if (!arg)
                    return 0;
                more = rt.depth() > depth;
                if (args)
                    args = arg + sep + args;
                else
                    args = arg;
            }

            r.put('(');
            txt = args->value(&len);
            r.put(txt, len);
            r.put(')');
        }
        else
        {
            size_t remove = rt.depth() - depth;
            record(equation_error, "Malformed equation, %u removed", remove);
            rt.drop(remove);
        }
    }
    if (quoted)
        r.put('\'');
    return r.size();
}


object_p expression::quoted(id ty) const
// ----------------------------------------------------------------------------
//   If an equation contains a single symbol, return that
// ----------------------------------------------------------------------------
{
    byte  *p = (byte *) payload();
    size_t size = leb128<size_t>(p);
    object_p first = (object_p) p;
    if ((ty == ID_object || first->type() == ty) && first->size() == size)
        return first;
    return nullptr;
}


size_t expression::size_in_expression(object_p obj)
// ----------------------------------------------------------------------------
//   Size of an object in an equation
// ----------------------------------------------------------------------------
//   Inside an equation object, equations are reduced to their payload
{
    if (obj->type() == ID_expression)
        return expression_p(obj)->length();
    return obj->size();
}


expression::expression(id type, algebraic_r arg)
// ----------------------------------------------------------------------------
//   Build an equation object from an object
// ----------------------------------------------------------------------------
    : program(type, nullptr, 0)
{
    byte *p = (byte *) payload();

    // Compute the size of the program
    size_t size =  size_in_expression(arg);

    // Write the size of the program
    p = leb128(p, size);

    // Write the arguments
    size_t objsize = 0;
    byte_p objptr = nullptr;
    if (expression_p eq = arg->as<expression>())
    {
        objptr = eq->value(&objsize);
    }
    else
    {
        objsize = arg->size();
        objptr = byte_p(arg);
    }
    memmove(p, objptr, objsize);
    p += objsize;
}


size_t expression::required_memory(id type, algebraic_r arg)
// ----------------------------------------------------------------------------
//   Size of an equation object from an object
// ----------------------------------------------------------------------------
{
    size_t size = size_in_expression(arg);
    size += leb128size(size);
    size += leb128size(type);
    return size;
}


expression::expression(id type, id op, algebraic_r arg)
// ----------------------------------------------------------------------------
//   Build an equation from one argument
// ----------------------------------------------------------------------------
    : program(type, nullptr, 0)
{
    byte *p = (byte *) payload();

    // Compute the size of the program
    size_t size =  leb128size(op) + size_in_expression(arg);

    // Write the size of the program
    p = leb128(p, size);

    // Write the arguments
    size_t objsize = 0;
    byte_p objptr = nullptr;
    if (expression_p eq = arg->as<expression>())
    {
        objptr = eq->value(&objsize);
    }
    else
    {
        objsize = arg->size();
        objptr = byte_p(arg);
    }
    memmove(p, objptr, objsize);
    p += objsize;

    // Write the opcode
    p = leb128(p, op);
}


size_t expression::required_memory(id type, id op, algebraic_r arg)
// ----------------------------------------------------------------------------
//   Size of an equation object with one argument
// ----------------------------------------------------------------------------
{
    size_t size = leb128size(op) + size_in_expression(arg);
    size += leb128size(size);
    size += leb128size(type);
    return size;
}


expression::expression(id type, id op, algebraic_r x, algebraic_r y)
// ----------------------------------------------------------------------------
//   Build an equation from two arguments
// ----------------------------------------------------------------------------
    : program(type, nullptr, 0)
{
    byte *p = (byte *) payload();

    // Compute the size of the program
    size_t size =  leb128size(op) + size_in_expression(x) + size_in_expression(y);

    // Write the size of the program
    p = leb128(p, size);

    // Write the first argument
    size_t objsize = 0;
    byte_p objptr = nullptr;
    if (expression_p eq = x->as<expression>())
    {
        objptr = eq->value(&objsize);
    }
    else
    {
        objsize = x->size();
        objptr = byte_p(x);
    }
    memmove(p, objptr, objsize);
    p += objsize;

    // Write the second argument
    if (expression_p eq = y->as<expression>())
    {
        objptr = eq->value(&objsize);
    }
    else
    {
        objsize = y->size();
        objptr = byte_p(y);
    }
    memmove(p, objptr, objsize);
    p += objsize;

    // Write the opcode
    p = leb128(p, op);
}


size_t expression::required_memory(id type, id op, algebraic_r x, algebraic_r y)
// ----------------------------------------------------------------------------
//   Size of an equation object with one argument
// ----------------------------------------------------------------------------
{
    size_t size = leb128size(op)+size_in_expression(x)+size_in_expression(y);
    size += leb128size(size);
    size += leb128size(type);
    return size;
}


expression::expression(id type, id op, algebraic_g args[], uint arity)
// ----------------------------------------------------------------------------
//   Build an equation with 'arity' arguments
// ----------------------------------------------------------------------------
    : program(type, nullptr, 0)
{
    byte *p = (byte *) payload();

    // Compute the size of the program
    size_t size =  leb128size(op);
    for (uint a = 0; a < arity; a++)
        size += size_in_expression(args[a]);

    // Write the size of the program
    p = leb128(p, size);

    // Write the arguments
    size_t objsize = 0;
    byte_p objptr = nullptr;
    for (uint a = 0; a < arity; a++)
    {
        algebraic_p arg = args[arity + ~a];
        if (expression_p eq = arg->as<expression>())
        {
            objptr = eq->value(&objsize);
        }
        else
        {
            objsize = arg->size();
            objptr = byte_p(arg);
        }
        memmove(p, objptr, objsize);
        p += objsize;
    }

    // Write the opcode
    p = leb128(p, op);
}


size_t expression::required_memory(id type, id op,
                                   algebraic_g args[], uint arity)
// ----------------------------------------------------------------------------
//   Size of an equation object with 'arity' arguments
// ----------------------------------------------------------------------------
{
    size_t size = leb128size(op);
    for (uint a = 0; a < arity; a++)
        size += size_in_expression(args[a]);
    size += leb128size(size);
    size += leb128size(type);
    return size;
}



// ============================================================================
//
//   Equation rewrite engine
//
// ============================================================================
//
//   The equation rewrite engine works by expanding equation objects on
//   the stack, and matching them step by step.
//
//   When a symbol is encountered, it is recorded in locals as a pair
//   of entries, one for the symbol, one for its value.
//   If a symbol is seen twice, it must match exactly for the rule to match.
//   For example, a pattern like `X - X` will match if the two 'X' are 'same'
//
//   During rewrite, the stack is used to build three arrays, each being
//   the exploded content of the respective equation
//   - The 'from' pattern       [from..from+fromsz]
//   - The 'eq' value           [eq  ..eq+eqsz]
//
//   eq:        sin(a+3) - cos(a+3)             a 3 + sin a 3 + cos -
//   match:     sin x    - cos x                    x sin     x cos -
//

static expression_p grab_arguments(size_t &eq, size_t &eqsz)
// ----------------------------------------------------------------------------
//   Fetch an argument using the arity to know how many things to use
// ----------------------------------------------------------------------------
{
    size_t   len   = 0;
    size_t   arity = 1;
    scribble scr;
    while (arity && len < eqsz)
    {
        object_p obj = rt.stack(eq + len);
        arity--;
        arity += obj->arity();
        len++;
    }
    if (arity)
    {
        record(equation_error, "Argument gets %u beyond size %u", arity, eqsz);
        return nullptr;
    }

    size_t sz = len;
    while (len--)
    {
        object_p obj = rt.stack(eq + len);
        if (!rt.append(obj->size(), byte_p(obj)))
            return nullptr;
    }
    eq += sz;
    eqsz -= sz;
    list_p list = list::make(object::ID_expression, scr.scratch(), scr.growth());
    return expression_p(list);
}


static bool must_be_integer(symbol_p symbol)
// ------------------------------------------------------------------------
//   Convention for naming integers in rewrite rules
// ----------------------------------------------------------------------------
{
    char first = tolower(object::payload(symbol)[1]);
    return strchr("ijklmnpq", first) != nullptr;
}


static bool must_be_unique(symbol_p symbol)
// ------------------------------------------------------------------------
//   Convention for naming unique terms in rewrite rules
// ----------------------------------------------------------------------------
{
    char first = tolower(object::payload(symbol)[1]);
    return strchr("uvw", first) != nullptr;
}


static size_t check_match(size_t eq, size_t eqsz,
                          size_t from, size_t fromsz)
// ----------------------------------------------------------------------------
//   Check for structure match between eq and from
// ----------------------------------------------------------------------------
{
    size_t eqs = eq;
    size_t locals = rt.locals();
    while (fromsz && eqsz)
    {
        // Check what we match against
        object_p ftop = rt.stack(from);
        if (!ftop)
            return false;
        object::id fty = ftop->type();

        // Check if this this is a symbol.
        if (fty == object::ID_symbol)
        {
            // Check if the symbol already exists
            symbol_p name = symbol_p(ftop);
            object_p found = nullptr;
            size_t symbols = rt.locals() - locals;
            for (size_t l = 0; !found && l < symbols; l += 2)
            {
                symbol_p existing = symbol_p(rt.local(l));
                if (!existing)
                    return 0;
                if (existing->is_same_as(name))
                    found = rt.local(l+1);
            }

            // Get the value matching the symbol
            ftop = grab_arguments(eq, eqsz);
            if (!ftop)
                return 0;

            if (!found)
            {
                // Check if we expect an integer value
                if (must_be_integer(name))
                {
                    // At this point, if we have an integer, it was
                    // wrapped in an equation by grab_arguments.
                    size_t depth = rt.depth();
                    if (program::run(ftop) != object::OK)
                        return 0;
                    if (rt.depth() != depth + 1)
                    {
                        rt.type_error();
                        return 0;
                    }
                    ftop = rt.pop();

                    // We must have an integer
                    fty = ftop->type();
                    if (fty != object::ID_integer)
                        return 0;

                    // We always special-case zero as a terminating condition
                    if (ftop->is_zero())
                        return 0;
                }

                // Check if the name must be unique in the locals
                if (must_be_unique(name))
                {
                    for (size_t l = 0; l < symbols; l += 2)
                    {
                        symbol_p existing = symbol_p(rt.local(l+1));
                        if (!existing || existing->is_same_as(symbol_p(ftop)))
                            return 0;
                        symbol_p ename = symbol_p(rt.local(l));
                        if (must_be_unique(ename))
                        {
                            // Check if order of names and values match
                            symbol_p ftn = ftop->as_quoted<symbol>();
                            if (!ftn)
                                return 0;
                            symbol_p en = existing->as_quoted<symbol>();
                            if (!en)
                                return 0;
                            int cmpnames = name->compare_to(ename);
                            int cmpvals = ftn->compare_to(en);
                            if (cmpnames * cmpvals < 0)
                                return 0;
                        }
                    }
                }

                // Grab the parameter that corresponds and store it
                if (!rt.push(name) || !rt.push(ftop) || !rt.locals(2))
                    return 0;
            }
            else
            {
                // If there is a mismatch, rewrite fails
                if (!found->is_same_as(ftop))
                    return 0;
            }
        }
        else
        {
            // If not a symbol, we need an exact match
            object_p top = rt.stack(eq);
            if (!top || !top->is_same_as(ftop))
                return 0;
            eq++;
            eqsz--;
        }
        from++;
        fromsz--;
    }

    // If there is a leftover in `from`, then this is a mismatch
    if (fromsz)
        return false;

    // Otherwise, we covered the whole 'from' pattern, we have a match
    // Return size matched in source equation
    return eq - eqs;
}


expression_p expression::rewrite(expression_r from, expression_r to) const
// ----------------------------------------------------------------------------
//   If we match pattern in `from`, then rewrite using pattern in `to`
// ----------------------------------------------------------------------------
//   For example, if this equation is `3 + sin(X + Y)`, from is `A + B` and
//   to is `B + A`, then the output will be `sin(Y + X) + 3`.
//
{
    // Remember the current stack depth and locals
    size_t     locals   = rt.locals();
    size_t     depth    = rt.depth();

    // Need a GC pointer since stack operations may move us
    expression_g eq       = this;

    // Information about part we replace
    bool       replaced = false;
    size_t     matchsz  = 0;
    uint       rewrites = Settings.MaxRewrites();

    // Loop while there are replacements found
    do
    {
        // Location of expanded equation
        size_t eqsz = 0, fromsz = 0;
        size_t eqst = 0, fromst = 0;
        bool compute = false;

        replaced = false;

        // Expand 'from' on the stack and remember where it starts
        for (object_p obj : *from)
            if (!rt.push(obj))
                goto err;
        fromsz = rt.depth() - depth;

        // Expand this equation on the stack, and remember where it starts
        for (object_p obj : *eq)
            if (!rt.push(obj))
                goto err;
        eqsz = rt.depth() - depth - fromsz;

        // Keep checking sub-expressions until we find a match
        size_t eqlen = eqsz;
        fromst = eqst + eqsz;
        while (eqsz)
        {
            // Check if there is a match of this sub-equation
            matchsz = check_match(eqst, eqsz, fromst, fromsz);
            if (matchsz)
                break;

            // Check next step in the equation
            eqst++;
            eqsz--;
        }

        // We don't need the on-stack copies of 'eq' and 'to' anymore
        ASSERT(rt.depth() >= depth);
        rt.drop(rt.depth() - depth);

        // If we matched a sub-equation, perform replacement
        if (matchsz)
        {
            scribble scr;
            size_t   where  = 0;

            // We matched from the back of the equation object
            eqst = eqlen - matchsz - eqst;

            // Copy from the original
            for (expression::iterator it = eq->begin(); it != eq->end(); ++it)
            {
                ASSERT(*it);
                if (where < eqst || where >= eqst + matchsz)
                {
                    // Copy from source equation directly
                    object_p obj = *it;
                    if (!rt.append(obj->size(), byte_p(obj)))
                        return nullptr;
                }
                else if (!replaced)
                {
                    // Insert a version of 'to' where symbols are replaced
                    for (object_p tobj : *to)
                    {
                        if (tobj->type() == ID_symbol)
                        {
                            // Check if we find the matching pattern in local
                            symbol_p name = symbol_p(tobj);
                            object_p found = nullptr;
                            size_t symbols = rt.locals() - locals;
                            for (size_t l = 0; !found && l < symbols; l += 2)
                            {
                                symbol_p existing = symbol_p(rt.local(l));
                                if (!existing)
                                    continue;
                                if (existing->is_same_as(name))
                                    found = rt.local(l+1);
                            }
                            if (found)
                            {
                                tobj = found;
                                if (must_be_integer(name))
                                    compute = true;
                            }
                        }

                        // Only copy the payload of equations
                        size_t tobjsize = tobj->size();
                        if (expression_p teq = tobj->as<expression>())
                            tobj = teq->objects(&tobjsize);
                        if (!rt.append(tobjsize, byte_p(tobj)))
                            return nullptr;
                    }

                    replaced = true;
                }
                where++;
            }

            // Restart anew with replaced equation
            eq = expression_p(list::make(ID_expression,
                                       scr.scratch(), scr.growth()));

            // If we had an integer matfched and replaced, execute equation
            if (compute)
            {
                // Need to evaluate e.g. 3-1 to get 2
                if (eq->run() != object::OK)
                    goto err;
                if (rt.depth() != depth+1)
                    goto err;
                object_p computed = rt.pop();
                if (!computed)
                    goto err;
                algebraic_g eqa = computed->as_algebraic();
                if (!eqa)
                    goto err;
                eq = eqa->as<expression>();
                if (!eq)
                    eq = expression::make(eqa);
            }

            // Drop the local names, we will recreate them on next match
            rt.unlocals(rt.locals() - locals);

            // Check if we are looping forever
            if (rewrites-- == 0)
            {
                rt.too_many_rewrites_error();
                goto err;
            }
        }
    } while (replaced && !interrupted());

err:
    ASSERT(rt.depth() >= depth);
    rt.drop(rt.depth() - depth);
    rt.unlocals(rt.locals() - locals);
    return eq;;
}


expression_p expression::rewrite(size_t size, const byte_p rewrites[]) const
// ----------------------------------------------------------------------------
//   Apply a series of rewrites
// ----------------------------------------------------------------------------
{
    expression_g eq = this;
    for (size_t i = 0; eq && i < size; i += 2)
        eq = eq->rewrite(expression_p(rewrites[i]), expression_p(rewrites[i+1]));
    return eq;
}


expression_p expression::rewrite_all(size_t size, const byte_p rewrites[]) const
// ----------------------------------------------------------------------------
//   Loop on the rewrites until the result stabilizes
// ----------------------------------------------------------------------------
{
    uint count = 0;
    expression_g last = nullptr;
    expression_g eq = this;
    while (count++ < Settings.MaxRewrites() && eq && +eq != +last)
    {
        // Check if we produced the same value
        if (last && last->is_same_as(eq))
            break;

        last = eq;
        eq = eq->rewrite(size, rewrites);
    }
    if (count >= Settings.MaxRewrites())
        rt.too_many_rewrites_error();
    return eq;
}


COMMAND_BODY(Rewrite)
// ----------------------------------------------------------------------------
//   Rewrite (From, To, Value): Apply rewrites
// ----------------------------------------------------------------------------
{
    object_p x = rt.stack(0);
    object_p y = rt.stack(1);
    object_p z = rt.stack(2);
    if (!x || !y || !z)
        return ERROR;
    expression_g eq = z->as<expression>();
    expression_g from = y->as<expression>();
    expression_g to = x->as<expression>();
    if (!from || !to || !eq)
    {
        rt.type_error();
        return ERROR;
    }

    eq = eq->rewrite(from, to);
    if (!eq)
        return ERROR;
    if (!rt.drop(2) || !rt.top(eq))
        return ERROR;

    return OK;
}



// ============================================================================
//
//    Actual rewrites for various rules
//
// ============================================================================

template <byte ...args>
constexpr byte eq<args...>::object_data[sizeof...(args)+2];

static eq_symbol<'x'> x;
static eq_symbol<'y'> y;
static eq_symbol<'z'> z;
static eq_symbol<'n'> n;
static eq_symbol<'m'> m;
static eq_symbol<'p'> p;
static eq_symbol<'u'> u;
static eq_symbol<'v'> v;
static eq_symbol<'w'> w;
static eq_integer<0>  zero;
static eq_neg_integer<-1> mone;
static eq_integer<1> one;
static eq_integer<2> two;
static eq_integer<3> three;

expression_p expression::expand() const
// ----------------------------------------------------------------------------
//   Run various rewrites to expand equation
// ----------------------------------------------------------------------------
{
    return rewrite_all(
        (x+y)*z,     x*z+y*z,
        x*(y+z),     x*y+x*z,
        (x-y)*z,     x*z-y*z,
        x*(y-z),     x*y-x*z,
        sq(x),       x*x,
        cubed(x),    x*x*x,
        x^zero,      one,
        x^one,       x,
        x^n,         x * (x^(n-one)),
        x * n,       n * x,
        v * u,       u * v,
        x * v * u,   x * u * v,
        one * x,     x,
        zero * x,    zero,
        n + x,       x + n,
        x + zero,    x,
        x - x,       zero,
        x + y - y,   x,
        x - y + y,   x,
        x * (y * z), (x * y) * z,
        x + (y + z), (x + y) + z,
        x + (y - z), (x + y) - z,
        x - y + z,   (x + z) - y,
        v + u,       u + v,
        x + v + u,   x + u + v
        );
}


expression_p expression::collect() const
// ----------------------------------------------------------------------------
//    Run various rewrites to collect terms / factor equation
// ----------------------------------------------------------------------------
{
    return rewrite_all(
        x*z+y*z,                (x+y)*z,
        x*y+x*z,                x*(y+z),
        x*z-y*z,                (x-y)*z,
        x*y-x*z,                x*(y-z),
        x*(x^n),                x^(n+one),
        (x^n)*x,                x^(n+one),
        (x^n)*(x^m),            x^(n+m),
        sq(x),                  x^two,
        cubed(x),               x^three,
        x * n,                  n * x,
        one * x,                x,
        zero * x,               zero,
        n + x,                  x + n,
        x + zero,               x,
        x - x,                  zero,
        n * x + x,              (n + one) * x,
        x + n * x,              (n + one) * x,
        m * x + n * x,          (m + n) * x,
        x * y * x,              (x^two) * y,
        x * y * y,              (y^two) * x,
        x + y + y,              two * y + x,
        (x ^ n) * y * x,        (x^(n + one)) * y,
        (x ^ n) * (x + y),      (x^(n+one)) + (x^n) * y,
        (x ^ n) * (y + x),      (x^(n+one)) + (x^n) * y,
        x + x,                  two * x
        );
}


expression_p expression::simplify() const
// ----------------------------------------------------------------------------
//   Run various rewrites to simplify equation
// ----------------------------------------------------------------------------
{
    return rewrite_all(
        x + zero,    x,
        zero + x,    x,
        x - zero,    x,
        zero - x,    x,
        x * zero,    zero,
        zero * x,    zero,
        x * one,     x,
        one * x,     x,
        x / one,     x,
        x / x,       one,
        one / x,     inv(x),
        x * x * x,   cubed(x),
        x * x,       sq(x),
        x ^ zero,    one,
        x ^ one,     x,
        x ^ two,     sq(x),
        x ^ three,   cubed(x),
        x ^ mone,    inv(x),
        (x^n)*(x^m), x ^ (n+m)
        );
}


algebraic_p expression::factor_out(algebraic_g expr,
                                 algebraic_g factor,
                                 algebraic_g &scale,
                                 algebraic_g &exponent)
// ----------------------------------------------------------------------------
//   Factor out the given factor from the equation
// ----------------------------------------------------------------------------
//   Given expr=A*X*(X*B)^3/(X*C)^6,
//   returns X^(-2) * (A*B^3/C^6), with factor=(A*B^3/C^6) and exponent=-2
{
    // Quick bail out in case of error down the line
    if (!expr || !factor)
        return nullptr;

    // Default is 1 * X ^ 0 * (rest-of-expr)
    scale = integer::make(1);
    exponent = integer::make(0);

    expression_g eq = expr->as<expression>();
    if (eq)
    {
        // Case where we have a single name or a constant, e.g. m or 1
        if (object_p inner = eq->quoted())
        {
            if (inner->is_algebraic())
            {
                expr = algebraic_p(inner);
                eq = nullptr;
            }
        }
    }

    // Check for anything that is not an equation
    if (!eq)
    {
        // We have a single element in the equation
        if (expr->is_same_as(factor))
        {
            // Factoring X as 1 * (1 * X ^ 1)
            exponent = scale;
            return factor;
        }

        // Factoring Y as Y * X ^ 0)
        scale = expr;
        return expr;
    }


    // Loop on all items in the equation, factoring out as we go
    algebraic_g x, y, xs, xe, ys, ye;
    algebraic_g one = integer::make(1);
    for (object_p obj : *eq)
    {
        id ty = obj->type();

        switch (ty)
        {
        case ID_mul:
            x = algebraic_p(rt.pop());
            y = algebraic_p(rt.pop());
            y = factor_out(y, factor, ys, ye);
            x = factor_out(x, factor, xs, xe);
            scale = ys * xs;
            exponent = ye + xe;
            x = y * x;
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        case ID_div:
            x = algebraic_p(rt.pop());
            y = algebraic_p(rt.pop());
            y = factor_out(y, factor, ys, ye);
            x = factor_out(x, factor, xs, xe);
            scale = ys / xs;
            exponent = ye - xe;
            x = y / x;
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        case ID_pow:
            x = algebraic_p(rt.pop());
            y = algebraic_p(rt.pop());
            y = factor_out(y, factor, ys, ye);
            ye = ye * x;
            scale = pow(ys, x);
            exponent = ye;
            x = pow(y, x);
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        case ID_inv:
            x = algebraic_p(rt.pop());
            x = factor_out(x, factor, xs, xe);
            scale = inv::run(xs);
            exponent = -xe;
            x = inv::run(x);
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        case ID_sq:
            x = algebraic_p(rt.pop());
            x = factor_out(x, factor, xs, xe);
            scale = xs * xs;
            exponent = xe + xe;
            x = x * x;
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        case ID_cubed:
            x = algebraic_p(rt.pop());
            x = factor_out(x, factor, xs, xe);
            scale = xs * xs * xs;
            exponent = xe + xe + xe;
            x = x * x * x;
            if (!x|| !rt.push(+x))
                return nullptr;
            break;

        default:
            if (program::run(obj) != OK)
                return nullptr;
        }
    }

    x = algebraic_p(rt.pop());
    return x;
}


algebraic_p expression::simplify_products() const
// ----------------------------------------------------------------------------
//   Simplify products, used notably to simplify units
// ----------------------------------------------------------------------------
//   Units are products and ratios of powers.
//   We rewrite that so that all terms are written at most once with the
//   corresponing (positive or negative) power
{
    // Case where we have a single name or a constant, e.g. 1_m or 1_1.
    if (object_p inner = quoted())
        if (inner->is_algebraic())
            return algebraic_p(inner);

    // Save auto-simplify and set it
    bool auto_simplify = Settings.AutoSimplify();
    Settings.AutoSimplify(true);
    save<bool> save(unit::mode, false);

    // Need a GC pointer since stack operations may move us
    expression_g eq   = this;
    algebraic_g  num  = integer::make(1);
    algebraic_g  den  = integer::make(1);

    // Loop factoring out variables, until there is no variable left
    bool done = false;
    while (!done)
    {
        done = true;
        for (object_p obj : *eq)
        {
            if (symbol_g sym = obj->as<symbol>())
            {
                algebraic_g scale, exponent;
                algebraic_g rest = factor_out(+eq, +sym, scale, exponent);
                if (!rest || !scale || !exponent)
                {
                    Settings.AutoSimplify(auto_simplify);
                    return nullptr;
                }
                if (exponent->is_negative(false))
                    den = den * pow(+sym, -exponent);
                else
                    num = num * pow(+sym, exponent);
                rest = scale;
                if (expression_p req = rest->as<expression>())
                {
                    eq = req;
                    done = false;
                }
                else
                {
                    if (rest->is_real())
                        num = rest * num;
                    else
                        num = num * rest;
                    eq = nullptr;
                }
                break;
            }
        }

        if (done && eq)
        {
            algebraic_g rest = +eq;
            num = num * rest;
        }
    }

    num = num / den;
    Settings.AutoSimplify(auto_simplify);
    return num;
}


expression_p expression::as_difference_for_solve() const
// ----------------------------------------------------------------------------
//   For the solver, transform A=B into A-B
// ----------------------------------------------------------------------------
//   Revisit: how to transform A and B, A or B, e.g. A=B and C=D ?
{
    return rewrite(x == y, x - y);
}


object_p expression::outermost_operator() const
// ----------------------------------------------------------------------------
//   Return the last operator in the equation
// ----------------------------------------------------------------------------
{
    object_p result = nullptr;
    for (object_p o : *this)
        result = o;
    return result;
}



// ============================================================================
//
//   Graphic rendering
//
// ============================================================================

using pixsize  = grob::pixsize;


grob_p expression::parentheses(grapher &g, grob_g what, uint padding)
// ----------------------------------------------------------------------------
//   Render parentheses around a grob
// ----------------------------------------------------------------------------
{
    if (!what)
        return nullptr;

    pixsize inw    = what->width();
    pixsize inh    = what->height();
    pixsize prw    = inh / 8;
    pixsize rw     = inw + 2 * prw + 2 + padding;
    pixsize rh     = inh;
    pixsize hh     = rh / 2;
    pixsize hh2    = hh * hh;

    grob_g result = g.grob(rw, rh);
    if (!result)
        return nullptr;

    // Copy inner content
    grob::surface ws = what->pixels();
    grob::surface rs = result->pixels();
    rs.fill(0, 0, rw, rh, g.background);
    rs.copy(ws, prw + padding, 0);

    // Draw the parentheses themselves
    for (coord y = 0; y < coord(hh); y++)
    {
        coord x = prw * y * y / hh2;
        pixsize pw = 1 + (prw - x) / 4;
        if (pw > 4)
            pw = 4;
        coord x1 = x + padding;
        coord x2 = x1 + pw;
        rs.fill(x1, hh+y, x2, hh+y, g.foreground);
        rs.fill(x1, hh-y, x2, hh-y, g.foreground);
        x = rw - 2 - x;
        x2 = x;
        x1 = x2 - pw;
        rs.fill(x1, hh+y, x2, hh+y, g.foreground);
        rs.fill(x1, hh-y, x2, hh-y, g.foreground);
    }

    return result;
}


grob_p expression::sqrt(grapher &g, grob_g what)
// ----------------------------------------------------------------------------
//  Draw a square root around the expression
// ----------------------------------------------------------------------------
{
    if (!what)
        return nullptr;

    pixsize inw    = what->width();
    pixsize inh    = what->height();
    pixsize prw    = inh / 4;
    pixsize prh    = 4;
    pixsize rw     = inw + prw + 4;
    pixsize rh     = inh + prh;

    grob_g result = g.grob(rw, rh);
    if (!result)
        return nullptr;

    // Copy inner content
    grob::surface ws = what->pixels();
    grob::surface rs = result->pixels();
    rs.fill(0, 0, rw, rh, g.background);
    rs.copy(ws, prw, prh);

    // Draw the square root itself
    rs.line(0,     rh/2 + prh, prw/2, rh-4,     2, g.foreground);
    rs.line(prw/2, rh-4,       prw-2, 0,        3, g.foreground);
    rs.line(prw-2, 0,          rw-3,  0,        2, g.foreground);
    rs.line(rw-3,  0,          rw-3,  prh,      2, g.foreground);

    g.voffset = prh / 2;

    return result;
}


grob_p expression::ratio(grapher &g, grob_g num, grob_g den)
// ----------------------------------------------------------------------------
//   Draw a ratio beween two grobs
// ----------------------------------------------------------------------------
{
    if (!num || !den)
        return nullptr;

    pixsize nw = num->width();
    pixsize nh = num->height();
    pixsize dw = den->width();
    pixsize dh = den->height();
    pixsize lh = 1 + (nh + dh) / 80;
    pixsize gw = nw > dw ? nw : dw;
    pixsize gh = nh + dh + 2 + lh;

    grob_g  result = g.grob(gw, gh);
    if (!result)
        return nullptr;

    grob::surface ns = num->pixels();
    grob::surface ds = den->pixels();
    grob::surface rs = result->pixels();

    rs.fill(0, 0, gw, gh, g.background);
    rs.copy(ns, (gw - nw) / 2, 0);
    rs.copy(ds, (gw - dw) / 2, nh + 2 + lh);
    rs.fill(0, nh, gw, nh + lh, g.foreground);

    g.voffset = 1 + coord(nh) + coord(lh) / 2 - coord(gh) / 2;

    return result;
}


grob_p expression::ratio(grapher &g, cstring num, grob_g den)
// ----------------------------------------------------------------------------
//   Ratio with a constant
// ----------------------------------------------------------------------------
{
    symbol_g nobj = symbol::make(num);
    if (!nobj)
        return nullptr;
    grob_g n = object::do_graph(nobj, g);
    return ratio(g, n, den);
}


grob_p expression::infix(grapher &g,
                         coord vx, grob_g x,
                         coord vs, cstring sep,
                         coord vy, grob_g y)
// ----------------------------------------------------------------------------
//   Draw an infix between two grobs, e.g. "A+B"
// ----------------------------------------------------------------------------
{
    if (!x || !y)
        return nullptr;

    // Render the separator
    symbol_g sobj = symbol::make(sep);
    if (!sobj)
        return nullptr;
    grob_g s = object::do_graph(sobj, g);
    return infix(g, vx, x, vs, s, vy, y);
}


grob_p expression::infix(grapher &g,
                         coord vx, grob_g x,
                         coord vs, grob_g s,
                         coord vy, grob_g y)
// ----------------------------------------------------------------------------
//  Draw an infix between two grobs with a third one in the middle
// ----------------------------------------------------------------------------
{
    if (!x || !y || !s)
        return nullptr;

    pixsize xw = x->width();
    pixsize xh = x->height();
    pixsize yw = y->width();
    pixsize yh = y->height();
    pixsize sw = s->width();
    pixsize sh = s->height();
    pixsize gw = xw + sw + yw;

    coord   xt = -vx - xh / 2;
    coord   yt = -vy - yh / 2;
    coord   st = -vs - sh / 2;
    coord   xb = -vx + xh / 2 - 1;
    coord   yb = -vy + yh / 2 - 1;
    coord   sb = -vs + sh / 2 - 1;

    coord t = xt;
    if (t > yt)
        t = yt;
    if (t > st)
        t = st;

    coord b = xb;
    if (b < yb)
        b = yb;
    if (b < sb)
        b = sb;

    pixsize gh = b - t + 1;

    grob_g result = g.grob(gw, gh);
    if (!result)
        return nullptr;

    grob::surface xs = x->pixels();
    grob::surface ys = y->pixels();
    grob::surface ss = s->pixels();
    grob::surface rs = result->pixels();

    coord xx = 0;
    coord sx = xx + xw;
    coord yx = sx + sw;

    rs.fill(0, 0, gw, gh, g.background);
    rs.copy(xs, xx, xt - t);
    rs.copy(ss, sx, st - t);
    rs.copy(ys, yx, yt - t);

    g.voffset = st - t + coord(sh)/2 - coord(gh)/2;

    return result;
}


grob_p expression::suscript(grapher &g,
                            coord vx, grob_g x,
                            coord vy, grob_g y,
                            int dir, bool alignleft)
// ----------------------------------------------------------------------------
//  Position grob y on the right of x
// ----------------------------------------------------------------------------
//  dir can be 0 (centered), 1 (superscript) or -1 (subscript)
{
    if (!x || !y)
        return nullptr;

    pixsize xw     = x->width();
    pixsize xh     = x->height();
    pixsize yw     = y->width();
    pixsize yh     = y->height();
    pixsize gw     = xw + yw;

    coord   voff   = (1 - dir) * xh / 2;
    coord   xt     = -vx - xh / 2;
    coord   yt     = -vy + xt + voff - yh / 2;
    coord   xb     = xt + xh - 1;
    coord   yb     = yt + yh - 1;
    coord   t      = xt < yt ? xt : yt;
    coord   b      = xb > yb ? xb : yb;

    pixsize gh = b - t + 1;
    grob_g result = g.grob(gw, gh);
    if (!result)
        return nullptr;

    grob::surface xs = x->pixels();
    grob::surface ys = y->pixels();
    grob::surface rs = result->pixels();

    rs.fill(0, 0, gw, gh, g.background);
    rs.copy(xs, 0,  xt - t);
    rs.copy(ys, xw, yt - t);
    if (alignleft)
        g.voffset = xt - t + coord(xh)/2 - coord(gh)/2;
    else
        g.voffset = yt - t + coord(yh)/2 - coord(gh)/2 + vy;


    return result;
}


grob_p expression::suscript(grapher &g,
                            coord vx, grob_g x,
                            coord vy, cstring exp,
                            int dir, bool alignleft)
// ----------------------------------------------------------------------------
//   Position a superscript
// ----------------------------------------------------------------------------
{
    symbol_g yobj = symbol::make(exp);
    if (!yobj)
        return nullptr;
    settings::font_id savef = g.font;
    g.reduce_font();
    grob_g y = object::do_graph(yobj, g);
    g.font = savef;
    return suscript(g, vx, x, vy, y, dir, alignleft);
}


grob_p expression::suscript(grapher &g,
                            coord vx, cstring xstr,
                            coord vy, grob_g y,
                            int dir, bool alignleft)
// ----------------------------------------------------------------------------
//   Position a superscript
// ----------------------------------------------------------------------------
{
    symbol_g xobj = symbol::make(xstr);
    if (!xobj)
        return nullptr;
    grob_g x = object::do_graph(xobj, g);
    return suscript(g, vx, x, vy, y, dir, alignleft);
}


grob_p expression::prefix(grapher &g,
                          coord vx, grob_g x,
                          coord vy, grob_g y, int dir)
// ----------------------------------------------------------------------------
//   Draw a prefix, e.g. "sin"
// ----------------------------------------------------------------------------
{
    return suscript(g, vx, x, vy, y, dir);
}


grob_p expression::prefix(grapher &g,
                          coord vx, cstring pfx,
                          coord vy, grob_g y, int dir)
// ----------------------------------------------------------------------------
//   Draw a prefix, e.g. "-"
// ----------------------------------------------------------------------------
{
    symbol_g xobj = symbol::make(pfx);
    if (!xobj)
        return nullptr;
    grob_g x = object::do_graph(xobj, g);
    return suscript(g, vx, x, vy, y, dir);
}


grob_p expression::sumprod(grapher &g, bool isprod,
                           coord vi, grob_g index,
                           coord vf, grob_g first,
                           coord vl, grob_g last,
                           coord ve, grob_g expr)
// ----------------------------------------------------------------------------
//  Create a sum or product expression
// ----------------------------------------------------------------------------
{
    using std::max;
    using size = blitter::size;


    if (!index || !first || !last || !expr)
        return nullptr;

    auto fid = g.font;
    g.reduce_font();
    grob_g lower  = infix(g, vi, index, 0, "=", vf, first);
    g.font = fid;
    if (!lower)
        return nullptr;

    grob::surface xs   = expr->pixels();
    size          xh   = xs.height();
    size          xw   = xs.width();
    grob_g        sign = isprod ? product(g, xh) : sum(g, xh);
    if (!sign)
        return nullptr;

    size    lh     = lower->height() + sign->height() + last->height();
    size    lw     = max(max(lower->width(), sign->width()), last->width());
    size    gh     = max(lh, xh);
    size    gw     = lw + xw;

    grob_g  result = g.grob(gw, gh);
    if (!result)
        return nullptr;
    grob::surface rs = result->pixels();
    rs.fill(0, 0, gw, gh, g.background);

    grob::surface sgs = sign->pixels();
    rs.copy(sgs, (lw - sgs.width()) / 2, last->height());
    grob::surface las = last->pixels();
    rs.copy(las, (lw - las.width()) / 2, 0 + 0 * vl);
    grob::surface los = lower->pixels();
    rs.copy(los, (lw - los.width()) / 2, gh - los.height());
    coord xx = lw;
    coord xy = las.height() + sgs.height() / 2 - xs.height() / 2 - ve;
    rs.copy(xs, xx, xy);
    g.voffset = xy + (coord(xh) - coord(gh)) / 2 + ve;

    return result;
}


grob_p expression::sum(grapher &g, blitter::size h)
// ----------------------------------------------------------------------------
//   Create a 'sum' sign of height h
// ----------------------------------------------------------------------------
{
    using size    = blitter::size;

    size   w      = h * 3 / 4;
    grob_g result = g.grob(w, h);
    if (!result)
        return nullptr;

    grob::surface rs = result->pixels();
    rs.fill(0,          0,      w-1,    h-1,            g.background);

    rs.fill(0,          0,      w-1,    2,              g.foreground);
    rs.fill(w-2,        0,      w-1,    4,              g.foreground);
    rs.fill(w-2,        h-5,    w-1,    h-1,            g.foreground);
    rs.fill(0,          h-3,    w-1,    h-1,            g.foreground);
    rs.line(0,          0,    w/2-1,    h/2-1,  4,      g.foreground);
    rs.line(0,          h-3,  w/2-1,    h/2-1,  4,      g.foreground);

    return result;
}


grob_p expression::product(grapher &g, blitter::size h)
// ----------------------------------------------------------------------------
//   Create a 'product' sign of height h
// ----------------------------------------------------------------------------
{
    using size    = blitter::size;
    size   w      = h * 3 / 4;
    grob_g result = g.grob(w, h);
    if (!result)
        return nullptr;

    grob::surface rs = result->pixels();
    rs.fill(0,          0,      w-1,    h-1,    g.background);

    rs.fill(0,          0,      w-1,    2,      g.foreground);
    rs.fill(4,          0,      8,      h-1,    g.foreground);
    rs.fill(w-8,        0,      w-4,    h-1,    g.foreground);

    return result;
}




static inline cstring mulsep()
// ----------------------------------------------------------------------------
//   Return the separator for multiplication
// ----------------------------------------------------------------------------
{
    return Settings.UseDotForMultiplication() ? "·" : "×";
}


grob_p expression::graph(grapher &g, uint depth, int &precedence)
// ----------------------------------------------------------------------------
//   Render a single object as a graphical object
// ----------------------------------------------------------------------------
{
    if (object_g obj = rt.pop())
    {
        int arity = obj->arity();
        switch(arity)
        {
        case 0:
            // Symbols and other non-algebraics, e.g. numbers
            precedence = obj->precedence();
            if (precedence == precedence::NONE)
                precedence = precedence::SYMBOL;
            g.voffset = 0;
            return obj->graph(g);

        case 1:
        {
            int      argp = 0;
            id       oid  = obj->type();
            auto     savef = g.font;
            if (oid == ID_exp || oid == ID_exp10 || oid == ID_exp2)
                g.reduce_font();
            grob_g   arg  = graph(g, depth, argp);
            coord    va   = g.voffset;
            int      maxp = oid == ID_neg ? precedence::MULTIPLICATIVE
                : precedence::SYMBOL;
            bool paren = (argp < maxp &&
                          oid != ID_sqrt && oid != ID_inv &&
                          oid != ID_exp && oid != ID_exp10 && oid != ID_exp2 &&
                          oid != ID_cbrt);
            if (paren)
                arg = parentheses(g, arg, 3);
            precedence = precedence::FUNCTION;

            g.font = savef;

            switch(oid)
            {
            case ID_sq:
                precedence = precedence::FUNCTION_POWER;
                return suscript(g, va, arg, 0, "2");
            case ID_cubed:
                precedence = precedence::FUNCTION_POWER;
                return suscript(g, va, arg, 0, "3");
            case ID_exp:
                precedence = precedence::FUNCTION_POWER;
                return suscript(g, 0, "e", va, arg);
            case ID_exp10:
                precedence = precedence::FUNCTION_POWER;
                return suscript(g, 0, "10", va, arg);
            case ID_exp2:
                precedence = precedence::FUNCTION_POWER;
                return suscript(g, 0, "2", va, arg);
            case ID_neg:
                precedence = precedence::ADDITIVE;
                return prefix(g, 0, "-", va, arg);
            case ID_fact:
                precedence = precedence::SYMBOL;
                return suscript(g, va, arg, 0, "!", 0);
            case ID_sqrt:
                precedence = precedence::FUNCTION_POWER;
                return sqrt(g, arg);
            case ID_inv:
                precedence = precedence::FUNCTION_POWER;
                return ratio(g, "1", arg);
            case ID_cbrt:
            {
                auto fid = g.font;
                arg = sqrt(g, arg);
                g.reduce_font();
                arg = suscript(g, 0, "3", va, arg, -1, false);
                g.font = fid;
                return arg;
            }

            default:
                break;
            }
            g.voffset = 0;
            grob_g fn = obj->graph(g);
            coord  vf = g.voffset;
            return paren ? prefix(g, vf, fn, va, arg)
                         : infix(g, vf, fn, 0, " ", va, arg);
        }
        break;
        case 2:
        {
            int    lprec = 0, rprec = 0;
            id     oid = obj->type();
            auto   fid  = g.font;
            if (oid == ID_pow || oid == ID_xroot ||
                oid == ID_comb || oid == ID_perm)
                g.reduce_font();
            grob_g rg   = graph(g, depth, rprec);
            coord  rv   = g.voffset;
            if (oid != ID_comb && oid != ID_perm)
                g.font      = fid;
            grob_g lg   = graph(g, depth, lprec);
            coord  lv   = g.voffset;
            int    prec = obj->precedence();
            g.font = fid;
            if (prec == precedence::FUNCTION &&
                oid != ID_xroot && oid != ID_comb && oid != ID_perm)
            {
                grob_g arg = infix(g, lv, lg, 0, ";", rv, rg);
                coord  av  = g.voffset;
                arg = parentheses(g, arg);
                precedence = precedence::FUNCTION;
                g.voffset = 0;
                grob_g op  = obj->graph(g);
                coord  ov  = g.voffset;
                return prefix(g, ov, op, av, arg);
            }

            if (oid != ID_div && oid != ID_xroot &&
                oid != ID_comb && oid != ID_perm)
            {
                if (lprec < prec)
                    lg = parentheses(g, lg);
                if (rprec < prec ||
                    (rprec == prec && (oid == ID_sub || oid == ID_div)))
                    rg = parentheses(g, rg);
            }
            precedence = prec;
            switch (oid)
            {
            case ID_pow: return suscript(g, lv, lg, rv, rg);
            case ID_div: return ratio(g, lg, rg);
            case ID_mul: return infix(g, lv, lg, 0, mulsep(), rv, rg);
            case ID_xroot:
            {
                lg = sqrt(g, lg);
                rg = suscript(g, rv, rg, lv, lg, -1, false);
                return rg;
            }
            case ID_comb:
            case ID_perm:
                rg = infix(g, lv, lg, 0, ",", rv, rg);
                rv = g.voffset;
                lg = suscript(g, 0, oid == ID_comb ? "C" : "P", rv, rg, -1);
                return lg;

            default: break;
            }
            g.voffset = 0;
            grob_g op = obj->graph(g);
            coord ov = g.voffset;
            return infix(g, lv, lg, ov, op, rv, rg);
        }
        break;

        case 4:
        {
            id oid = obj->type();
            if (oid == ID_Sum || oid == ID_Product)
            {
                int    eprec = 0;
                auto   fid   = g.font;
                grob_g expr  = graph(g, depth, eprec);
                coord  ve    = g.voffset;
                g.reduce_font();
                grob_g last  = graph(g, depth, eprec);
                coord  vl    = g.voffset;
                grob_g first = graph(g, depth, eprec);
                coord  vf    = g.voffset;
                grob_g index = graph(g, depth, eprec);
                coord  vi    = g.voffset;
                g.font = fid;

                return sumprod(g, oid == ID_Product,
                               vi, index,
                               vf, first,
                               vl, last,
                               ve, expr);
            }
        }

        [[fallthrough]];
        default:
        {
            grob_g args = nullptr;
            coord argsv = 0;
            for (int a = 0; a < arity; a++)
            {
                int    prec = 0;
                grob_g arg  = graph(g, depth, prec);
                coord  argv = g.voffset;
                if (a)
                    args = infix(g, argv, arg, 0, ";", argsv, args);
                else
                    args = arg;
                argsv = g.voffset;
            }
            args = parentheses(g, args);
            precedence = precedence::FUNCTION;
            g.voffset = 0;
            grob_g op = obj->graph(g);
            coord ov = g.voffset;
            return prefix(g, ov, op, argsv, args);
        }
        }
    }

    return nullptr;
}


GRAPH_BODY(expression)
// ----------------------------------------------------------------------------
//   Render an expression graphically
// ----------------------------------------------------------------------------
{
    expression_g expr  = o;
    size_t       depth = rt.depth();
    bool         ok    = true;
    bool         funcall = o->type() == ID_funcall;

    // First push all things so that we have the outermost operators first
    for (object_p obj : *expr)
    {
        ASSERT(obj);
        ok = rt.push(obj);
        if (!ok)
            break;
    }

    if (!ok)
    {
        // We ran out of memory pushing things
        if (size_t remove = rt.depth() - depth)
            rt.drop(remove);
        return nullptr;
    }

    int    prec   = 0;
    grob_g result = graph(g, depth, prec);
    bool   more   = rt.depth() > depth;
    if (more)
    {
        if (funcall)
        {
            grob_g args  = nullptr;
            coord  vr    = g.voffset;
            coord  voffs = 0;
            while (more)
            {
                int    aprec = 0;
                grob_p argg  = graph(g, depth, aprec);
                if (!argg)
                    return nullptr;
                int va = g.voffset;
                more = rt.depth() > depth;
                if (args)
                {
                    args = infix(g, va, argg, 0, ";", voffs, args);
                    voffs = g.voffset;
                    if (!args)
                        return nullptr;
                }
                else
                {
                    args = argg;
                    voffs = va;
                }
            }
            args = parentheses(g, args);
            if (!args)
                return nullptr;
            voffs = g.voffset;
            result = prefix(g, vr, result, voffs, args);
        }
        else
        {
            size_t remove = rt.depth() - depth;
            record(equation_error, "Malformed equation, %u removed", remove);
            rt.drop(remove);
        }
    }
    return result;
}



// ============================================================================
//
//   User-defined function calls
//
// ============================================================================

PARSE_BODY(funcall)
// ----------------------------------------------------------------------------
//    Parse a function call within an expression
// ----------------------------------------------------------------------------
{
    // If not inside an expression, we can't have a function call
    if (!p.precedence)
        return SKIP;

    // We need to have a name followed by parentheses and comma-separated args
    utf8    source = p.source;
    size_t  max    = p.length;
    size_t  parsed = 0;

    // First character must be alphabetic
    unicode cp = utf8_codepoint(source);
    if (!is_valid_as_name_initial(cp))
        return SKIP;
    parsed = utf8_next(source, parsed, max);

    // Other characters must be alphabetic
    while (parsed < max && is_valid_in_name(source + parsed))
        parsed = utf8_next(source, parsed, max);
    size_t namelen = parsed;

    // Skip whitespace
    while (parsed < max && utf8_whitespace(utf8_codepoint(source + parsed)))
        parsed = utf8_next(source, parsed, max);

    // Skip if this is not a function call
    if (parsed >= max || utf8_codepoint(source + parsed) != '(')
        return SKIP;

    // Record the name
    symbol_g name = rt.make<symbol>(ID_symbol, source, namelen);
    source = p.source;          // In case of GC

    // Skip the opening parenthese
    parsed = utf8_next(source, parsed, max);

    scribble scr;
    cp = utf8_codepoint(source + parsed);
    while (parsed < max && cp != ')')
    {
        parser  child(p, source + parsed, LOWEST);
        result rc = list_parse(ID_expression, child, 0, 0);
        if (rc != OK)
            return rc;
        object_g obj = child.out;
        if (!obj)
            return ERROR;
        parsed += child.end;

        size_t objsize = obj->size();
        if (expression_p eq = obj->as<expression>())
            obj = eq->objects(&objsize);

        byte *objcopy = rt.allocate(objsize);
        if (!objcopy)
            return ERROR;
        memmove(objcopy, (byte *) obj, objsize);

        source = p.source;      // In case of GC
        cp = utf8_codepoint(source + parsed);
        if (cp != ')' && cp != ';')
        {
            rt.syntax_error().source(source + parsed);
            return ERROR;
        }
        parsed = utf8_next(source, parsed, max);
    }

    if (cp != ')')
    {
        rt.unterminated_error().source(p.source, parsed);
        return ERROR;
    }

    // Copy the name last
    size_t namesize = name->size();
    byte *namecopy = rt.allocate(namesize);
    if (!namecopy)
        return ERROR;
    memmove(namecopy, (byte *) name, namesize);


    // Create the function call object
    gcbytes scratch = scr.scratch();
    size_t  alloc   = scr.growth();
    p.end           = parsed;
    p.out           = rt.make<funcall>(ID_funcall, scratch, alloc);
    return p.out ? OK : ERROR;
}


EVAL_BODY(funcall)
// ----------------------------------------------------------------------------
//   Function calls get evaluated immediately
// ----------------------------------------------------------------------------
{
    return o->run(true);
}
