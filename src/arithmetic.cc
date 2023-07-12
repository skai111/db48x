// ****************************************************************************
//  arithmetic.cc                                                 DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of basic arithmetic operations
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

#include "arithmetic.h"

#include "bignum.h"
#include "decimal-32.h"
#include "decimal-64.h"
#include "decimal128.h"
#include "fraction.h"
#include "functions.h"
#include "integer.h"
#include "list.h"
#include "runtime.h"
#include "settings.h"
#include "text.h"

#include <bit>
#include <bitset>


RECORDER(arithmetic,            16, "Arithmetic");
RECORDER(arithmetic_error,      16, "Errors from arithmetic code");

bool arithmetic::real_promotion(algebraic_g &x, algebraic_g &y)
// ----------------------------------------------------------------------------
//   Promote x or y to the largest of both types
// ----------------------------------------------------------------------------
{
    id xt = x->type();
    id yt = y->type();
    if (is_integer(xt) && is_integer(yt))
        // If we got here, we failed an integer op, e.g. 2/3, promote to real
        return real_promotion(x) && real_promotion(y);

    if (!is_real(xt) || !is_real(yt))
        return false;

    uint16_t prec  = Settings.precision;
    id       minty = prec > BID64_MAXDIGITS ? ID_decimal128
                   : prec > BID32_MAXDIGITS ? ID_decimal64
                                            : ID_decimal32;
    if (is_decimal(xt) && xt > minty)
        minty = xt;
    if (is_decimal(yt) && yt > minty)
        minty = yt;

    return (xt == minty || real_promotion(x, minty))
        && (yt == minty || real_promotion(y, minty));
}


fraction_g arithmetic::fraction_promotion(algebraic_g &x)
// ----------------------------------------------------------------------------
//  Check if we can promote the number to a fraction
// ----------------------------------------------------------------------------
{
    id ty = x->type();
    if (ty >= FIRST_FRACTION_TYPE && ty <= LAST_FRACTION_TYPE)
        return fraction_g((fraction *) object_p(x));
    if (ty >= ID_integer && ty <= ID_neg_integer)
    {
        integer_g n = integer_p(object_p(x));
        integer_g d = integer::make(1);
        fraction_g f = fraction::make(n, d);
        return f;
    }
    if (ty >= ID_bignum && ty <= ID_neg_bignum)
    {
        bignum_g n = bignum_p(object_p(x));
        bignum_g d = bignum::make(1);
        fraction_g f = big_fraction::make(n, d);
        return f;
    }
    return nullptr;
}


template<>
inline bool arithmetic::non_numeric<add>(algebraic_g &x, algebraic_g & y,
                                         object::id &xt, object::id &yt)
// ----------------------------------------------------------------------------
//   Deal with non-numerical data types for addition
// ----------------------------------------------------------------------------
//   This deals with:
//   - Text + text: Concatenation of text
//   - Text + object: Concatenation of text + object text
//   - Object + text: Concatenation of object text + text
{
    if (xt == object::ID_text && yt == object::ID_text)
    {
        text_g xs = x->as<text>();
        text_g ys = y->as<text>();
        x = algebraic_p(xs + ys);
        return true;
    }

    // Not yet implemented
    return false;
}


inline bool add::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   Check if adding two integers works or if we need to promote to real
// ----------------------------------------------------------------------------
{
    // For integer types of the same sign, promote to real if we overflow
    if ((xt == ID_neg_integer) == (yt == ID_neg_integer))
    {
        ularge sum = xv + yv;

        // Do not promot to real if we have based numbers as input
        if ((sum < xv || sum < yv) && is_real(xt) && is_real(yt))
            return false;

        xv = sum;
        // Here, the type of x is the type of the result
        return true;
    }

    // Opposite sign: the difference in magnitude always fit in an integer type
    if (!is_real(xt))
    {
        // Based numbers keep the base of the number in X
        xv = xv - yv;
    }
    else if (yv >= xv)
    {
        // Case of (-3) + (+2) or (+3) + (-2): Change the sign of X
        xv = yv - xv;
        xt = (xv == 0 || xt == ID_neg_integer) ? ID_integer : ID_neg_integer;
    }
    else
    {
        // Case of (-3) + (+4) or (+3) + (-4): Keep the sign of X
        xv = xv - yv;
    }
    return true;
}


