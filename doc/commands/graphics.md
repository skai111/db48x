# Graphic commands

DB48X features a number of graphic commands. While displaying graphics, the
stack and headers will no longer be updated.

## Coordinates

DB48X recognizes the following types of coordinates

* *Pixel coordinates* are specified using based numbers such as `#0`, and
  correspond to exact pixels on the screen, and . Pixels are counted starting
  from the top-left corner of the screen, with the horizontal coordinate going
  from `10#0` to `10#399`, and the vertical coordinate going from `10#0` to
  `10#239`.

* *User unit coordinates* are scaled according to the content of the `PPAR` or
  `PlotParameters` reserved variables.

* *Text coordinates* are given on a square grid with a size corresponding to the
  height of a text line in the selected font. They can be fractional.

Coordinates can be given using one the following object types:

* A complex number, where the real part represents the horizontal coordinate and
  the imaginary part represents the vertical coordinate.

* A 2-element list or vector containing the horizontal and vertical coordinates.

* A 1-element list of vector containing one of the above.

For some operations, the list or vector can contain additional parameters beyond
the coordinates. The selection of unit or pixel coordinates is done on a per
coordinate basis. For exmaple, `{ 0 0 }` will be the origin in user coordinates,
in the center of the screen if no `PPAR` or `PlotParameters` variable is
present.

Note that unlike on the HP48, a complex value in DB48X can
contain a based number.


## ClearLCD (CLLCD)

Clear the LCD display, and block updates of the header or menu areas.


## DrawText (DISP)

Draw the text or object in level 2 at the position indicated by level 1. A text
is drawn without the surrounding quotation marks.

If the position in level 1 is an integer, fraction or real number, it is
interpreted as a line number starting at 1 for the top of the screen. For
example, `"Hello" 1 disp` will draw `Hello` at the top of the screen.
If the position is a based number, it is a row number in pixels. For example,
`"Hello" #120d disp` will show the test in the middle of the screen.

If the position in level 1 is a complex number or a list, it is interpreted as
specifying both the horizontal or vertical coordinates, in either pixel or unit
coordinates. For example `"Hello" { 0 0 } disp` will draw `Hello` starting in
the center of the screen.

Text is drawn using the stack font by default, using the
[foreground](#foreground) and [background](#background) patterns.

If level 1 contains a list with more than 2 elements, additional elements
provide:

* A *font number* for the text

* An *erase* flag (default true) which indicates whether the background for the
  text should be drawn or not.

* An *invert* flag (default false) which, if set, will swap the foreground and
  background patterns.

For example, `"Hello" { #0 #0 3 true true } DrawText` will draw `Hello` in the
top-left corner (`#0 #0`) with the largest (editor) font (font identifier `3`),
erasing the background (the first `true`), in reverse colors (the second
`true`).

## DrawStyledText (DISPXY)

Draw the text or object in level 3 at the position indicated by level 2, using
the font specified in level 1. This behaves like [DrawText](#drawtext), except
for the additional parameter specifying the font size.

This command is compatible with the HP50G usage, where the position is specified
by a list containing two decimal integer values. A font size of 1 displays with
a small font, a font size of 2 displays with a regular font.

In addition to this HP50G-compatible usage, `DispXY` will also accept:

* A smaller font size, 0, for the help size, and other font sizes above 2, which
  are compatible with the values given to [StackFont](#stackfont).

* The position can accept the same values as [DrawText](#drawtext), including a
  single integer value indicating a line number, a fractional line position, or
  coordinates scaled according to [PlotParameters](#plotparameters).


## Show

Display the first level of the stack using the entire screen, with a possible
scroll using the 4, 8, 6 and 2 keys if the object is larger than fits on screen.
This makes it possible to comfortably examine very large objects, like `300!`, a
large program or a complicated equation. Arrow keys can also be used for
horizonal or vertical scrolling.

The maximum size of the graphic object is defined by the
[MaximumShowWidth](#maximumshowwidth) and
[MaximumShowHeight](#maximumshowheight) settings.

## DrawLine (line)

Draw a line between two points specified by level 1 and level 2 of the stack.

The width of the line is specified by [LineWidth](#linewidth). The line is drawn
using the [foreground](#foreground) pattern.


## PlotParameters (PPAR)

The `PlotParameters` reserved variable defines the plot parameters, as a list,
with the following elements:

* *Lower Left* coordinates as a complex (default `-10-6i`)

* *Upper Right* coordinates as a complex (default `10+6i`)

* *Independent variable* name (default `x`)

* *Resolution* specifying the interval between values of the independent
  variable (default `0`). A binary numnber specifies a resolution in pixels.

* *Axes* which can be a complex giving the origin of the axes (default `0+0i`),
  or a list containing the origin, the tick mark specification, and the names of
  the axes.

* *Type* of plot (default `function`)

* *Dependent variable* name (default `y`)
