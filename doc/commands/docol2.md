# Flow control

## If

The `if` statement provides conditional structurs that let a program make
decisions. It comes in two forms:

* `if` *condition* `then` *true-clause* `end`: This evaluates *condition* and,
  if true, evaluates *true-clause*.

* `if` *condition* `then` *true-clause* `else` *false-clause* `end`: This
  evaluates *condition* and, if true, evaluates *true-clause*, otherwise
  evaluates *false-clause*.

A condition is true if:
* It is a number with a non-zero value
* It is the word `True`

A condition is false if:
* It is a number with a zero value
* It is the word `False`


## Case

The `case` statement can be used to select one case among many.
Inside a `case`, there is a list of conditions, each followed by `then` or
`when`. Code following `then` or `when` is executed when the condition is met.

* A condition preceding `then` is a boolean condition, similar to the condition
  in an `if` statement.

* A condition preceding `when` is a value that must match the current value on
  the stack exactly.

For example, `X case dup 0 < then "N" end dup 0 > then "P" end "Z" end`
will return the value `"N"`, `"P"` or `"Z"` depending on whether `X` is
negative, positive or null.

The `when` syntax is useful to test exact values, for example
`X case 0 when "zero" end 1 when "one" end 2 when "two" end end` will compute
the English spelling for value `0`, `1` and `2`.


## THENCASE
Conditional CASE ... THEN ... END THEN ... END END statement


## ENDTHEN
Conditional CASE ... THEN ... END THEN ... END END statement


## ENDCASE
Conditional CASE ... THEN ... END THEN ... END END statement


## FOR
Loop FOR ... NEXT/STEP statement


## START
Loop START ... NEXT/STEP statement


## NEXT
Loop FOR/START ... NEXT statement


## STEP
Loop FOR/START ... STEP statement


## DO
Loop DO ... UNTIL ... END statement


## UNTIL
Loop DO ... UNTIL ... END statement


## ENDDO
Loop DO ... UNTIL ... END statement


## WHILE
Loop WHILE ... REPEAT ... END statement


## REPEAT
Loop WHILE ... REPEAT ... END statement


## ENDWHILE
Loop WHILE ... REPEAT ... END statement


## IFERR
Conditional IFERR ... THEN ... ELSE ... END statement


## THENERR
Conditional IFERR ... THEN ... ELSE ... END statement


## ELSEERR
Conditional IFERR ... THEN ... ELSE ... END statement


## ENDERR
Conditional IFERR ... THEN ... ELSE ... END statement


## FORUP
Loop FORUP ... NEXT/STEP statement


## FORDN
Loop FORUP ... NEXT/STEP statement
