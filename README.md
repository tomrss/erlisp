# Erlisp

Simple Lisp interpreter.

## Erlisp and the Lisp family

This language is heavily inspired by Emacs Lisp, chosen as a simple
and straightforward Lisp implementation.  In particular, the main
source for ErLisp are very early Emacs versions, particularly Emacs 16
(rms, 1985).

## Usage

Dependencies: optionally `readline` (used for Erlisp REPL). If not wanted, remove flags in Makefile.

NOTE: makefile is hand-rolled so flags and other must be managed
manually in Makefile, no autoconf at the moment.

Compilation:

```shell
make
```

Testing:

```shell
make test
```
