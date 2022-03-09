use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, Data, DataStruct, DeriveInput, Fields};

struct Field<'a> {
    name: &'a proc_macro2::Ident,
    _ty: &'a syn::Type,
}

fn struct_fields(data: &DataStruct) -> Vec<Field> {
    match &data.fields {
        Fields::Named(named) => named
            .named
            .iter()
            .map(|field| Field {
                name: field.ident.as_ref().unwrap(),
                _ty: &field.ty,
            })
            .collect(),
        Fields::Unnamed(_) => unimplemented!("fracpack does not support unnamed fields"),
        Fields::Unit => unimplemented!("fracpack does not support unit struct"),
    }
}

pub fn fracpack_macro_impl(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let generics = &input.generics;
    let fields = match &input.data {
        Data::Struct(data) => struct_fields(data),
        Data::Enum(_data) => unimplemented!("fracpack does not support enum"),
        Data::Union(_) => unimplemented!("fracpack does not support union"),
    };
    let fixed_size = fields
        .iter()
        .map(|field| {
            let name = &field.name;
            quote! {self.#name.fixed_size()}
        })
        .fold(quote! {2}, |acc, new| quote! {#acc + #new});
    let positions: Vec<syn::Ident> = fields
        .iter()
        .map(|field| {
            let name = &field.name;
            let concatenated = format!("pos_{}", name);
            syn::Ident::new(&concatenated, name.span())
        })
        .collect();
    let pack_fixed_members = fields
        .iter()
        .enumerate()
        .map(|(i, field)| {
            let name = &field.name;
            let pos = &positions[i];
            quote! {
                let #pos = dest.len() as u32;
                self.#name.pack_fixed(dest);
            }
        })
        .fold(quote! {}, |acc, new| quote! {#acc #new});
    let pack_variable_members = fields
        .iter()
        .enumerate()
        .map(|(i, field)| {
            let name = &field.name;
            let pos = &positions[i];
            quote! {
                self.#name.repack_fixed(#pos, dest.len() as u32, dest);
                self.#name.pack_variable(dest);
            }
        })
        .fold(quote! {}, |acc, new| quote! {#acc #new});
    TokenStream::from(quote! {
        impl fracpack::Packable for #name #generics {
            fn fixed_size(&self) -> u32 {4}
            fn pack_fixed(&self, dest: &mut Vec<u8>) {
                dest.extend_from_slice(&0u32.to_le_bytes());
            }
            fn repack_fixed(&self, fixed_pos: u32, heap_pos: u32, dest: &mut Vec<u8>) {
                dest[fixed_pos as usize..fixed_pos as usize + 4]
                    .copy_from_slice(&(heap_pos-fixed_pos).to_le_bytes());
            }
            fn pack_variable(&self, dest: &mut Vec<u8>) {
                // TODO: check if fixed_size is too big
                let heap = #fixed_size;
                (heap as u16).pack(dest);
                #pack_fixed_members
                #pack_variable_members
            }
            fn pack(&self, dest: &mut Vec<u8>) {
                self.pack_variable(dest);
            }
        }
    })
}
