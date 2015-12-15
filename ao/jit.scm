(define-module (ao jit))

(use-modules (system foreign))
(use-modules (ice-9 common-list))

(load-extension "libao" "libao_init")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; This is our local store.  When populated, it's an opaque pointer to a
;; Store object (created in C++ with make-store)
(define store #nil)

;; Converts the given argument to a token
;; Based on argument type, the following behavior occurs
;; - A number is converted to constant
;; - A symbol and further arguments is converted to an operator
(define-public (make-token a . args)
    "Creates a token in the global store.
A token is returned without changes
A number is converted to a constant
A symbol and further arguments are converted to an operation"
    (if (= 0 (length args))
        (cond ((pointer? a) a)
              ((number? a) (token-const store a))
              (else (error "Failed to construct token" a)))
        (token-op store a (map make-token args))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-public (interval? i)
    "Check to see if the given argument is a valid Interval"
    (and (pair? i) (not (list? i)) (number? (car i)) (number? (cdr i))))

(define (number-list? i)  (and (list? i) (every number? i)))

(define (default-like x)
  (cond ((number? x)    0)
        ((interval? x)  '(0 . 0))
        ((number-list? x)   (make-list (length x) 0))
        (else (error "Invalid type in default-like" x))))

(define (wrap-tree t arity)
   (let ;; Generic evaluator that dispatches based on argument type
        ((eval-generic (lambda (x y z)
            (cond ((every interval? (list x y z))
                        (tree-eval-interval t x y z))
                  ((every number-list? (list x y z))
                        (tree-eval-doubles t x y z))
                  ((every number? (list x y z))
                        (tree-eval-double t x y z))
                  (else (error "Input arguments are of invalid types"))))))

       ;; Create a local function based on function arity
       (cond ((= 2 arity) (lambda (x y) (eval-generic x y (default-like x))))
             ((= 3 arity) (lambda (x y z) (eval-generic x y z))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-public (tree? t)
    "Checks to see if the given object is a tagged tree pointer"
    (and (pair? t) (eq? (car t) 'ptr-tree) (pointer? (cdr t))))

(define-public (tree-ptr t)
    "Extracts the pointer from a tagged tree pointer"
    (cdr t))

(define-public (to-tree f)
    "Compile an arithmetic lambda function to a tagged tree pointer"
    (set! store (make-store))
    (let* ((arity (car (procedure-minimum-arity f)))
           (x (token-x store))
           (y (token-y store))
           (z (token-z store))
           (root (cond ((= 2 arity) (make-token (f x y)))
                       ((= 3 arity) (make-token (f x y z)))
                       (else (error "Invalid function arity" f))))
           (out (make-tree store root)))
       (set! store #nil)
       (cons 'ptr-tree out)))

(define-public (jit f)
    "Compiles an arithmetic lambda function into a wrapped function"
    (wrap-tree (cdr (to-tree f)) (car (procedure-minimum-arity f))))
