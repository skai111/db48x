#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H
// ****************************************************************************
//  polynomial.h                                                  DB48X project
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
/*
     A polynomial is represented as a structure very similar to arrays,
     except that the program block is replaced with an array block.

     0. ID_polynomial
     1. Total length for fast skipping
     2. Number of variables
     3. Sequence of variable names, each one being
     3.1 Variable 1 name length
     3.2 Variable 1 name
     4. Sequence of array objects, each being in the form:
     4.1. Factor value, a real or complex number
     4.2. N variable exponents, one per variable

     Variables are sorted in alphabetical order
     Exponents are sorted in decreasing lexicographic order

     For example 2/3 * X^37 * Y^42 + 1.25 * X^23 * Y^55 + (2+3i)*X - 1 is:

     0. ID_polynomials
     1. [Total length]
     2. 2 (two variables, X and Y)
     3. Two variables, X and Y (X comes first alphabetically)
        1 X 1 Y
     4. Fraction(2/3) 37 42
        Decimal(1.25) 23 55
        Complex(2+3i) 1  0
        Neg_Integer(-1) 0 0

     Polynomials are never parsed directly, but they can be built by symbolic
     operations on expressions.
*/


#include "expression.h"
#include "runtime.h"
#include "symbol.h"

GCP(polynomial);

struct polynomial : expression
// ----------------------------------------------------------------------------
//   Representation for polynomials
// ----------------------------------------------------------------------------
{
    polynomial(id type, gcbytes by, size_t len) : expression(type, by, len)
    { }

    // Make from individual components or whole expressions
    static polynomial_p make(algebraic_p expr);
    static polynomial_p make(symbol_p expr);
    static polynomial_p make(expression_p expr, bool error = false);

    // Write in the scratchpad a combination of the variables of two polynoms
    static byte *copy_variables(polynomial_r x, byte *previous = nullptr);

    // Add, sub, mul or div by another polynom
    static polynomial_p neg(polynomial_r x);
    static polynomial_p addsub(polynomial_r x, polynomial_r y, bool sub);
    static polynomial_p add(polynomial_r x, polynomial_r y);
    static polynomial_p sub(polynomial_r x, polynomial_r y);
    static polynomial_p mul(polynomial_r x, polynomial_r y);
    static polynomial_p div(polynomial_r x, polynomial_r y);
    static polynomial_p mod(polynomial_r x, polynomial_r y);
    static bool         quorem(polynomial_r x, polynomial_r y,
                               polynomial_g &q, polynomial_g &r);
    static polynomial_p pow(polynomial_r x, integer_r y);

    // Return total length of the polynomial in bytes
    size_t length() const
    {
        return text::length();
    }

    // Access variables in the polynomial
    size_t   variables() const;
    symbol_g variable(size_t index) const;
    utf8     variable(size_t index, size_t *len) const;

    // Iterating over factors and exponents
    struct iterator
    {
        typedef iterator &value_type;

        explicit iterator(polynomial_p poly, bool at_end = false)
            : poly(poly), size(), variables(), offset()
        {
            byte_p first = byte_p(poly);
            byte_p p     = poly->payload();
            size       = leb128<size_t>(p);
            size += p - first;
            variables    = leb128<size_t>(p);
            if (at_end)
            {
                offset = size;
            }
            else
            {
                for (size_t v = 0; v < variables; v++)
                {
                    // Skip each name
                    size_t vlen = leb128<size_t>(p);
                    p += vlen;
                }
                offset = p - first;
            }
        }

        algebraic_p factor()
        {
            algebraic_p scalar    = algebraic_p(poly) + offset;
            object_p    exponents = scalar->skip();
            offset = exponents - object_p(poly);
            return scalar;
        }

        ularge exponent()
        {
            byte_p p = byte_p(poly) + offset;
            uint exp = leb128<ularge>(p);
            offset = p - byte_p(poly);
            return exp;
        }

        bool operator==(const iterator &o) const
        {
            return +o.poly  == +poly
                && o.offset == offset
                && o.size == size
                && o.variables == variables;
        }

        bool operator!=(const iterator &o) const
        {
            return !(o==*this);
        }

        // We don't increment, because it's really factor() and exponent()
        // that increment the offset.
        iterator& operator++()
        {
            if (offset < size)
            {
                factor();
                for (size_t v = 0; v < variables; v++)
                    exponent();
            }
            return *this;
        }
        iterator operator++(int)
        {
            iterator prev = *this;
            ++(*this);
            return prev;
        }
        value_type operator*()
        {
            return *this;
        }

        polynomial_g poly;
        size_t       size;
        size_t       variables;
        size_t       offset;
    };

    iterator begin() const      { return iterator(this); }
    iterator end() const        { return iterator(this, true); }

public:
    OBJECT_DECL(polynomial);
    PARSE_DECL(polynomial);
    EVAL_DECL(polynomial);
    RENDER_DECL(polynomial);
    GRAPH_DECL(polynomial);
};


FUNCTION(ToPolynomial);

#endif // POLYNOMIAL_H
