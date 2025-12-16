use axity::run_source;
use axity::AxityError;

#[test]
fn float_literal_prints() -> Result<(), AxityError> {
    let src = "let x: flt = 1.5; print(x);";
    let out = run_source(src)?;
    assert_eq!(out, "1.500000\n");
    Ok(())
}

#[test]
fn scientific_literal_prints() -> Result<(), AxityError> {
    let src = "let x: flt = 1.5e1; print(x);";
    let out = run_source(src)?;
    assert_eq!(out, "15.000000\n");
    Ok(())
}

#[test]
fn unary_neg_int_and_flt() -> Result<(), AxityError> {
    let src = "let a: int = -3; let b: flt = -3.25; print(a); print(b);";
    let out = run_source(src)?;
    assert_eq!(out, "-3\n-3.250000\n");
    Ok(())
}

#[test]
fn trig_zero_values() -> Result<(), AxityError> {
    let src = "print(sin(0)); print(cos(0)); print(tan(0));";
    let out = run_source(src)?;
    assert_eq!(out, "0.000000\n1.000000\n0.000000\n");
    Ok(())
}

#[test]
fn str_alias_works() -> Result<(), AxityError> {
    let src = r#"let s: str = "hi"; print(s);"#;
    let out = run_source(src)?;
    assert_eq!(out, "hi\n");
    Ok(())
}
