use axity::run_source;
use axity::AxityError;

#[test]
fn retry_in_while_skips_print() -> Result<(), AxityError> {
    let src = r#"
let i: int = 0;
while i < 5 {
    i = i + 1;
    if i == 3 { retry; }
    print(i);
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "1\n2\n4\n5\n");
    Ok(())
}

#[test]
fn retry_in_for_skips_iteration_body() -> Result<(), AxityError> {
    let src = r#"
for let j: int = 0; j < 5; j++ {
    if j == 2 { retry; }
    print(j);
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "0\n1\n3\n4\n");
    Ok(())
}
