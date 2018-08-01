macro_rules! enum_match {
    ($val:ident, $pattern:pat) => {{
        match $val {
            $pattern => true,
            _ => false,
        }
    }};
}
