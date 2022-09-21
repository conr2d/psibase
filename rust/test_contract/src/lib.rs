use psibase::*;

#[contract(example)]
mod example_contract {
    #[action]
    pub fn hi() {
        psibase::write_console("hello!!!");
    }

    #[action]
    pub fn add(a: i32, b: i32) -> i32 {
        a + b
    }

    #[action]
    pub fn multiply(a: i32, b: i32) -> i32 {
        a * b
    }
}
