use axity::{run_file, AxityError};

#[test]
fn import_functions() -> Result<(), AxityError> {
    let out = run_file("examples/import_main.ax")?;
    assert_eq!(out, "5\n");
    Ok(())
}

