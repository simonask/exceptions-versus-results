#include "parser.hpp"
#include <cctype>
#include <string>
#include <iostream>

struct ParserWithResults : IParser {
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

        Result<int64_t> inner_expression() {
            Result<Op> op = operation();
            if (op.is_error) {
                return Result<int64_t>{op.error};
            }
            Result<int64_t> left = expression();
            if (left.is_error) {
                return left;
            }
            Result<int64_t> right = expression();
            if (right.is_error) {
                return right;
            }

            switch (op.ok) {
                case Op::Add: return Result<int64_t>{left.ok + right.ok};
                case Op::Sub: return Result<int64_t>{left.ok - right.ok};
                case Op::Mul: return Result<int64_t>{left.ok * right.ok};
                case Op::Div: return Result<int64_t>{left.ok / right.ok};
                default: return Result<int64_t>{ErrorKind::InvalidOperator};
            }
        }

        Result<int64_t> expression() {
            skip_whitespace();
            char c = peek();
            if (c == '(') {
                get_char();
                skip_whitespace();
                Result<int64_t> val = expression();
                if (val.is_error) {
                    return val;
                }
                skip_whitespace();
                auto x = expect_char(')');
                if (x.is_error) {
                    return Result<int64_t>{x.error};
                }
                return val;
            } else if (c >= '0' && c <= '9') {
                return number();
            } else {
                return inner_expression();
            }
        }

        Result<Op> operation() {
            auto c = get_char();
            if (c.is_error) {
                return Result<Op>{c.error};
            }

            switch (c.ok) {
                case '+': return Result<Op>{Op::Add};
                case '-': return Result<Op>{Op::Sub};
                case '*': return Result<Op>{Op::Mul};
                case '/': return Result<Op>{Op::Div};
                default:  return Result<Op>{ErrorKind::InvalidOperator};
            }
        }

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

        Result<char> expect_char(char c) {
            Result<char> x = get_char();
            if (x.is_error) {
                return x;
            }
            if (x.ok != c) {
                return Result<char>{ErrorKind::InvalidCharacter};
            }
            return x;
        }

        Result<char> get_char() {
            if (p == end) {
                return Result<char>{ErrorKind::UnexpectedEOF};
            }
            return Result<char>{*p++};
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
        Result<int64_t> result = p.expression();
        if (result.is_error) {
            return 0;
        } else {
            return result.ok;
        }
    }
};

std::unique_ptr<IParser> make_parser_with_results() {
    return std::unique_ptr<IParser>{new ParserWithResults};
}
