use std::collections::HashMap;
use std::rc::Rc;
use std::cell::RefCell;

#[derive(Debug, Clone)]
pub enum Value {
    Int(i64),
    Flt(i64),
    Str(String),
    Array(Rc<RefCell<Vec<Value>>>),
    Bool(bool),
    Map(Rc<RefCell<std::collections::HashMap<String, Value>>>),
    Obj(Rc<RefCell<std::collections::HashMap<String, Value>>>),
    Lambda(Rc<Lambda>),
    Buffer(Rc<RefCell<Vec<u8>>>),
    Object(Rc<RefCell<Object>>),
}

#[derive(Debug)]
pub struct Object {
    pub class: String,
    pub fields: HashMap<String, Value>,
}

#[derive(Debug)]
pub struct Lambda {
    pub params: Vec<crate::ast::Param>,
    pub ret: crate::types::Type,
    pub body: Vec<crate::ast::Stmt>,
}
#[derive(Debug)]
pub struct Runtime {
    pub scopes: Vec<HashMap<String, Value>>,
    pub func_index: HashMap<String, usize>,
    pub class_index: HashMap<String, usize>,
}

impl Runtime {
    pub fn new() -> Self { Self { scopes: vec![HashMap::new()], func_index: HashMap::new(), class_index: HashMap::new() } }
    pub fn get(&self, name: &str) -> Option<Value> {
        for i in (0..self.scopes.len()).rev() { if let Some(v) = self.scopes[i].get(name) { return Some(v.clone()); } }
        None
    }
    pub fn set(&mut self, name: String, v: Value) { if let Some(m) = self.scopes.last_mut() { m.insert(name, v); } }
    pub fn assign(&mut self, name: &str, v: Value) -> bool {
        for i in (0..self.scopes.len()).rev() { if self.scopes[i].contains_key(name) { self.scopes[i].insert(name.to_string(), v); return true; } }
        false
    }
    pub fn push_scope(&mut self) { self.scopes.push(HashMap::new()); }
    pub fn pop_scope(&mut self) { self.scopes.pop(); }
    pub fn fmt_env(&self) -> String {
        let mut out = String::new();
        for (si, scope) in self.scopes.iter().enumerate() {
            out.push_str(&format!("scope {}:\n", si));
            for (k, v) in scope {
                out.push_str(&format!("  {} = {}\n", k, crate::interpreter::fmt_value(v, 2)));
            }
        }
        out
    }
}
