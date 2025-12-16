use axity::{lexer, parser, AxityError};

#[test]
fn parse_function() -> Result<(), AxityError> {
    let src = "fn add(a: int, b: int) -> int { return a + b; }";
    let toks = lexer::lex(src)?;
    let ast = parser::parse(&toks)?;
    assert_eq!(ast.items.len(), 1);
    Ok(())
}