inline bool add::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   We can always add two big integers (memory permitting)
// ----------------------------------------------------------------------------
{
    x = x + y;
    return byte_p(x) != nullptr;
}


inline bool add::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   We can always add two fractions
// ----------------------------------------------------------------------------
{
    x = x + y;
    return byte_p(x) != nullptr;
}


inline bool sub::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   Check if subtracting two integers works or if we need to promote to real
// ----------------------------------------------------------------------------
{
    // For integer types of opposite sign, promote to real if we overflow
    if ((xt == ID_neg_integer) != (yt == ID_neg_integer))
    {
        ularge sum = xv + yv;
        if ((sum < xv || sum < yv) && is_real(xt) && is_real(yt))
            return false;
        xv = sum;

        // The type of x gives us the correct sign for the difference:
        //   -2 - 3 is -5, 2 - (-3) is 5:
        return true;
    }

    // Same sign: the difference in magnitude always fit in an integer type
    if (!is_real(xt))
    {
        // Based numbers keep the base of the number in X
        xv = xv - yv;
    }
    else if (yv >= xv)
    {
        // Case of (+3) - (+4) or (-3) - (-4): Change the sign of X
        xv = yv - xv;
        xt = (xv == 0 || xt == ID_neg_integer) ? ID_integer : ID_neg_integer;
    }
    else
    {
        // Case of (-3) - (-2) or (+3) - (+2): Keep the sign of X
        xv = xv - yv;
    }
    return true;
}


inline bool sub::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   We can always subtract two big integers (memory permitting)
// ----------------------------------------------------------------------------
{
    x = x - y;
    return byte_p(x) != nullptr;
}


inline bool sub::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   We can always subtract two fractions (memory permitting)
// ----------------------------------------------------------------------------
{
    x = x - y;
    return byte_p(x) != nullptr;
}


template <>
inline bool arithmetic::non_numeric<mul>(algebraic_g &x,
                                         algebraic_g &y,
                                         object::id  &xt,
                                         object::id  &yt)
// ----------------------------------------------------------------------------
//   Deal with non-numerical data types for multiplication
// ----------------------------------------------------------------------------
//   This deals with:
//   - Text * integer: Repeat the text
//   - Integer * text: Repeat the text
{
    if (xt == object::ID_text && yt == object::ID_integer)
    {
        text_g xs = x->as<text>();
        integer_p ys = y->as<integer>();
        uint yn = ys->value<uint>();
        x = algebraic_p(xs * yn);
        xt = text::ID_text;
        return true;
    }

    if (xt == object::ID_integer && yt == object::ID_text)
    {
        integer_p xs = x->as<integer>();
        uint xn = xs->value<uint>();
        text_g ys = y->as<text>();
        x = algebraic_p(ys * xn);
        xt = text::ID_text;
        return true;
    }

    // Not yet implemented
    return false;
}


inline bool mul::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   Check if multiplying two integers works or if we need to promote to real
// ----------------------------------------------------------------------------
{
    // If one of the two objects is a based number, always use integer mul
    if (!is_real(xt) || !is_real(yt))
    {
        xv = xv * yv;
        return true;
    }

    // Check if there is an overflow
    // Can's use std::countl_zero yet (-std=c++20 breaks DMCP)
    if (std::__countl_zero(xv) + std::__countl_zero(yv) < int(8*sizeof(ularge)))
        return false;

    // Check if the multiplication generates a larger result. Is this correct?
    ularge product = xv * yv;

    // Check the sign of the product
    xt = (xt == ID_neg_integer) == (yt == ID_neg_integer)
        ? ID_integer
        : ID_neg_integer;
    xv = product;
    return true;
}


inline bool mul::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   We can always multiply two big integers (memory permitting)
// ----------------------------------------------------------------------------
{
    x = x * y;
    return byte_p(x) != nullptr;
}


