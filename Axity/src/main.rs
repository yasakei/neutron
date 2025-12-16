#![allow(warnings)]

use std::env;
use axity::run_file;
use axity::{run_source_with_runtime};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 { eprintln!("usage: axity [--dump-tokens] [--dump-ast] <file.ax>"); std::process::exit(1); }
    let mut dump_tokens = false;
    let mut dump_ast = false;
    let mut file = None;
    for a in &args[1..] {
        if a == "--dump-tokens" { dump_tokens = true; }
        else if a == "--dump-ast" { dump_ast = true; }
        else { file = Some(a.clone()); }
    }
    let file = match file { Some(f) => f, None => { eprintln!("usage: axity [--dump-tokens] [--dump-ast] <file.ax>"); std::process::exit(1) } };
    if dump_tokens || dump_ast {
        let src = std::fs::read_to_string(&file).unwrap_or_default();
        let toks = axity::lexer::lex(&src).unwrap_or_default();
        if dump_tokens {
            for t in &toks { println!("{:?} @{}:{}", t.kind, t.span.line, t.span.col); }
        }
        if dump_ast {
            let ast = axity::parser::parse(&toks).unwrap();
            println!("{:#?}", ast);
        }
    }
    if file == "repl" || file == "--repl" {
        use std::io::{self, Write};
        let mut rt = axity::runtime::Runtime::new();
        loop {
            print!("axity> ");
            io::stdout().flush().ok();
            let mut line = String::new();
            if io::stdin().read_line(&mut line).is_err() { break; }
            let line = line.trim_end();
            if line.is_empty() { continue; }
            if line.starts_with(':') {
                if line == ":quit" { break; }
                else if line.starts_with(":load ") {
                    let path = &line[6..].trim();
                    match run_file(path) {
                        Ok(out) => print!("{}", out),
                        Err(e) => eprintln!("{}", e),
                    }
                } else if line == ":env" {
                    print!("{}", rt.fmt_env());
                } else {
                    eprintln!("unknown command");
                }
                continue;
            }
            match run_source_with_runtime(line, &mut rt) {
                Ok(out) => print!("{}", out),
                Err(e) => eprintln!("{}", e),
            }
        }
        return;
    }
    match run_file(&file) {
        Ok(out) => print!("{}", out),
        Err(e) => { eprintln!("{}", e); std::process::exit(1); }
    }
}
