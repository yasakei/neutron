use axity::run_source;
use axity::AxityError;

#[test]
fn iife_basic() -> Result<(), AxityError> {
    let src = r#"
let r: int = fn (a: int, b: int) -> int { return a + b; }(2, 3);
print(r);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "5\n");
    Ok(())
}

#[test]
fn buffers_ops() -> Result<(), AxityError> {
    let src = r#"
let buf: buffer = buffer_new(3);
buffer_set(buf, 0, 65);
buffer_set(buf, 1, 66);
buffer_set(buf, 2, 67);
print(buffer_len(buf));
print(buffer_to_string(buf));
let buf2: buffer = buffer_from_string("hi");
buffer_push(buf2, 33);
print(buffer_to_string(buf2));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "3\nABC\nhi!\n");
    Ok(())
}
