// ****************************************************************************
//  equations.cc                                                  DB48X project
// ****************************************************************************
//
//   File Description:
//
//      Implementation of the equations library
//
//
//
//
//
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

#include "equations.h"

#include "expression.h"
#include "grob.h"
#include "renderer.h"
#include "solve.h"
#include "variables.h"

RECORDER(equations,         16, "Equation objects");
RECORDER(equations_error,   16, "Error on equation objects");


// ============================================================================
//
//   Equation definitions
//
// ============================================================================

static const cstring basic_equations[] =
// ----------------------------------------------------------------------------
//   List of basic equations
// ----------------------------------------------------------------------------
//   clang-format off
{
    // ------------------------------------------------------------------------
    //   Columns and beams
    // ------------------------------------------------------------------------

    "Columns and Beams", nullptr,

    "Elastic Buckling",  "{ "
    "  '(Pcr_kN)=(Ⓒπ²*(E_kPa)*(A_cm^2))/sq((K*(L_m))/(r_cm))' "
    "  '(Pcr_kN)=(Ⓒπ²*(E_kPa)*(I_mm^4))/sq(K*(L_m))' "
    "  '(σcr_kPa)=(Pcr_kN)/(A_cm^2)' "
    "  '(r_cm)= sqrt((I_mm^4)/(A_cm^2))' "
    "}",

    "Eccentric Columns", "{"
    "  '(σmax_kPa)=((P_kN)/(A_cm^2))*(1+((ε_cm)*(c_cm))/sq(r_cm)*inv(cos(K/2*((L_m)/(r_cm))*sqrt((P_kN)/((E_kPa)*(A_cm^2)))*1_r)))'"
    "  '(r_cm)=sqrt((I_mm^4)/(A_cm^2))'"
    "}",

    "Simple Deflection", "{"
    "  '(y_cm)=((P_kN)*((L_m)-(a_m)))*(x_m)/(6*(L_m)*(E_kPa)*(I_mm^4))*(x²+(L-a)²-L²)-((M_N*m)*x)/(E*I)*((c_m)-x²/(6*L)-L/3-c²/(2*L))-((w_N/m)*x)/(24*E*I)*(L³+x²*(x-2*L))'"
    "}",

    "Simple Slope", "{"
    "  '(θ_°)=(1_r)*(((P_N)*((L_m)-(a_m)))/(6*L*(E_kPa)*(I_mm^4))*(3*(x_m)²+(L-a)²-L²)-(M_N*m)/(E*I)*(c-(x²-c²)/(2*L)-L/3)-(w_N/m)/(24*E*I)*(L³+x²*(4*x-6*L)))'"
    "}",

    "Simple Moment", "{"
    "  '(Mx_N*m)=(P_N)*IFTE((x_m)≤(a_m);((L_m)-(a_m))*(x_m);(L-x)*a)/L+(M_N*m)*IFTE(x≤(c_m);x;x-L)/L+((w_N/m)*x*(L-x))/2'"
    "}",

    "Simple Shear", "{"
    "  '(V_N)=((P_N)*((L_m)-(a_m)))/L+(M_N*m)/L+((w_N/m)*(L-2*(x_m)))/2'"
    "}",

    "Cantilever Deflection", "{"
    "  '(y_m)=((P_N)*IFTE((x_m)≤(a_m);x;a)²)/(6*(E_kPa)*(I_mm^4))*IFTE(x≤a;x-3*a;a-3*x)+(M_N*m)*IFTE(x≤(c_m);x²;c*(2*x-c))/(2*E*I)-((w_N/m)*x²)/(24*E*I)*(6*L²-4*L*x+x²)'"
    "}",

    "Cantilever Slope", "{"
    "  '(θ_°)=(1_r)*((P_N)*IFTE((x_m)≤(a_m);x*(x-2*a);-a²)/(2*(E_kPa)*(I_mm^4))+(M_N*m)*IFTE(x≤(c_m);x;c)/(E*I)-((w_N/m)*x)/(6*E*I)*(3*L²-3*L*x+x²))'"
    "}",

    "Cantilever Moment", "{"
    "  '(Mx_N*m)=IFTE((x_m)≤(a_m);1;0)*(P_N)*((x_m)-(a_m))+IFTE(x≤(c_m);1;0)*(M_N*m)-(w_N/m)/2*(L²-2*L*x+x²)'"
    "}",


    "Cantilever Shear", "{"
    "  '(V_n)=IFTE((x_m)≤(a_m);1;0)*(P_N)+(w_N/m)*((L_m)-(x_m))'"
    "}",

    // ------------------------------------------------------------------------
    //   Physics
    // ------------------------------------------------------------------------

    "Phys",     nullptr,

    "RelativityMassEnergy",             "'(E_J)=(m_kg)*Ⓒc^2'",
    "IdealGas",                         "'(P_Pa)*(V_m^3)=(n_mol)*ⒸR*(T_K)'"
};
//   clang-format on


