# Exceptions vs. Result Types

Prompted by a discussion on Reddit, I set out to measure which approach to
error handling approach is the most performant. I measure 3 contemporary
C++ compilers (Apple's version of Clang, GCC 5.1, and GCC 4.9) with different
optimization flags (-Os, -O3), as well as the Rust 1.0 compiler. Note that both
Clang and Rust use LLVM as the backend for codegen.

This is mainly intended as an evaluation of the two approaches from a performance
viewpoint (and not, say, one of code clarity, elegance, or even correctness). The
original aim was to compare the principal techniques of error handling between Rust
and C++, but since the Rust compiler has only just reached 1.0, it is fair to assume
that mainstream C++ compilers and optimizers are generally much more mature, and as
such a comparison between a C++ implementation and a Rust implementation is bound to
be skewed in favor of C++. Therefore, I'm emulating a `Result` type in C++ that is
clearly simpler and not as safe as Rust's `Result`, but which should theoretically
cause the compiler to generate equivalent code to the best that could be hoped for
with a hypothetically mature Rust compiler.

Mostly for fun, I'm also including a real Rust implementation to showcase how language
support can make working with `Result` types quite ergonomic when compared with the
(admittedly, contrived) implementation in C++.


## Motivation

Thanks to advances in type systems, returning error values is seeing a revival. Modern
languages such as Rust provide a generic `Result` type that is a type-safe union with
`Ok` and `Err` variants. A more crude predecessor of this is the pattern in C where 
a function takes an out-parameter and returns an integer that indicates whether or not
the out-parameter was succesfully filled out, and otherwise an error code.

Exceptions on modern architectures are generally implemented with the assumption that
exceptions are truly exceptional events under normal program flow — i.e., they are
assumed to be very rare, and so exception handling code is allowed by most compilers
to be quite expensive if it allows "happy path" execution to perform better. When an
exception is thrown, the program must unwind the stack and call destructors along the
way. There are many ways to achieve this, but current compilers generally implement
"Zero-Cost Exception Handling", whereby information about the stack is generated at
compile-time and interpreted when an exception is thrown in such a way that the correct
destructors are called in the correct order. The advantage of this approach is that
is little to no bookkeeping to do at runtime, meaning that as long as no exception is
thrown, the code can perform almost as well as code that has no exceptions at all,
because it doesn't have to check for errors manually.

The question then becomes whether the speedup achieved by not doing any error checking
at runtime is worth the increase in binary size needed for the extra information about
the stack layout needed by the runtime. Exceptions also have multiple other drawbacks
in the form of complicating program flow and sometimes making it hard to reason about
where or how an error occurred, so a programmer may have many motivations to avoid
them, even if they perform well under normal circumstances.

It should be noted here that there are plenty of use cases where errors aren't exactly
"exceptional" and performance under error handling really does matter. Such a program
may derive benefit from using a `Result` type pattern, so let's examine that approach.

Since a `Result<T, E>` (where `T` is the expected return value and `E` is the type of
the error), we might expect an implementation to represent this type as a tagged union,
i.e. a union of T and E with a 1-byte tag indicated whether it contains a value of type
`T` or a value of type `E`, indicating an error. This means that `sizeof(Result<T,E>)`
will be at least equal to `max(sizeof(T), sizeof(E)) + 1`. Here we see the crux of the
issue: a `Result<size_t, int>` (where the int represents an error code) will be at least
9 bytes on a 64-bit platform, i.e. one more byte than what fits in a register. One
might then expect code that uses result types heavily to be harder to optimize for the
compiler, since the increased size of return values will increase register pressure, i.e.
the compiler needs two registers to represent a value of the native word size, instead
of just one.

Additionally, while exceptions let the compiler decide how to unwind the stack, error
handling in the style of `Result` types requires that the return value is checked
by the caller before the value is used. This means an extra branch for every function
call that returns a `Result`, which intuitively should mean a slowdown, but the question
is whether this will be canceled out by optimizations in the compiler (notably inlining
and static program flow analysis), not to mention branch prediction in the CPU.

## A Note on Benchmarking

Benchmarking is notoriously difficult, but I have tried to minimize potential sources
of error in the following ways:

1. Function calls are timed with the process time instead of wall clock time. This
   mostly cancels out any delays caused by process rescheduling on part of the OS.
   Specifically, I'm using `getrusage` to get the user-space process time, since the
   benchmarks I wrote don't invoke any kernel system calls, and I'm not interested in
   delays caused by page faults, since I'm comparing things that have identical memory
   access patterns.

2. Inlining is selectively disabled to avoid some function calls being eliminated altogether,
   and to make it easier to profile and instrument the resulting programs.

3. Whole program optimization can sneakily eliminate many code paths, even when inlining
   is disabled.

## The Test Program

The optimizers saw through my initial attempts and promptly proceeded to cancel out most
of operations that I had so carefully planned for it to carry out. This anecdote shouldn't
be ignored: It's important to remember that the following conclusions are only valid under
a very specific set of circumstances, and that optimizers will often generate code that
completely turn performance expectations on its head — or just turns something that you
thought would be expensive into barely a no-op.

To protect myself from optimizers, I ended up making a small S-expression prefix calculator
that understands sequences of mathematical operations in the following format:

```
+ (+ 2 (* 4 5)) 3
```

This input will be understood as `(2 + (4 * 5)) + 3`, and the output would be 23.

The very simple grammar rules look more or less like this:

```
expression: '(' expression ')'
          | number
          | inner_expression
          ;

inner_expression: operation expression expression
                ;

operation: '+'
         | '-'
         | '*'
         | '/'
         ;

number: ['0'..'9']+
      ;
```

Negative numbers are not supported (except with the notation `(- 0 n)`).

The programs that I will be measuring are implementations of a simple recursive-descent
parser that understand this syntax and internally use either exceptions or a custom `Result`
type to report syntax errors.

Each implementation can detect three kinds of errors:

```c++
enum class ErrorKind {
    InvalidOperator,
    InvalidCharacter,
    UnexpectedEOF,
};
```

In the version emulating `Result` types, a `Result<T>` struct is implemented like so:

```c++
template <class T>
struct Result {
    union {
        T ok;
        ErrorKind error;
    };
    bool is_error;

    explicit Result(T ok) : is_error(false) { this->ok = ok; }
    explicit Result(ErrorKind error) : is_error(true) { this->error = error; }
};
```

Note that I'm blindly assuming that `T` will be a POD type here, ignoring any potential issues
with having non-POD types in unions. I am also placing the "tag" (`is_error`) at the end of the
struct to get the best possible alignment (since `T` is likely to require alignment > 1, placing
the boolean at the beginning of the struct is likely to make the compiler insert unfortunate
amounts of padding).

In the exception-handling version, the error is encapsulated in an exception object called
`Error`:

```c++
struct Error {
    ErrorKind kind;

    Error(ErrorKind kind) : kind(kind) {}
};
```

### Comparing Implementations

Both parsers work with two `const char*` pointers, `p` and `end`, representing the current position
in the stream and the end of the stream, respectively. The parser reads on `char` at a time.

The guts of the parser is the `get_char()` method:

```c++
char get_char() {
    if (p == end) {
        throw Error{ErrorKind::UnexpectedEOF};
    }
    return *p++;
}
```

In the version with `Result` types, it looks slightly different:

```c++
Result<char> get_char() {
    if (p == end) {
        return Result<char>{ErrorKind::UnexpectedEOF};
    }
    return Result<char>{*p++};
}
```

To give an example of one of the rules used in the grammar, here is how each parser parses a
`number`.

With exceptions:

```c++
int64_t number() {
    int64_t result = 0;
    while (std::isdigit(peek())) {
        char c = get_char();
        result *= 10;
        result += c - '0';
    }
    return result;
}
```

With result types:

```c++
Result<int64_t> number() {
    int64_t result = 0;
    while (std::isdigit(peek())) {
        auto c = get_char();
        if (c.is_error) {
            return Result<int64_t>{c.error};
        }

        result *= 10;
        result += c.ok - '0';
    }
    return Result<int64_t>{result};
}
```

One of the core advantages of result types over error code return values is that they can be
structured in such a way that the type system actually forces you to do the error checking
when necessary. My home-made `Result<T>` has no such restriction, but I should still make sure
to always check the error case, even when I can reasonably guarantee that the error case will
never happen at a particular place in the code, because a similar error check will be made with
more advanced result types, and it will be left to the optimizer to figure out which checks can
be removed. In the above, the optimizer is likely to figure out that `c.is_error` is never going
to be true when `std::isdigit` returns true, but whether or not it is able to do so is part
of what we're testing here.

As you can plainly see, the version using result types is decidedly more verbose, but it is also
more clear to the reader exactly where potential errors may occur, since they are explicitly
handled and passed on to the caller.

For the complete parser implementations, please refer to the files `parser_with_expressions.cpp`
and `parser_with_results.cpp` in this repository.

## Results

To build these examples, you need GNU Make installed, as well as GCC 5.1, GCC 4.9, Clang, and
Rust 1.0. The following benchmarks were obtained on an iMac with a 3.4 GHz Core i7 CPU,
with the command:

```
$ ITERATIONS=100000 make
```

As input, the parser is invoked with two programs: One that runs error-free, and one that
contains a syntax error. Please refer to files `input.ok` and `input.err` in this repository
for the full listing.

First, let's have a look at Clang:

|         | benchmark               | time      |
|---------|-------------------------|----------:|
|clang-O3 | Exceptions, no errors   | 34,102 µs |
|clang-O3 | Result, no errors       | 46,257 µs |
|clang-Os | Exceptions, no errors   | 63,827 µs |
|clang-Os | Result, no errors       | 64,410 µs |

The biggest surprise here is how much difference there is with different optimization flags.
When optimizing for size, it doesn't seem to make any real difference which approach is taken,
however it should be noted that the `-O3` version actually yields a smaller binary size
(61,572 bytes vs. 63724 bytes with `-Os`).

When compiling with `-O3`, however, exceptions do seem to represent the more efficient choice.

Let's see what happens when we run it with errors happening:

|         | benchmark               | time       |
|---------|-------------------------|-----------:|
|clang-O3 | Exceptions, with errors | 154,591 µs |
|clang-O3 | Result, with errors     |  24,942 µs |
|clang-Os | Exceptions, with errors | 198,453 µs |
|clang-Os | Result, with errors     |  35,809 µs |

That is quite abysmal, but not surprising considering the extra work that has to happen when
an exception is thrown. Note that the version with result types actually runs faster when it
encounters an error — this is likely because the error happens about halfway through the program,
and it doesn't really distinguish errors from non-errors, so the optimizer likely considers the
error case just as likely as the non-error case. The error is therefore just equivalent to
early termination, but the cost of that flexibility is paid if there is most only valid programs
going through the interpreter.

Let's look at GCC 5.1:

|          | benchmark               | time      |
|----------|-------------------------|----------:|
|gcc5.1-O3 | Exceptions, no errors   | 24,065 µs |
|gcc5.1-O3 | Result, no errors       | 24,704 µs |
|gcc5.1-Os | Exceptions, no errors   | 55,330 µs |
|gcc5.1-Os | Result, no errors       | 59,468 µs |

Here it's interesting to see that the gap in `-O3` mode is extremely small, likely below the
margin of error. GCC 5.1 has made huge improvements to its optimizer, and especially compared
with Clang, they are really starting to pay off. It should also be noted that the `-Os` version
compiled with GCC 5.1 is almost half the binary size of the `-Os` version compiled with Clang
(33,224 bytes versus 63,724 bytes), and that even the `-O3` version with GCC 5.1 beats Clang's
`-Os` version on code size by 6 KB.

When errors start happening, the picture changes a little bit, though:

|          | benchmark               | time       |
|----------|-------------------------|-----------:|
|gcc5.1-O3 | Exceptions, with errors | 407,959 µs |
|gcc5.1-O3 | Result, with errors     |  13,512 µs |
|gcc5.1-Os | Exceptions, with errors | 856,775 µs |
|gcc5.1-Os | Result, with errors     |  33,727 µs |

That's a whopping factor 17 slower than when no errors were occurring with `-O3`, and a factor 15
slower with `-Os`. Also, as you'll notice, GCC 5.1's exception handling is more than twice as slow
as Clang's.

For completeness' sake, and because not everybody has the chance to upgrade to GCC 5.1 yet,
I have included the numbers for GCC 4.9 as well:

|          | benchmark               | time      |
|----------|-------------------------|----------:|
|gcc4.9-O3 | Exceptions, no errors   | 23,408 µs |
|gcc4.9-O3 | Result, no errors       | 26,299 µs |
|gcc4.9-Os | Exceptions, no errors   | 53,954 µs |
|gcc4.9-Os | Result, no errors       | 62,782 µs |

|          | benchmark               | time       |
|----------|-------------------------|-----------:|
|gcc4.9-O3 | Exceptions, with errors | 717,592 µs |
|gcc4.9-O3 | Result, with errors     |  14,640 µs |
|gcc4.9-Os | Exceptions, with errors | 743,138 µs |
|gcc4.9-Os | Result, with errors     |  35‚492 µs |

These numbers are not too surprising, but it's kind of interesting that exception-enabled code
that runs without errors has actually gotten a little bit slower with GCC 5.1. Exception handling
itself has gotten faster, though, especially with `-O3`.

## Comparing compilers

As promised, I also made a Rust implementation of this parser, to get an idea of sort of where
it stands for mostly equivalent code. I'll repeat my disclaimer that the comparison is largely
completely unfair at this point, especially since the most obvious way to implement a parser
like this in Rust is without the use of `unsafe` code, which means that the Rust implementation
gives far stronger formal guarantees with respect to the correctness of the implementation. It
also does showcase how expressive Rust code can be using result types, especially in the presence
of the `try!` macro.

Since Rust doesn't have exceptions (well, not in the same sense anyway), I'm only comparing the
versions that use result types here. The Rust version is built with `cargo build --release`, which
by default uses the equivalent of `-O3`, so likewise I'm only comparing versions at that
optimization level. These numbers were obtained with the official Rust 1.0 compiler.

Please refer to `src/parser.rs` for the implementation tested here in Rust.

|           | benchmark         | time      |
|-----------|-------------------|----------:|
| gcc5.1-O3 | Result, no errors | 24,704 µs |
| gcc4.9-O3 | Result, no errors | 26,299 µs |
| clang-O3  | Result, no errors | 46,257 µs |
| rustc-O3  | Result, no errors | 86,399 µs |

# Conclusion

Really, this is completely in line with conventional wisdom about how to use exceptions, which is
for truly exceptional, locally irrecoverable errors. Not wrong user input, and certainly not flow
control. Whether a parser falls into that category really depends on what the parser is parsing,
not to mention how it got there.

All in all, the real-world difference between the two approaches to error handling is going to
be marginal. However, if performance is critical, and the structure of the program allows it,
and the circumstances are controlled in such a way that it is a very exceptional occurrence (but
not impossible) for the program to end up in an invalid state, then use exceptions.

But in other circumstances, given their benefits, perhaps consider a result type. :-)
