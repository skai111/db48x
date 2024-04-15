# Flags

Flags are truth value that can be controled and tested by the user.
User flags are identified by a natural number. There are `MaxFlags` user flags (default is 128).
System flags are identified by a settings name or a negative integer.


## SETLOCALE
Change the separator symbols


## SETNFMT
Change the display format for numbers


## SetFlag (SF)

Set a user or system flag.

`33 SF` sets user flag 0.
`'MixedFractions' SetFlag` enables the `MixedFractions` setting.

## ClearFlag (CF)

Clear a user or system flag

## FlipFlag

Invert a user or system flag

## TestFlagSet (FS?)

Test if a flag is set

## TestFlagClear (FC?)

Test if a flag is clear

## TestFlagClearThenClear (FC?C)

Test if a flag is clear, then clear it

## TestFlagSetThenClear (FS?C)

Test if a flag is set, then clear it

## TestFlagClearThenSet (FC?S)

Test if a flag is clear, then set it

## TestFlagSetThenSet (FS?S)

Test if a flag is set, then set it

## FlagsToBinary (RCLF)

Recall all system flags as a binary number.


## BinaryToFlags (STOF)

Store and replace all system flags from a binary number
