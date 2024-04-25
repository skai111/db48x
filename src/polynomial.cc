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


size_t polynomial::variables() const
// ----------------------------------------------------------------------------
//   Return the number of variables
// ----------------------------------------------------------------------------
{
    byte_p p      = byte_p(this);
    byte_p first  = p;
    size_t length = leb128<size_t>(p);
    size_t nvars = leb128<size_t>(p);
    return (size_t(p - first) < length) ? nvars : 0;
}


symbol_g polynomial::variable(uint index) const
// ----------------------------------------------------------------------------
//   Return the variable at the given index as a symbol
// ----------------------------------------------------------------------------
{
    byte_p p      = byte_p(this);
    byte_p first  = p;
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
    return symbol::make(p, vlen);
}


PARSE_BODY(polynomial)
// ----------------------------------------------------------------------------
//   No parsing for polynomials, they are only generated
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
    algebraic_g result = +integer::make(0);
    for (auto term : *poly)
    {
        algebraic_g factor = term.factor();
        for (size_t v = 0; v < nvars; v++)
        {
            ularge exponent = term.exponent();
            algebraic_g value = +integer::make(exponent);
            value = pow(vars[v], value);
            factor = factor * value;
            if (!factor)
                return ERROR;
        }
        result = result + factor;
        if (!result)
            return ERROR;
    }

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
        factor->render(r);

        for (size_t v = 0; v < nvars; v++)
        {
            ularge exponent = term.exponent();
            if (exponent)
            {
                r.put(mul);
                vars[v]->render(r);
                if (exponent > 1)
                {
                    r.put(unicode(L'↑'));
                    r.printf("%llu", exponent);
                }
            }
        }
    }

    // We are done, push the result
    return r.size();
}


GRAPH_BODY(polynomial)
// ----------------------------------------------------------------------------
//  Render a polynomial as a graphic expression
// ----------------------------------------------------------------------------
{
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
        grob_g      factg  = factor->graph(g);
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
                factg = infix(g, vf, factg, 0, mul, vt, termg);
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

    // We are done, push the result
    return result;
}
