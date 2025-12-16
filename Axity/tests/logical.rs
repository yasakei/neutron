use axity::run_source;
use axity::AxityError;

#[test]
fn logical_aliases_and_ops() -> Result<(), AxityError> {
    let src = "print(true and false); print(true or false); print(!true); print(true && true); print(false || true);";
    let out = run_source(src)?;
    assert_eq!(out, "false\ntrue\nfalse\ntrue\ntrue\n");
    Ok(())
}