static runtime &invalid_equation_error()
// ----------------------------------------------------------------------------
//    Return the error message for invalid equations
// ----------------------------------------------------------------------------
{
    return rt.invalid_equation_error();
}


static symbol_p equation_label(symbol_r sym)
// ----------------------------------------------------------------------------
//   Simplify equations to show then in menu label
// ----------------------------------------------------------------------------
{
    if (sym)
    {
        size_t   len    = 0;
        utf8     source = sym->value(&len);
        if (object_p obj = object::parse(source, len))
            if (expression_p expr = obj->as<expression>())
                if (symbol_p ssym = expr->as_symbol(false))
                    return ssym;
    }
    return sym;
}


const equation::config equation::equations =
// ----------------------------------------------------------------------------
//  Define the configuration for the equations
// ----------------------------------------------------------------------------
{
    .menu_help      = "",
    .help           = "",
    .prefix         = L'Ⓔ',
    .type           = ID_equation,
    .first_menu     = ID_EquationsMenu00,
    .last_menu      = ID_EquationsMenu99,
    .name           = ID_EquationName,
    .value          = ID_EquationValue,
    .command        = ID_EquationSolver,
    .file           = "config/equations.csv",
    .builtins       = basic_equations,
    .nbuiltins      = sizeof(basic_equations) / sizeof(*basic_equations),
    .error          = invalid_equation_error,
    .label          = equation_label
};



// ============================================================================
//
//   Menu implementation
//
// ============================================================================

PARSE_BODY(equation)
// ----------------------------------------------------------------------------
//    Skip, the actual parsing is done in the symbol parser
// ----------------------------------------------------------------------------
{
    return do_parsing(equations, p);
}


EVAL_BODY(equation)
// ----------------------------------------------------------------------------
//   Equations always evaluate to their value
// ----------------------------------------------------------------------------
{
    object_g value = o->value();
    return rt.push(+value) ? OK : ERROR;
}


RENDER_BODY(equation)
// ----------------------------------------------------------------------------
//   Render the equation into the given buffer
// ----------------------------------------------------------------------------
{
    equation_g eq = o;
    do_rendering(equations, o, r);
    if (!r.editing() && Settings.ShowEquationBody())
    {
        if (object_g obj = eq->value())
        {
            r.put(':');
            obj->render(r);
        }
    }
    return r.size();
}


GRAPH_BODY(equation)
// ----------------------------------------------------------------------------
//   Render "normally"
// ----------------------------------------------------------------------------
{
    equation_g eq = o;
    if (Settings.ShowEquationBody())
    {
        if (object_g val = eq->value())
        {
            size_t namelen = 0;
            utf8 name = eq->name(&namelen);
            if (symbol_g namesym = symbol::make(name, namelen))
            {
                if (grob_g valg = val->graph(g))
                {
                    coord vv = g.voffset;
                    g.voffset = 0;
                    if (grob_g nameg = object::do_graph(+namesym, g))
                    {
                        coord nv = g.voffset;
                        g.voffset = 0;
                        grob_g r = expression::infix(g,
                                                     nv, nameg,
                                                     0, ":",
                                                     vv, valg);
                        return r;
                    }
                }
            }
        }
    }
    return object::do_graph(o, g);
}


