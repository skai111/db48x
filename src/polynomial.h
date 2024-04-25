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
//
/*   A polynomial is represented as a structure very similar to arrays,
     except that the program block is replaced with an array block.

     0. ID_polynomial
     1. Total length for fast skipping
     2. Number of variables
     3. Sequence of variable names, each one being
     3.1 Variable 1 name length
     3.2 Variable 1 name
     4. Length of array block
     5. Sequence of array objects, each being in the form:
     5.1. Factor value, a real or complex number
     5.2. N variable exponents, one per variable

     Exponents are sorted in decreasing lexicographic order.

     For example 2/3 * X^37 * Y^42 + 1.25 * X^23 * Y^55 + (2+3i)*X - 1 is:

     0. ID_polynomials
     1. [Total length]
     2. 2 (two variables, X and Y)
     3. Two variables, Y and X (Y comes first because 55 > 37)
        1 Y 1 X
     4. [Array block length]
     5. Decimal(1.25) 55 23
        Fraction(2/3) 42 37
        Complex(2+3i) 0  1
        Neg_Integer(-1) 0 0

     Polynomials are never parsed directly, but they can be built by symbolic
     operations on expressions.
*/


#include "expression.h"
#include "symbol.h"

GCP(polynomial);

struct polynomial : expression
// ----------------------------------------------------------------------------
//   Representation for polynomials
// ----------------------------------------------------------------------------
{
    polynomial(id type, gcbytes bytes, size_t len)
        : expression(type, bytes, len)
    { }

    // Return total length of the polynomial in bytes
    size_t length() const
    {
        return text::length();
    }

    // Access variables in the polynomial
    size_t   variables() const;
    symbol_g variable(uint index) const;

    // Iterating over factors and exponents
    struct iterator
    {
        typedef iterator &value_type;

        explicit iterator(polynomial_p poly, bool at_end = false)
            : poly(poly), length(), variables(), offset()
        {
            byte_p p     = byte_p(poly);
            byte_p first = p;
            length       = leb128<size_t>(p);
            variables    = leb128<size_t>(p);
            for (size_t v = 0; v < variables; v++)
            {
                // Skip each name
                size_t vlen = leb128<size_t>(p);
                p += vlen;
            }
            offset = at_end ? length : p - first;
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
                && o.length == length
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
            return *this;
        }
        iterator operator++(int)
        {
            return *this;
        }
        value_type operator*()
        {
            return *this;
        }

        polynomial_g poly;
        size_t       length;
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

#endif // POLYNOMIAL_H
