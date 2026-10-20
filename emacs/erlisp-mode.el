;;; erlisp-mode.el --- Major mode for editing Erlisp files -*- lexical-binding: t -*-

;; Copyright (C) 2026  Tommaso Rossi

;; Author: Tommaso Rossi <tommaso.rossi@protonmail.com>

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:

;; Major mode for editing Erlisp files.

;;; Code:

;; TODO: TEMP for dev
(defvar erlisp-executable nil
  "Path to the erlisp executable.
If nil, use erlisp in the project root, or in `default-directory'
outside of a project.")

(defun erlisp--executable ()
  "Return the erlisp executable to run."
  (or erlisp-executable
      (expand-file-name "erlisp"
                        (if-let ((project (project-current)))
                            (project-root project)
                          default-directory))))

(defconst erlisp-font-lock-keywords
  `(("(\\(defmacro\\)\\_>\\s-*(?\\(\\(?:\\sw\\|\\s_\\)+\\)?"
     (1 font-lock-keyword-face)
     (2 font-lock-function-name-face nil t))
    ("(\\(deftest\\)\\_>\\s-*(\\(\\(?:\\sw\\|\\s_\\)+\\)"
     (1 font-lock-keyword-face)
     (2 font-lock-function-name-face))
    (,(concat "(" (regexp-opt '("progn" "condition-case" "unwind-protect"
                                "test-run")
                              t)
              "\\_>")
     (1 font-lock-keyword-face))
    (,(concat "(" (regexp-opt '("assert" "assert-equal" "assert-error"
                                "signal")
                              t)
              "\\_>")
     (1 font-lock-builtin-face)))
  "Extra font-lock keywords for Erlisp, on top of the Scheme ones.")

;; scheme-indent-function is a global symbol property: these also apply
;; to scheme-mode buffers
(dolist (spec '((defmacro . 1)
                (deftest . 1)
                (condition-case . 2)
                (unwind-protect . 1)
                (call-with-port . 1)
                (with-output-to-port . 1)
                (with-output-to-file . 1)
                (with-input-from-port . 1)
                (with-input-from-file . 1)
                (with-input-from-string . 1)))
  (put (car spec) 'scheme-indent-function (cdr spec)))

(define-derived-mode erlisp-mode
  scheme-mode "Erlisp"
  "Major mode for Erlisp."
  (font-lock-add-keywords nil erlisp-font-lock-keywords))

(add-to-list 'auto-mode-alist '("\\.erlisp\\'" . erlisp-mode))

(defun run-erlisp ()
  "Run an inferior Erlisp process."
  (interactive)
  (pop-to-buffer
   (compilation-start
    (erlisp--executable)
    t
    (lambda (_) (format "*erlisp: %s *" default-directory)))))

(provide 'erlisp-mode)
;;; erlisp-mode.el ends here
