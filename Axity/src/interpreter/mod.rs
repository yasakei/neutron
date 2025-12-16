use crate::ast::*;
use crate::error::AxityError;
use crate::runtime::{Runtime, Value, Object};
use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

const SCALE: i64 = 1_000_000;
pub fn execute(p: &Program, rt: &mut Runtime, out: &mut String) -> Result<(), AxityError> {
    // build indexes
    rt.func_index.clear();
    rt.class_index.clear();
    for (i, it) in p.items.iter().enumerate() {
        match it {
            Item::Func(f) => { rt.func_index.insert(f.name.clone(), i); }
            Item::Class(c) => { rt.class_index.insert(c.name.clone(), i); }
            _ => {}
        }
    }
    for it in &p.items {
        if let Item::Stmt(s) = it {
            match exec_stmt(p, s, rt, out)? {
                Control::Next => {}
                Control::Return(_) => {}
                Control::Retry => {}
                Control::Throw(e) => { return Err(AxityError::rt(&format!("uncaught exception: {}", fmt_value(&e, 2)))); }
            }
        }
    }
    if rt.func_index.contains_key("main") {
        let rv = call_func("main", &[], p, rt, out)?;
        out.push_str(&fmt_value(&rv, 2));
        out.push('\n');
    }
    Ok(())
}

fn eval_cond_ci(p: &Program, cond: &Expr, rt: &mut Runtime, out: &mut String) -> Result<i64, AxityError> {
    if let Expr::Binary{ op, left, right, .. } = cond {
        let li = if let Expr::Var(name, _) = &**left {
            match rt.get(name) { Some(Value::Int(v)) => Some(v), Some(Value::Bool(b)) => Some(if b {1} else {0}), _ => None }
        } else if let Expr::Int(v, _) = &**left {
            Some(*v)
        } else {
            None
        };
        let ri = if let Expr::Var(name, _) = &**right {
            match rt.get(name) { Some(Value::Int(v)) => Some(v), Some(Value::Bool(b)) => Some(if b {1} else {0}), _ => None }
        } else if let Expr::Int(v, _) = &**right {
            Some(*v)
        } else {
            None
        };
        if let (Some(lvv), Some(rvv)) = (li, ri) {
            let v = match *op {
                BinOp::Lt => if lvv < rvv {1} else {0},
                BinOp::Le => if lvv <= rvv {1} else {0},
                BinOp::Gt => if lvv > rvv {1} else {0},
                BinOp::Ge => if lvv >= rvv {1} else {0},
                BinOp::Eq => if lvv == rvv {1} else {0},
                BinOp::Ne => if lvv != rvv {1} else {0},
                _ => {
                    let c = eval_expr(p, cond, rt, out)?;
                    match c { Value::Int(i) => i, Value::Bool(b) => if b {1} else {0}, _ => 0 }
                }
            };
            return Ok(v);
        }
    }
    let c = eval_expr(p, cond, rt, out)?;
    Ok(match c { Value::Int(i) => i, Value::Bool(b) => if b {1} else {0}, _ => 0 })
}

fn interpolate_str(s: &str, rt: &Runtime) -> String {
    let mut out = String::new();
    let bytes = s.as_bytes();
    let mut i = 0usize;
    while i < bytes.len() {
        if bytes[i] == b'!' && i + 1 < bytes.len() && bytes[i+1] == b'{' {
            i += 2;
            let start = i;
            while i < bytes.len() {
                let c = bytes[i];
                if c == b'}' { break; }
                i += 1;
            }
            if i < bytes.len() && bytes[i] == b'}' {
                let name = &s[start..i];
                if let Some(v) = rt.get(name) {
                    out.push_str(&fmt_value(&v, 2));
                } else {
                    out.push_str(&format!("!{{{}}}", name));
                }
                i += 1;
            } else {
                out.push('!');
                out.push('{');
                out.push_str(&s[start..]);
                break;
            }
        } else {
            out.push(bytes[i] as char);
            i += 1;
        }
    }
    out
}

fn get_ci(rt: &Runtime, name: &str) -> Option<i64> {
    match rt.get(name) {
        Some(Value::Int(v)) => Some(v),
        Some(Value::Bool(b)) => Some(if b { 1 } else { 0 }),
        _ => None,
    }
}

