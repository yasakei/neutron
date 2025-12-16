#![allow(warnings)]

pub mod token;
pub mod lexer;
pub mod ast;
pub mod parser;
pub mod types;
pub mod type_checker;
pub mod runtime;
pub mod interpreter;
pub mod error;

pub use error::AxityError;

pub fn run_source(source: &str) -> Result<String, AxityError> {
    let tokens = lexer::lex(source)?;
    let ast = parser::parse(&tokens)?;
    type_checker::check(&ast)?;
    let mut rt = runtime::Runtime::new();
    let mut out = String::new();
    interpreter::execute(&ast, &mut rt, &mut out)?;
    Ok(out)
}

pub fn run_source_with_runtime(source: &str, rt: &mut runtime::Runtime) -> Result<String, AxityError> {
    let tokens = lexer::lex(source)?;
    let ast = parser::parse(&tokens)?;
    type_checker::check(&ast)?;
    let mut out = String::new();
    interpreter::execute(&ast, rt, &mut out)?;
    Ok(out)
}

pub fn run_file(path: &str) -> Result<String, AxityError> {
    use std::fs;
    use std::path::{Path, PathBuf};
    let base = Path::new(path).parent().map(|p| p.to_path_buf()).unwrap_or(PathBuf::from("."));
    let src = fs::read_to_string(path).map_err(|e| AxityError::rt(&format!("read error: {}", e)))?;
    let tokens = lexer::lex(&src)?;
    let mut ast = parser::parse(&tokens)?;
    resolve_imports(&mut ast, &base, &mut std::collections::HashSet::new())?;
    type_checker::check(&ast)?;
    let mut rt = runtime::Runtime::new();
    let mut out = String::new();
    interpreter::execute(&ast, &mut rt, &mut out)?;
    Ok(out)
}

fn resolve_imports(prog: &mut ast::Program, base: &std::path::Path, visited: &mut std::collections::HashSet<std::path::PathBuf>) -> Result<(), AxityError> {
    use std::fs;
    use std::path::PathBuf;
    let mut extra_items: Vec<ast::Item> = Vec::new();
    let mut remaining: Vec<ast::Item> = Vec::new();
    for it in prog.items.drain(..) {
        match it {
            ast::Item::Import(p, sp) => {
                let mut full = PathBuf::from(base);
                full.push(&p);
                let canon = full.clone();
                if visited.contains(&canon) { continue; }
                visited.insert(canon.clone());
                let src = fs::read_to_string(&full).map_err(|e| AxityError::parse(&format!("import read error: {}", e), sp.clone()))?;
                let toks = crate::lexer::lex(&src)?;
                let mut imp_prog = crate::parser::parse(&toks)?;
                let imp_base = canon.parent().map(|x| x.to_path_buf()).unwrap_or(base.to_path_buf());
                resolve_imports(&mut imp_prog, &imp_base, visited)?;
                for i in imp_prog.items { if !matches!(i, ast::Item::Import(_, _)) { extra_items.push(i); } }
            }
            other => remaining.push(other),
        }
    }
    prog.items = remaining;
    prog.items.extend(extra_items);
    Ok(())
}
