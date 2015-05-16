extern crate libc;

mod benchmark;
mod parser;
use benchmark::Benchmark;
use parser::Parser;

use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::path::Path;

use libc::funcs::posix01::resource::getrusage;
use libc::types::os::common::bsd43::{rusage};

fn get_process_time_us() -> u64 {
    unsafe {
        let mut u: rusage = std::mem::uninitialized();
        getrusage(0, &mut u);
        (u.ru_utime.tv_sec as u64) * 1000000 + (u.ru_utime.tv_usec as u64)
    }
}

fn time_lambda<F: FnOnce()>(func: F) -> u64 {
    let before = get_process_time_us();
    func();
    let after = get_process_time_us();
    after - before
}

fn run_benchmark<B: Benchmark>(benchmark: B, description: &'static str, iterations: u64) -> u64 {
    let mut csv: File = OpenOptions::new().append(true).write(true).open("results.csv").unwrap();
    print!("{0: >#20}  {1: <#50}  ", "rustc", description);

    let us = time_lambda(move || {
        for _ in (0..iterations) {
            benchmark.run();
        }
    });

    println!("{0: >#10}µs", us);
    writeln!(csv, "{0};{1};{2}", "rustc", description, us).unwrap();
    
    us
}

struct BenchmarkParser {
    program: String
}

impl BenchmarkParser {
    fn new<P: AsRef<Path>>(path: P) -> BenchmarkParser {
        let mut file = OpenOptions::new().read(true).open(path).unwrap();
        let mut program: String = Default::default();
        file.read_to_string(&mut program).unwrap();
        BenchmarkParser { program: program }
    }
}

impl Benchmark for BenchmarkParser {
    fn run(&self) -> i64 {
        let result = Parser.execute(&self.program);
        result as i64
    }
}

fn main() {
    let args: Vec<_> = std::env::args().collect();
    if args.len() != 2 {
        println!("Please provide number of iterations as first argument.");
        std::process::exit(1);
    }
    let iterations = match args[1].parse::<u64>() {
        Ok(x) => x,
        Err(err) => {
            println!("Please provice number of iterations as first argument (error: {0})", err);
            std::process::exit(1)
        }
    };

    run_benchmark(BenchmarkParser::new("input.ok"), "parser-results-no-errors", iterations);
    run_benchmark(BenchmarkParser::new("input.err"), "parser-results-with-errors", iterations);
}