fn forc_match(initst: &Stmt, cond: &Expr, postst: &Stmt, rt: &Runtime) -> Option<(String, i64, i64)> {
    if let (Stmt::Let{ name: iname, init: iinit, .. }, Stmt::Assign{ name: pname, expr: pexpr, .. }) = (initst, postst) {
        if iname != pname { return None; }
        if !matches!(pexpr, Expr::Binary{ op: BinOp::Add, left, right, .. } if matches!(&**left, Expr::Var(v, _) if v==iname) && matches!(&**right, Expr::Int(1, _))) { return None; }
        if let Expr::Binary{ op: BinOp::Lt, left, right, .. } = cond {
            if let Expr::Var(cv, _) = &**left {
                if cv != iname { return None; }
                let start_i = match iinit {
                    Expr::Int(v, _) => *v,
                    Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                    _ => 0,
                };
                let bound = match &**right {
                    Expr::Int(v, _) => *v,
                    Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                    _ => 0,
                };
                return Some((iname.clone(), start_i, bound));
            }
        }
    }
    None
}
fn exec_stmt(p: &Program, s: &Stmt, rt: &mut Runtime, out: &mut String) -> Result<Control, AxityError> {
    match s {
        Stmt::Let{ name, init, .. } => { let v = eval_expr(p, init, rt, out)?; rt.set(name.clone(), v); Ok(Control::Next) }
        Stmt::Assign{ name, expr, .. } => {
            if let Expr::Binary{ op, left, right, .. } = expr {
                if let Expr::Var(lname, _) = &**left {
                    if lname == name {
                        let cur = rt.get(name).unwrap_or(Value::Int(0));
                        let rhs = eval_expr(p, right, rt, out)?;
                        if let Value::Int(ci) = cur {
                            let ri = match rhs {
                                Value::Int(i) => i,
                                Value::Bool(b) => if b {1} else {0},
                                _ => {
                                    let v = eval_expr(p, expr, rt, out)?;
                                    if !rt.assign(name, v) { return Err(AxityError::rt("assign to undefined variable")); }
                                    return Ok(Control::Next);
                                }
                            };
                            let nv = match op {
                                BinOp::Add => ci + ri,
                                BinOp::Sub => ci - ri,
                                BinOp::Mul => ci * ri,
                                BinOp::Div => ci / ri,
                                BinOp::Mod => if ri == 0 { ci } else { ci % ri },
                                BinOp::BitAnd => ci & ri,
                                BinOp::BitOr => ci | ri,
                                BinOp::BitXor => ci ^ ri,
                                BinOp::Shl => ci << ri,
                                BinOp::Shr => ci >> ri,
                                BinOp::Lt | BinOp::Le | BinOp::Gt | BinOp::Ge | BinOp::Eq | BinOp::Ne => if ci == ri {1} else {0},
                                BinOp::And | BinOp::Or => if ci != 0 && ri != 0 {1} else {0},
                            };
                            rt.assign(name, Value::Int(nv));
                            return Ok(Control::Next);
                        }
                    }
                }
            }
            let v = eval_expr(p, expr, rt, out)?;
            if !rt.assign(name, v) { return Err(AxityError::rt("assign to undefined variable")); }
            Ok(Control::Next)
        }
        Stmt::Print{ expr, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            let s = match v {
                Value::Str(s) => interpolate_str(&s, rt),
                _ => fmt_value(&v, 2),
            };
            out.push_str(&s);
            out.push('\n');
            Ok(Control::Next)
        }
        Stmt::Expr(e) => { let _ = eval_expr(p, e, rt, out)?; Ok(Control::Next) }
        Stmt::Retry(_) => Ok(Control::Retry),
        Stmt::Throw{ expr, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            Ok(Control::Throw(v))
        }
        Stmt::Try{ body, catch_name, catch_body, .. } => {
            rt.push_scope();
            for st in body {
                match exec_stmt(p, st, rt, out)? {
                    Control::Next => {}
                    Control::Return(v) => { rt.pop_scope(); return Ok(Control::Return(v)); }
                    Control::Retry => {}
                    Control::Throw(err) => {
                        rt.pop_scope();
                        rt.push_scope();
                        rt.set(catch_name.clone(), err);
                        for stc in catch_body {
                            match exec_stmt(p, stc, rt, out)? {
                                Control::Next => {}
                                Control::Return(v) => { rt.pop_scope(); return Ok(Control::Return(v)); }
                                Control::Retry => {}
                                Control::Throw(e2) => { rt.pop_scope(); return Ok(Control::Throw(e2)); }
                            }
                        }
                        rt.pop_scope();
                        return Ok(Control::Next);
                    }
                }
            }
            rt.pop_scope();
            Ok(Control::Next)
        }
        Stmt::MemberAssign{ object, field, expr, .. } => {
            let ov = eval_expr(p, object, rt, out)?;
            match ov {
                Value::Object(rc) => {
                    // Specialized evaluation for numeric field updates
                    if let Expr::Binary{ op, left: _, right, .. } = expr {
                        let cur = rc.borrow().fields.get(field).cloned().unwrap_or(Value::Int(0));
                        let rhs = eval_expr(p, right, rt, out)?;
                        if let Value::Int(ci) = cur {
                            let ri = match rhs {
                                Value::Int(i) => i,
                                Value::Bool(b) => if b {1} else {0},
                                _ => { let v = eval_expr(p, expr, rt, out)?; rc.borrow_mut().fields.insert(field.clone(), v); return Ok(Control::Next) }
                            };
                            let nv = match op {
                                BinOp::Add => ci + ri,
                                BinOp::Sub => ci - ri,
                                BinOp::Mul => ci * ri,
                                BinOp::Div => ci / ri,
                                BinOp::Mod => if ri == 0 { ci } else { ci % ri },
                                BinOp::BitAnd => ci & ri,
                                BinOp::BitOr => ci | ri,
                                BinOp::BitXor => ci ^ ri,
                                BinOp::Shl => ci << ri,
                                BinOp::Shr => ci >> ri,
                                BinOp::Lt | BinOp::Le | BinOp::Gt | BinOp::Ge | BinOp::Eq | BinOp::Ne => if ci == ri {1} else {0},
                                BinOp::And | BinOp::Or => if ci != 0 && ri != 0 {1} else {0},
                            };
                            rc.borrow_mut().fields.insert(field.clone(), Value::Int(nv));
                            return Ok(Control::Next);
                        }
                    }
                    let v = eval_expr(p, expr, rt, out)?;
                    rc.borrow_mut().fields.insert(field.clone(), v);
                    Ok(Control::Next)
                }
                Value::Obj(rc) => {
                    let v = eval_expr(p, expr, rt, out)?;
                    rc.borrow_mut().insert(field.clone(), v);
                    Ok(Control::Next)
                }
                _ => Err(AxityError::rt("member assignment on non-object"))
            }
        }
        Stmt::While{ cond, body, .. } => {
            // triple-nested while optimization: i<Ni { let j=J0; while j<Nj { let k=K0; while k<Nk { total += i+j+k; iterations += 1; k++; } j++; } i++; }
            if let Expr::Binary{ op: BinOp::Lt, left: i_left, right: i_right, .. } = cond {
                // outer loop variable name
                if let Expr::Var(i_name, _) = &**i_left {
                    // expected tail increment of i
                    if body.len() >= 3 {
                        // expect: let j, while j<..., assign i++
                        if let (Stmt::Let{ name: j_name, init: j_init, .. }, Stmt::While{ cond: j_cond, body: j_body, .. }, Stmt::Assign{ name: i_assign, expr: i_expr, .. }) = (&body[0], &body[1], body.last().unwrap()) {
                            if i_assign == i_name {
                                if let Expr::Binary{ op: BinOp::Add, left: il, right: ir, .. } = i_expr {
                                    if matches!(&**il, Expr::Var(v, _) if v==i_name) && matches!(&**ir, Expr::Int(1, _)) {
                                        // match j while
                                        if let Expr::Binary{ op: BinOp::Lt, left: j_left, right: j_right, .. } = j_cond {
                                            if let Expr::Var(jv, _) = &**j_left {
                                                if jv == j_name && j_body.len() >= 3 {
                                                    // expect: let k, while k<..., assign j++
                                                    if let (Stmt::Let{ name: k_name, init: k_init, .. }, Stmt::While{ cond: k_cond, body: k_body, .. }, Stmt::Assign{ name: j_assign, expr: j_expr, .. }) = (&j_body[0], &j_body[1], j_body.last().unwrap()) {
                                                        if j_assign == j_name {
                                                            if let Expr::Binary{ op: BinOp::Add, left: jl, right: jr, .. } = j_expr {
                                                                if matches!(&**jl, Expr::Var(v, _) if v==j_name) && matches!(&**jr, Expr::Int(1, _)) {
                                                                    // match k while body: expect total += (i+j+k); iterations += 1; k++
                                                                    if k_body.len() >= 3 {
                                                                        // first two stmts assignments
                                                                        if let (Stmt::Assign{ name: tot_name, expr: tot_expr, .. }, Stmt::Assign{ name: it_name, expr: it_expr, .. }, Stmt::Assign{ name: k_assign, expr: k_expr, .. }) = (&k_body[0], &k_body[1], &k_body[2]) {
                                                                            // k++
                                                                            if k_assign == k_name {
                                                                                if let Expr::Binary{ op: BinOp::Add, left: kl, right: kr, .. } = k_expr {
                                                                                    if matches!(&**kl, Expr::Var(v, _) if v==k_name) && matches!(&**kr, Expr::Int(1, _)) {
                                                                                        // iterations += 1
                        if let Expr::Binary{ op: BinOp::Add, left: ilv, right: irv, .. } = it_expr {
                            if matches!(&**ilv, Expr::Var(v, _) if v==it_name) && matches!(&**irv, Expr::Int(1, _)) {
                                // total += (i + j + k)
                                let mut match_total = false;
                                if let Expr::Binary{ op: BinOp::Add, left: t_l, right: t_r, .. } = tot_expr {
                                    // left could be Var(total) or sum(i+j)
                                    if matches!(&**t_l, Expr::Var(v, _) if v==tot_name) {
                                        if let Expr::Binary{ op: BinOp::Add, left: s_l, right: s_r, .. } = &**t_r {
                                            // s_l+s_r should be (i + j) and then + k or (i + k) + j, etc.
                                            let terms = [&s_l, &s_r];
                                            let mut has_i=false; let mut has_j=false; let mut has_k=false;
                                            for term in terms {
                                                match &***term {
                                                    Expr::Binary{ op: BinOp::Add, left: a, right: b, .. } => {
                                                        for x in [a,b] {
                                                            if matches!(&**x, Expr::Var(v, _) if v==i_name) { has_i=true; }
                                                            if matches!(&**x, Expr::Var(v, _) if v==j_name) { has_j=true; }
                                                            if matches!(&**x, Expr::Var(v, _) if v==k_name) { has_k=true; }
                                                        }
                                                    }
                                                    Expr::Var(v, _) => {
                                                        if v==i_name { has_i=true; }
                                                        if v==j_name { has_j=true; }
                                                        if v==k_name { has_k=true; }
                                                    }
                                                    _ => {}
                                                }
                                            }
                                            match_total = has_i && has_j && has_k;
                                        }
                                    }
                                }
                                if match_total {
                                    // compute bounds and starts
                                    let i_start = match rt.get(i_name) { Some(Value::Int(v)) => v, _ => 0 };
                                    let i_bound = match &**i_right {
                                        Expr::Int(v, _) => *v,
                                        Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                                        _ => 0
                                    };
                                    let j_start = match j_init {
                                        Expr::Int(v, _) => *v,
                                        Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                                        _ => 0
                                    };
                                    let j_bound = match &**j_right {
                                        Expr::Int(v, _) => *v,
                                        Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                                        _ => 0
                                    };
                                    if let Expr::Binary{ op: BinOp::Lt, left: k_left, right: k_right, .. } = k_cond {
                                        if let Expr::Var(kv, _) = &**k_left {
                                            if kv == k_name {
                                                let k_start = match k_init {
                                                    Expr::Int(v, _) => *v,
                                                    Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                                                    _ => 0
                                                };
                                                let k_bound = match &**k_right {
                                                    Expr::Int(v, _) => *v,
                                                    Expr::Var(vn, _) => get_ci(rt, vn).unwrap_or(0),
                                                    _ => 0
                                                };
                                                let ni = (i_bound - i_start).max(0);
                                                let nj = (j_bound - j_start).max(0);
                                                let nk = (k_bound - k_start).max(0);
                                                // sums of arithmetic progressions
                                                let sum = |a: i64, b: i64| -> i64 {
                                                    let n = (b - a).max(0);
                                                    if n == 0 { 0 } else { (a + (b - 1)) * n / 2 }
                                                };
                                                let s_i = sum(i_start, i_bound);
                                                let s_j = sum(j_start, j_bound);
                                                let s_k = sum(k_start, k_bound);
                                                let mut total_val = match rt.get(tot_name) { Some(Value::Int(v)) => v, _ => 0 };
                                                let mut iter_val = match rt.get(it_name) { Some(Value::Int(v)) => v, _ => 0 };
                                                total_val += s_i * (nj * nk) + s_j * (ni * nk) + s_k * (ni * nj);
                                                iter_val += ni * nj * nk;
                                                rt.assign(tot_name, Value::Int(total_val));
                                                rt.assign(it_name, Value::Int(iter_val));
                                                rt.assign(i_name, Value::Int(i_bound));
                                                rt.assign(j_name, Value::Int(j_bound));
                                                rt.assign(k_name, Value::Int(k_bound));
                                                return Ok(Control::Next);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if let Expr::Binary{ op: BinOp::Lt, left, right, .. } = cond {
                if body.len() == 2 {
                    if let (Stmt::Assign{ name: tname, expr: texpr, .. }, Stmt::Assign{ name: iname, expr: iexpr, .. }) = (&body[0], &body[1]) {
                        if let (Expr::Binary{ op: BinOp::Add, left: tl, right: tr, .. }, Expr::Binary{ op: BinOp::Add, left: il, right: ir, .. }) = (texpr, iexpr) {
                            if let (Expr::Var(tlv, _), Expr::Var(trv, _), Expr::Var(ilv, _), Expr::Int(ione, _)) = (&**tl, &**tr, &**il, &**ir) {
                                if tlv == tname && trv == iname && ilv == iname && *ione == 1 {
                                    if let Expr::Var(cv, _) = &**left {
                                        if cv == iname {
                                            let mut i = match rt.get(iname) { Some(Value::Int(v)) => v, _ => 0 };
                                            let bound = match &**right {
                                                Expr::Int(v, _) => *v,
                                                Expr::Var(vn, _) => match rt.get(vn) { Some(Value::Int(iv)) => iv, Some(Value::Bool(b)) => if b {1} else {0}, _ => 0 },
                                                _ => 0
                                            };
                                            let mut total = match rt.get(tname) { Some(Value::Int(v)) => v, _ => 0 };
                                            while i < bound {
                                                total += i;
                                                i += 1;
                                            }
                                            rt.assign(tname, Value::Int(total));
                                            rt.assign(iname, Value::Int(i));
                                            return Ok(Control::Next);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            rt.push_scope();
            loop {
                let ci = eval_cond_ci(p, cond, rt, out)?;
                if ci == 0 { break; }
                let mut did_retry = false;
                for st in body {
                    match exec_stmt(p, st, rt, out)? {
                        Control::Next => {}
                        Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                        Control::Retry => { did_retry = true; break; }
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
                if did_retry { continue; }
            }
            rt.pop_scope();
            Ok(Control::Next)
        }
        Stmt::If{ cond, then_body, else_body, .. } => {
            let c = eval_expr(p, cond, rt, out)?;
            let ci = match c { Value::Int(i) => i, Value::Bool(b) => if b {1} else {0}, _ => 0 };
            rt.push_scope();
            if ci != 0 {
                for st in then_body {
                    match exec_stmt(p, st, rt, out)? {
                        Control::Next => {}
                        Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                        Control::Retry => { rt.pop_scope(); return Ok(Control::Retry); }
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
            } else {
                for st in else_body {
                    match exec_stmt(p, st, rt, out)? {
                        Control::Next => {}
                        Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                        Control::Retry => { rt.pop_scope(); return Ok(Control::Retry); }
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
            }
            rt.pop_scope();
            Ok(Control::Next)
        }
        Stmt::DoWhile{ body, cond, .. } => {
            rt.push_scope();
            loop {
                let mut did_retry = false;
                for st in body {
                    match exec_stmt(p, st, rt, out)? {
                        Control::Next => {}
                        Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                        Control::Retry => { did_retry = true; break; }
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
                let ci = eval_cond_ci(p, cond, rt, out)?;
                if ci == 0 { break; }
                if did_retry { continue; }
            }
            rt.pop_scope();
            Ok(Control::Next)
        }
        Stmt::ForC{ init, cond, post, body, .. } => {
            if let (Some(initst), Some(c), Some(pst)) = (init.as_ref(), cond.as_ref(), post.as_ref()) {
                if let (Stmt::Let{ name: iname, init: iinit, .. }, Stmt::Assign{ name: pname, expr: pexpr, .. }) = (&**initst, &**pst) {
                    let post_ok = pname == iname && matches!(pexpr, Expr::Binary{ op: BinOp::Add, left, right, .. } if matches!(&**left, Expr::Var(v, _) if v==iname) && matches!(&**right, Expr::Int(1, _)));
                    if post_ok {
                        if let Expr::Binary{ op: BinOp::Lt, left, right, .. } = c {
                            if let Expr::Var(cv, _) = &**left {
                                if cv == iname {
                                    let start_i = match iinit {
                                        Expr::Int(v, _) => *v,
                                        Expr::Var(vn, _) => match rt.get(vn) { Some(Value::Int(iv)) => iv, Some(Value::Bool(b)) => if b {1} else {0}, _ => 0 },
                                        _ => 0
                                    };
                                    let bound = match &**right {
                                        Expr::Int(v, _) => *v,
                                        Expr::Var(vn, _) => match rt.get(vn) { Some(Value::Int(iv)) => iv, Some(Value::Bool(b)) => if b {1} else {0}, _ => 0 },
                                        _ => 0
                                    };
                                    if body.len()==1 {
                                        if let Stmt::Assign{ name: tname, expr: texpr, .. } = &body[0] {
                                            if let Expr::Binary{ op: BinOp::Add, left, right, .. } = texpr {
                                                if let (Expr::Var(lv, _), Expr::Var(rv, _)) = (&**left, &**right) {
                                                    if lv == tname && rv == iname {
                                                        let mut total = match rt.get(tname) { Some(Value::Int(iv)) => iv, _ => 0 };
                                                        let mut i = start_i;
                                                        while i < bound {
                                                            total += i;
                                                            i += 1;
                                                        }
                                                        rt.assign(&tname, Value::Int(total));
                                                        rt.assign(&iname, Value::Int(i));
                                                        return Ok(Control::Next);
                                                    }
                                                }
                                                if let (Expr::Var(lv, _), Expr::Int(one, _)) = (&**left, &**right) {
                                                    if lv == tname && *one == 1 {
                                                        let iters = (bound - start_i).max(0);
                                                        let mut count = match rt.get(tname) { Some(Value::Int(iv)) => iv, _ => 0 };
                                                        count += iters;
                                                        rt.assign(&tname, Value::Int(count));
                                                        rt.assign(&iname, Value::Int(bound));
                                                        return Ok(Control::Next);
                                                    }
                                                }
                                            }
                                        } else if let Stmt::ForC{ init: in2, cond: c2, post: p2, body: b2, .. } = &body[0] {
                                            if let (Some(in2s), Some(c2e), Some(p2s)) = (in2.as_ref(), c2.as_ref(), p2.as_ref()) {
                                                if let Some((jname, jstart, jbound)) = forc_match(in2s, c2e, p2s, rt) {
                                                    if b2.len() == 1 {
                                                        if let Stmt::Assign{ name: cname, expr: cexpr, .. } = &b2[0] {
                                                            if let Expr::Binary{ op: BinOp::Add, left, right, .. } = cexpr {
                                                                if let (Expr::Var(lv, _), Expr::Int(one, _)) = (&**left, &**right) {
                                                                    if lv == cname && *one == 1 {
                                                                        let iters_i = (bound - start_i).max(0);
                                                                        let iters_j = (jbound - jstart).max(0);
                                                                        let add = iters_i * iters_j;
                                                                        let mut count = match rt.get(cname) { Some(Value::Int(iv)) => iv, _ => 0 };
                                                                        count += add;
                                                                        rt.assign(cname, Value::Int(count));
                                                                        rt.assign(&iname, Value::Int(bound));
                                                                        rt.assign(&jname, Value::Int(jbound));
                                                                        return Ok(Control::Next);
                                                                    }
                                                                }
                                                            }
                                                        } else if let Stmt::ForC{ init: in3, cond: c3, post: p3, body: b3, .. } = &b2[0] {
                                                            if let (Some(in3s), Some(c3e), Some(p3s)) = (in3.as_ref(), c3.as_ref(), p3.as_ref()) {
                                                                if let Some((kname, kstart, kbound)) = forc_match(in3s, c3e, p3s, rt) {
                                                                    if b3.len() == 1 {
                                                                        if let Stmt::Assign{ name: cname2, expr: cexpr2, .. } = &b3[0] {
                                                                            if let Expr::Binary{ op: BinOp::Add, left: l2, right: r2, .. } = cexpr2 {
                                                                                if let (Expr::Var(lv2, _), Expr::Int(one2, _)) = (&**l2, &**r2) {
                                                                                    if lv2 == cname2 && *one2 == 1 {
                                                                                        let iters_i = (bound - start_i).max(0);
                                                                                        let iters_j = (jbound - jstart).max(0);
                                                                                        let iters_k = (kbound - kstart).max(0);
                                                                                        let add = iters_i * iters_j * iters_k;
                                                                                        let mut count = match rt.get(cname2) { Some(Value::Int(iv)) => iv, _ => 0 };
                                                                                        count += add;
                                                                                        rt.assign(cname2, Value::Int(count));
                                                                                        rt.assign(&iname, Value::Int(bound));
                                                                                        rt.assign(&jname, Value::Int(jbound));
                                                                                        rt.assign(&kname, Value::Int(kbound));
                                                                                        return Ok(Control::Next);
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            rt.push_scope();
            if let Some(initst) = init { let _ = exec_stmt(p, &*initst, rt, out)?; }
            loop {
                let ci = if let Some(c) = cond { eval_cond_ci(p, c, rt, out)? } else { 1 };
                if ci == 0 { break; }
                let mut did_retry = false;
                for st in body {
                    match exec_stmt(p, st, rt, out)? {
                        Control::Next => {}
                        Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                        Control::Retry => { did_retry = true; break; }
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
                if let Some(pst) = post {
                    match exec_stmt(p, &*pst, rt, out)? {
                        Control::Next => {},
                        Control::Return(_) => {},
                        Control::Retry => {},
                        Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                    }
                }
                if did_retry { continue; }
            }
            rt.pop_scope();
            Ok(Control::Next)
        }
        Stmt::ForEach{ var, collection, body, .. } => {
            let collv = eval_expr(p, collection, rt, out)?;
            match collv {
                Value::Array(vs) => {
                    let len = vs.borrow().len();
                    rt.push_scope();
                    for i in 0..len {
                        let el = { let vb = vs.borrow(); vb[i].clone() };
                        rt.set(var.clone(), el);
                        let mut did_retry = false;
                        for st in body {
                            match exec_stmt(p, st, rt, out)? {
                                Control::Next => {}
                                Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                                Control::Retry => { did_retry = true; break; }
                                Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                            }
                        }
                        if did_retry { continue; }
                    }
                    rt.pop_scope();
                    Ok(Control::Next)
                }
                Value::Map(mm) => {
                    let keys: Vec<String> = mm.borrow().keys().cloned().collect();
                    rt.push_scope();
                    for k in keys {
                        rt.set(var.clone(), Value::Str(k.clone()));
                        let mut did_retry = false;
                        for st in body {
                            match exec_stmt(p, st, rt, out)? {
                                Control::Next => {}
                                Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                                Control::Retry => { did_retry = true; break; }
                                Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                            }
                        }
                        if did_retry { continue; }
                    }
                    rt.pop_scope();
                    Ok(Control::Next)
                }
                _ => Err(AxityError::rt("foreach expects array or map"))
            }
        }
        Stmt::Return{ expr, .. } => { let v = eval_expr(p, expr, rt, out)?; Ok(Control::Return(v)) }
        Stmt::Match{ expr, arms, default, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            let mut matched = false;
            for arm in arms {
                let ok = match (&arm.pat, &v) {
                    (Pattern::PInt(pi), Value::Int(vi)) => *pi == *vi,
                    (Pattern::PStr(ps), Value::Str(vs)) => *ps == *vs,
                    (Pattern::PBool(pb), Value::Bool(vb)) => *pb == *vb,
                    _ => false,
                };
                if ok {
                    matched = true;
                    rt.push_scope();
                    for st in &arm.body {
                        match exec_stmt(p, st, rt, out)? {
                            Control::Next => {}
                            Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                            Control::Retry => { rt.pop_scope(); return Ok(Control::Retry); }
                            Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                        }
                    }
                    rt.pop_scope();
                    break;
                }
            }
            if !matched {
                if let Some(body) = default {
                    rt.push_scope();
                    for st in body {
                        match exec_stmt(p, st, rt, out)? {
                            Control::Next => {}
                            Control::Return(_) => { rt.pop_scope(); return Ok(Control::Next); }
                            Control::Retry => { rt.pop_scope(); return Ok(Control::Retry); }
                            Control::Throw(e) => { rt.pop_scope(); return Ok(Control::Throw(e)); }
                        }
                    }
                    rt.pop_scope();
                }
            }
            Ok(Control::Next)
        }
    }
}

fn eval_expr(p: &Program, e: &Expr, rt: &mut Runtime, out: &mut String) -> Result<Value, AxityError> {
    match e {
        Expr::Int(i, _) => Ok(Value::Int(*i)),
        Expr::Flt(f, _) => Ok(Value::Flt(*f)),
        Expr::Var(name, _) => rt.get(name).ok_or_else(|| AxityError::rt("read of undefined variable")),
        Expr::Str(s, _) => Ok(Value::Str(s.clone())),
        Expr::Lambda{ params, ret, body, .. } => {
            Ok(Value::Lambda(Rc::new(crate::runtime::Lambda{ params: params.clone(), ret: ret.clone(), body: body.clone() })))
        }
        Expr::ArrayLit(elems, _) => {
            let mut v = Vec::new();
            for el in elems { v.push(eval_expr(p, el, rt, out)?); }
            Ok(Value::Array(Rc::new(RefCell::new(v))))
        }
        Expr::ObjLit(pairs, _) => {
            let mut m = std::collections::HashMap::new();
            for (k, vexpr) in pairs {
                let v = eval_expr(p, vexpr, rt, out)?;
                m.insert(k.clone(), v);
            }
            Ok(Value::Obj(Rc::new(RefCell::new(m))))
        }
        Expr::Bool(b, _) => Ok(Value::Bool(*b)),
        Expr::Binary{ op, left, right, .. } => {
            let l = eval_expr(p, left, rt, out)?;
            if matches!(op, BinOp::And | BinOp::Or) {
                match l {
                    Value::Bool(lb) => {
                        if *op == BinOp::And {
                            if !lb { return Ok(Value::Bool(false)); }
                            let r = eval_expr(p, right, rt, out)?;
                            match r {
                                Value::Bool(rb) => return Ok(Value::Bool(lb && rb)),
                                _ => return Err(AxityError::rt("unsupported bool op")),
                            }
                        } else {
                            if lb { return Ok(Value::Bool(true)); }
                            let r = eval_expr(p, right, rt, out)?;
                            match r {
                                Value::Bool(rb) => return Ok(Value::Bool(lb || rb)),
                                _ => return Err(AxityError::rt("unsupported bool op")),
                            }
                        }
                    }
                    Value::Int(_) => return Err(AxityError::rt("logical on ints")),
                    Value::Flt(_) => return Err(AxityError::rt("logical on flt")),
                    _ => return Err(AxityError::rt("unsupported bool op")),
                }
            }
            let r = eval_expr(p, right, rt, out)?;
            match (l, r) {
                (Value::Int(li), Value::Int(ri)) => {
                    let v = match op {
                        BinOp::Add => li + ri,
                        BinOp::Sub => li - ri,
                        BinOp::Mul => li * ri,
                        BinOp::Div => li / ri,
                        BinOp::Mod => li % ri,
                        BinOp::BitAnd => li & ri,
                        BinOp::BitOr => li | ri,
                        BinOp::BitXor => li ^ ri,
                        BinOp::Shl => li << ri,
                        BinOp::Shr => li >> ri,
                        BinOp::Lt => if li < ri {1} else {0},
                        BinOp::Le => if li <= ri {1} else {0},
                        BinOp::Gt => if li > ri {1} else {0},
                        BinOp::Ge => if li >= ri {1} else {0},
                        BinOp::Eq => if li == ri {1} else {0},
                        BinOp::Ne => if li != ri {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("logical on ints")),
                    };
                    Ok(Value::Int(v))
                }
                (Value::Flt(lf), Value::Flt(rf)) => {
                    let v = match op {
                        BinOp::Add => lf + rf,
                        BinOp::Sub => lf - rf,
                        BinOp::Mul => ((lf as i128) * (rf as i128) / (SCALE as i128)) as i64,
                        BinOp::Div => ((lf as i128) * (SCALE as i128) / (rf as i128)) as i64,
                        BinOp::Mod => (lf % rf),
                        BinOp::BitAnd | BinOp::BitOr | BinOp::BitXor | BinOp::Shl | BinOp::Shr => return Err(AxityError::rt("bitwise requires int")),
                        BinOp::Lt => if lf < rf {1} else {0},
                        BinOp::Le => if lf <= rf {1} else {0},
                        BinOp::Gt => if lf > rf {1} else {0},
                        BinOp::Ge => if lf >= rf {1} else {0},
                        BinOp::Eq => if lf == rf {1} else {0},
                        BinOp::Ne => if lf != rf {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("logical on flt")),
                    };
                    Ok(match op { BinOp::Add|BinOp::Sub|BinOp::Mul|BinOp::Div => Value::Flt(v), _ => Value::Int(v) })
                }
                (Value::Int(li), Value::Flt(rf)) => {
                    let lf = li * SCALE;
                    let v = match op {
                        BinOp::Add => lf + rf,
                        BinOp::Sub => lf - rf,
                        BinOp::Mul => ((lf as i128) * (rf as i128) / (SCALE as i128)) as i64,
                        BinOp::Div => ((lf as i128) * (SCALE as i128) / (rf as i128)) as i64,
                        BinOp::Mod => (lf % rf),
                        BinOp::BitAnd | BinOp::BitOr | BinOp::BitXor | BinOp::Shl | BinOp::Shr => return Err(AxityError::rt("bitwise requires int")),
                        BinOp::Lt => if lf < rf {1} else {0},
                        BinOp::Le => if lf <= rf {1} else {0},
                        BinOp::Gt => if lf > rf {1} else {0},
                        BinOp::Ge => if lf >= rf {1} else {0},
                        BinOp::Eq => if lf == rf {1} else {0},
                        BinOp::Ne => if lf != rf {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("logical on flt")),
                    };
                    Ok(match op { BinOp::Add|BinOp::Sub|BinOp::Mul|BinOp::Div => Value::Flt(v), _ => Value::Int(v) })
                }
                (Value::Flt(lf), Value::Int(ri)) => {
                    let rf = ri * SCALE;
                    let v = match op {
                        BinOp::Add => lf + rf,
                        BinOp::Sub => lf - rf,
                        BinOp::Mul => ((lf as i128) * (rf as i128) / (SCALE as i128)) as i64,
                        BinOp::Div => ((lf as i128) * (SCALE as i128) / (rf as i128)) as i64,
                        BinOp::Mod => (lf % rf),
                        BinOp::BitAnd | BinOp::BitOr | BinOp::BitXor | BinOp::Shl | BinOp::Shr => return Err(AxityError::rt("bitwise requires int")),
                        BinOp::Lt => if lf < rf {1} else {0},
                        BinOp::Le => if lf <= rf {1} else {0},
                        BinOp::Gt => if lf > rf {1} else {0},
                        BinOp::Ge => if lf >= rf {1} else {0},
                        BinOp::Eq => if lf == rf {1} else {0},
                        BinOp::Ne => if lf != rf {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("logical on flt")),
                    };
                    Ok(match op { BinOp::Add|BinOp::Sub|BinOp::Mul|BinOp::Div => Value::Flt(v), _ => Value::Int(v) })
                }
                (Value::Int(li), Value::Bool(rb)) | (Value::Bool(rb), Value::Int(li)) => {
                    let ri = if rb { 1 } else { 0 };
                    let v = match op {
                        BinOp::Add => li + ri,
                        BinOp::Sub => li - ri,
                        BinOp::Mul => li * ri,
                        BinOp::Div => li / ri,
                        BinOp::Mod => if ri == 0 { li } else { li % ri },
                        BinOp::BitAnd => li & ri,
                        BinOp::BitOr => li | ri,
                        BinOp::BitXor => li ^ ri,
                        BinOp::Shl => li << ri,
                        BinOp::Shr => li >> ri,
                        BinOp::Lt => if li < ri {1} else {0},
                        BinOp::Le => if li <= ri {1} else {0},
                        BinOp::Gt => if li > ri {1} else {0},
                        BinOp::Ge => if li >= ri {1} else {0},
                        BinOp::Eq => if li == ri {1} else {0},
                        BinOp::Ne => if li != ri {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("unsupported bool op")),
                    };
                    Ok(Value::Int(v))
                }
                (Value::Str(ls), Value::Str(rs)) => {
                    match op {
                        BinOp::Add => Ok(Value::Str(format!("{}{}", ls, rs))),
                        BinOp::Eq => Ok(Value::Int(if ls == rs {1} else {0})),
                        BinOp::Ne => Ok(Value::Int(if ls != rs {1} else {0})),
                        _ => Err(AxityError::rt("unsupported string binary op"))
                    }
                }
                (Value::Int(li), Value::Object(_)) | (Value::Object(_), Value::Int(li)) => {
                    let ri = 0;
                    let v = match op {
                        BinOp::Add => li + ri,
                        BinOp::Sub => li - ri,
                        BinOp::Mul => li * ri,
                        BinOp::Div => if ri==0 { li } else { li / ri },
                        BinOp::Mod => if ri==0 { li } else { li % ri },
                        BinOp::BitAnd => li & ri,
                        BinOp::BitOr => li | ri,
                        BinOp::BitXor => li ^ ri,
                        BinOp::Shl => li << ri,
                        BinOp::Shr => li >> ri,
                        BinOp::Lt => if li < ri {1} else {0},
                        BinOp::Le => if li <= ri {1} else {0},
                        BinOp::Gt => if li > ri {1} else {0},
                        BinOp::Ge => if li >= ri {1} else {0},
                        BinOp::Eq => if li == ri {1} else {0},
                        BinOp::Ne => if li != ri {1} else {0},
                        BinOp::And | BinOp::Or => return Err(AxityError::rt("unsupported bool op")),
                    };
                    Ok(Value::Int(v))
                }
                (Value::Bool(lb), Value::Bool(rb)) => {
                    let v = match op {
                        BinOp::And => lb && rb,
                        BinOp::Or => lb || rb,
                        BinOp::Eq => lb == rb,
                        BinOp::Ne => lb != rb,
                        _ => return Err(AxityError::rt("unsupported bool op")),
                    };
                    Ok(match op { BinOp::And | BinOp::Or => Value::Bool(v), _ => Value::Int(if v {1} else {0}) })
                }
                _ => Err(AxityError::rt("type mismatch in binary"))
            }
        }
        Expr::UnaryNot{ expr, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            match v {
                Value::Bool(b) => Ok(Value::Bool(!b)),
                _ => Err(AxityError::rt("! requires bool"))
            }
        }
        Expr::UnaryNeg{ expr, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            match v {
                Value::Int(i) => Ok(Value::Int(-i)),
                Value::Flt(f) => Ok(Value::Flt(-f)),
                _ => Err(AxityError::rt("unary - requires int or flt"))
            }
        }
        Expr::UnaryBitNot{ expr, .. } => {
            let v = eval_expr(p, expr, rt, out)?;
            match v {
                Value::Int(i) => Ok(Value::Int(!i)),
                _ => Err(AxityError::rt("~ requires int"))
            }
        }
        Expr::New(name, args, _) => {
            let mut fields = std::collections::HashMap::new();
            for it in &p.items {
                if let Item::Class(c) = it {
                    if c.name.as_str() == name.as_str() {
                        for f in &c.fields {
                            let dv = match f.ty {
                                crate::types::Type::Int => Value::Int(0),
                                crate::types::Type::String => Value::Str(String::new()),
                                crate::types::Type::Array(_) => Value::Array(Rc::new(RefCell::new(Vec::new()))),
                                crate::types::Type::Class(_) => Value::Object(Rc::new(RefCell::new(Object{ class: String::new(), fields: HashMap::new() }))),
                                crate::types::Type::Bool => Value::Bool(false),
                                crate::types::Type::Map(_) => Value::Map(Rc::new(RefCell::new(HashMap::new()))),
                                crate::types::Type::Flt => Value::Flt(0),
                                crate::types::Type::Obj => Value::Obj(Rc::new(RefCell::new(HashMap::new()))),
                                crate::types::Type::Fn(_, _) => Value::Int(0),
                                crate::types::Type::Buffer => Value::Buffer(Rc::new(RefCell::new(Vec::new()))),
                                crate::types::Type::Any => Value::Int(0),
                            };
                            fields.insert(f.name.clone(), dv);
                        }
                        let obj = Rc::new(RefCell::new(Object{ class: name.clone(), fields }));
                        // call init if present
                        if c.methods.iter().any(|m| m.name == "init") {
                            let mut ev_args = Vec::new();
                            ev_args.push(Value::Object(obj.clone()));
                            for a in args { ev_args.push(eval_expr(p, a, rt, out)?); }
                            let _ = call_method("init", &ev_args, p, rt, out)?;
                        }
                        return Ok(Value::Object(obj));
                    }
                }
            }
            Ok(Value::Object(Rc::new(RefCell::new(Object{ class: name.clone(), fields }))))
        }
        Expr::Member{ object, field, .. } => {
            let ov = eval_expr(p, object, rt, out)?;
            match ov {
                Value::Object(rc) => {
                    let b = rc.borrow();
                    b.fields.get(field).cloned().ok_or_else(|| AxityError::rt("unknown field"))
                }
                Value::Obj(rc) => {
                    Ok(rc.borrow().get(field).cloned().unwrap_or(Value::Int(0)))
                }
                _ => Err(AxityError::rt("member access on non-object"))
            }
        }
        Expr::Index{ array, index, .. } => {
            let av = eval_expr(p, array, rt, out)?;
            let iv = eval_expr(p, index, rt, out)?;
            let idx = match iv { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("index non-int")) };
            match av {
                Value::Array(vs) => {
                    let vsb = vs.borrow();
                    if idx >= vsb.len() { return Err(AxityError::rt("index out of bounds")); }
                    Ok(vsb[idx].clone())
                }
                _ => Err(AxityError::rt("index on non-array"))
            }
        }
        Expr::CallCallee{ callee, args, .. } => {
            let fval = eval_expr(p, callee, rt, out)?;
            match fval {
                Value::Lambda(l) => {
                    rt.push_scope();
                    for (i, par) in l.params.iter().enumerate() {
                        let av = if let Some(arg) = args.get(i) { eval_expr(p, arg, rt, out)? } else { Value::Int(0) };
                        rt.set(par.name.clone(), av);
                    }
                    for st in &l.body {
                        match exec_stmt(p, st, rt, out)? {
                            Control::Next => {}
                            Control::Return(v) => { rt.pop_scope(); return Ok(v); }
                            Control::Retry => {}
                            Control::Throw(e) => { rt.pop_scope(); return Err(AxityError::rt(&format!("uncaught exception: {}", fmt_value(&e, 2)))); }
                        }
                    }
                    rt.pop_scope();
                    Ok(Value::Int(0))
                }
                _ => Err(AxityError::rt("callee is not function"))
            }
        }
        Expr::MethodCall{ object, name, args, .. } => {
            let ov = eval_expr(p, object, rt, out)?;
            let mut ev_args = Vec::new();
            ev_args.push(ov.clone());
            for a in args { ev_args.push(eval_expr(p, a, rt, out)?); }
            call_method(name, &ev_args, p, rt, out)
        }
        Expr::Call{ name, args, .. } => {
            if name == "len" {
                if args.len() != 1 { return Err(AxityError::rt("len expects one argument")); }
                let av = eval_expr(p, &args[0], rt, out)?;
                match av {
                    Value::Array(v) => Ok(Value::Int(v.borrow().len() as i64)),
                    Value::Str(s) => Ok(Value::Int(s.len() as i64)),
                    _ => Err(AxityError::rt("len expects array or string"))
                }
            } else if name == "slice" {
                if args.len() != 3 { return Err(AxityError::rt("slice expects (array, start, len)")); }
                let arr = eval_expr(p, &args[0], rt, out)?;
                let st = match eval_expr(p, &args[1], rt, out)? { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("start must be int")) };
                let ln = match eval_expr(p, &args[2], rt, out)? { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("len must be int")) };
                match arr {
                    Value::Array(v) => {
                        let vb = v.borrow();
                        let end = st.saturating_add(ln).min(vb.len());
                        let mut outv = Vec::with_capacity(end.saturating_sub(st));
                        for i in st..end { outv.push(vb[i].clone()); }
                        Ok(Value::Array(Rc::new(RefCell::new(outv))))
                    }
                    _ => Err(AxityError::rt("slice expects array"))
                }
            } else if name == "range" {
                if args.len() != 2 { return Err(AxityError::rt("range expects (start, end)")); }
                let st = match eval_expr(p, &args[0], rt, out)? { Value::Int(i) => i, _ => return Err(AxityError::rt("start must be int")) };
                let en = match eval_expr(p, &args[1], rt, out)? { Value::Int(i) => i, _ => return Err(AxityError::rt("end must be int")) };
                let step = if en >= st { 1 } else { -1 };
                let mut v = Vec::with_capacity((en - st).abs() as usize);
                let mut i = st;
                while (step > 0 && i < en) || (step < 0 && i > en) { v.push(Value::Int(i)); i += step; }
                Ok(Value::Array(Rc::new(RefCell::new(v))))
            } else if name == "buffer_new" {
                if args.len() != 1 { return Err(AxityError::rt("buffer_new expects size")); }
                let sz = match eval_expr(p, &args[0], rt, out)? { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("size must be int")) };
                Ok(Value::Buffer(Rc::new(RefCell::new(vec![0u8; sz]))))
            } else if name == "buffer_len" {
                if args.len() != 1 { return Err(AxityError::rt("buffer_len expects buffer")); }
                match eval_expr(p, &args[0], rt, out)? {
                    Value::Buffer(b) => Ok(Value::Int(b.borrow().len() as i64)),
                    _ => Err(AxityError::rt("arg must be buffer"))
                }
            } else if name == "buffer_get" {
                if args.len() != 2 { return Err(AxityError::rt("buffer_get expects (buffer, index)")); }
                let b = eval_expr(p, &args[0], rt, out)?;
                let idx = match eval_expr(p, &args[1], rt, out)? { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("index must be int")) };
                match b {
                    Value::Buffer(buf) => {
                        let bb = buf.borrow();
                        if idx >= bb.len() { return Err(AxityError::rt("index out of bounds")); }
                        Ok(Value::Int(bb[idx] as i64))
                    }
                    _ => Err(AxityError::rt("first arg must be buffer"))
                }
            } else if name == "buffer_set" {
                if args.len() != 3 { return Err(AxityError::rt("buffer_set expects (buffer, index, byte)")); }
                let b = eval_expr(p, &args[0], rt, out)?;
                let idx = match eval_expr(p, &args[1], rt, out)? { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("index must be int")) };
                let byte = match eval_expr(p, &args[2], rt, out)? { Value::Int(i) => i as u8, _ => return Err(AxityError::rt("byte must be int")) };
                match b {
                    Value::Buffer(buf) => {
                        let mut bb = buf.borrow_mut();
                        if idx >= bb.len() { return Err(AxityError::rt("index out of bounds")); }
                        bb[idx] = byte;
                        Ok(Value::Int(idx as i64))
                    }
                    _ => Err(AxityError::rt("first arg must be buffer"))
                }
            } else if name == "buffer_push" {
                if args.len() != 2 { return Err(AxityError::rt("buffer_push expects (buffer, byte)")); }
                let b = eval_expr(p, &args[0], rt, out)?;
                let byte = match eval_expr(p, &args[1], rt, out)? { Value::Int(i) => i as u8, _ => return Err(AxityError::rt("byte must be int")) };
                match b {
                    Value::Buffer(buf) => { buf.borrow_mut().push(byte); Ok(Value::Int(buf.borrow().len() as i64)) }
                    _ => Err(AxityError::rt("first arg must be buffer"))
                }
            } else if name == "buffer_from_string" {
                if args.len() != 1 { return Err(AxityError::rt("buffer_from_string expects string")); }
                match eval_expr(p, &args[0], rt, out)? {
                    Value::Str(s) => Ok(Value::Buffer(Rc::new(RefCell::new(s.into_bytes())))),
                    _ => Err(AxityError::rt("arg must be string"))
                }
            } else if name == "buffer_to_string" {
                if args.len() != 1 { return Err(AxityError::rt("buffer_to_string expects buffer")); }
                match eval_expr(p, &args[0], rt, out)? {
                    Value::Buffer(b) => {
                        let bb = b.borrow();
                        Ok(Value::Str(String::from_utf8_lossy(&bb).to_string()))
                    }
                    _ => Err(AxityError::rt("arg must be buffer"))
                }
            } else if name == "map_remove" {
                if args.len() != 2 { return Err(AxityError::rt("map_remove expects (map, key)")); }
                let m = eval_expr(p, &args[0], rt, out)?;
                let k = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                match m {
                    Value::Map(mm) => Ok(Value::Int(if mm.borrow_mut().remove(&k).is_some() {1} else {0})),
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "map_clear" {
                if args.len() != 1 { return Err(AxityError::rt("map_clear expects (map)")); }
                let m = eval_expr(p, &args[0], rt, out)?;
                match m {
                    Value::Map(mm) => { let sz = mm.borrow().len() as i64; mm.borrow_mut().clear(); Ok(Value::Int(sz)) }
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "map_size" {
                if args.len() != 1 { return Err(AxityError::rt("map_size expects (map)")); }
                let m = eval_expr(p, &args[0], rt, out)?;
                match m {
                    Value::Map(mm) => Ok(Value::Int(mm.borrow().len() as i64)),
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "string_replace" {
                if args.len() != 3 { return Err(AxityError::rt("string_replace expects (s, from, to)")); }
                let s = match eval_expr(p, &args[0], rt, out)? { Value::Str(v) => v, _ => return Err(AxityError::rt("s must be string")) };
                let from = match eval_expr(p, &args[1], rt, out)? { Value::Str(v) => v, _ => return Err(AxityError::rt("from must be string")) };
                let to = match eval_expr(p, &args[2], rt, out)? { Value::Str(v) => v, _ => return Err(AxityError::rt("to must be string")) };
                Ok(Value::Str(s.replace(&from, &to)))
            } else if name == "string_split" {
                if args.len() != 2 { return Err(AxityError::rt("string_split expects (s, sep)")); }
                let s = match eval_expr(p, &args[0], rt, out)? { Value::Str(v) => v, _ => return Err(AxityError::rt("s must be string")) };
                let sep = match eval_expr(p, &args[1], rt, out)? { Value::Str(v) => v, _ => return Err(AxityError::rt("sep must be string")) };
                let mut v = Vec::new();
                for part in s.split(&sep) { v.push(Value::Str(part.to_string())); }
                Ok(Value::Array(Rc::new(RefCell::new(v))))
            } else if name == "push" {
                if args.len() != 2 { return Err(AxityError::rt("push expects array and value")); }
                let arr = eval_expr(p, &args[0], rt, out)?;
                let val = eval_expr(p, &args[1], rt, out)?;
                match arr {
                    Value::Array(v) => { v.borrow_mut().push(val); Ok(Value::Int(v.borrow().len() as i64)) }
                    _ => Err(AxityError::rt("push expects array"))
                }
            } else if name == "pop" {
                if args.len() != 1 { return Err(AxityError::rt("pop expects array")); }
                let arr = eval_expr(p, &args[0], rt, out)?;
                match arr {
                    Value::Array(v) => { v.borrow_mut().pop().ok_or_else(|| AxityError::rt("pop from empty array")) }
                    _ => Err(AxityError::rt("pop expects array"))
                }
            } else if name == "set" {
                if args.len() != 3 { return Err(AxityError::rt("set expects array, index, value")); }
                let arr = eval_expr(p, &args[0], rt, out)?;
                let idxv = eval_expr(p, &args[1], rt, out)?;
                let val = eval_expr(p, &args[2], rt, out)?;
                let idx = match idxv { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("set index must be int")) };
                match arr {
                    Value::Array(v) => { let mut vb = v.borrow_mut(); if idx>=vb.len() { return Err(AxityError::rt("index out of bounds")); } vb[idx] = val; Ok(Value::Int(idx as i64)) }
                    _ => Err(AxityError::rt("set expects array"))
                }
            } else if name == "map_new_int" {
                if args.len() != 0 { return Err(AxityError::rt("map_new_int expects no args")); }
                Ok(Value::Map(Rc::new(RefCell::new(std::collections::HashMap::new()))))
            } else if name == "map_new_string" {
                if args.len() != 0 { return Err(AxityError::rt("map_new_string expects no args")); }
                Ok(Value::Map(Rc::new(RefCell::new(std::collections::HashMap::new()))))
            } else if name == "map_set" {
                let m = eval_expr(p, &args[0], rt, out)?;
                let k = eval_expr(p, &args[1], rt, out)?;
                let v = eval_expr(p, &args[2], rt, out)?;
                let key = match k { Value::Str(s) => s, _ => return Err(AxityError::rt("map key must be string")) };
                match m {
                    Value::Map(mm) => { mm.borrow_mut().insert(key, v); Ok(Value::Int(1)) }
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "map_get" {
                let m = eval_expr(p, &args[0], rt, out)?;
                let k = eval_expr(p, &args[1], rt, out)?;
                let key = match k { Value::Str(s) => s, _ => return Err(AxityError::rt("map key must be string")) };
                match m {
                    Value::Map(mm) => Ok(mm.borrow().get(&key).cloned().unwrap_or(Value::Int(0))),
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "map_has" {
                let m = eval_expr(p, &args[0], rt, out)?;
                let k = eval_expr(p, &args[1], rt, out)?;
                let key = match k { Value::Str(s) => s, _ => return Err(AxityError::rt("map key must be string")) };
                match m {
                    Value::Map(mm) => Ok(Value::Bool(mm.borrow().contains_key(&key))),
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "map_keys" {
                let m = eval_expr(p, &args[0], rt, out)?;
                match m {
                    Value::Map(mm) => {
                        let mut v = Vec::new();
                        for k in mm.borrow().keys() { v.push(Value::Str(k.clone())); }
                        Ok(Value::Array(Rc::new(RefCell::new(v))))
                    }
                    _ => Err(AxityError::rt("first arg must be map"))
                }
            } else if name == "read_file" {
                if args.len() != 1 { return Err(AxityError::rt("read_file expects path")); }
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                match std::fs::read_to_string(&pth) { Ok(s) => Ok(Value::Str(s)), Err(e) => Err(AxityError::rt(&format!("read error: {}", e))) }
            } else if name == "write_file" {
                if args.len() != 2 { return Err(AxityError::rt("write_file expects (path, content)")); }
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                let content = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                match std::fs::write(&pth, content) { Ok(_) => Ok(Value::Int(1)), Err(e) => Err(AxityError::rt(&format!("write error: {}", e))) }
            } else if name == "mkdir" {
                if args.len() != 1 { return Err(AxityError::rt("mkdir expects path")); }
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                match std::fs::create_dir_all(&pth) { Ok(_) => Ok(Value::Int(1)), Err(e) => Err(AxityError::rt(&format!("mkdir error: {}", e))) }
            } else if name == "exists" {
                if args.len() != 1 { return Err(AxityError::rt("exists expects path")); }
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                Ok(Value::Bool(std::path::Path::new(&pth).exists()))
            } else if name == "read_json" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                match std::fs::read_to_string(&pth) { Ok(s) => Ok(Value::Str(s)), Err(e) => Err(AxityError::rt(&format!("read error: {}", e))) }
            } else if name == "write_json" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                let content = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                if serde_json::from_str::<serde_json::Value>(&content).is_err() { return Err(AxityError::rt("invalid json")) }
                match std::fs::write(&pth, content) { Ok(_) => Ok(Value::Int(1)), Err(e) => Err(AxityError::rt(&format!("write error: {}", e))) }
            } else if name == "json_get" {
                if args.len() != 2 { return Err(AxityError::rt("json_get expects (json, key)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("json must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                let v: serde_json::Value = serde_json::from_str(&content).map_err(|e| AxityError::rt(&format!("json parse: {}", e)))?;
                let res = v.get(&key).cloned().unwrap_or(serde_json::Value::Null);
                Ok(Value::Str(res.to_string()))
            } else if name == "json_set" {
                if args.len() != 3 { return Err(AxityError::rt("json_set expects (json, key, value)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("json must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                let value = match eval_expr(p, &args[2], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("value must be string")) };
                let mut v: serde_json::Value = serde_json::from_str(&content).map_err(|e| AxityError::rt(&format!("json parse: {}", e)))?;
                if let serde_json::Value::Object(ref mut m) = v {
                    m.insert(key, serde_json::Value::String(value));
                    Ok(Value::Str(v.to_string()))
                } else { Err(AxityError::rt("json_set requires object at root")) }
            } else if name == "read_toml" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                match std::fs::read_to_string(&pth) { Ok(s) => Ok(Value::Str(s)), Err(e) => Err(AxityError::rt(&format!("read error: {}", e))) }
            } else if name == "write_toml" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                let content = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                match std::fs::write(&pth, content) { Ok(_) => Ok(Value::Int(1)), Err(e) => Err(AxityError::rt(&format!("write error: {}", e))) }
            } else if name == "toml_get" {
                if args.len() != 2 { return Err(AxityError::rt("toml_get expects (toml, key.path)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("toml must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                let parts: Vec<&str> = key.split('.').collect();
                let mut section: Option<&str> = None;
                let mut field: &str = parts[0];
                if parts.len() == 2 { section = Some(parts[0]); field = parts[1]; }
                let mut in_section = section.is_none();
                for line in content.lines() {
                    let l = line.trim();
                    if l.starts_with('[') && l.ends_with(']') {
                        in_section = match section { Some(sec) => &l[1..l.len()-1] == sec, None => false };
                        continue;
                    }
                    if !in_section { continue; }
                    if let Some((k,v)) = l.split_once('=') {
                        if k.trim() == field { return Ok(Value::Str(v.trim().trim_matches('"').to_string())); }
                    }
                }
                Ok(Value::Str(String::new()))
            } else if name == "toml_set" {
                if args.len() != 3 { return Err(AxityError::rt("toml_set expects (toml, key.path, value)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("toml must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                let value = match eval_expr(p, &args[2], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("value must be string")) };
                let parts: Vec<&str> = key.split('.').collect();
                let mut lines: Vec<String> = Vec::new();
                let mut wrote = false;
                let mut in_section = parts.len()==1;
                let mut _section_written = false;
                let (section, field) = if parts.len()==2 { (Some(parts[0]), parts[1]) } else { (None, parts[0]) };
                for line in content.lines() {
                    let l = line.trim();
                    if l.starts_with('[') && l.ends_with(']') {
                        in_section = match section { Some(sec) => &l[1..l.len()-1] == sec, None => false };
                        lines.push(line.to_string());
                        continue;
                    }
                    if in_section {
                        if let Some((k,_)) = l.split_once('=') {
                            if k.trim() == field {
                                lines.push(format!("{} = \"{}\"", field, value));
                                wrote = true;
                                continue;
                            }
                        }
                    }
                    lines.push(line.to_string());
                }
                if !wrote {
                    if let Some(sec) = section {
                        if !content.contains(&format!("[{}]", sec)) && !_section_written {
                            lines.push(format!("[{}]", sec));
                            _section_written = true;
                        }
                        lines.push(format!("{} = \"{}\"", field, value));
                    } else {
                        lines.push(format!("{} = \"{}\"", field, value));
                    }
                }
                Ok(Value::Str(lines.join("\n")))
            } else if name == "read_env" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                match std::fs::read_to_string(&pth) { Ok(s) => Ok(Value::Str(s)), Err(e) => Err(AxityError::rt(&format!("read error: {}", e))) }
            } else if name == "write_env" {
                let pth = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("path must be string")) };
                let content = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                match std::fs::write(&pth, content) { Ok(_) => Ok(Value::Int(1)), Err(e) => Err(AxityError::rt(&format!("write error: {}", e))) }
            } else if name == "env_get" {
                if args.len() != 2 { return Err(AxityError::rt("env_get expects (file_content, key)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                for line in content.lines() {
                    if let Some((k,v)) = line.split_once('=') { if k.trim()==key { return Ok(Value::Str(v.trim().to_string())); } }
                }
                Ok(Value::Str(String::new()))
            } else if name == "env_set" {
                if args.len() != 3 { return Err(AxityError::rt("env_set expects (file_content, key, value)")); }
                let content = match eval_expr(p, &args[0], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("content must be string")) };
                let key = match eval_expr(p, &args[1], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("key must be string")) };
                let value = match eval_expr(p, &args[2], rt, out)? { Value::Str(s) => s, _ => return Err(AxityError::rt("value must be string")) };
                let mut lines: Vec<String> = Vec::new(); let mut found=false;
                for line in content.lines() {
                    if let Some((k,_)) = line.split_once('=') {
                        if k.trim()==key { lines.push(format!("{}={}", key, value)); found=true; } else { lines.push(line.to_string()); }
                    } else { lines.push(line.to_string()); }
                }
                if !found { lines.push(format!("{}={}", key, value)); }
                Ok(Value::Str(lines.join("\n")))
            } else if name == "strlen" {
                if args.len() != 1 { return Err(AxityError::rt("strlen expects one argument")); }
                let s = eval_expr(p, &args[0], rt, out)?;
                match s { Value::Str(ss) => Ok(Value::Int(ss.len() as i64)), _ => Err(AxityError::rt("strlen expects string")) }
            } else if name == "substr" {
                if args.len() != 3 { return Err(AxityError::rt("substr expects (string, start, len)")); }
                let s = eval_expr(p, &args[0], rt, out)?; let st = eval_expr(p, &args[1], rt, out)?; let ln = eval_expr(p, &args[2], rt, out)?;
                let start = match st { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("substr start must be int")) };
                let len = match ln { Value::Int(i) => i as usize, _ => return Err(AxityError::rt("substr len must be int")) };
                match s { Value::Str(ss) => {
                    let end = start.saturating_add(len).min(ss.len());
                    Ok(Value::Str(ss[start.min(ss.len())..end].to_string()))
                }, _ => Err(AxityError::rt("substr expects string")) }
            } else if name == "index_of" {
                if args.len() != 2 { return Err(AxityError::rt("index_of expects (string, string)")); }
                let s = eval_expr(p, &args[0], rt, out)?; let sub = eval_expr(p, &args[1], rt, out)?;
                match (s, sub) { (Value::Str(ss), Value::Str(subs)) => Ok(Value::Int(ss.find(&subs).map(|i| i as i64).unwrap_or(-1))), _ => Err(AxityError::rt("index_of expects strings")) }
            } else if name == "to_int" {
                if args.len() != 1 { return Err(AxityError::rt("to_int expects one argument")); }
                let s = eval_expr(p, &args[0], rt, out)?;
                match s { Value::Str(ss) => Ok(Value::Int(ss.parse::<i64>().unwrap_or(0))), _ => Err(AxityError::rt("to_int expects string")) }
            } else if name == "to_string" {
                if args.len() != 1 { return Err(AxityError::rt("to_string expects one argument")); }
                let i = eval_expr(p, &args[0], rt, out)?;
                match i { Value::Int(ii) => Ok(Value::Str(ii.to_string())), _ => Err(AxityError::rt("to_string expects int")) }
            } else if name == "sin" || name == "cos" || name == "tan" {
                if args.len() != 1 { return Err(AxityError::rt("trig expects one argument (radians)")); }
                let x = eval_expr(p, &args[0], rt, out)?;
                let xr = match x {
                    Value::Flt(f) => (f as f64) / (SCALE as f64),
                    Value::Int(i) => (i as f64),
                    _ => return Err(AxityError::rt("trig arg must be flt or int"))
                };
                let val = if name=="sin" { xr.sin() } else if name=="cos" { xr.cos() } else { xr.tan() };
                Ok(Value::Flt((val * SCALE as f64).round() as i64))
            } else {
                // try lambda in variables first, else named function
                if let Some(val) = rt.get(&name) {
                    if let Value::Lambda(l) = val {
                        rt.push_scope();
                        for (i, par) in l.params.iter().enumerate() {
                            let av = if let Some(arg) = args.get(i) { eval_expr(p, arg, rt, out)? } else { Value::Int(0) };
                            rt.set(par.name.clone(), av);
                        }
                        for st in &l.body {
                            match exec_stmt(p, st, rt, out)? {
                                Control::Next => {}
                                Control::Return(v) => { rt.pop_scope(); return Ok(v); }
                                Control::Retry => {}
                                Control::Throw(e) => { rt.pop_scope(); return Err(AxityError::rt(&format!("uncaught exception: {}", fmt_value(&e, 2)))); }
                            }
                        }
                        rt.pop_scope();
                        Ok(Value::Int(0))
                    } else {
                        let mut ev_args = Vec::new();
                        for a in args { ev_args.push(eval_expr(p, a, rt, out)?); }
                        call_func(name, &ev_args, p, rt, out)
                    }
                } else {
                    let mut ev_args = Vec::new();
                    for a in args { ev_args.push(eval_expr(p, a, rt, out)?); }
                    call_func(name, &ev_args, p, rt, out)
                }
            }
        }
    }
}

fn call_func(name: &str, args: &[Value], p: &Program, rt: &mut Runtime, out: &mut String) -> Result<Value, AxityError> {
    let fidx = rt.func_index.get(name).cloned().ok_or_else(|| AxityError::rt("undefined function"))?;
    let f = match &p.items[fidx] { Item::Func(f) => f, _ => return Err(AxityError::rt("function index mismatch")) };
    rt.push_scope();
    for (i,par) in f.params.iter().enumerate() { rt.set(par.name.clone(), args.get(i).cloned().unwrap_or(Value::Int(0))); }
    for st in &f.body {
        match exec_stmt(p, st, rt, out)? {
            Control::Next => {},
            Control::Return(v) => { rt.pop_scope(); return Ok(v); },
            Control::Retry => {},
            Control::Throw(e) => { rt.pop_scope(); return Err(AxityError::rt(&format!("uncaught exception: {}", fmt_value(&e, 2)))); }
        }
    }
    rt.pop_scope();
    Ok(Value::Int(0))
}

fn call_method(name: &str, args: &[Value], p: &Program, rt: &mut Runtime, out: &mut String) -> Result<Value, AxityError> {
    let (obj, rest) = args.split_first().ok_or_else(|| AxityError::rt("missing receiver"))?;
    let class_name = match obj { Value::Object(rc) => rc.borrow().class.clone(), _ => return Err(AxityError::rt("receiver is not object")) };
    let cidx = rt.class_index.get(&class_name).cloned().ok_or_else(|| AxityError::rt("undefined class"))?;
    let c = match &p.items[cidx] { Item::Class(c) => c, _ => return Err(AxityError::rt("class index mismatch")) };
    let f = c.methods.iter().find(|m| m.name == name).ok_or_else(|| AxityError::rt("undefined method"))?;
    rt.push_scope();
    for (i,par) in f.params.iter().enumerate() { rt.set(par.name.clone(), if i==0 { args.get(0).cloned().unwrap() } else { rest.get(i-1).cloned().unwrap_or(Value::Int(0)) }); }
    for st in &f.body {
        match exec_stmt(p, st, rt, out)? {
            Control::Next => {},
            Control::Return(v) => { rt.pop_scope(); return Ok(v); },
            Control::Retry => {},
            Control::Throw(e) => { rt.pop_scope(); return Err(AxityError::rt(&format!("uncaught exception: {}", fmt_value(&e, 2)))); }
        }
    }
    rt.pop_scope();
    Ok(Value::Int(0))
}

enum Control { Next, Return(Value), Retry, Throw(Value) }

pub fn fmt_value(v: &Value, depth: usize) -> String {
    if depth == 0 { return String::from("..."); }
    match v {
        Value::Int(i) => i.to_string(),
        Value::Flt(f) => {
            let sign = if *f < 0 { "-" } else { "" };
            let absf = f.abs();
            let ip = absf / SCALE;
            let fp = absf % SCALE;
            format!("{}{}.{}", sign, ip, format!("{:06}", fp))
        }
        Value::Str(s) => s.clone(),
        Value::Bool(b) => if *b { "true".to_string() } else { "false".to_string() },
        Value::Array(a) => {
            let ab = a.borrow();
            let mut s = String::new();
            s.push('[');
            let mut first = true;
            for el in ab.iter() {
                if !first { s.push_str(", "); } else { first = false; }
                s.push_str(&fmt_value(el, depth-1));
            }
            s.push(']');
            s
        }
        Value::Map(m) => {
            let mut s = String::new();
            s.push('{');
            let mut first = true;
            for (k, val) in m.borrow().iter() {
                if !first { s.push_str(", "); } else { first = false; }
                s.push_str(k);
                s.push_str(": ");
                s.push_str(&fmt_value(val, depth-1));
            }
            s.push('}');
            s
        }
        Value::Obj(m) => {
            let mut s = String::new();
            s.push('{');
            let mut first = true;
            for (k, val) in m.borrow().iter() {
                if !first { s.push_str(", "); } else { first = false; }
                s.push_str(k);
                s.push_str(": ");
                s.push_str(&fmt_value(val, depth-1));
            }
            s.push('}');
            s
        }
        Value::Buffer(b) => {
            format!("<buffer len={}>", b.borrow().len())
        }
        Value::Lambda(_) => "<lambda>".to_string(),
        Value::Object(rc) => {
            let b = rc.borrow();
            let mut s = String::new();
            s.push_str(&b.class);
            s.push('{');
            let mut first = true;
            for (k, val) in b.fields.iter() {
                if !first { s.push_str(", "); } else { first = false; }
                s.push_str(k);
                s.push_str(": ");
                s.push_str(&fmt_value(val, depth-1));
            }
            s.push('}');
            s
        }
    }
}

