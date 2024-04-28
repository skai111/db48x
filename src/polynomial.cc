// ****************************************************************************
//  polynomial.c                                                  DB48X project
// ****************************************************************************
//
//   File Description:
//
//    Dense representation of multivariate polynomials
//
//    Some operations on polynomials are much easier or faster if done
//    with a numerical representation of the coefficients.
//    We choose a dense representation here in line with the primary objective
//    of DB48X to run on very memory-constrainted machines like the DM42
//
//
//
// ****************************************************************************
//   (C) 2024 Christophe de Dinechin <christophe@dinechin.org>
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

#include "polynomial.h"

#include "arithmetic.h"
#include "expression.h"
#include "grob.h"
#include "integer.h"
#include "leb128.h"


polynomial_p polynomial::make(algebraic_p value)
// ----------------------------------------------------------------------------
//   Convert a value into an algebraic with zero variables
// ----------------------------------------------------------------------------
{
    if (!value || value->type() == ID_polynomial)
        return polynomial_p(value);

    if (expression_g expr = value->as<expression>())
    {
        value = nullptr;
        if (object_p quoted = expr->as_quoted())
            if (algebraic_p alg = quoted->as_algebraic())
                value = alg;
        if (!value)
            return make(expr);
    }

    if (symbol_g sym = value->as<symbol>())
        return make(sym);
    if (!value->is_numeric_constant())
        return nullptr;

    // Case where we have a numerical constant
    scribble scr;
    algebraic_g avalue = value;
    size_t sz = value->size();
    byte *p = rt.allocate(1 + sz);
    if (!p)
        return nullptr;
    *p++ = 0; // Number of variables = 0
    memcpy(p, +avalue, sz);
    gcbytes data = scr.scratch();
    size_t datasz = scr.growth();
    return rt.make<polynomial>(data, datasz);
}


polynomial_p polynomial::make(symbol_p name)
// ----------------------------------------------------------------------------
//   Convert a name into an algebraic with a single variable
// ----------------------------------------------------------------------------
{
    if (!name || name->type() != ID_symbol)
        return nullptr;

    scribble scr;
    symbol_g aname = name;
    byte_p   src   = name->payload();
    byte_p   p     = src;
    size_t   len   = leb128<size_t>(p);
    size_t   namesz = p + len - src;
    size_t   polysz = namesz + integer::required_memory(ID_integer, 1) + 2;
    byte    *dst   = rt.allocate(polysz);
    if (!dst)
        return nullptr;
    dst = leb128(dst, 1);               // Number of variables = 1
    memcpy(dst, src, namesz);           // Copy name
    dst += namesz;
    dst = leb128(dst, ID_integer);      // Encode constant 1 (scaling factor)
    dst = leb128(dst, 1);
    dst = leb128(dst, 1);               // Encode exponent 1
    gcbytes data = scr.scratch();
    size_t datasz = scr.growth();
    return rt.make<polynomial>(data, datasz);
}


static bool polynomial_op(size_t depth, polynomial_p (*op)(polynomial_r x))
// ----------------------------------------------------------------------------
//   Unary operation
// ----------------------------------------------------------------------------
{
    if (rt.depth() - depth >= 1)
        if (polynomial_g arg = rt.top()->as<polynomial>())
            if (polynomial_p result = op(arg))
                if (rt.top(result))
                    return true;
    return false;
}


static bool polynomial_op(size_t depth, polynomial_p (*op)(polynomial_r x,
                                                           polynomial_r y))
// ----------------------------------------------------------------------------
//   Binary operation
// ----------------------------------------------------------------------------
{
    if (rt.depth() - depth >= 2)
        if (polynomial_g x = rt.pop()->as<polynomial>())
            if (polynomial_g y = rt.top()->as<polynomial>())
                if (polynomial_p result = op(y, x))
                    if (rt.top(result))
                        return true;
    return false;
}


static bool polynomial_op(size_t depth,
                          polynomial_p (*op)(polynomial_r y,
                                             integer_r x),
                          integer_r xi)
