use std::{borrow::Cow, rc::Rc, sync::Arc};

pub trait Reflect {
    type StaticType: 'static + Reflect;
    fn reflect<V: Visitor>(visitor: V) -> V::Return;
}

pub trait Visitor {
    type Return;
    type TupleVisitor: UnnamedFieldsVisitor<Self::Return>;
    type StructTupleVisitor: UnnamedFieldsVisitor<Self::Return>;
    type NamedFieldsVisitor: NamedFieldsVisitor<Self::Return>;
    type EnumFieldsVisitor: EnumFieldsVisitor<Self::Return>;

    fn custom_json(self) -> Self;
    fn definition_will_not_change(self) -> Self;

    fn builtin_type<T: Reflect>(self, name: &'static str) -> Self::Return;
    fn refed<Inner: Reflect>(self) -> Self::Return;
    fn mut_ref<Inner: Reflect>(self) -> Self::Return;
    fn boxed<Inner: Reflect>(self) -> Self::Return;
    fn rc<Inner: Reflect>(self) -> Self::Return;
    fn arc<Inner: Reflect>(self) -> Self::Return;
    fn container<T: Reflect, Inner: Reflect>(self) -> Self::Return;
    fn array<Inner: Reflect, const SIZE: usize>(self) -> Self::Return;
    fn tuple<T: Reflect>(self, fields_len: usize) -> Self::TupleVisitor;
    fn struct_alias<T: Reflect, Inner: Reflect>(self, name: Cow<'static, str>) -> Self::Return;
    fn struct_tuple<T: Reflect>(
        self,
        name: Cow<'static, str>,
        fields_len: usize,
    ) -> Self::StructTupleVisitor;
    fn struct_named<T: Reflect>(
        self,
        name: Cow<'static, str>,
        fields_len: usize,
    ) -> Self::NamedFieldsVisitor;
    fn enumeration<T: Reflect>(self, name: Cow<'static, str>) -> Self::EnumFieldsVisitor;
}

pub trait UnnamedFieldsVisitor<Return> {
    fn field<T: Reflect>(self) -> Self;
    fn end(self) -> Return;
}

pub trait NamedFieldsVisitor<Return> {
    type MethodsVisitor: MethodsVisitor<Return>;

    fn field<T: Reflect>(self, name: Cow<'static, str>) -> Self;
    fn end(self) -> Return;
    fn with_methods(self) -> Self::MethodsVisitor;
}

pub trait MethodsVisitor<Return> {}

pub trait EnumFieldsVisitor<Return> {}

impl<'a, T: Reflect> Reflect for &'a T {
    type StaticType = &'static T::StaticType;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.refed::<T>()
    }
}

impl<'a, T: Reflect> Reflect for &'a mut T {
    type StaticType = &'static mut T::StaticType;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.mut_ref::<T>()
    }
}

impl<T: Reflect> Reflect for Box<T> {
    type StaticType = Box<T::StaticType>;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.boxed::<T>()
    }
}

impl<T: Reflect> Reflect for Rc<T> {
    type StaticType = Rc<T::StaticType>;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.rc::<T>()
    }
}

impl<T: Reflect> Reflect for Arc<T> {
    type StaticType = Arc<T::StaticType>;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.arc::<T>()
    }
}

impl<T: Reflect> Reflect for Vec<T>
where
    T::StaticType: Sized,
{
    type StaticType = Vec<T::StaticType>;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.container::<Vec<T>, T>()
    }
}

impl<T: Reflect, const SIZE: usize> Reflect for [T; SIZE]
where
    T::StaticType: Sized,
{
    type StaticType = [T::StaticType; SIZE];
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.array::<T, SIZE>()
    }
}

impl<'a, T: Reflect + 'a> Reflect for &'a [T]
where
    T::StaticType: Sized,
{
    type StaticType = &'static [T::StaticType];
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.container::<&'a [T], T>()
    }
}

impl<'a, T: Reflect + 'a> Reflect for &'a mut [T]
where
    T::StaticType: Sized,
{
    type StaticType = &'static [T::StaticType];
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.container::<&'a mut [T], T>()
    }
}

macro_rules! builtin_impl {
    ($ty:ty, $name:literal) => {
        impl Reflect for $ty {
            type StaticType = $ty;
            fn reflect<V: Visitor>(visitor: V) -> V::Return {
                visitor.builtin_type::<$ty>($name)
            }
        }
    };
}

builtin_impl!(bool, "bool");
builtin_impl!(u8, "u8");
builtin_impl!(u16, "u16");
builtin_impl!(u32, "u32");
builtin_impl!(u64, "u64");
builtin_impl!(i8, "i8");
builtin_impl!(i16, "i16");
builtin_impl!(i32, "i32");
builtin_impl!(i64, "i64");
builtin_impl!(f32, "f32");
builtin_impl!(f64, "f64");
builtin_impl!(String, "string");

impl<'a> Reflect for &'a str {
    type StaticType = &'static str;
    fn reflect<V: Visitor>(visitor: V) -> V::Return {
        visitor.builtin_type::<&'a str>("string")
    }
}

macro_rules! tuple_impls {
    ($($len:expr => ($($n:tt $name:ident)*))+) => {
        $(
            impl<$($name: Reflect),*> Reflect for ($($name,)*)
            where $($name::StaticType: Sized),*
            {
                type StaticType = ($($name::StaticType,)*);

                #[allow(non_snake_case)]
                fn reflect<V: Visitor>(visitor: V) -> V::Return
                {
                    visitor.tuple::<($($name,)*)>($len)
                    $(.field::<$name>())*
                    .end()
                }
            }
        )+
    }
}

tuple_impls! {
    1 => (0 T0)
    2 => (0 T0 1 T1)
    3 => (0 T0 1 T1 2 T2)
    4 => (0 T0 1 T1 2 T2 3 T3)
    5 => (0 T0 1 T1 2 T2 3 T3 4 T4)
    6 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5)
    7 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6)
    8 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7)
    9 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8)
    10 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9)
    11 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10)
    12 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10 11 T11)
    13 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10 11 T11 12 T12)
    14 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10 11 T11 12 T12 13 T13)
    15 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10 11 T11 12 T12 13 T13 14 T14)
    16 => (0 T0 1 T1 2 T2 3 T3 4 T4 5 T5 6 T6 7 T7 8 T8 9 T9 10 T10 11 T11 12 T12 13 T13 14 T14 15 T15)
}
