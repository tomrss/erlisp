; load-test-cell is a cons created by test_eval.c before loading this file
(setcar load-test-cell 1)
(setcar load-test-cell (+ (car load-test-cell) 41))
; must be visible in the caller's env after load
(define load-test-define 7)