// ----------------------------------------------------------------------------
//   Binary power operation
// ----------------------------------------------------------------------------
{
    if (xi)
        if (rt.depth() - depth >= 2)
            if (polynomial_g x = rt.pop()->as<polynomial>())
                if (polynomial_g y = rt.top()->as<polynomial>())
                    if (polynomial_p result = op(y, xi))
                        if (rt.top(result))
                            return true;
    return false;
}


polynomial_p polynomial::make(expression_p expr, bool error)
// ----------------------------------------------------------------------------
//   Check if an expression has the right structure for a polynomial
// ----------------------------------------------------------------------------
{
    // If the expression is already a polynomial, return it
    if (!expr || expr->type() == ID_polynomial)
        return polynomial_p(expr);
    if (expr->type() != ID_expression)
    {
        if (error)
            rt.type_error();
        return nullptr;
    }

    // First check that what we have is compatible with expectations
    size_t    depth = rt.depth();
    integer_g power = nullptr;
    for (object_p obj : *expr)
    {
        ASSERT(obj && "We must have valid objects in expressions");
        id ty = obj->type();

        // Save integer exponents for `pow`
        if (ty == ID_integer)
            power = integer_p(obj);
        else if (ty != ID_pow)
            power = nullptr;

        // Check which types are valid in a polynomial
        if (is_real(ty) || (ty == ID_polar || ty == ID_rectangular))
        {
            algebraic_g arg = algebraic_p(obj);
            polynomial_g poly = make(arg);
            if (!poly)
                goto error;
            rt.push(+poly);
        }
        else if (ty == ID_symbol)
        {
            symbol_g sym = symbol_p(obj);
            polynomial_g poly = make(sym);
            if (!poly)
                goto error;
            rt.push(+poly);
        }
        else if (ty == ID_neg)
        {
            if (!polynomial_op(depth, neg))
                goto error;
        }
        else if (ty == ID_add)
        {
            if (!polynomial_op(depth, add))
                goto error;
        }
        else if (ty == ID_sub)
        {
            if (!polynomial_op(depth, sub))
                goto error;
        }
        else if (ty == ID_mul)
        {
            if (!polynomial_op(depth, mul))
                goto error;
        }
        else if (ty == ID_pow)
        {
            if (!polynomial_op(depth, pow, power))
                goto error;
        }
        else
        {
            // All other operators are invalid in a polynom
            if (error)
                rt.value_error();
            goto error;
        }
    }

    if (rt.depth() == depth + 1)
        if (polynomial_p result = rt.pop()->as<polynomial>())
            return result;

error:
    // Case where we had an error: drop anything we pushed on the stack
    if (size_t removing = rt.depth() - depth)
        rt.drop(removing);
    return nullptr;
}