inline bool mul::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   We can always multiply two fractions (memory permitting)
// ----------------------------------------------------------------------------
{
    x = x * y;
    return byte_p(x) != nullptr;
}


inline bool div::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   Check if dividing two integers works or if we need to promote to real
// ----------------------------------------------------------------------------
{
    // Check divide by zero
    if (yv == 0)
    {
        rt.zero_divide_error();
        return false;
    }

    // If one of the two objects is a based number, always used integer div
    if (!is_real(xt) || !is_real(yt))
    {
        xv = xv / yv;
        return true;
    }

    // Check if there is a remainder - If so, switch to fraction
    if (xv % yv)
        return false;

    // Perform the division
    xv = xv / yv;

    // Check the sign of the ratio
    xt = (xt == ID_neg_integer) == (yt == ID_neg_integer)
        ? ID_integer
        : ID_neg_integer;
    return true;
}


inline bool div::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   Division works if there is no remainder
// ----------------------------------------------------------------------------
{
    if (!y)
    {
        rt.zero_divide_error();
        return false;
    }
    bignum_g q = nullptr;
    bignum_g r = nullptr;
    id type = bignum::product_type(x->type(), y->type());
    bool result = bignum::quorem(x, y, type, &q, &r);
    if (result)
        result = bignum_p(r) != nullptr;
    if (result)
    {
        if (r->is_zero())
            x = q;                  // Integer result
        else
            x = bignum_p(fraction_p(big_fraction::make(x, y))); // Wrong-cast
    }
    return result;
}


inline bool div::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   Division of fractions, except division by zero
// ----------------------------------------------------------------------------
{
    if (!y->numerator())
    {
        rt.zero_divide_error();
        return false;
    }
    x = x / y;
    return byte_p(x) != nullptr;
}


inline bool mod::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   The modulo of two integers is always an integer
// ----------------------------------------------------------------------------
{
    // Check divide by zero
    if (yv == 0)
    {
        rt.zero_divide_error();
        return false;
    }

    // If one of the two objects is a based number, always used integer mod
    if (!is_real(xt) || !is_real(yt))
    {
        xv = xv % yv;
        return true;
    }

    // Perform the modulo
    xv = xv % yv;
    if (xt == ID_neg_integer)
        xv = xv - yv;

    // The resulting type is always positive
    xt = ID_integer;
    return true;
}


inline bool mod::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   Modulo always works except divide by zero
// ----------------------------------------------------------------------------
{
    bignum_g r = x % y;
    if (byte_p(r) == nullptr)
        return false;
    if (y->type() == ID_neg_bignum)
        x = y - r;
    else
        x = r;
    return byte_p(x) != nullptr;
}


inline bool mod::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   Modulo of fractions, except division by zero
// ----------------------------------------------------------------------------
{
    if (!y->numerator())
    {
        rt.zero_divide_error();
        return false;
    }
    x = x % y;
    return byte_p(x) != nullptr;
}


