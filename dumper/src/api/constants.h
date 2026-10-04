namespace Api {
    namespace TypeEnum {
        constexpr int END         = 0x00;
        constexpr int VOID        = 0x01;
        constexpr int BOOLEAN     = 0x02;
        constexpr int CHAR        = 0x03;
        constexpr int I1          = 0x04;
        constexpr int U1          = 0x05;
        constexpr int I2          = 0x06;
        constexpr int U2          = 0x07;
        constexpr int I4          = 0x08;
        constexpr int U4          = 0x09;
        constexpr int I8          = 0x0a;
        constexpr int U8          = 0x0b;
        constexpr int R4          = 0x0c;
        constexpr int R8          = 0x0d;
        constexpr int STRING      = 0x0e;
        constexpr int PTR         = 0x0f;
        constexpr int BYREF       = 0x10;
        constexpr int VALUETYPE   = 0x11;
        constexpr int CLASS       = 0x12;
        constexpr int VAR         = 0x13;
        constexpr int ARRAY       = 0x14;
        constexpr int GENERICINST = 0x15;
        constexpr int TYPEDBYREF  = 0x16;
        constexpr int I           = 0x18;
        constexpr int U           = 0x19;
        constexpr int FNPTR       = 0x1b;
        constexpr int OBJECT      = 0x1c;
        constexpr int SZARRAY     = 0x1d;
        constexpr int MVAR        = 0x1e;
        constexpr int CMOD_REQD   = 0x1f;
        constexpr int CMOD_OPT    = 0x20;
        constexpr int INTERNAL    = 0x21;

        constexpr int MODIFIER    = 0x40;
        constexpr int SENTINEL    = 0x41;
        constexpr int PINNED      = 0x45;

        constexpr int ENUM        = 0x55;
    };

    namespace FieldAttributes {
        constexpr int FIELD_ACCESS_MASK   = 0x0007;
        constexpr int COMPILER_CONTROLLED = 0x0000;
        constexpr int PRIVATE             = 0x0001;
        constexpr int FAM_AND_ASSEM       = 0x0002;
        constexpr int ASSEMBLY            = 0x0003;
        constexpr int FAMILY              = 0x0004;
        constexpr int FAM_OR_ASSEM        = 0x0005;
        constexpr int PUBLIC              = 0x0006;

        constexpr int STATIC              = 0x0010;
        constexpr int INIT_ONLY           = 0x0020;
        constexpr int LITERAL             = 0x0040;
        constexpr int NOT_SERIALIZED      = 0x0080;
        constexpr int SPECIAL_NAME        = 0x0200;
        constexpr int PINVOKE_IMPL        = 0x2000;

        /* For runtime use only */
        constexpr int RESERVED_MASK       = 0x9500;
        constexpr int RT_SPECIAL_NAME     = 0x0400;
        constexpr int HAS_FIELD_MARSHAL   = 0x1000;
        constexpr int HAS_DEFAULT         = 0x8000;
        constexpr int HAS_FIELD_RVA       = 0x0100;
    };

    namespace MethodAttributes {
        constexpr int MEMBER_ACCESS_MASK  = 0x0007;
        constexpr int COMPILER_CONTROLLED = 0x0000;
        constexpr int PRIVATE             = 0x0001;
        constexpr int FAM_AND_ASSEM       = 0x0002;
        constexpr int ASSEM               = 0x0003;
        constexpr int FAMILY              = 0x0004;
        constexpr int FAM_OR_ASSEM        = 0x0005;
        constexpr int PUBLIC              = 0x0006;

        constexpr int STATIC              = 0x0010;
        constexpr int FINAL               = 0x0020;
        constexpr int VIRTUAL             = 0x0040;
        constexpr int HIDE_BY_SIG         = 0x0080;

        constexpr int VTABLE_LAYOUT_MASK  = 0x0100;
        constexpr int REUSE_SLOT          = 0x0000;
        constexpr int NEW_SLOT            = 0x0100;

        constexpr int STRICT              = 0x0200;
        constexpr int ABSTRACT            = 0x0400;
        constexpr int SPECIAL_NAME        = 0x0800;

        constexpr int PINVOKE_IMPL        = 0x2000;
        constexpr int UNMANAGED_EXPORT    = 0x0008;
    };

    namespace TypeAttributes {
        constexpr int VISIBILITY_MASK      = 0x00000007;
        constexpr int NOT_PUBLIC           = 0x00000000;
        constexpr int PUBLIC               = 0x00000001;
        constexpr int NESTED_PUBLIC        = 0x00000002;
        constexpr int NESTED_PRIVATE       = 0x00000003;
        constexpr int NESTED_FAMILY        = 0x00000004;
        constexpr int NESTED_ASSEMBLY      = 0x00000005;
        constexpr int NESTED_FAM_AND_ASSEM = 0x00000006;
        constexpr int NESTED_FAM_OR_ASSEM  = 0x00000007;

        constexpr int LAYOUT_MASK          = 0x00000018;
        constexpr int AUTO_LAYOUT          = 0x00000000;
        constexpr int SEQUENTIAL_LAYOUT    = 0x00000008;
        constexpr int EXPLICIT_LAYOUT      = 0x00000010;

        constexpr int CLASS_SEMANTIC_MASK  = 0x00000020;
        constexpr int CLASS                = 0x00000000;
        constexpr int INTERFACE            = 0x00000020;

        constexpr int ABSTRACT             = 0x00000080;
        constexpr int SEALED               = 0x00000100;
        constexpr int SPECIAL_NAME         = 0x00000400;

        constexpr int IMPORT               = 0x00001000;
        constexpr int SERIALIZABLE         = 0x00002000;

        constexpr int STRING_FORMAT_MASK   = 0x00030000;
        constexpr int ANSI_CLASS           = 0x00000000;
        constexpr int UNICODE_CLASS        = 0x00010000;
        constexpr int AUTO_CLASS           = 0x00020000;

        constexpr int BEFORE_FIELD_INIT    = 0x00100000;
        constexpr int FORWARDER            = 0x00200000;

        constexpr int RESERVED_MASK        = 0x00040800;
        constexpr int RT_SPECIAL_NAME      = 0x00000800;
        constexpr int HAS_SECURITY         = 0x00040000;
    };

    namespace ParameterAttributes {
        constexpr int IN                = 0x0001;
        constexpr int OUT               = 0x0002;
        constexpr int OPTIONAL          = 0x0010;
        constexpr int RESERVED_MASK     = 0xf000;
        constexpr int HAS_DEFAULT       = 0x1000;
        constexpr int HAS_FIELD_MARSHAL = 0x2000;
        constexpr int UNUSED            = 0xcfe0;
    };

    namespace BindingFlags {
        constexpr int DEFAULT                = 0;
        constexpr int IGNORE_CASE            = 0x0000001;
        constexpr int DECLARED_ONLY          = 0x0000002;
        constexpr int INSTANCE               = 0x0000004;
        constexpr int STATIC                 = 0x0000008;
        constexpr int PUBLIC                 = 0x0000010;
        constexpr int NON_PUBLIC             = 0x0000020;
        constexpr int FLATTEN_HIERARCHY      = 0x0000040;
        constexpr int INVOKE_METHOD          = 0x0000100;
        constexpr int CREATE_INSTANCE        = 0x0000200;
        constexpr int GET_FIELD              = 0x0000400;
        constexpr int SET_FIELD              = 0x0000800;
        constexpr int GET_PROPERTY           = 0x0001000;
        constexpr int SET_PROPERTY           = 0x0002000;
        constexpr int PUT_DISP_PROPERTY      = 0x0004000;
        constexpr int PUT_REF_DISP_PROPERTY  = 0x0008000;
        constexpr int EXACT_BINDING          = 0x0010000;
        constexpr int SUPPRESS_CHANGE_TYPE   = 0x0020000;
        constexpr int OPTIONAL_PARAM_BINDING = 0x0040000;
        constexpr int IGNORE_RETURN          = 0x1000000;
        constexpr int DO_NOT_WRAP_EXCEPTIONS = 0x2000000;
    }
}