byte *polynomial::copy_variables(polynomial_r x, byte *prev)
// ----------------------------------------------------------------------------
//   Copy variables from an existing polynomial, return pointer at end
// ----------------------------------------------------------------------------
{
    if (!x)
        return nullptr;

    gcmbytes gprev  = prev;
    size_t   ovars  = prev ? leb128<size_t>(prev) : 0;
    size_t   ovoffs = prev  - +gprev;

    byte_p   xp     = x->payload();
    size_t   xsz    = leb128<size_t>(xp);
    size_t   nvars  = leb128<size_t>(xp);
    size_t   offset = xp - byte_p(+x);

    // Insert variables in copy
    for (size_t v = 0; v < nvars; v++)
    {
        if (offset >= xsz)
            return nullptr;

        // Scan next variable in polynomial x
        xp = byte_p(+x) + offset;
        size_t vlen = leb128<size_t>(xp);

        // Check if a copy of that variable already exists
        byte_p old  = nullptr;
        int    cmp  = -1;
        if (prev)
        {
            // Restart from beginning of variables
            prev = gprev + ovoffs;
            for (size_t ov = 0; ov < ovars; ov++)
            {
                byte_p oldvar = prev;
                size_t ovlen = leb128<size_t>(prev);
                cmp   = memcmp(prev, xp, std::min(ovlen, vlen));
                if (cmp >= 0)
                {
                    old = oldvar;
                    if (cmp == 0)
                        cmp = ovlen - vlen;
                    break;
                }
            }
        }

        size_t vsz  = leb128size(vlen) + vlen;
        if (cmp)
        {
            // Size needed for variable
            size_t offs = old - +gprev;
            bool vszchg = !prev || leb128size(ovars+1) != leb128size(ovars);
            byte  *copy = rt.allocate(vsz + vszchg);
            if (!copy)
                return nullptr;
            ovars++;
            if (!prev)
            {
                gprev = prev = copy;
                copy = (byte *) leb128(+gprev, ovars);
            }
            else
            {
                if (vszchg)
                    memmove((byte *) +gprev + 1, +gprev, copy - +gprev);
                leb128(+gprev, ovars);
            }
            if (!old)
            {
                memcpy(copy, byte_p(+x) + offset, vsz);
            }
            else
            {
                old = +gprev + offs;
                size_t copysz = copy - old;
                memmove((byte *) old + vsz, old, copysz);
                memcpy((byte *) old, byte_p(+x) + offset, vsz);
            }
        }
        offset += vsz;
    }

    return (byte *) +gprev;
}


polynomial_p polynomial::neg(polynomial_r x)
// ----------------------------------------------------------------------------
//  Negate a polynomial by negating the constant in all terms
// ----------------------------------------------------------------------------
{
    scribble scr;
    gcbytes  polycopy = copy_variables(x);
    size_t   nvars    = x->variables();
    for (auto term : *x)
    {
        algebraic_g factor = term.factor();
        factor = -factor;
        size_t sz = factor->size();
        byte *np = rt.allocate(sz);
        if (!np)
            return nullptr;
        memcpy(np, +factor, sz);
        for (size_t v = 0; v < nvars; v++)
        {
            ularge exponent = term.exponent();
            byte *ep = rt.allocate(leb128size(exponent));
            if (!ep)
                return nullptr;
            leb128(ep, exponent);
        }
    }
    gcbytes data = scr.scratch();
    size_t datasz = scr.growth();
    return rt.make<polynomial>(data, datasz);
}


