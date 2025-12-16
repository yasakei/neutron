use axity::run_source;
use axity::AxityError;

#[test]
fn do_while_prints() -> Result<(), AxityError> {
    let src = "let x: int = 0; do { print(x); x = x + 1; } while x < 3;";
    let out = run_source(src)?;
    assert_eq!(out, "0\n1\n2\n");
    Ok(())
}

#[test]
fn for_c_style_prints() -> Result<(), AxityError> {
    let src = "for let i: int = 0; i < 3; i++ { print(i); }";
    let out = run_source(src)?;
    assert_eq!(out, "0\n1\n2\n");
    Ok(())
}

#[test]
fn foreach_array_prints() -> Result<(), AxityError> {
    let src = "let xs: array<int> = [1,2,3]; for n in xs { print(n); }";
    let out = run_source(src)?;
    assert_eq!(out, "1\n2\n3\n");
    Ok(())
}
