; loaded by test_builtins.c: the error in the middle must not lose the
; definitions of the forms before it
(define load-error-c 1)
(signal (quote load-test-error) nil)
(define load-error-d 2)