polynomial_p polynomial::addsub(polynomial_r x, polynomial_r y, bool sub)
// ----------------------------------------------------------------------------
//  Add or subtract two polynomials
// ----------------------------------------------------------------------------
{
    scribble scr;
    gcbytes  result = copy_variables(x);
    result          = copy_variables(y, (byte *) +result);
    if (!result)
        return nullptr;

    byte_p p        = +result;
    size_t nvars    = leb128<size_t>(p);
    size_t xvars    = x->variables();
    size_t yvars    = y->variables();
    ularge xexp[nvars];
    ularge yexp[nvars];
    size_t xvar[xvars];
    size_t yvar[yvars];

    // Map variables in x and y to variables in the result
    for (size_t v = 0; v < nvars; v++)
    {
        size_t nlen = leb128<size_t>(p);
        for (size_t xv = 0; xv < xvars; xv++)
        {
            size_t xlen = 0;
            utf8   xname = x->variable(xv, &xlen);
            if (xlen == nlen && memcmp(xname, p, xlen) == 0)
                xvar[xv] = v;
        }
        for (size_t yv = 0; yv < yvars; yv++)
        {
            size_t ylen = 0;
            utf8   yname = y->variable(yv, &ylen);
            if (ylen == nlen && memcmp(yname, p, ylen) == 0)
                yvar[yv] = v;
        }
        p += nlen;
    }

    // Add all the terms in X
    for (auto xterm : *x)
    {
        for (size_t v = 0; v < nvars; v++)
            xexp[v] = 0;

        // Computer the factor of the variables in polynomial x
        algebraic_g xfactor = xterm.factor();
        for (size_t xv = 0; xv < xvars; xv++)
            xexp[xvar[xv]] = xterm.exponent();

        // Check if we have the same factors in polynomial y
        for (auto yterm : *y)
        {
            for (size_t v = 0; v < nvars; v++)
                yexp[v] = 0;

            algebraic_g yfactor = yterm.factor();
            for (size_t yv = 0; yv < yvars; yv++)
                yexp[yvar[yv]] = yterm.exponent();

            bool sameexps = true;
            for (size_t v = 0; sameexps && v < nvars; v++)
                sameexps = xexp[v] == yexp[v];
            if (sameexps)
                xfactor = sub ? xfactor - yfactor : xfactor + yfactor;
        }
        if (!xfactor)
            return nullptr;
        if (!xfactor->is_zero())
        {
            size_t sz = xfactor->size();
            byte *p = rt.allocate(sz);
            if (!p)
                return nullptr;
            memcpy(p, +xfactor, sz);
            p += sz;
            for (size_t v = 0; v < nvars; v++)
            {
                p = rt.allocate(leb128size(xexp[v]));
                if (!p)
                    return nullptr;
                leb128(p, xexp[v]);
            }
        }
    }

    // Add all the terms in Y
    for (auto yterm : *y)
    {
        for (size_t v = 0; v < nvars; v++)
            yexp[v] = 0;

        // Computer the factor of the variables in polynomial y
        algebraic_g yfactor = yterm.factor();
        for (size_t yv = 0; yv < yvars; yv++)
            yexp[yvar[yv]] = yterm.exponent();

        // Check if we have the same factors in polynomial X
        for (auto xterm : *x)
        {
            for (size_t v = 0; v < nvars; v++)
                xexp[v] = 0;

            algebraic_g xfactor = xterm.factor();
            for (size_t xv = 0; xv < xvars; xv++)
                xexp[xvar[xv]] = xterm.exponent();

            bool sameexps = true;
            for (size_t v = 0; sameexps && v < nvars; v++)
                sameexps = xexp[v] == yexp[v];
            if (sameexps)
                yfactor = nullptr; // Already done in the X loop
        }

        if (yfactor && !yfactor->is_zero())
        {
            if (sub)
                yfactor = -yfactor;

            size_t sz = yfactor->size();
            byte *p = rt.allocate(sz);
            if (!p)
                return nullptr;
            memcpy(p, +yfactor, sz);
            p += sz;
            for (size_t v = 0; v < nvars; v++)
            {
                p = rt.allocate(leb128size(yexp[v]));
                if (!p)
                    return nullptr;
                leb128(p, yexp[v]);
            }
        }
    }

    gcbytes data = scr.scratch();
    size_t datasz = scr.growth();
    return rt.make<polynomial>(data, datasz);
}


polynomial_p polynomial::add(polynomial_r x, polynomial_r y)
// ----------------------------------------------------------------------------
//  Add two polynomials
// ----------------------------------------------------------------------------
{
    return addsub(x, y, false);
}


polynomial_p polynomial::sub(polynomial_r x, polynomial_r y)
// ----------------------------------------------------------------------------
//  Subtract two polynomials
// ----------------------------------------------------------------------------
{
    return addsub(x, y, true);
}



