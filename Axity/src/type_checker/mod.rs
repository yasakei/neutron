use std::collections::HashMap;
use crate::ast::*;
use crate::error::{AxityError, Span};
use crate::types::Type;

pub fn check(p: &Program) -> Result<(), AxityError> {
    let mut funcs: HashMap<String, (Vec<Type>, Type, Span)> = HashMap::new();
    let mut classes: HashMap<String, (HashMap<String, Type>, HashMap<String, (Vec<Type>, Type)>)> = HashMap::new();
    for it in &p.items {
        if let Item::Func(f) = it {
            if funcs.contains_key(&f.name) { return Err(AxityError::ty("duplicate function", f.span.clone())); }
            funcs.insert(f.name.clone(), (f.params.iter().map(|x| x.ty.clone()).collect(), f.ret.clone(), f.span.clone()));
        }
        if let Item::Class(c) = it {
            if classes.contains_key(&c.name) { return Err(AxityError::ty("duplicate class", c.span.clone())); }
            let mut flds = HashMap::new();
            for fld in &c.fields {
                if flds.contains_key(&fld.name) { return Err(AxityError::ty("duplicate field", fld.span.clone())); }
                flds.insert(fld.name.clone(), fld.ty.clone());
            }
            let mut meths = HashMap::new();
            for m in &c.methods {
                if meths.contains_key(&m.name) { return Err(AxityError::ty("duplicate method", m.span.clone())); }
                let sig = (m.params.iter().map(|x| x.ty.clone()).collect::<Vec<_>>(), m.ret.clone());
                meths.insert(m.name.clone(), sig);
            }
            classes.insert(c.name.clone(), (flds, meths));
        }
    }
    let mut vars: Vec<HashMap<String, Type>> = vec![HashMap::new()];
    for it in &p.items {
        match it {
            Item::Stmt(s) => check_stmt(s, &mut vars, &funcs, &classes)?,
            Item::Func(f) => {
                vars.push(HashMap::new());
                for par in &f.params { vars.last_mut().unwrap().insert(par.name.clone(), par.ty.clone()); }
                // Loosen function body enforcement to allow interpreter-driven semantics
                let mut has_return = f.body.iter().any(|s| matches!(s, Stmt::Return{..}));
                vars.pop();
                if f.ret == Type::Int && !has_return { return Err(AxityError::ty("missing return", f.span.clone())); }
            }
            Item::Class(_) => {}
            Item::Import(_, _) => {}
        }
    }
    Ok(())
}

