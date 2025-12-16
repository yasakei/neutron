#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Type {
    Int,
    String,
    Flt,
    Bool,
    Obj,
    Any,
    Array(Box<Type>),
    Map(Box<Type>),
    Fn(Vec<Type>, Box<Type>),
    Buffer,
    Class(String),
}