polynomial_p polynomial::mul(polynomial_r x, polynomial_r y)
// ----------------------------------------------------------------------------
//   Multiply two polynomials
// ----------------------------------------------------------------------------
{
    scribble scr;
    gcbytes  result = copy_variables(x);
    result          = copy_variables(y, (byte *) +result);
    if (!result)
        return nullptr;

    byte_p p        = +result;
    size_t nvars    = leb128<size_t>(p);
    size_t xvars    = x->variables();
    size_t yvars    = y->variables();
    ularge xexp[nvars];
    ularge yexp[nvars];
    size_t xvar[xvars];
    size_t yvar[yvars];

    // Map variables in x and y to variables in the result
    for (size_t v = 0; v < nvars; v++)
    {
        size_t nlen = leb128<size_t>(p);
        for (size_t xv = 0; xv < xvars; xv++)
        {
            size_t xlen = 0;
            utf8   xname = x->variable(xv, &xlen);
            if (xlen == nlen && memcmp(xname, p, xlen) == 0)
                xvar[xv] = v;
        }
        for (size_t yv = 0; yv < yvars; yv++)
        {
            size_t ylen = 0;
            utf8   yname = y->variable(yv, &ylen);
            if (ylen == nlen && memcmp(yname, p, ylen) == 0)
                yvar[yv] = v;
        }
        p += nlen;
    }

    // Loop over all the terms in X
    for (auto xterm : *x)
    {
        for (size_t v = 0; v < nvars; v++)
            xexp[v] = 0;

        // Computer the factor of the variables in polynomial x
        algebraic_g xfactor = xterm.factor();
        for (size_t xv = 0; xv < xvars; xv++)
            xexp[xvar[xv]] = xterm.exponent();

        // Check if we have the same factors in polynomial y
        for (auto yterm : *y)
        {
            for (size_t v = 0; v < nvars; v++)
                yexp[v] = 0;

            algebraic_g yfactor = yterm.factor();
            for (size_t yv = 0; yv < yvars; yv++)
                yexp[yvar[yv]] = yterm.exponent();

            xfactor = xfactor * yfactor;
            if (!xfactor)
                return nullptr;
            if (!xfactor->is_zero())
            {
                size_t sz = xfactor->size();
                byte *p = rt.allocate(sz);
                if (!p)
                    return nullptr;
                memcpy(p, +xfactor, sz);
                p += sz;
                for (size_t v = 0; v < nvars; v++)
                {
                    ularge exp = xexp[v] + yexp[v];
                    p = rt.allocate(leb128size(exp));
                    p = leb128(p, exp);
                }
            }
        }
    }

    gcbytes data = scr.scratch();
    size_t datasz = scr.growth();
    return rt.make<polynomial>(data, datasz);
}



polynomial_p polynomial::div(polynomial_r x, polynomial_r y)
// ----------------------------------------------------------------------------
//  Euclidean divide of polynomials
// ----------------------------------------------------------------------------
{
    polynomial_g q, r;
    if (quorem(x, y, q, r))
        return q;
    return nullptr;
}



polynomial_p polynomial::mod(polynomial_r x, polynomial_r y)
// ----------------------------------------------------------------------------
//  Euclidean remainder of polynomials
// ----------------------------------------------------------------------------
{
    polynomial_g q, r;
    if (quorem(x, y, q, r))
        return r;
    return nullptr;
}


bool polynomial::quorem(polynomial_r x, polynomial_r y,
                        polynomial_g &q, polynomial_g &r)
// ----------------------------------------------------------------------------
//  Quotient and remainder of two polynomials
// ----------------------------------------------------------------------------
{
    rt.unimplemented_error();
    return false;
}



polynomial_p polynomial::pow(polynomial_r x, integer_r y)
// ----------------------------------------------------------------------------
//  Elevate a polynomial to some integer power
// ----------------------------------------------------------------------------
{
    ularge exp = y->value<ularge>();
    polynomial_g r = nullptr;
    polynomial_g m = x;
    while (exp)
    {
        if (exp & 1)
        {
            r = r ? mul(r, m) : +m;
            if (!r)
                return nullptr;
        }
        m = mul(m, m);
        if (!m)
            return nullptr;
        exp >>= 1;
    }

    if (!r)
    {
        algebraic_g one = integer::make(1);
        r = polynomial::make(one);
    }
    return r;
}


size_t polynomial::variables() const
// ----------------------------------------------------------------------------
//   Return the number of variables
// ----------------------------------------------------------------------------
{
    byte_p first  = byte_p(this);
    byte_p p      = payload();
    size_t length = leb128<size_t>(p);
    size_t nvars = leb128<size_t>(p);
    return (size_t(p - first) < length) ? nvars : 0;
}


