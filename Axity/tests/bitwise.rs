use axity::run_source;
use axity::AxityError;

#[test]
fn bitwise_ops() -> Result<(), AxityError> {
    let src = "print(5 & 1); print(5 | 1); print(5 ^ 1); print(~5); print(5 << 1); print(5 >> 1);";
    let out = run_source(src)?;
    assert_eq!(out, "1\n5\n4\n-6\n10\n2\n");
    Ok(())
}