HELP_BODY(equation)
// ----------------------------------------------------------------------------
//   Help topic for equations
// ----------------------------------------------------------------------------
{
    return o->do_instance_help(equation::equations);
}


MENU_BODY(equation_menu)
// ----------------------------------------------------------------------------
//   Build a equations menu
// ----------------------------------------------------------------------------
{
    return o->do_submenu(equation::equations, mi);
}


HELP_BODY(equation_menu)
// ----------------------------------------------------------------------------
//   Show the help for the given equation menu
// ----------------------------------------------------------------------------
{
    return o->do_menu_help(equation::equations, o);
}


MENU_BODY(EquationsMenu)
// ----------------------------------------------------------------------------
//   The equations menu is dynamically populated
// ----------------------------------------------------------------------------
{
    return equation::do_collection_menu(equation::equations, mi);
}


utf8 equation_menu::name(id type, size_t &len)
// ----------------------------------------------------------------------------
//   Return the name for a menu entry
// ----------------------------------------------------------------------------
{
    return do_name(equation::equations, type, len);
}


COMMAND_BODY(EquationName)
// ----------------------------------------------------------------------------
//   Put the name of a equation on the stack
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (constant_p cst = equation::do_key(equation::equations, key))
        if (equation_p eq = cst->as<equation>())
            if (rt.push(eq))
                return OK;
    if (!rt.error())
        rt.type_error();
    return ERROR;
}


INSERT_BODY(EquationName)
// ----------------------------------------------------------------------------
//   Put the name of a equation in the editor
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    return ui.insert_softkey(key, " Ⓔ", " ", false);
}


HELP_BODY(EquationName)
// ----------------------------------------------------------------------------
//   Put the help for a given equation name
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (constant_p cst = equation::do_key(equation::equations, key))
        if (equation_p eq = cst->as<equation>())
            return eq->help();
    return utf8("Equations");
}


COMMAND_BODY(EquationValue)
// ----------------------------------------------------------------------------
//   Put the value of a equation on the stack
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (constant_p cst = equation::do_key(equation::equations, key))
        if (equation_p eq = cst->as<equation>())
            if (object_p value = eq->value())
                if (rt.push(value))
                    return OK;
    if (!rt.error())
        rt.type_error();
    return ERROR;
}


INSERT_BODY(EquationValue)
// ----------------------------------------------------------------------------
//   Insert the value of a equation
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (object_p cstobj = equation::do_key(equation::equations, key))
        if (equation_p eq = cstobj->as<equation>())
            if (object_p value = eq->value())
                return ui.insert_object(value, " ", " ");
    return ERROR;
}


HELP_BODY(EquationValue)
// ----------------------------------------------------------------------------
//   Put the help for a given equation value
// ----------------------------------------------------------------------------
{
    return EquationName::do_help(nullptr);
}



COMMAND_BODY(EquationSolver)
// ----------------------------------------------------------------------------
//   Solve for a given equation
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (constant_p cst = equation::do_key(equation::equations, key))
        if (equation_p eq = cst->as<equation>())
            if (directory::store_here(static_object(ID_Equation), eq))
                if (const SolvingMenu *smenu =
                    object::static_object(ID_SolvingMenu)->as<SolvingMenu>())
                    return smenu->object::evaluate();
    if (!rt.error())
        rt.type_error();
    return ERROR;
}


INSERT_BODY(EquationSolver)
// ----------------------------------------------------------------------------
//   Insert the code in a program to solve a library equation
// ----------------------------------------------------------------------------
{
    int key = ui.evaluating;
    if (constant_p cst = equation::do_key(equation::equations, key))
        if (equation_p eq = cst->as<equation>())
            if (rt.push(eq))
                return ui.insert_object(eq, " ", " StEQ SolvingMenu ");
    return ERROR;
}


HELP_BODY(EquationSolver)
// ----------------------------------------------------------------------------
//   Put the help for a given equation value
// ----------------------------------------------------------------------------
{
    return EquationName::do_help(nullptr);
}


COMMAND_BODY(LibEq)
// ----------------------------------------------------------------------------
//   Evaluate a library equation
// ----------------------------------------------------------------------------
{
    return equation::lookup_command(equation::equations, false);
}
