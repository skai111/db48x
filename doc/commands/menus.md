## Catalog

The `Catalog` command is triggered by the `+` command while in _Alpha_ mode, or
using 🟨 _+_ otherwise (_CAT_). It is an auto-completing catalog of all the
available commands or characters.

When entering a program, all commands matching the text currently matched will
be shown. For example, if you typed `A`, all commands containing an `A` are
displayed in alphabetical order. If you type `ACO`, `ACOS, `ACOSH` and
`DataColumn` (a possible spelling for `ColΣ`) will all be shown.

Inside text, all characters related to the last entered character will be shown.
For example, if you typed `A`, options to add accents to `A` or to select the
Greek `α` or the related `ª` character. If no matching character is found,
the `CharactersMenu` [characters menu](#CharactersMenu) is presented, allowing
you to select specific characters from predefined categories.


## CharactersMenu

The `CharactersMenu`, which can be accessed using 🟦 _2_ (_CHAR_), presents a
convenient way to insert characters.

While entering text, the [Catalog](#Catalog) also presents matching characters,
or the characters menu if none is found.

The characters menus and the catalog behaviour can be configured by modifying
the `config/characters.csv` file. Each row in that file contains two texts:

* The first is the name of the menu. If the name is empty, the menu is not
  shown, but is used to find characters related to a given character.

* The second is the list of characters to be shown.


## ConstantsMenu

The `ConstantsMenu` gives access to numerous [constants](#constants), shown in
various categories. This includes basic mathematical constants such as `π` or
`e`, as well as constants related to chemistry, physics, computer science or
history.

The constants menu is defined by the `config/constants.csv` file. You are
encouraged to tailor this file to suit your own needs.
