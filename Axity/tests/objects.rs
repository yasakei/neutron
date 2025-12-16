use axity::run_source;
use axity::AxityError;

#[test]
fn object_property_access() -> Result<(), AxityError> {
    let src = r#"
let names: obj = { "name": "George", "name2": "John" };
print(names.name);
print(names.name2);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "George\nJohn\n");
    Ok(())
}

#[test]
fn nested_object_access() -> Result<(), AxityError> {
    let src = r#"
let user: obj = { "name": "Alice", "meta": { "age": 30, "city": "NY" } };
print(user.name);
print(user.meta.city);
"#;
    let out = run_source(src)?;
    assert_eq!(out, "Alice\nNY\n");
    Ok(())
}
