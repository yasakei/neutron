use axity::run_source;
use axity::AxityError;

#[test]
fn run_loop_prints() -> Result<(), AxityError> {
    let src = "let x: int = 0; while x < 3 { print(x); x = x + 1; }";
    let out = run_source(src)?;
    assert_eq!(out, "0\n1\n2\n");
    Ok(())
}

#[test]
fn type_error_undefined_var() {
    let src = "print(y);";
    let res = run_source(src);
    assert!(res.is_err());
}

#[test]
fn call_function_in_expr() -> Result<(), AxityError> {
    let src = "fn add(a: int, b: int) -> int { return a + b; } print(add(2,3));";
    let out = run_source(src)?;
    assert_eq!(out, "5\n");
    Ok(())
}

#[test]
fn strings_concat() -> Result<(), AxityError> {
    let src = r#"let a: string = "hi"; let b: string = " there"; print(a + b);"#;
    let out = run_source(src)?;
    assert_eq!(out, "hi there\n");
    Ok(())
}

#[test]
fn classes_field_and_method() -> Result<(), AxityError> {
    let src = r#"
class Point {
    let x: int;
    let y: int;
    fn move(self: Point, dx: int, dy: int) -> int {
        self.x = self.x + dx;
        self.y = self.y + dy;
        return 0;
    }
}
let p: Point = new Point;
p.move(2, 3);
print(p.x);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "2\n");
    Ok(())
}

#[test]
fn array_indexing_and_len() -> Result<(), AxityError> {
    let src = r#"
let xs: array<int> = [1,2,3];
print(len(xs));
print(xs[1]);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "3\n2\n");
    Ok(())
}

#[test]
fn if_else_branching() -> Result<(), AxityError> {
    let src = r#"
let x: int = 0;
if x == 0 {
    print(1);
} else {
    print(2);
}
"#;
    let out = run_source(src)?;
    assert_eq!(out, "1\n");
    Ok(())
}

#[test]
fn maps_basic() -> Result<(), AxityError> {
    let src = r#"
let m: map<int> = map_new_int();
map_set(m, "a", 10);
print(map_get(m, "a"));
print(map_has(m, "b"));
"#;
    let out = run_source(src)?;
    assert_eq!(out, "10\nfalse\n");
    Ok(())
}
