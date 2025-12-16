use axity::run_source;
use axity::AxityError;

#[test]
fn function_return_used_in_assignment() -> Result<(), AxityError> {
    let src = r#"
fn add(a: int, b: int) -> int { return a + b; }
let s: int = add(2, 3);
print(s);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "5\n");
    Ok(())
}

#[test]
fn main_return_printed() -> Result<(), AxityError> {
    let src = r#"
fn main() -> int { return 9; }
"#;
    let out = run_source(src)?;
    assert_eq!(out, "9\n");
    Ok(())
}

#[test]
fn return_string() -> Result<(), AxityError> {
    let src = r#"
fn greet(name: str) -> str { return "hello " + name; }
print(greet("George"));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "hello George\n");
    Ok(())
}

#[test]
fn return_float() -> Result<(), AxityError> {
    let src = r#"
fn num() -> flt { return 1.5; }
print(num());
"#;
    let out = run_source(src)?;
    assert_eq!(out, "1.500000\n");
    Ok(())
}

#[test]
fn return_obj() -> Result<(), AxityError> {
    let src = r#"
fn make(name: str) -> obj { return { "name": name }; }
let o: obj = make("Alice");
print(o.name);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "Alice\n");
    Ok(())
}

#[test]
fn main_return_string_printed() -> Result<(), AxityError> {
    let src = r#"
fn main() -> str { return "ok"; }
"#;
    let out = run_source(src)?;
    assert_eq!(out, "ok\n");
    Ok(())
}

#[test]
fn return_bool() -> Result<(), AxityError> {
    let src = r#"
fn flag() -> bool { return true; }
print(flag());
"#;
    let out = run_source(src)?;
    assert_eq!(out, "true\n");
    Ok(())
}

#[test]
fn return_array() -> Result<(), AxityError> {
    let src = r#"
fn arr() -> array<int> { return [1,2]; }
print(arr());
"#;
    let out = run_source(src)?;
    assert_eq!(out, "[1, 2]\n");
    Ok(())
}

#[test]
fn return_map() -> Result<(), AxityError> {
    let src = r#"
fn mk() -> map<str> {
  let m: map<str> = map_new_string();
  map_set(m, "k", "v");
  return m;
}
let m: map<str> = mk();
print(map_get(m, "k"));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "v\n");
    Ok(())
}

#[test]
fn return_class() -> Result<(), AxityError> {
    let src = r#"
class Box { let x: int; }
fn mk() -> Box { let z: Box = new Box(); return z; }
let b: Box = mk();
print(b.x);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "0\n");
    Ok(())
}
