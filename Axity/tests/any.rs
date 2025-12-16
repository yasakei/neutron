use axity::run_source;
use axity::AxityError;

#[test]
fn any_variable_assignment_changes() -> Result<(), AxityError> {
    let src = r#"
let v: any = 1;
print(v);
v = "hi";
print(v);
v = { "k": "v" };
print(v.k);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "1\nhi\nv\n");
    Ok(())
}

#[test]
fn any_function_identity() -> Result<(), AxityError> {
    let src = r#"
fn id(x: any) -> any { return x; }
print(id(42));
print(id("hello"));
print(id({ "a": 1 }).a);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "42\nhello\n1\n");
    Ok(())
}
