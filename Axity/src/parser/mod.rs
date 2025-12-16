use crate::ast::*;
use crate::error::AxityError;
use crate::token::{Token, TokenKind};
use crate::types::Type;

pub fn parse(tokens: &[Token]) -> Result<Program, AxityError> {
    let mut p = Parser { tokens, i: 0 };
    p.program()
}

struct Parser<'a> {
    tokens: &'a [Token],
    i: usize,
}

impl<'a> Parser<'a> {
    fn peek(&self) -> &'a Token { &self.tokens[self.i] }
    fn next(&mut self) -> &'a Token { let t = &self.tokens[self.i]; self.i += 1; t }
    fn expect(&mut self, kind: TokenKind) -> Result<Token, AxityError> {
        let t = self.next().clone();
        if t.kind == kind { Ok(t) } else { Err(AxityError::parse("unexpected token", t.span)) }
    }
    fn program(&mut self) -> Result<Program, AxityError> {
        let mut items = Vec::new();
        while self.peek().kind != TokenKind::Eof {
            if self.peek().kind == TokenKind::Fn { items.push(Item::Func(self.function()?)); }
            else if self.peek().kind == TokenKind::Import {
                let it = self.import_item()?;
                items.push(it);
            }
            else if self.peek().kind == TokenKind::Class { items.push(Item::Class(self.class_def()?)); }
            else { items.push(Item::Stmt(self.statement()?)); }
        }
        Ok(Program { items })
    }
    fn import_item(&mut self) -> Result<Item, AxityError> {
        let tok = self.expect(TokenKind::Import)?;
        let path = match self.next().kind.clone() {
            TokenKind::StringLit(s) => s,
            _ => return Err(AxityError::parse("expected string path", tok.span))
        };
        if self.peek().kind == TokenKind::Semicolon { self.next(); }
        Ok(Item::Import(path, tok.span))
    }
    fn class_def(&mut self) -> Result<ClassDef, AxityError> {
        let ct = self.expect(TokenKind::Class)?;
        let name = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected class name", ct.span)) };
        self.expect(TokenKind::LBrace)?;
        let mut fields = Vec::new();
        let mut methods = Vec::new();
        loop {
            match self.peek().kind.clone() {
                TokenKind::Let => {
                    let sp = self.next().span.clone();
                    let fname = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected field name", sp)) };
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    self.expect(TokenKind::Semicolon)?;
                    fields.push(Field{ name: fname, ty, span: sp });
                }
                TokenKind::Fn => {
                    methods.push(self.function()?);
                }
                TokenKind::RBrace => { break; }
                _ => return Err(AxityError::parse("unexpected token in class", self.peek().span.clone()))
            }
        }
        self.expect(TokenKind::RBrace)?;
        Ok(ClassDef{ name, fields, methods, span: ct.span })
    }
    fn function(&mut self) -> Result<Function, AxityError> {
        let fn_tok = self.expect(TokenKind::Fn)?;
        let name = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected identifier", fn_tok.span)) };
        self.expect(TokenKind::LParen)?;
        let mut params = Vec::new();
        if self.peek().kind != TokenKind::RParen {
            loop {
                let pname = match self.next().kind.clone() {
                    TokenKind::Ident(s) => s,
                    TokenKind::SelfKw => "self".to_string(),
                    _ => return Err(AxityError::parse("expected identifier", self.peek().span.clone()))
                };
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                params.push(Param{ name: pname, ty, span: fn_tok.span.clone() });
                if self.peek().kind == TokenKind::Comma { self.next(); } else { break; }
            }
        }
        self.expect(TokenKind::RParen)?;
        self.expect(TokenKind::Arrow)?;
        let ret = self.parse_type()?;
        self.expect(TokenKind::LBrace)?;
        let mut body = Vec::new();
        while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
        self.expect(TokenKind::RBrace)?;
        Ok(Function { name, params, ret, body, span: fn_tok.span })
    }
    fn parse_type(&mut self) -> Result<Type, AxityError> {
        let t = self.next().clone();
        match t.kind {
            TokenKind::IntType => Ok(Type::Int),
            TokenKind::StringType => Ok(Type::String),
            TokenKind::BoolType => Ok(Type::Bool),
            TokenKind::FltType => Ok(Type::Flt),
            TokenKind::ObjType => Ok(Type::Obj),
            TokenKind::BufferType => Ok(Type::Buffer),
            TokenKind::AnyType => Ok(Type::Any),
            TokenKind::ArrayKw => {
                self.expect(TokenKind::Less)?;
                let inner = self.parse_type()?;
                self.expect(TokenKind::Greater)?;
                Ok(Type::Array(Box::new(inner)))
            }
            TokenKind::MapKw => {
                self.expect(TokenKind::Less)?;
                let inner = self.parse_type()?;
                self.expect(TokenKind::Greater)?;
                Ok(Type::Map(Box::new(inner)))
            }
            TokenKind::Ident(ref s) => Ok(Type::Class(s.clone())),
            _ => Err(AxityError::parse("unknown type", t.span))
        }
    }
    fn statement(&mut self) -> Result<Stmt, AxityError> {
        match self.peek().kind.clone() {
            TokenKind::Let => {
                let lt = self.next().span.clone();
                let name = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected identifier", lt)) };
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Assign)?;
                let init = self.expr()?;
                self.expect(TokenKind::Semicolon)?;
                Ok(Stmt::Let{ name, ty, init, span: lt })
            }
            TokenKind::Throw => {
                let sp = self.next().span.clone();
                let e = self.expr()?;
                if self.peek().kind == TokenKind::Semicolon { self.next(); }
                Ok(Stmt::Throw{ expr: e, span: sp })
            }
            TokenKind::Try => {
                let sp = self.next().span.clone();
                self.expect(TokenKind::LBrace)?;
                let mut body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                self.expect(TokenKind::Catch)?;
                let cname = match self.peek().kind.clone() {
                    TokenKind::Ident(s) => { self.next(); s }
                    _ => "error".to_string()
                };
                self.expect(TokenKind::LBrace)?;
                let mut cbody = Vec::new();
                while self.peek().kind != TokenKind::RBrace { cbody.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                Ok(Stmt::Try{ body, catch_name: cname, catch_body: cbody, span: sp })
            }
            TokenKind::Retry => {
                let sp = self.next().span.clone();
                if self.peek().kind == TokenKind::Semicolon { self.next(); }
                Ok(Stmt::Retry(sp))
            }
            TokenKind::Do => {
                let sp = self.next().span.clone();
                self.expect(TokenKind::LBrace)?;
                let mut body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                self.expect(TokenKind::While)?;
                let cond = self.expr()?;
                if self.peek().kind == TokenKind::Semicolon { self.next(); }
                Ok(Stmt::DoWhile{ body, cond, span: sp })
            }
            TokenKind::For => {
                let sp = self.next().span.clone();
                // foreach: for ident in expr { body }
                if let TokenKind::Ident(var) = self.peek().kind.clone() {
                    let save_i = self.i;
                    let _tok = self.next().clone();
                    if self.peek().kind == TokenKind::In {
                        self.next();
                        let coll = self.expr()?;
                        self.expect(TokenKind::LBrace)?;
                        let mut body = Vec::new();
                        while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                        self.expect(TokenKind::RBrace)?;
                        return Ok(Stmt::ForEach{ var, collection: coll, body, span: sp });
                    } else {
                        self.i = save_i;
                    }
                }
                // C-style: for init; cond; post { body }
                let init: Option<Box<Stmt>> = {
                    if self.peek().kind == TokenKind::Semicolon {
                        self.next();
                        None
                    } else if self.peek().kind == TokenKind::Let {
                        // parse let ... ;
                        let lt = self.next().span.clone();
                        let name = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected identifier", lt)) };
                        self.expect(TokenKind::Colon)?;
                        let ty = self.parse_type()?;
                        self.expect(TokenKind::Assign)?;
                        let init_e = self.expr()?;
                        self.expect(TokenKind::Semicolon)?;
                        Some(Box::new(Stmt::Let{ name, ty, init: init_e, span: lt }))
                    } else {
                        // parse assignment or expr statement up to semicolon
                        let e = self.expr()?;
                        self.expect(TokenKind::Semicolon)?;
                        Some(Box::new(Stmt::Expr(e)))
                    }
                };
                let cond = if self.peek().kind == TokenKind::Semicolon { self.next(); None } else { let c = self.expr()?; self.expect(TokenKind::Semicolon)?; Some(c) };
                let post = if self.peek().kind == TokenKind::LBrace { None } else {
                    let save_i2 = self.i;
                    if let TokenKind::Ident(name) = self.peek().kind.clone() {
                        let _id_tok = self.next().clone();
                        if self.peek().kind == TokenKind::PlusPlus {
                            let sp = self.next().span.clone();
                            let one = Expr::Int(1, sp.clone());
                            let expr = Expr::Binary{ op: BinOp::Add, left: Box::new(Expr::Var(name.clone(), sp.clone())), right: Box::new(one), span: sp.clone() };
                            Some(Box::new(Stmt::Assign{ name, expr, span: sp }))
                        } else if self.peek().kind == TokenKind::MinusMinus {
                            let sp = self.next().span.clone();
                            let one = Expr::Int(1, sp.clone());
                            let expr = Expr::Binary{ op: BinOp::Sub, left: Box::new(Expr::Var(name.clone(), sp.clone())), right: Box::new(one), span: sp.clone() };
                            Some(Box::new(Stmt::Assign{ name, expr, span: sp }))
                        } else {
                            self.i = save_i2;
                            None
                        }
                    } else {
                        None
                    }
                };
                self.expect(TokenKind::LBrace)?;
                let mut body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                Ok(Stmt::ForC{ init, cond, post, body, span: sp })
            }
            TokenKind::Match => {
                let sp = self.next().span.clone();
                let e = self.expr()?;
                self.expect(TokenKind::LBrace)?;
                let mut arms = Vec::new();
                let mut default = None;
                while self.peek().kind != TokenKind::RBrace {
                    if self.peek().kind == TokenKind::Case {
                        self.next();
                        let pat = match self.peek().kind.clone() {
                            TokenKind::IntLit(v) => { let t = self.next().clone(); Pattern::PInt(v) }
                            TokenKind::StringLit(ref s) => { let t = self.next().clone(); Pattern::PStr(s.clone()) }
                            TokenKind::TrueKw => { self.next(); Pattern::PBool(true) }
                            TokenKind::FalseKw => { self.next(); Pattern::PBool(false) }
                            _ => return Err(AxityError::parse("invalid pattern", self.peek().span.clone()))
                        };
                        self.expect(TokenKind::Colon)?;
                        self.expect(TokenKind::LBrace)?;
                        let mut body = Vec::new();
                        while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                        self.expect(TokenKind::RBrace)?;
                        arms.push(MatchArm{ pat, body });
                    } else if self.peek().kind == TokenKind::Default {
                        self.next();
                        self.expect(TokenKind::Colon)?;
                        self.expect(TokenKind::LBrace)?;
                        let mut body = Vec::new();
                        while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                        self.expect(TokenKind::RBrace)?;
                        default = Some(body);
                    } else {
                        return Err(AxityError::parse("expected case/default", self.peek().span.clone()));
                    }
                }
                self.expect(TokenKind::RBrace)?;
                Ok(Stmt::Match{ expr: e, arms, default, span: sp })
            }
            TokenKind::Print => {
                let sp = self.next().span.clone();
                self.expect(TokenKind::LParen)?;
                let e = self.expr()?;
                self.expect(TokenKind::RParen)?;
                self.expect(TokenKind::Semicolon)?;
                Ok(Stmt::Print{ expr: e, span: sp })
            }
            TokenKind::While => {
                let sp = self.next().span.clone();
                let cond = self.expr()?;
                self.expect(TokenKind::LBrace)?;
                let mut body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                Ok(Stmt::While{ cond, body, span: sp })
            }
            TokenKind::If => {
                let sp = self.next().span.clone();
                let cond = self.expr()?;
                self.expect(TokenKind::LBrace)?;
                let mut then_body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { then_body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                let mut else_body = Vec::new();
                if self.peek().kind == TokenKind::Else {
                    self.next();
                    self.expect(TokenKind::LBrace)?;
                    while self.peek().kind != TokenKind::RBrace { else_body.push(self.statement()?); }
                    self.expect(TokenKind::RBrace)?;
                }
                Ok(Stmt::If{ cond, then_body, else_body, span: sp })
            }
            TokenKind::Return => {
                let sp = self.next().span.clone();
                let e = self.expr()?;
                self.expect(TokenKind::Semicolon)?;
                Ok(Stmt::Return{ expr: e, span: sp })
            }
            TokenKind::Ident(_) | TokenKind::SelfKw => {
                let start = self.next().clone();
                let mut base = match start.kind {
                    TokenKind::Ident(ref s) => Expr::Var(s.clone(), start.span.clone()),
                    TokenKind::SelfKw => Expr::Var("self".to_string(), start.span.clone()),
                    _ => unreachable!()
                };
                while self.peek().kind == TokenKind::Dot || self.peek().kind == TokenKind::LBracket {
                    if self.peek().kind == TokenKind::Dot {
                        self.next();
                        let fld = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected member name", self.peek().span.clone())) };
                        if self.peek().kind == TokenKind::LParen {
                            self.expect(TokenKind::LParen)?;
                            let mut args = Vec::new();
                            if self.peek().kind != TokenKind::RParen {
                                loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                            }
                            self.expect(TokenKind::RParen)?;
                            base = Expr::MethodCall{ object: Box::new(base), name: fld, args, span: start.span.clone() };
                        } else {
                            base = Expr::Member{ object: Box::new(base), field: fld, span: start.span.clone() };
                        }
                    } else {
                        self.expect(TokenKind::LBracket)?;
                        let idx = self.expr()?;
                        self.expect(TokenKind::RBracket)?;
                        base = Expr::Index{ array: Box::new(base), index: Box::new(idx), span: start.span.clone() };
                    }
                }
                if self.peek().kind == TokenKind::Assign {
                    let sp = self.next().span.clone();
                    let e = self.expr()?;
                    self.expect(TokenKind::Semicolon)?;
                    match base {
                        Expr::Member{ object, field, .. } => Ok(Stmt::MemberAssign{ object: *object, field, expr: e, span: sp }),
                        Expr::Var(name, _) => Ok(Stmt::Assign{ name, expr: e, span: sp }),
                        _ => Err(AxityError::parse("invalid assignment target", sp))
                    }
                } else if self.peek().kind == TokenKind::PlusPlus || self.peek().kind == TokenKind::MinusMinus {
                    let sp = self.next().span.clone();
                    let one = Expr::Int(1, sp.clone());
                    let expr = if matches!(self.tokens[self.i-1].kind, TokenKind::PlusPlus) {
                        Expr::Binary{ op: BinOp::Add, left: Box::new(base.clone()), right: Box::new(one), span: sp.clone() }
                    } else {
                        Expr::Binary{ op: BinOp::Sub, left: Box::new(base.clone()), right: Box::new(one), span: sp.clone() }
                    };
                    self.expect(TokenKind::Semicolon)?;
                    match base {
                        Expr::Member{ object, field, .. } => Ok(Stmt::MemberAssign{ object: *object, field, expr: expr, span: sp }),
                        Expr::Var(name, _) => Ok(Stmt::Assign{ name, expr: expr, span: sp }),
                        _ => Err(AxityError::parse("invalid increment/decrement target", sp))
                    }
                } else if self.peek().kind == TokenKind::LParen {
                    self.expect(TokenKind::LParen)?;
                    let mut args = Vec::new();
                    if self.peek().kind != TokenKind::RParen {
                        loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                    }
                    self.expect(TokenKind::RParen)?;
                    self.expect(TokenKind::Semicolon)?;
                    let name = match start.kind {
                        TokenKind::Ident(name) => name,
                        TokenKind::SelfKw => "self".to_string(),
                        _ => unreachable!()
                    };
                    Ok(Stmt::Expr(Expr::Call{ name, args, span: self.peek().span.clone() }))
                } else {
                    self.expect(TokenKind::Semicolon)?;
                    Ok(Stmt::Expr(base))
                }
            }
            _ => Err(AxityError::parse("unexpected token in statement", self.peek().span.clone()))
        }
    }
    fn expr(&mut self) -> Result<Expr, AxityError> { self.expr_or() }
    fn expr_or(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_and()?;
        loop {
            if self.peek().kind == TokenKind::OrOr {
                let sp = self.next().span.clone();
                let r = self.expr_and()?;
                e = Expr::Binary{ op: BinOp::Or, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_and(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_bit_or()?;
        loop {
            if self.peek().kind == TokenKind::AndAnd {
                let sp = self.next().span.clone();
                let r = self.expr_bit_or()?;
                e = Expr::Binary{ op: BinOp::And, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_bit_or(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_bit_xor()?;
        loop {
            if self.peek().kind == TokenKind::BitOr {
                let sp = self.next().span.clone();
                let r = self.expr_bit_xor()?;
                e = Expr::Binary{ op: BinOp::BitOr, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_bit_xor(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_bit_and()?;
        loop {
            if self.peek().kind == TokenKind::BitXor {
                let sp = self.next().span.clone();
                let r = self.expr_bit_and()?;
                e = Expr::Binary{ op: BinOp::BitXor, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_bit_and(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_eq()?;
        loop {
            if self.peek().kind == TokenKind::BitAnd {
                let sp = self.next().span.clone();
                let r = self.expr_eq()?;
                e = Expr::Binary{ op: BinOp::BitAnd, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_eq(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_rel()?;
        loop {
            let k = self.peek().kind.clone();
            if k == TokenKind::EqEq || k == TokenKind::NotEq {
                let op = if k == TokenKind::EqEq { BinOp::Eq } else { BinOp::Ne };
                let sp = self.next().span.clone();
                let r = self.expr_rel()?;
                e = Expr::Binary{ op, left: Box::new(e), right: Box::new(r), span: sp };
            } else { break; }
        }
        Ok(e)
    }
    fn expr_rel(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_shift()?;
        loop {
            let k = self.peek().kind.clone();
            let op = match k { TokenKind::Less => Some(BinOp::Lt), TokenKind::LessEq => Some(BinOp::Le), TokenKind::Greater => Some(BinOp::Gt), TokenKind::GreaterEq => Some(BinOp::Ge), _ => None };
            if let Some(op) = op { let sp = self.next().span.clone(); let r = self.expr_shift()?; e = Expr::Binary{ op, left: Box::new(e), right: Box::new(r), span: sp }; } else { break; }
        }
        Ok(e)
    }
    fn expr_shift(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_add()?;
        loop {
            let k = self.peek().kind.clone();
            let op = match k { TokenKind::Shl => Some(BinOp::Shl), TokenKind::Shr => Some(BinOp::Shr), _ => None };
            if let Some(op) = op { let sp = self.next().span.clone(); let r = self.expr_add()?; e = Expr::Binary{ op, left: Box::new(e), right: Box::new(r), span: sp }; } else { break; }
        }
        Ok(e)
    }
    fn expr_add(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_mul()?;
        loop {
            let k = self.peek().kind.clone();
            let op = match k { TokenKind::Plus => Some(BinOp::Add), TokenKind::Minus => Some(BinOp::Sub), _ => None };
            if let Some(op) = op { let sp = self.next().span.clone(); let r = self.expr_mul()?; e = Expr::Binary{ op, left: Box::new(e), right: Box::new(r), span: sp }; } else { break; }
        }
        Ok(e)
    }
    fn expr_mul(&mut self) -> Result<Expr, AxityError> {
        let mut e = self.expr_unary()?;
        loop {
            let k = self.peek().kind.clone();
            let op = match k { TokenKind::Star => Some(BinOp::Mul), TokenKind::Slash => Some(BinOp::Div), TokenKind::Percent => Some(BinOp::Mod), _ => None };
            if let Some(op) = op { let sp = self.next().span.clone(); let r = self.expr_unary()?; e = Expr::Binary{ op, left: Box::new(e), right: Box::new(r), span: sp }; } else { break; }
        }
        Ok(e)
    }
    fn expr_unary(&mut self) -> Result<Expr, AxityError> {
        if self.peek().kind == TokenKind::Bang {
            let sp = self.next().span.clone();
            let e = self.expr_unary()?;
            Ok(Expr::UnaryNot{ expr: Box::new(e), span: sp })
        } else if self.peek().kind == TokenKind::Minus {
            let sp = self.next().span.clone();
            let e = self.expr_unary()?;
            Ok(Expr::UnaryNeg{ expr: Box::new(e), span: sp })
        } else if self.peek().kind == TokenKind::Tilde {
            let sp = self.next().span.clone();
            let e = self.expr_unary()?;
            Ok(Expr::UnaryBitNot{ expr: Box::new(e), span: sp })
        } else {
            self.expr_primary()
        }
    }
    fn expr_primary(&mut self) -> Result<Expr, AxityError> {
        let t = self.next().clone();
        match t.kind {
            TokenKind::Fn => {
                self.expect(TokenKind::LParen)?;
                let mut params = Vec::new();
                if self.peek().kind != TokenKind::RParen {
                    loop {
                        let pname = match self.next().kind.clone() {
                            TokenKind::Ident(s) => s,
                            _ => return Err(AxityError::parse("expected identifier", self.peek().span.clone()))
                        };
                        self.expect(TokenKind::Colon)?;
                        let ty = self.parse_type()?;
                        params.push(Param{ name: pname, ty, span: t.span.clone() });
                        if self.peek().kind == TokenKind::Comma { self.next(); } else { break; }
                    }
                }
                self.expect(TokenKind::RParen)?;
                self.expect(TokenKind::Arrow)?;
                let ret = self.parse_type()?;
                self.expect(TokenKind::LBrace)?;
                let mut body = Vec::new();
                while self.peek().kind != TokenKind::RBrace { body.push(self.statement()?); }
                self.expect(TokenKind::RBrace)?;
                let lam = Expr::Lambda{ params, ret, body, span: t.span.clone() };
                if self.peek().kind == TokenKind::LParen {
                    self.expect(TokenKind::LParen)?;
                    let mut args = Vec::new();
                    if self.peek().kind != TokenKind::RParen {
                        loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                    }
                    self.expect(TokenKind::RParen)?;
                    Ok(Expr::CallCallee{ callee: Box::new(lam), args, span: t.span })
                } else {
                    Ok(lam)
                }
            }
            TokenKind::IntLit(v) => Ok(Expr::Int(v, t.span)),
            TokenKind::FltLit(v) => Ok(Expr::Flt(v, t.span)),
            TokenKind::StringLit(ref s) => Ok(Expr::Str(s.clone(), t.span)),
            TokenKind::LBrace => {
                let mut pairs: Vec<(String, Expr)> = Vec::new();
                if self.peek().kind != TokenKind::RBrace {
                    loop {
                        let key = match self.peek().kind.clone() {
                            TokenKind::StringLit(ref s) => { self.next(); s.clone() }
                            TokenKind::Ident(ref s) => { self.next(); s.clone() }
                            _ => return Err(AxityError::parse("expected key in object", self.peek().span.clone()))
                        };
                        self.expect(TokenKind::Colon)?;
                        let val = self.expr()?;
                        pairs.push((key, val));
                        if self.peek().kind == TokenKind::Comma { self.next(); } else { break; }
                    }
                }
                self.expect(TokenKind::RBrace)?;
                Ok(Expr::ObjLit(pairs, t.span))
            }
            TokenKind::New => {
                let name = match self.next().kind.clone() { TokenKind::Ident(s) => s, _ => return Err(AxityError::parse("expected class name", t.span)) };
                let mut args = Vec::new();
                if self.peek().kind == TokenKind::LParen {
                    self.expect(TokenKind::LParen)?;
                    if self.peek().kind != TokenKind::RParen {
                        loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                    }
                    self.expect(TokenKind::RParen)?;
                }
                Ok(Expr::New(name, args, t.span))
            }
            TokenKind::LBracket => {
                let mut elems = Vec::new();
                if self.peek().kind != TokenKind::RBracket {
                    loop { elems.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                }
                self.expect(TokenKind::RBracket)?;
                Ok(Expr::ArrayLit(elems, t.span))
            }
            TokenKind::Ident(ref s) => {
                let mut base: Expr;
                if self.peek().kind == TokenKind::LParen {
                    self.expect(TokenKind::LParen)?;
                    let mut args = Vec::new();
                    if self.peek().kind != TokenKind::RParen {
                        loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                    }
                    self.expect(TokenKind::RParen)?;
                    base = Expr::Call{ name: s.clone(), args, span: t.span.clone() };
                } else {
                    base = Expr::Var(s.clone(), t.span.clone());
                }
                while self.peek().kind == TokenKind::Dot || self.peek().kind == TokenKind::LBracket {
                    if self.peek().kind == TokenKind::Dot {
                        self.next();
                        let fld = match self.next().kind.clone() { TokenKind::Ident(s2) => s2, _ => return Err(AxityError::parse("expected member name", self.peek().span.clone())) };
                        if self.peek().kind == TokenKind::LParen {
                            self.expect(TokenKind::LParen)?;
                            let mut args = Vec::new();
                            if self.peek().kind != TokenKind::RParen {
                                loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                            }
                            self.expect(TokenKind::RParen)?;
                            base = Expr::MethodCall{ object: Box::new(base), name: fld, args, span: t.span.clone() };
                        } else {
                            base = Expr::Member{ object: Box::new(base), field: fld, span: t.span.clone() };
                        }
                    } else {
                        self.expect(TokenKind::LBracket)?;
                        let idx = self.expr()?;
                        self.expect(TokenKind::RBracket)?;
                        base = Expr::Index{ array: Box::new(base), index: Box::new(idx), span: t.span.clone() };
                    }
                }
                Ok(base)
            }
            TokenKind::TrueKw => Ok(Expr::Bool(true, t.span)),
            TokenKind::FalseKw => Ok(Expr::Bool(false, t.span)),
            TokenKind::SelfKw => {
                let mut base = Expr::Var("self".to_string(), t.span.clone());
                while self.peek().kind == TokenKind::Dot {
                    self.next();
                    let fld = match self.next().kind.clone() { TokenKind::Ident(s2) => s2, _ => return Err(AxityError::parse("expected member name", self.peek().span.clone())) };
                    if self.peek().kind == TokenKind::LParen {
                        self.expect(TokenKind::LParen)?;
                        let mut args = Vec::new();
                        if self.peek().kind != TokenKind::RParen {
                            loop { args.push(self.expr()?); if self.peek().kind == TokenKind::Comma { self.next(); } else { break; } }
                        }
                        self.expect(TokenKind::RParen)?;
                        base = Expr::MethodCall{ object: Box::new(base), name: fld, args, span: t.span.clone() };
                    } else {
                        base = Expr::Member{ object: Box::new(base), field: fld, span: t.span.clone() };
                    }
                }
                Ok(base)
            }
            TokenKind::LParen => { let e = self.expr()?; self.expect(TokenKind::RParen)?; Ok(e) }
            _ => Err(AxityError::parse("unexpected token in expression", t.span))
        }
    }
}

