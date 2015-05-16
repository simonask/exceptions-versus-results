#include "parser.hpp"
#include <cctype>
#include <string>
#include <iostream>

struct ParserWithExceptions : IParser {
    struct Error {
        ErrorKind kind;

        Error(ErrorKind kind) : kind(kind) {}
    };

    enum class Op {
        Add,
        Sub,
        Mul,
        Div,
    };

    struct Parser {
        const char* p;
        const char* end;

        Parser(const std::string& program) : p(program.data()), end(program.data() + program.size()) {}

        int64_t inner_expression() {
            Op op = operation();
            int64_t left = expression();
            int64_t right = expression();
            switch (op) {
                case Op::Add: return left + right;
                case Op::Sub: return left - right;
                case Op::Mul: return left * right;
                case Op::Div: return left / right;
                default: throw Error{ErrorKind::InvalidOperator};
            }
        }

        int64_t expression() {
            skip_whitespace();
            char c = peek();
            if (c == '(') {
                get_char();
                skip_whitespace();
                int64_t val = expression();
                skip_whitespace();
                expect_char(')');
                return val;
            } else if (c >= '0' && c <= '9') {
                return number();
            } else {
                return inner_expression();
            }
        }

        Op operation() {
            char c = get_char();
            switch (c) {
                case '+': return Op::Add;
                case '-': return Op::Sub;
                case '*': return Op::Mul;
                case '/': return Op::Div;
                default: throw Error{ErrorKind::InvalidOperator};
            }
        }

        int64_t number() {
            int64_t result = 0;
            while (std::isdigit(peek())) {
                char c = get_char();
                result *= 10;
                result += c - '0';
            }
            return result;
        }

        void expect_char(char c) {
            char x = get_char();
            if (x != c) {
                throw Error{ErrorKind::InvalidCharacter};
            }
        }

        char get_char() {
            if (p == end) {
                throw Error{ErrorKind::UnexpectedEOF};
            }
            return *p++;
        }

        char peek() {
            if (p == end) {
                return 0;
            }
            return *p;
        }

        void skip_whitespace() {
            while (std::iswspace(peek())) {
                get_char();
            }
        }
    };

    int64_t execute(const std::string& program) const final {
        Parser p{program};
        try {
            return p.expression();
        }
        catch (const Error& err) {
            return 0;
        }
    }
};

std::unique_ptr<IParser> make_parser_with_exceptions() {
    return std::unique_ptr<IParser>{new ParserWithExceptions};
}
