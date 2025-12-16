use std::fmt::{Display, Formatter};

#[derive(Debug, Clone)]
pub struct Span {
    pub line: usize,
    pub col: usize,
}

#[derive(Debug, Clone)]
pub enum AxityErrorKind {
    Lex(String),
    Parse(String),
    Type(String),
    Runtime(String),
}

#[derive(Debug, Clone)]
pub struct AxityError {
    pub kind: AxityErrorKind,
    pub span: Option<Span>,
}

impl AxityError {
    pub fn lex(msg: &str, span: Span) -> Self { Self { kind: AxityErrorKind::Lex(msg.to_string()), span: Some(span) } }
    pub fn parse(msg: &str, span: Span) -> Self { Self { kind: AxityErrorKind::Parse(msg.to_string()), span: Some(span) } }
    pub fn ty(msg: &str, span: Span) -> Self { Self { kind: AxityErrorKind::Type(msg.to_string()), span: Some(span) } }
    pub fn rt(msg: &str) -> Self { Self { kind: AxityErrorKind::Runtime(msg.to_string()), span: None } }
}


impl Display for AxityError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match (&self.kind, &self.span) {
            (AxityErrorKind::Lex(m), Some(s)) => write!(f, "lex error at {}:{}: {}", s.line, s.col, m),
            (AxityErrorKind::Parse(m), Some(s)) => write!(f, "parse error at {}:{}: {}", s.line, s.col, m),
            (AxityErrorKind::Type(m), Some(s)) => write!(f, "type error at {}:{}: {}", s.line, s.col, m),
            (AxityErrorKind::Runtime(m), Some(s)) => write!(f, "runtime error at {}:{}: {}", s.line, s.col, m),
            (AxityErrorKind::Runtime(m), None) => write!(f, "runtime error: {}", m),
            (k, None) => write!(f, "{:?}", k),
        }
    }
}

impl std::error::Error for AxityError {}