symbol_g polynomial::variable(size_t index) const
// ----------------------------------------------------------------------------
//   Return the variable at the given index as a symbol
// ----------------------------------------------------------------------------
{
    size_t len = 0;
    utf8 p = variable(index, &len);
    return symbol::make(p, len);
}


utf8 polynomial::variable(size_t index, size_t *len) const
// ----------------------------------------------------------------------------
//   Return the variable at the given index as a symbol
// ----------------------------------------------------------------------------
{
    byte_p first  = byte_p(this);
    byte_p p      = payload();
    size_t length = leb128<size_t>(p);
    size_t nvars  = leb128<size_t>(p);
    if (index >= nvars)
        return nullptr;

    for (size_t v = 0; v < index; v++)
    {
        size_t vlen = leb128<size_t>(p);
        p += vlen;
    }
    if (size_t(p - first) >= length)
        return nullptr;
    size_t vlen = leb128<size_t>(p);
    if (len)
        *len = vlen;
    return p;
}


PARSE_BODY(polynomial)
// ----------------------------------------------------------------------------
//   No parsing for polynomials, they are only generated from expressions
// ----------------------------------------------------------------------------
{
    return SKIP;
}


EVAL_BODY(polynomial)
// ----------------------------------------------------------------------------
//   We can evaluate polynomials a bit faster than usual expressions
// ----------------------------------------------------------------------------
{
    polynomial_g poly = o;
    size_t       nvars = poly->variables();
    algebraic_g  vars[nvars];

    // Evaluate each of the variables exactly once (this is where we save time)
    for (size_t v = 0; v < nvars; v++)
    {
        symbol_g var = poly->variable(v);
        object_p evaluated = var->evaluate();
        if (!evaluated)
            return ERROR;
        algebraic_g alg = evaluated->as_extended_algebraic();
        if (!alg)
        {
            rt.type_error();
            return ERROR;
        }
        vars[v] = alg;
    }

    // Loop over all factors
    algebraic_g result = nullptr;
    for (auto term : *poly)
    {
        algebraic_g factor = term.factor();
        if (!factor->is_zero())
        {
            for (size_t v = 0; v < nvars; v++)
            {
                ularge exponent = term.exponent();
                if (exponent)
                {
                    algebraic_g value = exponent == 1
                        ? vars[v]
                        : ::pow(vars[v], exponent);
                    factor = factor * value;
                    if (!factor)
                        return ERROR;
                }
            }
            result = result ? result + factor : factor;
            if (!result)
                return ERROR;
        }
    }
    if (!result)
        result = +integer::make(0);

    // We are done, push the result
    return rt.push(+result) ? OK : ERROR;
}


RENDER_BODY(polynomial)
// ----------------------------------------------------------------------------
//  Render a polynomial as text
// ----------------------------------------------------------------------------
{
    polynomial_g poly = o;
    size_t       nvars = poly->variables();
    symbol_g     vars[nvars];

    // Get each of the variables
    for (size_t v = 0; v < nvars; v++)
        vars[v] = poly->variable(v);

    // Loop over all factors
    bool first = true;
    unicode mul = Settings.UseDotForMultiplication() ? L'·' : L'×';
    for (auto term : *poly)
    {
        // Separate terms with +
        if (!first)
            r.put('+');
        first = false;

        // Emit the factor
        algebraic_g factor = term.factor();
        bool hasmul = !factor->is_one();
        if (hasmul)
            factor->render(r);

        for (size_t v = 0; v < nvars; v++)
        {
            ularge exponent = term.exponent();
            if (exponent)
            {
                if (hasmul)
                    r.put(mul);
                hasmul = true;
                vars[v]->render(r);
                if (exponent > 1)
                {
                    r.put(unicode(L'↑'));
                    r.printf("%llu", exponent);
                }
            }
        }
        if (!hasmul)
            factor->render(r);
    }

    // We are done, push the result
    return r.size();
}


