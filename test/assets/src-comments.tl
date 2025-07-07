;;;;; Test lexing comments

; a comment
     ; starting not a bol

;; this are comments
;; hopefully
;; they will be ignored by lexer
(format ; comment mid line
  ;; another comment
  #t "hello world")