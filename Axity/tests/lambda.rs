use axity::run_source;
use axity::AxityError;

#[test]
fn lambda_add() -> Result<(), AxityError> {
    let src = r#"
let f: obj = fn (a: int, b: int) -> int { return a + b; };
print(f(2, 3));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "5\n");
    Ok(())
}

#[test]
fn lambda_returns_string() -> Result<(), AxityError> {
    let src = r#"
let greet: obj = fn (name: str) -> str { return "hi " + name; };
print(greet("George"));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "hi George\n");
    Ok(())
}
