# Equations library

The DB48X calculator features a library of equations covering mathematics,
physics, chemistry and computer science. The built-in equations can be extended
using the `config/equations.csv` configuration file.


## Columns and Beams

The variables in the columns and beams section are:

* `o`: Eccentricity (offset) of load
* `σcr`: Critical stress
* `σmax`: Maximum stress
* `θ`: Slope at `x`
* `A`: Cross-sectional area
* `a`: Distance to point load
* `c`: Distance to edge fiber (Eccentric Columns), or Distance to applied moment (beams)
* `E`: Modulus of elasticity
* `I`: Moment of inertia
* `K`: Effective length factor of column
* `L`: Length of column or beam
* `M`: Applied moment
* `Mx`: Internal bending moment at x
* `P`: Load (Eccentric Columns), or Point load (beams)
* `Pcr`: Critical load
* `r`: Radius of gyration
* `V`: Shear force at x
* `w`: Distributed load
* `x`: Distance along beam
* `y`: Deflection at x


For simply supported beams and cantilever beams (“Simple Deflection” through
“Cantilever Shear”), the calculations differ depending upon the location of x
relative to the loads.

* Applied loads are positive downward.
* The applied moment is positive counterclockwise.
* Deflection is positive upward.
* Slope is positive counterclockwise
* Internal bending moment is positive counterclockwise on the left-hand part.
* Shear force is positive downward on the left-hand part.

### Elastic Buckling

These equations apply to a slender column (`K·L/r>100`) with length factor `K`.

![Elastic Buckling](img/ElasticBuckling.bmp)



## Electricity

## Fluids

## Forces and Energy

## Gases

## Heat transfer

## Magnetism

## Motion

## Optics

## Oscillations

## Plane geometry

## Solid geometry

## Solid state devices

## Stress analysis

## Waves
