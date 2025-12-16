use axity::AxityError;

#[test]
fn lex_basic() -> Result<(), AxityError> {
    let src = "let x: int = 1; print(x);";
    let toks = axity::lexer::lex(src)?;
    assert!(toks.len() > 0);
    Ok(())
}