GRAPH_BODY(polynomial)
// ----------------------------------------------------------------------------
//  Render a polynomial as a graphic expression
// ----------------------------------------------------------------------------
{
    if (Settings.InvertedPolynomialRender())
        std::swap(g.foreground, g.background);

    polynomial_g poly = o;
    size_t       nvars = poly->variables();
    grob_g       vars[nvars];

    // Get each of the variables and render it graphically
    for (size_t v = 0; v < nvars; v++)
    {
        symbol_g sym = poly->variable(v);
        grob_g   var = sym->graph(g);
        vars[v]      = var;
    }


    // Loop over all factors
    grob_g result = nullptr;
    coord   vr     = 0;
    cstring mul    = Settings.UseDotForMultiplication() ? "·" : "×";

    for (auto term : *poly)
    {
        // Render the factor
        algebraic_g factor = term.factor();
        grob_g      factg  = factor->is_one() ? nullptr : factor->graph(g);
        coord       vf     = 0;

        // Render the terms
        for (size_t v = 0; v < nvars; v++)
        {
            ularge exponent = term.exponent();
            if (exponent)
            {
                grob_g termg = vars[v];
                coord  vt    = 0;
                if (exponent > 1)
                {
                    char exptxt[16];
                    snprintf(exptxt, sizeof(exptxt), "%llu", exponent);
                    termg = suscript(g, vt, termg, 0, exptxt);
                    vt = g.voffset;
                }
                if (factg)
                    factg = infix(g, vf, factg, 0, mul, vt, termg);
                else
                    factg = termg;
                vf = g.voffset;
            }
        }

        // Addition of terms
        if (result)
            result = infix(g, vr, result, 0, "+", vf, factg);
        else
            result = factg;
        vr = g.voffset;
    }

    if (Settings.InvertedPolynomialRender())
        std::swap(g.foreground, g.background);

    // We are done, push the result
    return result;
}


FUNCTION_BODY(ToPolynomial)
// ----------------------------------------------------------------------------
//   Convert an expression as a polynomial
// ----------------------------------------------------------------------------
{
    if (!x)
        return nullptr;
    if (polynomial_p poly = polynomial::make(x))
        return poly;
    rt.type_error();
    return nullptr;
}


COMMAND_BODY(FromPolynomial)
// ----------------------------------------------------------------------------
//   Convert a polynomial to an expression
// ----------------------------------------------------------------------------
{
    if (object_p obj = rt.top())
        if (polynomial_p poly = obj->as<polynomial>())
            if (algebraic_p result = poly->as_expression())
                if (rt.top(result))
                    return OK;
    if (!rt.error())
        rt.type_error();
    return ERROR;
}


algebraic_p polynomial::as_expression() const
// ----------------------------------------------------------------------------
//   Rewrite a polynomial as a regular expression
// ----------------------------------------------------------------------------
{
    polynomial_g poly = this;
    size_t       nvars = poly->variables();
    algebraic_g  vars[nvars];

    // Evaluate each of the variables exactly once (this is where we save time)
    for (size_t v = 0; v < nvars; v++)
    {
        symbol_p var = poly->variable(v);
        vars[v] = var;
    }

    // Loop over all factors
    algebraic_g result = nullptr;
    for (auto term : *poly)
    {
        algebraic_g factor = term.factor();
        if (!factor->is_zero())
        {
            for (size_t v = 0; v < nvars; v++)
            {
                ularge exponent = term.exponent();
                if (exponent)
                {
                    algebraic_g value = exponent == 1
                        ? vars[v]
                        : ::pow(vars[v], exponent);
                    factor = factor->is_one() ? value : factor * value;
                    if (!factor)
                        return nullptr;;
                }
            }
            result = result ? result + factor : factor;
            if (!result)
                return nullptr;
        }
    }

    // If we did not have any term, just return 0
    if (!result)
        result = +integer::make(0);

    // We are done, return the result
    return result;
}
