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
(defvar erlisp-executable (expand-file-name "../erlisp"))

(define-derived-mode erlisp-mode
  scheme-mode "Erlisp"
  "Major mode for Erlisp.")

(add-to-list 'auto-mode-alist '("\\.erlisp\\'" . erlisp-mode))

(defun run-erlisp ()
  "Run an inferior Erlisp process."
  (interactive)
  (pop-to-buffer
   (compilation-start
    erlisp-executable
    t
    (lambda (_) (format "*erlisp: %s *" default-directory)))))

(provide 'erlisp-mode)
;;; erlisp-mode.el ends here
