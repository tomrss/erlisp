
(defvar erlisp-executable (expand-file-name "../erlisp"))

(defun run-erlisp ()
  "Run an inferior Erlisp process."
  (interactive)
  (pop-to-buffer
   (compilation-start
    erlisp-executable
    t
    (lambda (_) (format "*erlisp: %s *" default-directory)))))
