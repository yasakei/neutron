use axity::run_source;
use axity::AxityError;

#[test]
fn print_interpolation_and_concat() -> Result<(), AxityError> {
    let src = r#"
let name: str = "George";
print("hello !{name}");
print("hello " + name);
let count: int = 3;
print("count is !{count}");
"#;
    let out = run_source(src)?;
    assert_eq!(out, "hello George\nhello George\ncount is 3\n");
    Ok(())
}
