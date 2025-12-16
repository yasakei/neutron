use axity::run_source;
use axity::AxityError;

#[test]
fn numeric_comparisons_print_true() -> Result<(), AxityError> {
    let src = r#"
if 5 == 5 { print(true); } else { print(false); }
if 5 != 3 { print(true); } else { print(false); }
if 3 < 5 { print(true); } else { print(false); }
if 5 <= 5 { print(true); } else { print(false); }
if 7 > 3 { print(true); } else { print(false); }
if 5 >= 4 { print(true); } else { print(false); }
"#;
    let out = run_source(src)?;
    assert_eq!(out, "true\ntrue\ntrue\ntrue\ntrue\ntrue\n");
    Ok(())
}

#[test]
fn string_eq_ne() -> Result<(), AxityError> {
    let src = r#"
let a: str = "hi";
let b: str = "hi";
if a == b { print(true); } else { print(false); }
let c: str = "hi";
let d: str = "bye";
if c != d { print(true); } else { print(false); }
"#;
    let out = run_source(src)?;
    assert_eq!(out, "true\ntrue\n");
    Ok(())
}
