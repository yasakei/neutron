use axity::run_source;
use axity::AxityError;

#[test]
fn match_int_blocks() -> Result<(), AxityError> {
    let src = r#"
let x: int = 2;
match x {
  case 1: { print("one"); }
  case 2: { print("two"); }
  default: { print("other"); }
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "two\n");
    Ok(())
}

#[test]
fn match_string_blocks() -> Result<(), AxityError> {
    let src = r#"
let s: str = "hi";
match s {
  case "bye": { print("bye"); }
  case "hi": { print("greet"); }
  default: { print("none"); }
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "greet\n");
    Ok(())
}
