#include <sys/time.h>
#include <sys/resource.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "parser.hpp"

uint64_t get_process_time_us() {
    rusage u;
    ::getrusage(RUSAGE_SELF, &u);
    return u.ru_utime.tv_sec * 1000000 + u.ru_utime.tv_usec;
}

template <class F>
__attribute__((noinline))
uint64_t time_lambda_us(F func)
{
    uint64_t before = get_process_time_us();
    func();
    uint64_t after = get_process_time_us();
    return after - before;
}

struct TestParserWithExceptions {
    std::unique_ptr<IParser> calc;
    std::string program;
    TestParserWithExceptions(const char* input_file) : calc(make_parser_with_exceptions()) {
        std::ifstream f{input_file};
        std::getline(f, program);
    }

    uint64_t run(uint64_t state) {
        int64_t result = calc->execute(program);
        return static_cast<uint64_t>(result);
    }
};

struct TestParserWithResults {
    std::unique_ptr<IParser> calc;
    std::string program;
    TestParserWithResults(const char* input_file) : calc(make_parser_with_results()) {
        std::ifstream f{input_file};
        std::getline(f, program);
    }

    uint64_t run(uint64_t state) {
        int64_t result = calc->execute(program);
        return static_cast<uint64_t>(result);
    } 
};

#if !defined(COMPILER)
#error "Please recompile with -DCOMPILER=..."
#endif

#define Q2(X) #X
#define Q(X) Q2(X)

#define COMPILER_NAME Q(COMPILER)

template <class Test, class... Args>
__attribute__((noinline))
uint64_t run_benchmark(uint64_t state, const char* description, size_t iterations, Args&&... args) {
    Test test{std::forward<Args>(args)...};

    std::ofstream csv{"results.csv", std::ios_base::app};

    std::cout << std::setw(20) << std::right << COMPILER_NAME;
    std::cout << "  ";
    std::cout << std::setw(50) << std::left << description;
    std::cout << "  ";

    csv << COMPILER_NAME << ';' << description << ';';


    auto us = time_lambda_us([&]() {
        for (size_t i = 0; i < iterations; ++i) {
            state += test.run(state);
        }
    });

    std::cout << std::setw(10) << std::right << us << "µs\n";
    csv << us << '\n';
    return state;
}

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        std::cerr << "Please give number of iterations as argument.\n";
        return 1;
    }

    std::stringstream ss;
    ss << argv[1];
    size_t iterations;
    if (!(ss >> iterations)) {
        std::cerr << "First argument must be a number.\n";
        return 1;
    }

    run_benchmark<TestParserWithExceptions>(0, "parser-exceptions-no-errors", iterations, "input.ok");
    run_benchmark<TestParserWithResults>(0, "parser-results-no-errors", iterations, "input.ok");
    run_benchmark<TestParserWithExceptions>(0, "parser-exceptions-with-errors", iterations, "input.err");
    run_benchmark<TestParserWithResults>(0, "parser-results-with-errors", iterations, "input.err");
    return 0;
}
