#pragma once
#ifndef CALCULATOR_HPP
#define CALCULATOR_HPP

#include <memory>

struct IParser {
    virtual ~IParser() {}
    virtual int64_t execute(const std::string& program) const = 0;
};

enum class ErrorKind {
    InvalidOperator,
    InvalidCharacter,
    UnexpectedEOF,
};

std::unique_ptr<IParser> make_parser_with_exceptions();
std::unique_ptr<IParser> make_parser_with_results();

#endif // CALCULATOR_HPP