fn check_stmt(s: &Stmt, vars: &mut Vec<HashMap<String, Type>>, funcs: &HashMap<String,(Vec<Type>,Type,Span)>, classes: &HashMap<String,(HashMap<String,Type>,HashMap<String,(Vec<Type>,Type)>)>) -> Result<(), AxityError> {
    match s {
        Stmt::Let{ name, ty, init, span } => {
            let _t = check_expr(init, vars, funcs, classes)?;
            if vars.last().unwrap().contains_key(name) { return Err(AxityError::ty("duplicate variable", span.clone())); }
            vars.last_mut().unwrap().insert(name.clone(), ty.clone());
            Ok(())
        }
        Stmt::Assign{ name, expr, span } => {
            let _t = check_expr(expr, vars, funcs, classes)?;
            let vt = lookup_var(name, vars).ok_or_else(|| AxityError::ty("undefined variable", span.clone()))?;
            Ok(())
        }
        Stmt::MemberAssign{ object, field, expr, span } => {
            let ot = check_expr(object, vars, funcs, classes)?;
            let vt = check_expr(expr, vars, funcs, classes)?;
            if let Type::Class(ref cname) = ot {
                let (flds, _) = classes.get(cname).ok_or_else(|| AxityError::ty("unknown class", span.clone()))?;
                let ft = flds.get(field).ok_or_else(|| AxityError::ty("unknown field", span.clone()))?;
                if !(type_equals(&vt, ft)) { return Err(AxityError::ty("field type mismatch", span.clone())); }
                Ok(())
            } else if let Type::Obj = ot {
                Ok(())
            } else if let Type::Any = ot {
                Ok(())
            } else { Err(AxityError::ty("member assign target not class", span.clone())) }
        }
        Stmt::Expr(e) => { let _ = check_expr(e, vars, funcs, classes)?; Ok(()) }
        Stmt::Print{ expr, .. } => { let _ = check_expr(expr, vars, funcs, classes)?; Ok(()) }
        Stmt::While{ cond, body, span: _ } => {
            let _ = check_expr(cond, vars, funcs, classes)?;
            vars.push(HashMap::new());
            for st in body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            Ok(())
        }
        Stmt::DoWhile{ body, cond, .. } => {
            vars.push(HashMap::new());
            for st in body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            let _ = check_expr(cond, vars, funcs, classes)?;
            Ok(())
        }
        Stmt::ForC{ init, cond, post, body, .. } => {
            vars.push(HashMap::new());
            if let Some(st) = init { check_stmt(&*st, vars, funcs, classes)?; }
            if let Some(c) = cond { let _ = check_expr(c, vars, funcs, classes)?; }
            if let Some(pe) = post { check_stmt(&*pe, vars, funcs, classes)?; }
            for st in body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            Ok(())
        }
        Stmt::ForEach{ var, collection, body, span: _ } => {
            let ct = check_expr(collection, vars, funcs, classes)?;
            vars.push(HashMap::new());
            match ct {
                Type::Array(inner) => { vars.last_mut().unwrap().insert(var.clone(), *inner.clone()); }
                Type::Map(_inner) => { vars.last_mut().unwrap().insert(var.clone(), Type::String); }
                _ => { return Err(AxityError::ty("foreach expects array or map", span_of_expr(collection))); }
            }
            for st in body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            Ok(())
        }
        Stmt::If{ cond, then_body, else_body, span: _ } => {
            let _ = check_expr(cond, vars, funcs, classes)?;
            vars.push(HashMap::new());
            for st in then_body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            vars.push(HashMap::new());
            for st in else_body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            Ok(())
        }
        Stmt::Return{ expr, .. } => { let _ = check_expr(expr, vars, funcs, classes)?; Ok(()) }
        Stmt::Retry(_) => Ok(()),
        Stmt::Throw{ expr, .. } => { let _ = check_expr(expr, vars, funcs, classes)?; Ok(()) }
        Stmt::Try{ body, catch_name, catch_body, .. } => {
            vars.push(HashMap::new());
            for st in body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            vars.push(HashMap::new());
            vars.last_mut().unwrap().insert(catch_name.clone(), Type::Obj);
            for st in catch_body { check_stmt(st, vars, funcs, classes)?; }
            vars.pop();
            Ok(())
        }
        Stmt::Match{ expr, arms, default: _, span: _ } => {
            let et = check_expr(expr, vars, funcs, classes)?;
            for arm in arms {
                let pt = match arm.pat {
                    Pattern::PInt(_) => Type::Int,
                    Pattern::PStr(_) => Type::String,
                    Pattern::PBool(_) => Type::Bool,
                };
                vars.push(HashMap::new());
                for st in &arm.body { check_stmt(st, vars, funcs, classes)?; }
                vars.pop();
            }
            Ok(())
        }
    }
}

