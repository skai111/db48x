#ifndef INTEGRATE_H
#define INTEGRATE_H
// ****************************************************************************
//  integrate.h                                                       DB48X project
// ****************************************************************************
//
//   File Description:
//
//
//
//
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

#include "algebraic.h"
#include "command.h"
#include "symbol.h"

algebraic_p integrate(object_g    eq,
                      symbol_g    name,
                      algebraic_g low,
                      algebraic_g high);

COMMAND_DECLARE(Integrate);

#endif // INTEGRATE_H