inline bool rem::integer_ok(object::id &UNUSED xt, object::id &UNUSED yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   The reminder of two integers is always an integer
// ----------------------------------------------------------------------------
{
    // Check divide by zero
    if (yv == 0)
    {
        rt.zero_divide_error();
        return false;
    }

    // The type of the result is always the type of x
    xv = xv % yv;
    return true;
}


inline bool rem::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   Remainder always works except divide by zero
// ----------------------------------------------------------------------------
{
    x = x % y;
    return byte_p(x) != nullptr;
}


inline bool rem::fraction_ok(fraction_g &x, fraction_g &y)
// ----------------------------------------------------------------------------
//   Modulo of fractions, except division by zero
// ----------------------------------------------------------------------------
{
    if (!y->numerator())
    {
        rt.zero_divide_error();
        return false;
    }
    x = x % y;
    return byte_p(x) != nullptr;
}


inline bool pow::integer_ok(object::id &xt, object::id &yt,
                            ularge &xv, ularge &yv)
// ----------------------------------------------------------------------------
//   Compute Y^X
// ----------------------------------------------------------------------------
{
    // Check 0^0
    if (xv == 0 && yv == 0)
    {
        rt.undefined_operation_error();
        return false;
    }

    // Cannot raise to a negative power as integer
    if (yt == ID_neg_integer)
        return false;

    // Check the type of the result
    if (xt == ID_neg_integer)
        xt = (yv & 1) ? ID_neg_integer : ID_integer;

    // Compute result, check that it does not overflow
    ularge r = 1;
    while (yv)
    {
        if (yv & 1)
        {
            ularge p = r * xv;
            if (p < r || p < xv)
                return false;   // Integer overflow
            r = p;
        }
        yv /= 2;

        ularge nxv = xv * xv;
        if (yv && nxv < xv)
            return false;       // Integer overflow
        xv = nxv;
    }

    xv = r;
    return true;
}


inline bool pow::bignum_ok(bignum_g &x, bignum_g &y)
// ----------------------------------------------------------------------------
//   Compute y^x, works if x >= 0
// ----------------------------------------------------------------------------
{
    // Compute result, check that it does not overflow
    if (x->type() == ID_neg_integer)
        return false;
    x = bignum::pow(x, y);
    return byte_p(x) != nullptr;
}


inline bool pow::fraction_ok(fraction_g &UNUSED x, fraction_g &UNUSED y)
// ----------------------------------------------------------------------------
//   Compute y^x, works if x >= 0
// ----------------------------------------------------------------------------
{
    return false;
}


inline bool hypot::integer_ok(object::id &UNUSED xt, object::id &UNUSED yt,
                              ularge &UNUSED xv, ularge &UNUSED yv)
// ----------------------------------------------------------------------------
//   hypot() involves a square root, so not working on integers
// ----------------------------------------------------------------------------
//   Not trying to optimize the few cases where it works, e.g. 3^2+4^2=5^2
{
    return false;
}


inline bool hypot::bignum_ok(bignum_g &UNUSED x, bignum_g &UNUSED y)
// ----------------------------------------------------------------------------
//   Hypot never works with big integers
// ----------------------------------------------------------------------------
{
    return false;
}


inline bool hypot::fraction_ok(fraction_g &UNUSED x, fraction_g &UNUSED y)
// ----------------------------------------------------------------------------
//   Hypot never works with big integers
// ----------------------------------------------------------------------------
{
    return false;
}


inline bool atan2::integer_ok(object::id &UNUSED xt, object::id &UNUSED yt,
                              ularge &UNUSED xv, ularge &UNUSED yv)
// ----------------------------------------------------------------------------
//   atan2() involves a square root, so not working on integers
// ----------------------------------------------------------------------------
//   Not trying to optimize the few cases where it works, e.g. 3^2+4^2=5^2
{
    return false;
}


inline bool atan2::bignum_ok(bignum_g &UNUSED x, bignum_g &UNUSED y)
// ----------------------------------------------------------------------------
//   Atan2 never works with big integers
// ----------------------------------------------------------------------------
{
    return false;
}


inline bool atan2::fraction_ok(fraction_g &UNUSED x, fraction_g &UNUSED y)
// ----------------------------------------------------------------------------
//   Atan2 never works with big integers
// ----------------------------------------------------------------------------
{
    return false;
}



// ============================================================================
//
//   Shared evaluation code
//
// ============================================================================

algebraic_g arithmetic::evaluate(id             op,
                                 algebraic_g &  x,
                                 algebraic_g &  y,
                                 bid128_fn      op128,
                                 bid64_fn       op64,
                                 bid32_fn       op32,
                                 integer_fn     integer_ok,
                                 bignum_fn      bignum_ok,
                                 fraction_fn    fraction_ok,
                                 non_numeric_fn non_numeric)
// ----------------------------------------------------------------------------
//   Shared code for all forms of evaluation, does not use the RPL stack
// ----------------------------------------------------------------------------
{
    id xt = x->type();
    id yt = y->type();

    /* Integer types */
    bool ok = false;

    if (!ok && is_integer(xt) && is_integer(yt))
    {
        if (!is_bignum(xt) && !is_bignum(yt))
        {
            /* Perform conversion of integer values to the same base */
            integer_p xi = integer_p(object_p(x));
            integer_p yi = integer_p(object_p(y));
            if (xi->native() && yi->native())
            {
                ularge xv = xi->value<ularge>();
                ularge yv = yi->value<ularge>();
                if (integer_ok(xt, yt, xv, yv))
                {
                    x = rt.make<integer>(xt, xv);
                    ok = object_p(x) != nullptr;
                }
            }
            if (rt.error())
                return nullptr;
        }

        if (!ok)
        {
            if (!is_bignum(xt))
                xt = bignum_promotion(x);
            if (!is_bignum(yt))
                yt = bignum_promotion(y);

            // Proceed with big integers if native did not fit
            bignum_g xg = bignum_p(object_p(x));
            bignum_g yg = bignum_p(object_p(y));
            if (bignum_ok(xg, yg))
            {
                x = bignum_p(xg);
                ok = object_p(x) != nullptr;
            }
            if (rt.error())
                return nullptr;
        }
    }

    /* Fraction types */
    if (!ok && (x->is_fraction() || y->is_fraction() ||
                (op == ID_div && x->is_fractionable() && y->is_fractionable())))
    {
        if (fraction_g xf = fraction_promotion(x))
        {
            if (fraction_g yf = fraction_promotion(y))
            {
                ok = fraction_ok(xf, yf);
                if (ok)
                {
                    x = algebraic_p(fraction_p(xf));
                    ok = object_p(x);
                    if (ok)
                    {
                        bignum_g d = xf->denominator();
                        if (d->is(1))
                        {
                            x = algebraic_p(bignum_p(xf->numerator()));
                            ok = object_p(x);
                        }
                    }
                }
            }
        }
    }

    /* Real data types */
    if (!ok && real_promotion(x, y))
    {
        /* Here, x and y have the same type, a decimal type */
        xt = x->type();
        switch(xt)
        {
        case ID_decimal32:
        {
            bid32 xv = x->as<decimal32>()->value();
            bid32 yv = y->as<decimal32>()->value();
            bid32 res;
            op32(&res.value, &xv.value, &yv.value);
            x = rt.make<decimal32>(ID_decimal32, res);
            ok = true;
            break;
        }
        case ID_decimal64:
        {
            bid64 xv = x->as<decimal64>()->value();
            bid64 yv = y->as<decimal64>()->value();
            bid64 res;
            op64(&res.value, &xv.value, &yv.value);
            x = rt.make<decimal64>(ID_decimal64, res);
            ok = true;
            break;
        }
        case ID_decimal128:
        {
            bid128 xv = x->as<decimal128>()->value();
            bid128 yv = y->as<decimal128>()->value();
            bid128 res;
            op128(&res.value, &xv.value, &yv.value);
            x = rt.make<decimal128>(ID_decimal128, res);
            ok = true;
            break;
        }
        default:
            break;
        }
    }

    if (!ok)
        ok = non_numeric(x, y, xt, yt);

    if (!ok && x->is_symbolic() && y->is_symbolic())
    {
        x = rt.make<equation>(ID_equation, op, x, y);
        if (!x)
            return nullptr;
        ok = true;
    }

    if (!ok)
    {
        rt.type_error();
        return nullptr;
    }

    return x;
}


object::result arithmetic::evaluate(id             op,
                                    bid128_fn      op128,
                                    bid64_fn       op64,
                                    bid32_fn       op32,
                                    integer_fn     integer_ok,
                                    bignum_fn      bignum_ok,
                                    fraction_fn    fraction_ok,
                                    non_numeric_fn non_numeric)
// ----------------------------------------------------------------------------
//   Shared code for all forms of evaluation using the RPL stack
// ----------------------------------------------------------------------------
{
    // Fetch arguments from the stack
    algebraic_g y = (algebraic_p) rt.stack(1);
    if (!y)
        return ERROR;
    algebraic_g x = (algebraic_p) rt.stack(0);
    if (!x)
        return ERROR;

    // Evaluate the operation
    algebraic_g r = evaluate(op, y, x, op128, op64, op32,
                             integer_ok, bignum_ok, fraction_ok, non_numeric);

    // If result is valid, drop second argument and push result on stack
    if (r)
    {
        rt.drop();
        if (rt.top(r))
            return OK;
    }

    return ERROR;
}


// ============================================================================
//
//   128-bit stubs
//
// ============================================================================
//   The non-trivial functions like sqrt or exp are not present in the QSPI
//   on the DM42. Calling them causes a discrepancy with the QSPI content,
//   and increases the size of the in-flash image above what is allowed
//   So we need to stub out some bid64 and bid42 functions and compute them
//   using bid128

void bid64_pow(BID_UINT64 *pres, BID_UINT64 *px, BID_UINT64 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid64_to_bid128(&x128, px);
    bid64_to_bid128(&y128, py);
    bid128_pow(&res128, &x128, &y128);
    bid128_to_bid64(pres, &res128);
}


void bid32_pow(BID_UINT32 *pres, BID_UINT32 *px, BID_UINT32 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid32_to_bid128(&x128, px);
    bid32_to_bid128(&y128, py);
    bid128_pow(&res128, &x128, &y128);
    bid128_to_bid32(pres, &res128);
}


void bid64_hypot(BID_UINT64 *pres, BID_UINT64 *px, BID_UINT64 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid64_to_bid128(&x128, px);
    bid64_to_bid128(&y128, py);
    bid128_hypot(&res128, &x128, &y128);
    bid128_to_bid64(pres, &res128);
}


void bid32_hypot(BID_UINT32 *pres, BID_UINT32 *px, BID_UINT32 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid32_to_bid128(&x128, px);
    bid32_to_bid128(&y128, py);
    bid128_hypot(&res128, &x128, &y128);
    bid128_to_bid32(pres, &res128);
}


void bid64_atan2(BID_UINT64 *pres, BID_UINT64 *px, BID_UINT64 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid64_to_bid128(&x128, px);
    bid64_to_bid128(&y128, py);
    bid128_atan2(&res128, &x128, &y128);
    bid128_to_bid64(pres, &res128);
}


void bid32_atan2(BID_UINT32 *pres, BID_UINT32 *px, BID_UINT32 *py)
// ----------------------------------------------------------------------------
//   Perform the computation with bid128 code
// ----------------------------------------------------------------------------
{
    BID_UINT128 x128, y128, res128;
    bid32_to_bid128(&x128, px);
    bid32_to_bid128(&y128, py);
    bid128_atan2(&res128, &x128, &y128);
    bid128_to_bid32(pres, &res128);
}



// ============================================================================
//
//   Instantiations
//
// ============================================================================

template object::result arithmetic::evaluate<struct add>();
template object::result arithmetic::evaluate<struct sub>();
template object::result arithmetic::evaluate<struct mul>();
template object::result arithmetic::evaluate<struct div>();
template object::result arithmetic::evaluate<struct mod>();
template object::result arithmetic::evaluate<struct rem>();
template object::result arithmetic::evaluate<struct pow>();
template object::result arithmetic::evaluate<struct hypot>();
template object::result arithmetic::evaluate<struct atan2>();



// ============================================================================
//
//   C++ wrappers
//
// ============================================================================

algebraic_g operator-(algebraic_g x)
// ----------------------------------------------------------------------------
//   Negation
// ----------------------------------------------------------------------------
{
    return neg::evaluate(x);
}


algebraic_g operator+(algebraic_g x, algebraic_g y)
// ----------------------------------------------------------------------------
//   Addition
// ----------------------------------------------------------------------------
{
    return add::evaluate(x, y);
}


algebraic_g operator-(algebraic_g x, algebraic_g y)
// ----------------------------------------------------------------------------
//   Subtraction
// ----------------------------------------------------------------------------
{
    return sub::evaluate(x, y);
}


algebraic_g operator*(algebraic_g x, algebraic_g y)
// ----------------------------------------------------------------------------
//   Multiplication
// ----------------------------------------------------------------------------
{
    return mul::evaluate(x, y);
}


algebraic_g operator/(algebraic_g x, algebraic_g y)
// ----------------------------------------------------------------------------
//   Division
// ----------------------------------------------------------------------------
{
    return div::evaluate(x, y);
}