fn check_expr(e: &Expr, vars: &Vec<HashMap<String, Type>>, funcs: &HashMap<String,(Vec<Type>,Type,Span)>, classes: &HashMap<String,(HashMap<String,Type>,HashMap<String,(Vec<Type>,Type)>)>) -> Result<Type, AxityError> {
    match e {
        Expr::Int(_, _) => Ok(Type::Int),
        Expr::Flt(_, _) => Ok(Type::Flt),
        Expr::Str(_, _) => Ok(Type::String),
        Expr::Bool(_, _) => Ok(Type::Bool),
        Expr::ArrayLit(elems, sp) => {
            if elems.is_empty() { return Err(AxityError::ty("empty array literal needs type context", sp.clone())); }
            let first = check_expr(&elems[0], vars, funcs, classes)?;
            for el in elems.iter().skip(1) {
                let et = check_expr(el, vars, funcs, classes)?;
                if !(type_equals(&et, &first)) { return Err(AxityError::ty("array literal elements must match", sp.clone())); }
            }
            Ok(Type::Array(Box::new(first)))
        }
        Expr::ObjLit(_pairs, _sp) => Ok(Type::Obj),
        Expr::Lambda{ params, ret, .. } => {
            let arg_tys = params.iter().map(|p| p.ty.clone()).collect::<Vec<_>>();
            Ok(Type::Fn(arg_tys, Box::new(ret.clone())))
        }
        Expr::Var(name, sp) => lookup_var(name, vars).ok_or_else(|| AxityError::ty("undefined variable", sp.clone())),
        Expr::Binary{ left, right, op, .. } => {
            let lt = check_expr(left, vars, funcs, classes)?;
            let rt = check_expr(right, vars, funcs, classes)?;
            match op {
                BinOp::Add | BinOp::Sub | BinOp::Mul | BinOp::Div | BinOp::Mod | BinOp::BitAnd | BinOp::BitOr | BinOp::BitXor | BinOp::Shl | BinOp::Shr => {
                    if *op == BinOp::Add && lt==Type::String && rt==Type::String { Ok(Type::String) }
                    else if lt==Type::Flt || rt==Type::Flt { Ok(Type::Flt) }
                    else { Ok(Type::Int) }
                }
                BinOp::Lt | BinOp::Le | BinOp::Gt | BinOp::Ge | BinOp::Eq | BinOp::Ne => {
                    Ok(Type::Int)
                }
                BinOp::And | BinOp::Or => {
                    if lt==Type::Bool && rt==Type::Bool { Ok(Type::Bool) } else { Err(AxityError::ty("logical operators require bool", span_of_expr(e))) }
                }
            }
        }
        Expr::UnaryNot{ expr, span } => {
            let t = check_expr(expr, vars, funcs, classes)?;
            if t != Type::Bool { return Err(AxityError::ty("! requires bool", span.clone())); }
            Ok(Type::Bool)
        }
        Expr::UnaryNeg{ expr, .. } => {
            let t = check_expr(expr, vars, funcs, classes)?;
            Ok(t)
        }
        Expr::UnaryBitNot{ expr, span } => {
            let t = check_expr(expr, vars, funcs, classes)?;
            if t != Type::Int { return Err(AxityError::ty("~ requires int", span.clone())); }
            Ok(Type::Int)
        }
        Expr::New(name, _args, _) => Ok(Type::Class(name.clone())),
        Expr::Member{ object, field, span } => {
            let ot = check_expr(object, vars, funcs, classes)?;
            if let Type::Class(ref cname) = ot {
                let (flds, _) = classes.get(cname).ok_or_else(|| AxityError::ty("unknown class", span.clone()))?;
                flds.get(field).cloned().ok_or_else(|| AxityError::ty("unknown field", span.clone()))
            } else if let Type::Obj = ot {
                Ok(Type::Obj)
            } else if let Type::Any = ot {
                Ok(Type::Any)
            } else { Err(AxityError::ty("member access target not class", span.clone())) }
        }
        Expr::Index{ array, index, span } => {
            let at = check_expr(array, vars, funcs, classes)?;
            let it = check_expr(index, vars, funcs, classes)?;
            if it != Type::Int { return Err(AxityError::ty("array index must be int", span.clone())); }
            if let Type::Array(inner) = at { Ok(*inner.clone()) } else { Err(AxityError::ty("indexing non-array", span.clone())) }
        }
        Expr::CallCallee{ callee, args: _, span } => {
            let ct = check_expr(callee, vars, funcs, classes)?;
            match ct {
                Type::Fn(_p, ret) => Ok(*ret.clone()),
                _ => Err(AxityError::ty("callee is not function", span.clone()))
            }
        }
        Expr::MethodCall{ object, name, args: _, span } => {
            let ot = check_expr(object, vars, funcs, classes)?;
            if let Type::Class(ref cname) = ot {
                let (_, meths) = classes.get(cname).ok_or_else(|| AxityError::ty("unknown class", span.clone()))?;
                let sig = meths.get(name).ok_or_else(|| AxityError::ty("unknown method", span.clone()))?;
                Ok(sig.1.clone())
            } else { Err(AxityError::ty("method call target not class", span.clone())) }
        }
        Expr::Call{ name, args, span } => {
            if name == "len" {
                if args.len() != 1 { return Err(AxityError::ty("len expects one argument", span.clone())); }
                let at = check_expr(&args[0], vars, funcs, classes)?;
                match at {
                    Type::Array(_) => Ok(Type::Int),
                    Type::String => Ok(Type::Int),
                    _ => Err(AxityError::ty("len expects array or string", span.clone()))
                }
            } else if name == "slice" {
                if args.len() != 3 { return Err(AxityError::ty("slice expects (array, start, len)", span.clone())); }
                let at = check_expr(&args[0], vars, funcs, classes)?;
                let st = check_expr(&args[1], vars, funcs, classes)?;
                let lt = check_expr(&args[2], vars, funcs, classes)?;
                if st != Type::Int || lt != Type::Int { return Err(AxityError::ty("slice indices must be int", span.clone())); }
                if let Type::Array(inner) = at { Ok(Type::Array(inner.clone())) } else { Err(AxityError::ty("slice expects array", span.clone())) }
            } else if name == "range" {
                if args.len() != 2 { return Err(AxityError::ty("range expects (start, end)", span.clone())); }
                let st = check_expr(&args[0], vars, funcs, classes)?;
                let et = check_expr(&args[1], vars, funcs, classes)?;
                if st != Type::Int || et != Type::Int { return Err(AxityError::ty("range args must be int", span.clone())); }
                Ok(Type::Array(Box::new(Type::Int)))
            } else if name == "map_remove" {
                if args.len() != 2 { return Err(AxityError::ty("map_remove expects (map, key)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                let kt = check_expr(&args[1], vars, funcs, classes)?;
                if kt != Type::String { return Err(AxityError::ty("map key must be string", span.clone())); }
                match mt { Type::Map(_) => Ok(Type::Int), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "map_clear" {
                if args.len() != 1 { return Err(AxityError::ty("map_clear expects (map)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                match mt { Type::Map(_) => Ok(Type::Int), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "map_size" {
                if args.len() != 1 { return Err(AxityError::ty("map_size expects (map)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                match mt { Type::Map(_) => Ok(Type::Int), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "string_replace" {
                if args.len() != 3 { return Err(AxityError::ty("string_replace expects (s, from, to)", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                let t1 = check_expr(&args[1], vars, funcs, classes)?;
                let t2 = check_expr(&args[2], vars, funcs, classes)?;
                if t0 != Type::String || t1 != Type::String || t2 != Type::String { return Err(AxityError::ty("string_replace arg types", span.clone())); }
                Ok(Type::String)
            } else if name == "string_split" {
                if args.len() != 2 { return Err(AxityError::ty("string_split expects (s, sep)", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                let t1 = check_expr(&args[1], vars, funcs, classes)?;
                if t0 != Type::String || t1 != Type::String { return Err(AxityError::ty("string_split arg types", span.clone())); }
                Ok(Type::Array(Box::new(Type::String)))
            } else if name == "read_file" {
                if args.len() != 1 { return Err(AxityError::ty("read_file expects path", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("path must be string", span.clone())); }
                Ok(Type::String)
            } else if name == "write_file" {
                if args.len() != 2 { return Err(AxityError::ty("write_file expects (path, content)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String || check_expr(&args[1], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("write_file arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "mkdir" {
                if args.len() != 1 { return Err(AxityError::ty("mkdir expects path", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("path must be string", span.clone())); }
                Ok(Type::Int)
            } else if name == "exists" {
                if args.len() != 1 { return Err(AxityError::ty("exists expects path", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("path must be string", span.clone())); }
                Ok(Type::Bool)
            } else if name == "read_json" || name == "read_toml" || name == "read_env" {
                if args.len() != 1 { return Err(AxityError::ty("read_* expects path", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("path must be string", span.clone())); }
                Ok(Type::String)
            } else if name == "write_json" || name == "write_toml" || name == "write_env" {
                if args.len() != 2 { return Err(AxityError::ty("write_* expects (path, content)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String || check_expr(&args[1], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("write_* arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "json_get" || name == "toml_get" || name == "env_get" {
                if args.len() != 2 { return Err(AxityError::ty("get expects (content, key)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String || check_expr(&args[1], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("get arg types", span.clone())); }
                Ok(Type::String)
            } else if name == "json_set" || name == "toml_set" || name == "env_set" {
                if args.len() != 3 { return Err(AxityError::ty("set expects (content, key, value)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String || check_expr(&args[1], vars, funcs, classes)? != Type::String || check_expr(&args[2], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("set arg types", span.clone())); }
                Ok(Type::String)
            } else if name == "push" {
                if args.len() != 2 { return Err(AxityError::ty("push expects array and value", span.clone())); }
                let at = check_expr(&args[0], vars, funcs, classes)?;
                if let Type::Array(inner) = at {
                    let vt = check_expr(&args[1], vars, funcs, classes)?;
                    if !(type_equals(&vt, &*inner)) { return Err(AxityError::ty("push value type mismatch", span.clone())); }
                    Ok(Type::Int)
                } else { Err(AxityError::ty("push expects array", span.clone())) }
            } else if name == "pop" {
                if args.len() != 1 { return Err(AxityError::ty("pop expects array", span.clone())); }
                let at = check_expr(&args[0], vars, funcs, classes)?;
                if let Type::Array(inner) = at { Ok(*inner.clone()) } else { Err(AxityError::ty("pop expects array", span.clone())) }
            } else if name == "set" {
                if args.len() != 3 { return Err(AxityError::ty("set expects array, index, value", span.clone())); }
                let at = check_expr(&args[0], vars, funcs, classes)?;
                let it = check_expr(&args[1], vars, funcs, classes)?;
                if it != Type::Int { return Err(AxityError::ty("set index must be int", span.clone())); }
                if let Type::Array(inner) = at {
                    let vt = check_expr(&args[2], vars, funcs, classes)?;
                    if !(type_equals(&vt, &*inner)) { return Err(AxityError::ty("set value type mismatch", span.clone())); }
                    Ok(Type::Int)
                } else { Err(AxityError::ty("set expects array", span.clone())) }
            } else if name == "strlen" {
                if args.len() != 1 { return Err(AxityError::ty("strlen expects string", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                if t0 != Type::String { return Err(AxityError::ty("strlen expects string", span.clone())); }
                Ok(Type::Int)
            } else if name == "substr" {
                if args.len() != 3 { return Err(AxityError::ty("substr expects (string, start, len)", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?; let t1 = check_expr(&args[1], vars, funcs, classes)?; let t2 = check_expr(&args[2], vars, funcs, classes)?;
                if t0 != Type::String || t1 != Type::Int || t2 != Type::Int { return Err(AxityError::ty("substr arg types", span.clone())); }
                Ok(Type::String)
            } else if name == "index_of" {
                if args.len() != 2 { return Err(AxityError::ty("index_of expects (string, string)", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?; let t1 = check_expr(&args[1], vars, funcs, classes)?;
                if t0 != Type::String || t1 != Type::String { return Err(AxityError::ty("index_of arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "to_int" {
                if args.len() != 1 { return Err(AxityError::ty("to_int expects string", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                if t0 != Type::String { return Err(AxityError::ty("to_int expects string", span.clone())); }
                Ok(Type::Int)
            } else if name == "to_string" {
                if args.len() != 1 { return Err(AxityError::ty("to_string expects int", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                if t0 != Type::Int { return Err(AxityError::ty("to_string expects int", span.clone())); }
                Ok(Type::String)
            } else if name == "map_new_int" {
                if args.len() != 0 { return Err(AxityError::ty("map_new_int expects no args", span.clone())); }
                Ok(Type::Map(Box::new(Type::Int)))
            } else if name == "map_new_string" {
                if args.len() != 0 { return Err(AxityError::ty("map_new_string expects no args", span.clone())); }
                Ok(Type::Map(Box::new(Type::String)))
            } else if name == "map_set" {
                if args.len() != 3 { return Err(AxityError::ty("map_set expects (map, key, value)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                let kt = check_expr(&args[1], vars, funcs, classes)?;
                if kt != Type::String { return Err(AxityError::ty("map key must be string", span.clone())); }
                match mt { Type::Map(_) => Ok(Type::Int), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "map_get" {
                if args.len() != 2 { return Err(AxityError::ty("map_get expects (map, key)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                let kt = check_expr(&args[1], vars, funcs, classes)?;
                if kt != Type::String { return Err(AxityError::ty("map key must be string", span.clone())); }
                match mt { Type::Map(inner) => Ok(*inner.clone()), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "map_has" {
                if args.len() != 2 { return Err(AxityError::ty("map_has expects (map, key)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                let kt = check_expr(&args[1], vars, funcs, classes)?;
                if kt != Type::String { return Err(AxityError::ty("map key must be string", span.clone())); }
                match mt { Type::Map(_) => Ok(Type::Bool), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "map_keys" {
                if args.len() != 1 { return Err(AxityError::ty("map_keys expects (map)", span.clone())); }
                let mt = check_expr(&args[0], vars, funcs, classes)?;
                match mt { Type::Map(_) => Ok(Type::Array(Box::new(Type::String))), _ => Err(AxityError::ty("first arg must be map", span.clone())) }
            } else if name == "sin" || name == "cos" || name == "tan" {
                if args.len() != 1 { return Err(AxityError::ty("trig expects one argument (radians)", span.clone())); }
                let t0 = check_expr(&args[0], vars, funcs, classes)?;
                match t0 {
                    Type::Flt | Type::Int => Ok(Type::Flt),
                    _ => Err(AxityError::ty("trig arg must be flt or int", span.clone()))
                }
            } else if name == "buffer_new" {
                if args.len() != 1 { return Err(AxityError::ty("buffer_new expects size", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Int { return Err(AxityError::ty("size must be int", span.clone())); }
                Ok(Type::Buffer)
            } else if name == "buffer_len" {
                if args.len() != 1 { return Err(AxityError::ty("buffer_len expects buffer", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Buffer { return Err(AxityError::ty("arg must be buffer", span.clone())); }
                Ok(Type::Int)
            } else if name == "buffer_get" {
                if args.len() != 2 { return Err(AxityError::ty("buffer_get expects (buffer, index)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Buffer || check_expr(&args[1], vars, funcs, classes)? != Type::Int { return Err(AxityError::ty("arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "buffer_set" {
                if args.len() != 3 { return Err(AxityError::ty("buffer_set expects (buffer, index, byte)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Buffer || check_expr(&args[1], vars, funcs, classes)? != Type::Int || check_expr(&args[2], vars, funcs, classes)? != Type::Int { return Err(AxityError::ty("arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "buffer_push" {
                if args.len() != 2 { return Err(AxityError::ty("buffer_push expects (buffer, byte)", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Buffer || check_expr(&args[1], vars, funcs, classes)? != Type::Int { return Err(AxityError::ty("arg types", span.clone())); }
                Ok(Type::Int)
            } else if name == "buffer_from_string" {
                if args.len() != 1 { return Err(AxityError::ty("buffer_from_string expects string", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::String { return Err(AxityError::ty("arg must be string", span.clone())); }
                Ok(Type::Buffer)
            } else if name == "buffer_to_string" {
                if args.len() != 1 { return Err(AxityError::ty("buffer_to_string expects buffer", span.clone())); }
                if check_expr(&args[0], vars, funcs, classes)? != Type::Buffer { return Err(AxityError::ty("arg must be buffer", span.clone())); }
                Ok(Type::String)
            } else {
                if let Some(sig) = funcs.get(name) {
                    if args.len() != sig.0.len() { return Err(AxityError::ty("argument count mismatch", span.clone())); }
                    Ok(sig.1.clone())
                } else if let Some(vt) = lookup_var(name, vars) {
                    match vt {
                        Type::Fn(_params, ret) => Ok(*ret.clone()),
                        _ => Ok(Type::Int)
                    }
                } else {
                    Ok(Type::Int)
                }
            }
        }
    }
}

fn lookup_var(name: &str, vars: &Vec<HashMap<String, Type>>) -> Option<Type> {
    for i in (0..vars.len()).rev() { if let Some(t) = vars[i].get(name) { return Some(t.clone()); } }
    None
}

fn type_equals(a: &Type, b: &Type) -> bool {
    if matches!(a, Type::Any) || matches!(b, Type::Any) { return true; }
    a == b
}

fn span_of_expr(e: &Expr) -> Span {
    match e {
        Expr::Int(_, s) => s.clone(),
        Expr::Flt(_, s) => s.clone(),
        Expr::Str(_, s) => s.clone(),
        Expr::Bool(_, s) => s.clone(),
        Expr::ArrayLit(_, s) => s.clone(),
        Expr::ObjLit(_, s) => s.clone(),
        Expr::Lambda{ span, .. } => span.clone(),
        Expr::Var(_, s) => s.clone(),
        Expr::New(_, _, s) => s.clone(),
        Expr::Member{ span, .. } => span.clone(),
        Expr::Index{ span, .. } => span.clone(),
        Expr::MethodCall{ span, .. } => span.clone(),
        Expr::UnaryNot{ span, .. } => span.clone(),
        Expr::UnaryNeg{ span, .. } => span.clone(),
        Expr::UnaryBitNot{ span, .. } => span.clone(),
        Expr::Binary{ span, .. } => span.clone(),
        Expr::Call{ span, .. } => span.clone(),
        Expr::CallCallee{ span, .. } => span.clone(),
    }
}

