use axity::run_source;
use axity::AxityError;

#[test]
fn integer_arithmetic_and_mod() -> Result<(), AxityError> {
    let src = "print(5 + 3); print(10 - 4); print(6 * 7); print(15 / 3); print(10 % 3);";
    let out = run_source(src)?;
    assert_eq!(out, "8\n6\n42\n5\n1\n");
    Ok(())
}

#[test]
fn string_concat() -> Result<(), AxityError> {
    let src = r#"let a: str = "hi"; let b: str = " there"; print(a + b);"#;
    let out = run_source(src)?;
    assert_eq!(out, "hi there\n");
    Ok(())
}

#[test]
fn inc_dec_postfix() -> Result<(), AxityError> {
    let src = r#"
let x: int = 1;
x++;
print(x);
let y: int = 2;
y--;
print(y);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "2\n1\n");
    Ok(())
}
