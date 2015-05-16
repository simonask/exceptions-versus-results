use std::str::Chars;

#[derive(Copy,Clone)]
pub struct Parser;

struct ParserImpl<'a> {
    iter: Chars<'a>
}

enum Error {
    InvalidOperator,
    InvalidCharacter,
    UnexpectedEOF,
}

enum Op {
    Add,
    Sub,
    Mul,
    Div
}

pub type ParserResult<T> = Result<T, Error>;

impl Parser {
    pub fn execute<'a>(&self, program: &'a String) -> i64 {
        let mut p: ParserImpl = ParserImpl{iter: program.chars()};
        match p.expression() {
            Ok(x) => x,
            Err(_) => 0
        }
    }
}

impl<'a> ParserImpl<'a> {
    fn inner_expression<'b>(&'b mut self) -> ParserResult<i64> {
        self.operation().and_then(|op| {
            self.expression().and_then(|left| {
                self.expression().and_then(|right| {
                    match op {
                        Op::Add => Ok(left + right),
                        Op::Sub => Ok(left - right),
                        Op::Mul => Ok(left * right),
                        Op::Div => Ok(left / right)
                    }
                })
            })
        })
    }

    fn expression<'b>(&'b mut self) -> ParserResult<i64> {
        self.skip_whitespace();
        match self.peek() {
            '(' => {
                self.iter.next();
                self.skip_whitespace();
                let val = try!(self.expression());
                self.skip_whitespace();
                try!(self.expect_char(')'));
                Ok(val)
            },
            '0'...'9' => self.number(),
            _ => self.inner_expression()
        }
    }

    fn operation<'b>(&'b mut self) -> ParserResult<Op> {
        Ok(match try!(self.get_char()) {
            '+' => Op::Add,
            '-' => Op::Sub,
            '*' => Op::Mul,
            '/' => Op::Div,
            _ => return Err(Error::InvalidOperator)
        })
    }

    fn number<'b>(&'b mut self) -> ParserResult<i64> {
        let mut result: i64 = 0;
        while char::is_digit(self.peek(), 10) {
            let c = try!(self.get_char());
            result *= 10;
            result += c.to_digit(10).unwrap() as i64
        }
        Ok(result)
    }

    fn expect_char<'b>(&'b mut self, c: char) -> ParserResult<char> {
        match try!(self.get_char()) {
            x if x == c => Ok(c),
            _ => Err(Error::InvalidCharacter)
        }
    }

    fn get_char<'b>(&'b mut self) -> ParserResult<char> {
        match self.iter.next() {
            Some(x) => Ok(x),
            None => Err(Error::UnexpectedEOF)
        }
    }

    fn peek<'b>(&'b mut self) -> char {
        match self.iter.clone().peekable().peek() {
            Some(x) => *x,
            None => Default::default()
        }
    }

    fn skip_whitespace<'b>(&'b mut self) {
        while char::is_whitespace(self.peek()) {
            self.iter.next();
        }
    }
}
