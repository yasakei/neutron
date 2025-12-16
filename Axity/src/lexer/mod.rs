use crate::error::{AxityError, Span};
use crate::token::{Token, TokenKind};

pub fn lex(input: &str) -> Result<Vec<Token>, AxityError> {
    let mut out = Vec::new();
    let mut iter = input.chars().peekable();
    let mut line = 1usize;
    let mut col = 1usize;
    while let Some(&c) = iter.peek() {
        if c == '\n' { iter.next(); line += 1; col = 1; continue; }
        if c.is_whitespace() { iter.next(); col += 1; continue; }
        if c == '\"' {
            let start_col = col;
            iter.next(); col += 1;
            let mut s = String::new();
            while let Some(&ch) = iter.peek() {
                if ch == '\"' { break; }
                if ch == '\n' { return Err(AxityError::lex("unterminated string", Span{ line, col: start_col })); }
                if ch == '\\' {
                    iter.next(); col += 1;
                    match iter.peek().copied() {
                        Some('n') => { s.push('\n'); iter.next(); col += 1; }
                        Some('t') => { s.push('\t'); iter.next(); col += 1; }
                        Some('\\') => { s.push('\\'); iter.next(); col += 1; }
                        Some('\"') => { s.push('\"'); iter.next(); col += 1; }
                        Some(other) => { s.push(other); iter.next(); col += 1; }
                        None => break,
                    }
                } else {
                    s.push(ch); iter.next(); col += 1;
                }
            }
            if iter.peek() == Some(&'\"') { iter.next(); col += 1; } else { return Err(AxityError::lex("unterminated string", Span{ line, col: start_col })); }
            out.push(Token{ kind: TokenKind::StringLit(s), span: Span{ line, col: start_col } });
            continue;
        }
        if c.is_ascii_alphabetic() || c == '_' {
            let start_col = col;
            let mut s = String::new();
            while let Some(&ch) = iter.peek() {
                if ch.is_ascii_alphanumeric() || ch == '_' { s.push(ch); iter.next(); col += 1; } else { break; }
            }
            let kind = match s.as_str() {
                "let" => TokenKind::Let,
                "class" => TokenKind::Class,
                "import" => TokenKind::Import,
                "new" => TokenKind::New,
                "self" => TokenKind::SelfKw,
                "fn" => TokenKind::Fn,
                "return" => TokenKind::Return,
                "print" => TokenKind::Print,
                "while" => TokenKind::While,
                "if" => TokenKind::If,
                "else" => TokenKind::Else,
                "try" => TokenKind::Try,
                "catch" => TokenKind::Catch,
                "throw" => TokenKind::Throw,
                "retry" => TokenKind::Retry,
                "do" => TokenKind::Do,
                "for" => TokenKind::For,
                "in" => TokenKind::In,
                "match" => TokenKind::Match,
                "case" => TokenKind::Case,
                "default" => TokenKind::Default,
                "string" | "str" => TokenKind::StringType,
                "int" => TokenKind::IntType,
                "flt" => TokenKind::FltType,
                "obj" => TokenKind::ObjType,
                "buffer" => TokenKind::BufferType,
                "any" => TokenKind::AnyType,
                "bool" => TokenKind::BoolType,
                "true" => TokenKind::TrueKw,
                "false" => TokenKind::FalseKw,
                "and" => TokenKind::AndAnd,
                "or" => TokenKind::OrOr,
                "array" => TokenKind::ArrayKw,
                "map" => TokenKind::MapKw,
                _ => TokenKind::Ident(s),
            };
            out.push(Token{ kind, span: Span{ line, col: start_col } });
            continue;
        }
        if c.is_ascii_digit() {
            let start_col = col;
            let mut s = String::new();
            let mut has_dot = false;
            let mut has_exp = false;
            while let Some(&ch) = iter.peek() {
                if ch.is_ascii_digit() { s.push(ch); iter.next(); col += 1; }
                else if ch == '.' && !has_dot { has_dot = true; s.push(ch); iter.next(); col += 1; }
                else if (ch == 'e' || ch == 'E') && !has_exp { has_exp = true; s.push(ch); iter.next(); col += 1;
                    if let Some(&sign) = iter.peek() { if sign=='+' || sign=='-' { s.push(sign); iter.next(); col += 1; } }
                }
                else { break; }
            }
            if has_dot || has_exp {
                let f = s.parse::<f64>().unwrap_or(0.0);
                let scaled = (f * 1_000_000.0).round() as i64;
                out.push(Token{ kind: TokenKind::FltLit(scaled), span: Span{ line, col: start_col } });
            } else {
                let v = s.parse::<i64>().unwrap_or(0);
                out.push(Token{ kind: TokenKind::IntLit(v), span: Span{ line, col: start_col } });
            }
            continue;
        }
        match c {
            '(' => { out.push(Token{ kind: TokenKind::LParen, span: Span{ line, col } }); iter.next(); col += 1; }
            ')' => { out.push(Token{ kind: TokenKind::RParen, span: Span{ line, col } }); iter.next(); col += 1; }
            '{' => { out.push(Token{ kind: TokenKind::LBrace, span: Span{ line, col } }); iter.next(); col += 1; }
            '}' => { out.push(Token{ kind: TokenKind::RBrace, span: Span{ line, col } }); iter.next(); col += 1; }
            '[' => { out.push(Token{ kind: TokenKind::LBracket, span: Span{ line, col } }); iter.next(); col += 1; }
            ']' => { out.push(Token{ kind: TokenKind::RBracket, span: Span{ line, col } }); iter.next(); col += 1; }
            ':' => { out.push(Token{ kind: TokenKind::Colon, span: Span{ line, col } }); iter.next(); col += 1; }
            ';' => { out.push(Token{ kind: TokenKind::Semicolon, span: Span{ line, col } }); iter.next(); col += 1; }
            ',' => { out.push(Token{ kind: TokenKind::Comma, span: Span{ line, col } }); iter.next(); col += 1; }
            '.' => { out.push(Token{ kind: TokenKind::Dot, span: Span{ line, col } }); iter.next(); col += 1; }
            '-' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('>') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::Arrow, span: Span{ line, col: start_col } }); }
                else if let Some('-') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::MinusMinus, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Minus, span: Span{ line, col: start_col } }); }
            }
            '=' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('=') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::EqEq, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Assign, span: Span{ line, col: start_col } }); }
            }
            '+' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('+') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::PlusPlus, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Plus, span: Span{ line, col: start_col } }); }
            }
            '*' => { out.push(Token{ kind: TokenKind::Star, span: Span{ line, col } }); iter.next(); col += 1; }
            '/' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('/') = iter.peek().copied() {
                    // Could be '//' single-line or '///' block comment
                    iter.next(); col += 1;
                    if let Some('/') = iter.peek().copied() {
                        // '///' block comment start
                        iter.next(); col += 1;
                        // consume until closing '///'
                        loop {
                            match iter.peek().copied() {
                                Some('\n') => { iter.next(); line += 1; col = 1; }
                                Some('/') => {
                                    // check for following '//' to complete '///'
                                    let mut save = iter.clone();
                                    save.next(); // consumed current '/'
                                    if let Some('/') = save.peek().copied() {
                                        save.next();
                                        if let Some('/') = save.peek().copied() {
                                            // found closing '///', advance original iter accordingly
                                            iter.next(); // first '/'
                                            col += 1;
                                            iter.next(); // second '/'
                                            col += 1;
                                            iter.next(); // third '/'
                                            col += 1;
                                            break;
                                        } else {
                                            // it's '//' not '///' inside block, just advance one '/'
                                            iter.next(); col += 1;
                                        }
                                    } else {
                                        iter.next(); col += 1;
                                    }
                                }
                                Some(ch) => { iter.next(); col += 1; }
                                None => break,
                            }
                        }
                        continue;
                    } else {
                        // single-line comment: skip until end of line
                        while let Some(&ch) = iter.peek() {
                            if ch == '\n' { break; }
                            iter.next(); col += 1;
                        }
                        continue;
                    }
                } else {
                    out.push(Token{ kind: TokenKind::Slash, span: Span{ line, col: start_col } });
                }
            }
            '%' => { out.push(Token{ kind: TokenKind::Percent, span: Span{ line, col } }); iter.next(); col += 1; }
            '&' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('&') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::AndAnd, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::BitAnd, span: Span{ line, col: start_col } }); }
            }
            '|' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('|') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::OrOr, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::BitOr, span: Span{ line, col: start_col } }); }
            }
            '^' => { out.push(Token{ kind: TokenKind::BitXor, span: Span{ line, col } }); iter.next(); col += 1; }
            '~' => { out.push(Token{ kind: TokenKind::Tilde, span: Span{ line, col } }); iter.next(); col += 1; }
            '<' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('<') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::Shl, span: Span{ line, col: start_col } }); }
                else if let Some('=') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::LessEq, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Less, span: Span{ line, col: start_col } }); }
            }
            '>' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('>') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::Shr, span: Span{ line, col: start_col } }); }
                else if let Some('=') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::GreaterEq, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Greater, span: Span{ line, col: start_col } }); }
            }
            '!' => {
                let start_col = col;
                iter.next(); col += 1;
                if let Some('=') = iter.peek().copied() { iter.next(); col += 1; out.push(Token{ kind: TokenKind::NotEq, span: Span{ line, col: start_col } }); }
                else { out.push(Token{ kind: TokenKind::Bang, span: Span{ line, col: start_col } }); }
            }
            _ => {
                return Err(AxityError::lex("unexpected character", Span{ line, col }));
            }
        }
    }
    out.push(Token{ kind: TokenKind::Eof, span: Span{ line, col } });
    Ok(out)
}

