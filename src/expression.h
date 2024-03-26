#ifndef EXPRESSION_H
#define EXPRESSION_H
// ****************************************************************************
//  expression.h                                                    DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of algebraic expressions
//
//     Expressions are simply programs that are rendered and parsed specially
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


#include "program.h"
#include "settings.h"
#include "symbol.h"

GCP(expression);
GCP(grob);
struct grapher;

struct expression : program
// ----------------------------------------------------------------------------
//   An expression is a program with ' and ' as delimiters
// ----------------------------------------------------------------------------
//   We also need special parsing and rendering of algebraic objects
{
    expression(id type, gcbytes bytes, size_t len): program(type, bytes, len) {}
    static size_t required_memory(id i, gcbytes UNUSED bytes, size_t len)
    {
        return program::required_memory(i, bytes, len);
    }

    // Building an expression from an object
    expression(id type, algebraic_r arg);
    static size_t required_memory(id i, algebraic_r arg);

    // Building expressions from one or two arguments
    expression(id type, id op, algebraic_r arg);
    static size_t required_memory(id i, id op, algebraic_r arg);
    expression(id type, id op, algebraic_r x, algebraic_r y);
    static size_t required_memory(id i, id op, algebraic_r x, algebraic_r y);

    // Building expressions from an array of arguments
    expression(id type, id op, algebraic_g arg[], uint arity);
    static size_t required_memory(id i, id op,  algebraic_g arg[], uint arity);

    object_p quoted(id type = ID_object) const;
    static size_t size_in_expression(object_p obj);

    static expression_p make(algebraic_r x, id type = ID_expression)
    {
        if (!x)
            return nullptr;
        return rt.make<expression>(type, x);
    }

    static expression_p make(id op, algebraic_r x, id type = ID_expression)
    {
        if (!x)
            return nullptr;
        return rt.make<expression>(type, op, x);
    }

    static expression_p make(id op, algebraic_r x, algebraic_r y,
                           id type = ID_expression)
    {
        if (!x || !y)
            return nullptr;
        return rt.make<expression>(type, op, x, y);
    }

    static expression_p make(id op, algebraic_g args[], uint arity,
                             id type = ID_expression)
    {
        for (uint a = 0; a < arity; a++)
            if (!args[a])
                return nullptr;
        return rt.make<expression>(type, op, args, arity);
    }

    expression_p rewrite(expression_r from, expression_r to) const;
    expression_p rewrite(expression_p from, expression_p to) const
    {
        return rewrite(expression_g(from), expression_g(to));
    }
    expression_p rewrite(size_t size, const byte_p rewrites[]) const;
    expression_p rewrite_all(size_t size, const byte_p rewrites[]) const;

    static expression_p rewrite(expression_r eq,
                                expression_r from,
                                expression_r to)
    {
        return eq->rewrite(from, to);
    }

    template <typename ...args>
    expression_p rewrite(args... rest) const
    {
        static constexpr byte_p rewrites[] = { rest.as_bytes()... };
        return rewrite(sizeof...(rest), rewrites);
    }

    template <typename ...args>
    expression_p rewrite_all(args... rest) const
    {
        static constexpr byte_p rewrites[] = { rest.as_bytes()... };
        return rewrite_all(sizeof...(rest), rewrites);
    }

    expression_p expand() const;
    expression_p collect() const;
    expression_p simplify() const;
    expression_p as_difference_for_solve() const; // Transform A=B into A-B
    object_p     outermost_operator() const;
    size_t       render(renderer &r, bool quoted = false) const
    {
        return render(this, r, quoted);
    }
    algebraic_p        simplify_products() const;
    static algebraic_p factor_out(algebraic_g  expr,
                                  algebraic_g  factor,
                                  algebraic_g &scale,
                                  algebraic_g &exponent);

protected:
    static symbol_p render(uint depth, int &precedence, bool edit);
    static size_t   render(const expression *o, renderer &r, bool quoted);
    static symbol_p parentheses(symbol_g what);
    static symbol_p space(symbol_g what);

public:
    static grob_p   graph(grapher &g, uint depth, int &precedence);
    static grob_p   parentheses(grapher &g, grob_g x, uint padding = 0);
    static grob_p   sqrt(grapher &g, grob_g x);
    static grob_p   ratio(grapher &g, grob_g x, grob_g y);
    static grob_p   ratio(grapher &g, cstring x, grob_g y);
    static grob_p   infix(grapher &g,
                          coord vx, grob_g x,
                          coord vs, grob_g sep,
                          coord vy, grob_g y);
    static grob_p   infix(grapher &g,
                          coord vx, grob_g x,
                          coord vs, cstring sep,
                          coord vy, grob_g y);
    static grob_p   suscript(grapher &g,
                             coord vx, grob_g x,
                             coord vy, grob_g y,
                             int dir=1);
    static grob_p   suscript(grapher &g,
                             coord vx, grob_g x,
                             coord vy, cstring exp,
                             int dir=1);
    static grob_p   suscript(grapher &g,
                             coord vx, cstring x,
                             coord vy, grob_g y,
                             int dir=1);
    static grob_p   prefix(grapher &g,
                           coord vx, grob_g x,
                           coord vy, grob_g y,
                           int dir=0);
    static grob_p   prefix(grapher &g,
                           coord vx, cstring pfx,
                           coord vy, grob_g y,
                           int dir=0);

public:
    OBJECT_DECL(expression);
    PARSE_DECL(expression);
    RENDER_DECL(expression);
    GRAPH_DECL(expression);
    HELP_DECL(expression);

public:
    // Dependent and independent variables
    static symbol_g    *independent;
    static object_g    *independent_value;
    static symbol_g    *dependent;
    static object_g    *dependent_value;
};



