use axity::run_source;
use axity::AxityError;

#[test]
fn try_catch_string() -> Result<(), AxityError> {
    let src = r#"
try {
  throw "boom";
} catch err {
  print(err);
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "boom\n");
    Ok(())
}

#[test]
fn uncaught_exception_returns_error() {
    let src = r#"throw "fail";"#;
    let res = run_source(src);
    assert!(res.is_err());
}