// ============================================================================
//
//    C++ expression building (to create rules in C++ code)
//
// ============================================================================

template <byte ...args>
struct eq
// ----------------------------------------------------------------------------
//   A static expression builder for C++ code
// ----------------------------------------------------------------------------
{
    eq() {}
    static constexpr byte object_data[sizeof...(args) + 2] =
    {
        object::ID_expression,
        byte(sizeof...(args)),  // Must be less than 128...
        args...
    };
    constexpr byte_p as_bytes() const
    {
        return object_data;
    }
    constexpr expression_p as_expression() const
    {
        return expression_p(object_data);
    }

    // Negation operation
    eq<args..., object::ID_neg>
    operator-()         { return eq<args..., object::ID_neg>(); }

#define EQ_FUNCTION(name)                                       \
    eq<args..., object::ID_##name>                              \
    name()         { return eq<args..., object::ID_##name>(); }

    EQ_FUNCTION(sqrt);
    EQ_FUNCTION(cbrt);

    EQ_FUNCTION(sin);
    EQ_FUNCTION(cos);
    EQ_FUNCTION(tan);
    EQ_FUNCTION(asin);
    EQ_FUNCTION(acos);
    EQ_FUNCTION(atan);

    EQ_FUNCTION(sinh);
    EQ_FUNCTION(cosh);
    EQ_FUNCTION(tanh);
    EQ_FUNCTION(asinh);
    EQ_FUNCTION(acosh);
    EQ_FUNCTION(atanh);

    EQ_FUNCTION(log1p);
    EQ_FUNCTION(expm1);
    EQ_FUNCTION(log);
    EQ_FUNCTION(log10);
    EQ_FUNCTION(log2);
    EQ_FUNCTION(exp);
    EQ_FUNCTION(exp10);
    EQ_FUNCTION(exp2);
    EQ_FUNCTION(erf);
    EQ_FUNCTION(erfc);
    EQ_FUNCTION(tgamma);
    EQ_FUNCTION(lgamma);

    EQ_FUNCTION(abs);
    EQ_FUNCTION(sign);
    EQ_FUNCTION(inv);
    EQ_FUNCTION(neg);
    EQ_FUNCTION(sq);
    EQ_FUNCTION(cubed);
    EQ_FUNCTION(fact);

    EQ_FUNCTION(re);
    EQ_FUNCTION(im);
    EQ_FUNCTION(arg);
    EQ_FUNCTION(conj);

#undef EQ_FUNCTION

    // Arithmetic
    template<byte ...y>
    eq<args..., y..., object::ID_add>
    operator+(eq<y...>) { return eq<args..., y..., object::ID_add>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_sub>
    operator-(eq<y...>) { return eq<args..., y..., object::ID_sub>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_mul>
    operator*(eq<y...>) { return eq<args..., y..., object::ID_mul>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_div>
    operator/(eq<y...>) { return eq<args..., y..., object::ID_div>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_mod>
    operator%(eq<y...>) { return eq<args..., y..., object::ID_mod>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_rem>
    rem(eq<y...>) { return eq<args..., y..., object::ID_rem>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_pow>
    operator^(eq<y...>) { return eq<args..., y..., object::ID_pow>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_pow>
    pow(eq<y...>) { return eq<args..., y..., object::ID_pow>(); }

    // Comparisons
    template<byte ...y>
    eq<args..., y..., object::ID_TestLT>
    operator<(eq<y...>) { return eq<args..., y..., object::ID_TestLT>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_TestEQ>
    operator==(eq<y...>) { return eq<args..., y..., object::ID_TestEQ>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_TestGT>
    operator>(eq<y...>) { return eq<args..., y..., object::ID_TestGT>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_TestLE>
    operator<=(eq<y...>) { return eq<args..., y..., object::ID_TestLE>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_TestNE>
    operator!=(eq<y...>) { return eq<args..., y..., object::ID_TestNE>(); }

    template<byte ...y>
    eq<args..., y..., object::ID_TestGE>
    operator>=(eq<y...>) { return eq<args..., y..., object::ID_TestGE>(); }

};

#define EQ_FUNCTION(name)                               \
    template<byte ...x>                                 \
    eq<x..., object::ID_##name>                         \
    name(eq<x...> xeq)         { return xeq.name(); }


EQ_FUNCTION(sqrt);
EQ_FUNCTION(cbrt);

EQ_FUNCTION(sin);
EQ_FUNCTION(cos);
EQ_FUNCTION(tan);
EQ_FUNCTION(asin);
EQ_FUNCTION(acos);
EQ_FUNCTION(atan);

EQ_FUNCTION(sinh);
EQ_FUNCTION(cosh);
EQ_FUNCTION(tanh);
EQ_FUNCTION(asinh);
EQ_FUNCTION(acosh);
EQ_FUNCTION(atanh);

EQ_FUNCTION(log1p);
EQ_FUNCTION(expm1);
EQ_FUNCTION(log);
EQ_FUNCTION(log10);
EQ_FUNCTION(log2);
EQ_FUNCTION(exp);
EQ_FUNCTION(exp10);
EQ_FUNCTION(exp2);
EQ_FUNCTION(erf);
EQ_FUNCTION(erfc);
EQ_FUNCTION(tgamma);
EQ_FUNCTION(lgamma);

EQ_FUNCTION(abs);
EQ_FUNCTION(sign);
EQ_FUNCTION(inv);
EQ_FUNCTION(neg);
EQ_FUNCTION(sq);
EQ_FUNCTION(cubed);
EQ_FUNCTION(fact);

EQ_FUNCTION(re);
EQ_FUNCTION(im);
EQ_FUNCTION(arg);
EQ_FUNCTION(conj);

#undef EQ_FUNCTION

// Pi constant
// struct eq_pi : eq<object::ID_pi> {};

// Build a symbol out of a character
template <byte c>       struct eq_symbol  : eq<object::ID_symbol,  1, c> {};

// Build an integer constant
template <uint c, std::enable_if_t<(c >= 0 && c < 128), bool> = true>
struct eq_integer : eq<object::ID_integer, byte(c)> {};
template <int c, std::enable_if_t<(c <= 0 && c >= -128), bool> = true>
struct eq_neg_integer : eq<object::ID_neg_integer, byte(-c)> {};



// ============================================================================
//
//   User commands
//
// ============================================================================

COMMAND_DECLARE(Rewrite);

#endif // EXPRESSION_H
