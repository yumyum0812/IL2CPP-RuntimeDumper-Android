#include <chrono>
#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "api/functions.h"
#include "api/object.h"
#include "api/class.h"
#include "api/image.h"
#include "api/method.h"
#include "api/type.h"
#include "api/field.h"
#include "api/assembly.h"
#include "api/constants.h"
#include "api/thread.h"
#include "api/domain.h"
#include "api/string.h"
#include "api/array.h"
#include "api/property.h"
#include "utils/address.h"
#include "utils/jni_util.h"
#include "resolver.h"
#include "log.h"

using namespace Api;


constexpr auto INDENT = "    ";

const std::unordered_map<int, std::string> typeAliases = {
    {TypeEnum::VOID, "void"},
    {TypeEnum::BOOLEAN, "bool"},
    {TypeEnum::CHAR, "char"},
    {TypeEnum::I1, "sbyte"},
    {TypeEnum::U1, "byte"},
    {TypeEnum::I2, "short"},
    {TypeEnum::U2, "ushort"},
    {TypeEnum::I4, "int"},
    {TypeEnum::U4, "uint"},
    {TypeEnum::I8, "long"},
    {TypeEnum::U8, "ulong"},
    {TypeEnum::R4, "float"},
    {TypeEnum::R8, "double"},
    {TypeEnum::STRING, "string"},
    {TypeEnum::TYPEDBYREF, "TypedReference"},
    {TypeEnum::I, "IntPtr"},
    {TypeEnum::U, "UIntPtr"},
    {TypeEnum::OBJECT, "object"},
};

const std::unordered_map<int, std::string> typeSuffixes = {
    {TypeEnum::SZARRAY, "[]"},
    {TypeEnum::PTR, "*"},
};

const std::unordered_map<int, std::string> classAccessors = {
    {TypeAttributes::PUBLIC, "public"},
    {TypeAttributes::NESTED_PUBLIC, "public"},
    {TypeAttributes::NOT_PUBLIC, "internal"},
    {TypeAttributes::NESTED_FAM_AND_ASSEM, "private protected"},
    {TypeAttributes::NESTED_ASSEMBLY, "internal"},
    {TypeAttributes::NESTED_PRIVATE, "private"},
    {TypeAttributes::NESTED_FAMILY, "protected"},
    {TypeAttributes::NESTED_FAM_OR_ASSEM, "protected internal"},
};

const std::unordered_map<int, std::string> fieldAccessors = {
    {FieldAttributes::PUBLIC, "public"},
    {FieldAttributes::PRIVATE, "private"},
    {FieldAttributes::FAMILY, "protected"},
    {FieldAttributes::ASSEMBLY, "internal"},
    {FieldAttributes::FAM_OR_ASSEM, "protected internal"},
    {FieldAttributes::FAM_AND_ASSEM, "private protected"},
};

const std::unordered_map<int, std::string> methodAccessors = {
    {MethodAttributes::PUBLIC, "public"},
    {MethodAttributes::PRIVATE, "private"},
    {MethodAttributes::ASSEM, "internal"},
    {MethodAttributes::FAMILY, "protected"},
    {MethodAttributes::FAM_OR_ASSEM, "protected internal"},
    {MethodAttributes::FAM_AND_ASSEM, "private protected"},
};

const std::unordered_map<char, std::string> charEscapes = {
    {'\\', "\\\\"},
    {'"',  "\\\""},
    {'\n', "\\n"},
    {'\0', "\\0"},
    {'\a', "\\a"},
    {'\f', "\\f"},
    {'\n', "\\n"},
    {'\r', "\\r"},
    {'\t', "\\t"},
    {'\v', "\\v"}
};


uintptr_t libBase = 0;
size_t structBoxSize = 0;
Class* typeC = nullptr;
Method* arrGetValue = nullptr;
Method* tGetGenericArgs = nullptr;
Method* tGetField = nullptr;
Method* tGetProp = nullptr;
Method* mGetGenericArgs = nullptr;
Method* getUnityVer = nullptr;
struct {
    Method* getAttrs = nullptr;
    Method* getAttType = nullptr;
    Method* getConstructorArgs = nullptr;
    Method* getNamedArgs = nullptr;
} attM;
struct {
    Method* getValue = nullptr;
} attTypedArgM;
struct {
    Method* getName = nullptr;
    Method* getTypedValue = nullptr;
} attNamedArgM;


std::string JoinStr(const std::vector<std::string>& vars, const std::string& delim) {
    std::string out;
    for (int i = 0; i < vars.size(); i++) {
        if (i >= 1) out += delim;
        out += vars[i];
    }
    return out;
}

void RemoveSuffix(std::string& str, std::string_view suff) {
    if (str.ends_with(suff))
        str.erase(str.size() - suff.size());
}

std::string U16ToU8(const std::u16string& u16str) {
    std::u8string u8str = std::filesystem::path(u16str).u8string();
    std::string utf8str(reinterpret_cast<const char*>(u8str.data()), u8str.length());
    return utf8str;
}

Image* FindImage(const std::string& name) {
    auto domain = Domain::GetInstance();
    auto asms = domain->GetAssemblies();
    for (auto a : asms) {
        auto img = a->GetImage();
        if (img->GetName() == name) return img;
    }
    return nullptr;
}

Method* FindMethod(Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
    auto methods = klass->GetMethods();

    for (auto m : methods) {
        if (m->GetName() != name) continue;

        if (!rtName.empty()) {
            auto rt = m->GetReturnType();
            if (!rt || rt->GetName() != rtName) continue;
        }

        auto pc = m->GetParamCount();
        if (pc != pNames.size()) continue;

        bool ok = true;
        for (int i = 0; i < pc; i++) {
            auto c = m->GetParam(i);
            if (c->GetName() != pNames[i]) {
                ok = false;
                break;
            }
        }

        if (ok) return m;
    }

    return nullptr;
}

Method* FindMethodP(Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
    auto m = FindMethod(klass, name, rtName, pNames);
    if (m) return m;

    auto par = klass->GetParent();
    return FindMethod(par, name, rtName, pNames);
}

Method* FindMethodI(Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
    auto m = FindMethod(klass, name, rtName, pNames);

    auto inters = klass->GetInterfaces();
    for (auto p : inters) {
        if (p && p != klass) {
            auto met = FindMethod(p, name, rtName, pNames);
            if (met) return met;
        }
    }
    return nullptr;
}

std::string GetAppExtDir() {
    JavaVM* vm = JniUtil::GetJavaVM();
    if (!vm) {
        Log::Error("cannot obtain JavaVM");
        return "";
    }

    auto att = JniUtil::JniAttach(vm);
    JNIEnv* env = att.Attach();
    if (!env) {
        Log::Error("cannot attach JNI");
        return "";
    }

    auto app = JniUtil::GetApplication(env);
    if (!app) {
        Log::Error("cannot obtain Application");
        return "";
    }

    auto extDir = JniUtil::GetExternalFilesPath(env, app);
    if (extDir.empty()) {
        Log::Error("cannot obtain externalFilesDir");
        return "";
    }

    return extDir;
}

std::string FormatTypeName(Type* typ, bool def = false) {
    auto klass = Class::FromType(typ);
    auto tp = typ->GetTypeEnum();

    std::string res;

    auto dt = klass->GetDeclaringClass();
    if (dt)
        res += FormatTypeName(dt->GetType(), def) + ".";
    
    if (typeSuffixes.contains(tp)) {
        auto ec = klass->GetElementClass();
        if (ec && ec != klass)
            return res + FormatTypeName(ec->GetType(), def) + typeSuffixes.at(tp);
    }
    
    if (!def && typeAliases.contains(tp)) {
        return typeAliases.at(tp);
    }

    if (arrGetValue && tGetGenericArgs && (klass->IsGeneric() || klass->IsInflated())) {
        auto to = typ->GetObject();
        auto getGensV = to->GetVirtualMethod(tGetGenericArgs);
        auto args = (Array*) getGensV->Invoke(to);
        
        std::string pureName = klass->GetName();
        size_t pos = pureName.find('`');
        pureName = (pos != std::string::npos) ? pureName.substr(0, pos) : pureName;

        int len = args->GetLength();
        std::vector<std::string> ps;
        for (int i = 0; i < len; i++) {
            auto p = arrGetValue->Invoke(args, i);
            auto pt = Class::FromSystemType(p);
            auto ptC = pt->GetType();
            ps.push_back(FormatTypeName(ptC, def));
        }
        
        return pureName + "<" + JoinStr(ps, ", ") + ">";
    }

    return res + klass->GetName();
}

std::string FormatTypeType(Class* klass) {
    using namespace TypeAttributes;

    std::string out;

    int flags = klass->GetFlags();

    if ((flags & ABSTRACT) && (flags & SEALED))
        out += "static ";
    else if ((flags & INTERFACE) && (flags & ABSTRACT))
        out += "abstract ";
    else if (!klass->IsValueType() && !klass->IsEnum() && (flags & SEALED))
        out += "sealed ";
    if (flags & INTERFACE)
        out += "interface ";
    else if (klass->IsEnum())
        out += "enum ";
    else if (klass->IsValueType())
        out += "struct ";
    else
        out += "class ";

    return out;
}

std::string FormatTypeAccessor(Class* klass) {
    using namespace TypeAttributes;
    int flags = klass->GetFlags();

    int vis = flags & VISIBILITY_MASK;
    if (classAccessors.contains(vis)) {
        return classAccessors.at(vis) + " ";
    }

    return {};
}

std::string FormatMethodAccessor(Method* met) {
    using namespace MethodAttributes;

    std::string out;

    int flags = met->GetFlags();
    int isReuse = (flags & VTABLE_LAYOUT_MASK) == REUSE_SLOT;

    int acc = flags & MEMBER_ACCESS_MASK;
    if (methodAccessors.contains(acc)) {
        out += methodAccessors.at(acc);
        out += " ";
    }

    if (flags & STATIC)
        out += "static ";
    
    if (flags & ABSTRACT) {
        out += "abstract ";
        if (isReuse) out += "override ";
    } else if ((flags & FINAL) && isReuse) {
        out += "sealed override ";
    } else if (flags & VIRTUAL) {
        out += isReuse ? "override " : "virtual ";
    }
    
    if (flags & PINVOKE_IMPL)
        out += "extern ";

    return out;
}

std::string FormatFieldAccessor(Field* fld) {
    using namespace FieldAttributes;

    std::string out;

    int flags = fld->GetFlags();
    int vis = flags & FIELD_ACCESS_MASK;

    if (fieldAccessors.contains(vis)) {
        out += fieldAccessors.at(vis);
        out += " ";
    }

    if (flags & LITERAL) {
        out += "const ";
    } else {
        if (flags & STATIC)
            out += "static ";
        
        if (flags & INIT_ONLY)
            out += "readonly ";
    }

    return out;
}

std::string FormatParamType(Type* typ) {
    using namespace ParameterAttributes;

    std::string out;

    int attrs = typ->GetAttrs();
    bool isOut = (attrs & OUT) != 0;
    bool isIn = (attrs & IN) != 0;

    if (typ->IsByRef()) {
        if (isOut & isIn)
            out += "ref ";

        else if (isOut)
            out += "out ";

        else if (isIn)
            out += "in ";
            
    } else {
        if (isIn)
            out += "[In] ";
        if (isOut)
            out += "[Out] ";
    }
    
    return out;
}

std::vector<Object*> ObjListToVector(Object* obj) {
    auto klass = obj->GetClass();

    auto getCount = FindMethodI(klass, "get_Count", "System.Int32", {});
    if (!getCount) return {};

    auto getItem = FindMethodI(klass, "get_Item", "", {"System.Int32"});
    if (!getItem) return {};

    getCount = obj->GetVirtualMethod(getCount);
    getItem = obj->GetVirtualMethod(getItem);

    if (!getCount) {
        Log::Warn("get_Count is null for {}", FormatTypeName(klass->GetType()));
        return {};
    }
    if (!getItem) {
        Log::Warn("get_Item is null for {}", FormatTypeName(klass->GetType()));
        return {};
    }

    auto countObj = getCount->Invoke(obj);
    if (!countObj)
        return {};

    int count = *reinterpret_cast<int*>(countObj->Unbox());

    std::vector<Object*> vec;
    vec.reserve(count);

    for (int i = 0; i < count; i++) {
        auto v = getItem->Invoke(obj, i);
        vec.push_back(v);
    }

    return vec;
}

void InitReflection(Image* corlib) {
    auto objC = corlib->FindClass("System", "Object");
    if (!objC) {
        Log::Warn("class object not found");
    } else {
        structBoxSize = objC->GetInstanceSize();
        Log::Debug("object box size: 0x{:X}", structBoxSize);
    }

    typeC = corlib->FindClass("System", "Type");
    if (!typeC) {
        Log::Warn("class Type not found");
    } else {
        tGetGenericArgs = FindMethod(typeC, "GetGenericArguments", "System.Type[]", {});
        if (!tGetGenericArgs)
            Log::Warn("method Type.GetGenericArguments not found");

        tGetField = FindMethod(typeC, "GetField", "System.Reflection.FieldInfo", {"System.String"});
        if (!tGetField)
            Log::Warn("method Type.GetField not found");

        tGetProp = FindMethod(typeC, "GetProperty", "System.Reflection.PropertyInfo", {"System.String"});
        if (!tGetProp)
            Log::Warn("method Type.GetProperty not found");
    }

    auto methodC = corlib->FindClass("System.Reflection", "MethodBase");
    if (!methodC) {
        Log::Warn("MethodBase not found");
    } else {
        mGetGenericArgs = FindMethod(methodC, "GetGenericArguments", "System.Type[]", {});
        if (!tGetGenericArgs)
            Log::Warn("method MethodBase.GetGenericArguments not found");
    }

    auto arrC = corlib->FindClass("System", "Array");
    if (!arrC) {
        Log::Warn("System.Array not found");
    } else {
        arrGetValue = FindMethod(arrC, "GetValue", "System.Object", {"System.Int32"});
        if (!arrGetValue)
            Log::Warn("method Array.GetValue not found");
    }

    auto ueImg = FindImage("UnityEngine.CoreModule.dll");
    if (!ueImg) {
        Log::Warn("UnityEngine.CoreModule.dll not found");
    } else {
        auto c = ueImg->FindClass("UnityEngine", "Application");
        if (!c) {
            Log::Warn("Application not found");
        } else {
            getUnityVer = FindMethod(c, "get_unityVersion", "System.String", {});
            if (!getUnityVer) {
                Log::Warn("UnityEngine.get_unityVersion not found");
            }
        }
    }
}

void InitReflectionAtt(Image* corlib) {
    auto attC = corlib->FindClass("System.Reflection", "CustomAttributeData");
    if (!attC) {
        Log::Warn("class CustomAttributeData not found");
    } else {
        attM.getAttrs = FindMethod(
            attC,
            "GetCustomAttributes",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
            {"System.Reflection.MemberInfo"}
        );
        if (!attM.getAttrs)
            Log::Warn("method CustomAttributeData.GetCustomAttributes not found");
        
        attM.getAttType = FindMethod(
            attC,
            "get_AttributeType",
            "System.Type",
            {}
        );
        if (!attM.getAttType)
            Log::Warn("method CustomAttributeData.get_AttributeType not found");

        attM.getConstructorArgs = FindMethod(
            attC,
            "get_ConstructorArguments",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeTypedArgument>",
            {}
        );
        if (!attM.getConstructorArgs)
            Log::Warn("method CustomAttributeData.get_ConstructorArguments not found");

        attM.getNamedArgs = FindMethod(
            attC,
            "get_NamedArguments",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeNamedArgument>",
            {}
        );
        if (!attM.getNamedArgs)
            Log::Warn("method CustomAttributeData.get_NamedArguments not found");
    }

    auto typedArgC = corlib->FindClass("System.Reflection", "CustomAttributeTypedArgument");
    if (!typedArgC) {
        Log::Warn("method CustomAttributeTypedArgument not found");
    } else {
        attTypedArgM.getValue = FindMethod(
            typedArgC,
            "get_Value",
            "System.Object",
            {}
        );
        if (!attTypedArgM.getValue)
            Log::Warn("method CustomAttributeTypedArgument.get_Value not found");
    }

    auto namedArgC = corlib->FindClass("System.Reflection", "CustomAttributeNamedArgument");
    if (!namedArgC) {
        Log::Warn("class CustomAttributeNamedArgument not found");
    } else {
        attNamedArgM.getName = FindMethod(
            namedArgC,
            "get_MemberName",
            "System.String",
            {}
        );
        if (!attNamedArgM.getName)
            Log::Warn("method CustomAttributeNamedArgument.get_MemberName not found");

        attNamedArgM.getTypedValue = FindMethod(
            namedArgC,
            "get_TypedValue",
            "System.Reflection.CustomAttributeTypedArgument",
            {}
        );
        if (!attNamedArgM.getTypedValue)
            Log::Warn("method CustomAttributeNamedArgument.get_TypedValue not found");
    }
}

std::string FormalStringLiteral(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (charEscapes.contains(c)) {
            result += charEscapes.at(c);
            continue;
        }
        if (std::iscntrl(c)) {
            result += "\\x" + std::to_string(static_cast<int>(static_cast<unsigned char>(c)));
            continue;
        }
        if (c == '"') {
            result += "\\\"";
            continue;
        }
        result += c;
    }
    return "\"" + result + "\"";
}

template <std::integral T>
std::string FormatInteger(T value) {
    return std::format("{:d}", value);
}

template <std::floating_point T>
std::string FormatFloat(T value) {
    auto s = std::format("{}", value);
    if (!s.empty() &&
        std::all_of(s.begin() + (s.front() == '-' ? 1 : 0), s.end(), [](unsigned char c) {
            return std::isdigit(c);
        })) {
        s += ".0";
    }
    return s;
}

template <std::integral T>
std::string FormatOffset(T value) {
    if (value < 0) {
        using U = std::make_unsigned_t<T>;
        const auto magnitude = U{0} - static_cast<U>(value);
        return std::format("-0x{:X}", magnitude);
    }
    return std::format("0x{:X}", value);
}

std::string FormatClassSpecial(Class* klass) {
    // using namespace TypeAttributes;
    // int flags = klass->GetFlags();
    // if (flags & SPECIAL_NAME) {
    //     return " /* ★ */";
    // }
    return "";
}

std::string FormatMethodSpecial(Method* met) {
    // using namespace MethodAttributes;
    // int flags = met->GetFlags();
    // if (flags & SPECIAL_NAME) {
    //     return " /* ★ */";
    // }
    return "";
}

std::string FormatFieldSpecial(Field* fld) {
    // using namespace FieldAttributes;
    // int flags = fld->GetFlags();
    // if (flags & SPECIAL_NAME) {
    //     return " /* ★ */";
    // }
    return "";
}

std::string FormatFieldValue(Field* fld) {
    using namespace FieldAttributes;
    using namespace TypeEnum;

    int flags = fld->GetFlags();
    bool isConst = (flags & LITERAL) != 0;

    if (!isConst) return "";

    auto type = fld->GetType();
    auto klass = Class::FromType(type);
    if (klass->IsEnum()) {
        type = klass->GetEnumBaseType();
    }

    std::string out;

    auto appendInteger = [&]<std::integral T>() {
        T v = fld->GetStaticValue<T>();
        out += FormatInteger<T>(v);
    };

    auto appendFloat = [&]<std::floating_point T>() {
        T v = fld->GetStaticValue<T>();
        out += FormatFloat<T>(v);
    };

    switch (type->GetTypeEnum()) {
        case TypeEnum::CHAR: {
            uint16_t v = fld->GetStaticValue<uint16_t>();
            
            out += "'";
            if (std::iscntrl(v)) {
                out += "\\x" + std::format("{:X}", v);
            } else if (v == '\'') {
                out += "\\'";
            } else {
                out += std::string(1, v);
            }
            out += "'";
        } break;

        case TypeEnum::BOOLEAN: {
            bool value = fld->GetStaticValue<bool>();
            out += value ? "true" : "false";
        } break;

        case TypeEnum::STRING: {
            auto* value = fld->GetStaticValue<String*>();
            if (!value) out += "null";

            auto u16s = value->GetChars();
            auto s = U16ToU8(u16s);
            out += FormalStringLiteral(s);
        } break;

        case TypeEnum::CLASS:
        case TypeEnum::OBJECT: {
            auto* value = fld->GetStaticValue<Object*>();
            if (!value) out += "null";
        } break;

        case TypeEnum::I1: appendInteger.operator()<int8_t>(); break;
        case TypeEnum::U1: appendInteger.operator()<uint8_t>(); break;
        case TypeEnum::I2: appendInteger.operator()<int16_t>(); break;
        case TypeEnum::U2: appendInteger.operator()<uint16_t>(); break;
        case TypeEnum::I4: appendInteger.operator()<int32_t>(); break;
        case TypeEnum::U4: appendInteger.operator()<uint32_t>(); break;
        case TypeEnum::I8: appendInteger.operator()<int64_t>(); break;
        case TypeEnum::U8: appendInteger.operator()<uint64_t>(); break;
        case TypeEnum::I:  appendInteger.operator()<intptr_t>(); break;
        case TypeEnum::U:  appendInteger.operator()<uintptr_t>(); break;
        case TypeEnum::R4: appendFloat.operator()<float>(); break;
        case TypeEnum::R8: appendFloat.operator()<double>(); break;
    }

    if (!out.empty()) {
        out = " = " + out;
    }

    return out;
}

std::string FormatObject(Object* obj) {
    using namespace TypeEnum;

    if (!obj) return "null";

    auto klass = obj->GetClass();
    auto type = klass->GetType();

    std::string out;

    auto appendInteger = [&]<std::integral T>() {
        T v = *(T*) obj->Unbox();
        out += FormatInteger<T>(v);
    };

    auto appendFloat = [&]<std::floating_point T>() {
        T v = *(T*) obj->Unbox();
        out += FormatFloat<T>(v);
    };

    switch (type->GetTypeEnum()) {
        case TypeEnum::CHAR: {
            uint16_t v = *(uint16_t*) obj->Unbox();;
            
            out += "'";
            if (std::iscntrl(v))
                out += "\\x" + std::format("{:X}", v);
            else if (v == '\'')
                out += "\\'";
            else
                out += std::string(1, v);
            
            out += "'";
        } break;

        case TypeEnum::BOOLEAN: {
            bool value = *(bool*) obj->Unbox();;
            out += value ? "true" : "false";
        } break;

        case TypeEnum::STRING: {
            auto* value = (String*) obj;
            auto u16s = value->GetChars();
            auto s = U16ToU8(u16s);
            out += FormalStringLiteral(s);
        } break;

        case TypeEnum::I1: appendInteger.operator()<int8_t>(); break;
        case TypeEnum::U1: appendInteger.operator()<uint8_t>(); break;
        case TypeEnum::I2: appendInteger.operator()<int16_t>(); break;
        case TypeEnum::U2: appendInteger.operator()<uint16_t>(); break;
        case TypeEnum::I4: appendInteger.operator()<int32_t>(); break;
        case TypeEnum::U4: appendInteger.operator()<uint32_t>(); break;
        case TypeEnum::I8: appendInteger.operator()<int64_t>(); break;
        case TypeEnum::U8: appendInteger.operator()<uint64_t>(); break;
        case TypeEnum::I:  appendInteger.operator()<intptr_t>(); break;
        case TypeEnum::U:  appendInteger.operator()<uintptr_t>(); break;
        case TypeEnum::R4: appendFloat.operator()<float>(); break;
        case TypeEnum::R8: appendFloat.operator()<double>(); break;
        case TypeEnum::CLASS: {
            if (typeC && typeC->IsAssignableFrom(obj->GetClass())) {
                auto tName = Class::FromSystemType(obj)->GetType();
                return "typeof(" + FormatTypeName(tName) + ")";
            }
        } break;
    }

    if (out.empty())
        return "?";

    return out;
}

void DoAttr(Object* memberInfo, std::ofstream& out, int ind = 1) {
    if (!memberInfo) return;

    if (!attM.getAttrs ||
        !attM.getAttType || !attM.getConstructorArgs || !attM.getNamedArgs ||
        !attTypedArgM.getValue || !attNamedArgM.getTypedValue ||
        !attNamedArgM.getName
    ) return;

    Object* attrs = attM.getAttrs->Invoke(nullptr, memberInfo);
    if (!attrs) return;
    
    auto attrsV = ObjListToVector(attrs);
    for (Object* attr : attrsV) {
        if (!attr) continue;

        Object* attTypeO = attM.getAttType->Invoke(attr);
        if (!attTypeO) continue;

        auto attC = Class::FromSystemType(attTypeO);
        if (!attC) continue;
        
        auto attTypeName = attC->GetName();
        RemoveSuffix(attTypeName, "Attribute");

        for (int i = 0; i < ind; i++) {
            out << INDENT;
        }
        out << "[";
        out << attTypeName;

        Object* constrs = attM.getConstructorArgs->Invoke(attr);
        if (!constrs) continue;

        Object* nameds = attM.getNamedArgs->Invoke(attr);
        if (!nameds) continue;

        auto constrsV = ObjListToVector(constrs);
        auto namedsV = ObjListToVector(nameds);

        std::vector<std::string> argStrs;
        for (Object* typed : constrsV) {
            if (!typed) continue;

            void* typedUnbox = typed->Unbox();
            if (!typedUnbox) continue;

            Object* v = attTypedArgM.getValue->Invoke(typedUnbox);
            argStrs.push_back(FormatObject(v));
        }
        for (Object* named : namedsV) {
            if (!named) continue;

            void* namedUnbox = named->Unbox();
            if (!namedUnbox) continue;

            String* memberNameS = (String*) attNamedArgM.getName->Invoke(namedUnbox);
            auto memberName = memberNameS ? U16ToU8(memberNameS->GetChars()) : "null";

            Object* typed = attNamedArgM.getTypedValue->Invoke(namedUnbox);
            if (!typed) continue;

            void* typedUnbox = typed->Unbox();
            if (!typedUnbox) continue;

            Object* v = attTypedArgM.getValue->Invoke(typedUnbox);
            argStrs.push_back(memberName + " = " + FormatObject(v));
        }

        if (!argStrs.empty()) {
            out << "(";
            out << JoinStr(argStrs, ", ");
            out << ")";
        }
        out << "]";
        out << "\n";
    }
}

void DoClass(Class* klass, LibResolver& libRes, std::ofstream& out) {
    out << "// Namespace: ";
    out << klass->GetNamespace();
    out << "\n";

    std::vector<std::string> extNames;

    auto type = klass->GetType();
    bool isValueType = klass->IsValueType();
    bool isEnum = klass->IsEnum();

    auto pKlass = klass->GetParent();
    if (isEnum) {
        pKlass = Class::FromType(klass->GetEnumBaseType());

    } else if (isValueType) {
        pKlass = nullptr;

    } else if (pKlass) {
        auto pType = pKlass->GetType();
        if (pType->GetTypeEnum() == TypeEnum::OBJECT)
            pKlass = nullptr;
    }
    
    if (pKlass) {
        extNames.push_back(FormatTypeName(pKlass->GetType()));
    }

    auto ifaces = klass->GetInterfaces();
    for (int i = 0; i < ifaces.size(); i++) {
        extNames.push_back(FormatTypeName(ifaces[i]->GetType()));
    }

    std::string ext;
    if (!extNames.empty()) {
        ext = " : " + JoinStr(extNames, ", ");
    }

    Object* to = type->GetObject();
    DoAttr(to, out, 0);

    out << FormatTypeAccessor(klass) + FormatTypeType(klass) + FormatTypeName(klass->GetType(), true) + ext + " {" + FormatClassSpecial(klass);
    out << "\n";

    auto fields = klass->GetFields();
    for (auto f : fields) {
        if (tGetField) {
            auto fName = f->GetName();
            auto fNameS = String::New(fName.c_str());
            auto gf = to->GetVirtualMethod(tGetField);
            auto fo = gf->Invoke(to, fNameS);
            if (fo) {
                DoAttr(fo, out);
            }
        }

        auto fType = f->GetType();
        auto fFlag = f->GetFlags();
        bool isConst = fFlag & FieldAttributes::LITERAL;
        bool isStatic = fFlag & FieldAttributes::STATIC;
        if (isEnum && !isConst) continue;

        out << INDENT;
        if (!isEnum) out << FormatFieldAccessor(f);
        if (!isEnum) out << FormatTypeName(fType);
        if (!isEnum) out << " ";
        out << f->GetName();
        out << FormatFieldValue(f);
        out << (!isEnum ? ";" : ",");

        if (!isConst && !isStatic) {
            int off = f->GetOffset();
            int pad = isValueType ? structBoxSize : 0;
            if (off <= 0) {
                pad = 0;
                off = -1;
            }

            out << " // ";
            out << FormatOffset(off - pad);
        }

        out << FormatFieldSpecial(f);
        out << "\n";
    }

    auto props = klass->GetProperties();
    if (!fields.empty() && !props.empty())
        out << "\n";
    
    for (auto p : props) {
        if (tGetField) {
            auto pName = p->GetName();
            auto pNameS = String::New(pName.c_str());
            auto gf = to->GetVirtualMethod(tGetField);
            auto po = gf->Invoke(to, pNameS);
            if (po) {
                DoAttr(po, out);
            }
        }

        auto fGet = p->GetGetMethod();
        auto fSet = p->GetSetMethod();

        auto fEith = fGet ? fGet : fSet;
        if (!fEith) continue;

        out << INDENT;
        out << FormatMethodAccessor(fEith) + FormatTypeName(fEith->GetReturnType()) + " " + p->GetName();
        out << " { ";
        if (fGet) out << "get; ";
        if (fSet) out << "set; ";
        out << "}";
        out << "\n";
    }

    auto methods = klass->GetMethods();
    for (auto m : methods) {
        auto mName = m->GetName();
        auto rt = m->GetReturnType();
        bool isGeneric = m->IsGeneric();
        bool isInflated = m->IsInflated();
        bool isAbstract = (m->GetFlags() & MethodAttributes::ABSTRACT) != 0;

        std::vector<std::string> parFmts;

        auto count = m->GetParamCount();
        for (int i = 0; i < count; i++) {
            auto pt = m->GetParam(i);
            auto pn = m->GetParamName(i);

            parFmts.push_back(FormatParamType(pt) + FormatTypeName(pt) + " " + pn);
        }

        intptr_t va = -1;
        intptr_t rva = -1;
        intptr_t off = -1;

        if (!isInflated && !isGeneric) {
            va = (uintptr_t) m->GetPointer();
            if (va != UINTPTR_MAX && va != 0) {
                va = NormalizePointer(va);
                rva = va - libBase;
                off = libRes.RvaToOffset(rva);
            }
        }

        out << "\n";

        out << INDENT;
        out << std::format("// RVA: {} Offset: {} VA: {}", FormatOffset(rva), FormatOffset(off), FormatOffset(rva));
        out << "\n";

        DoAttr(m->GetObject(), out);

        out << INDENT;
        out << FormatMethodAccessor(m);
        out << FormatTypeName(rt);
        out << " ";
        out << mName;
        
        if ((isInflated || isGeneric) && arrGetValue && mGetGenericArgs) {
            auto to = m->GetObject();
            auto getGensV = to->GetVirtualMethod(mGetGenericArgs);
            auto args = (Array*) getGensV->Invoke(to);
            int len = args->GetLength();

            std::vector<std::string> pNames;
            for (int i = 0; i < len; i++) {
                auto p = arrGetValue->Invoke(args, i);
                auto pt = Class::FromSystemType(p);

                pNames.push_back(FormatTypeName(pt->GetType()));
            }
            
            out << "<";
            out << JoinStr(pNames, ", ");
            out << ">";
        }

        out << "(";
        out << JoinStr(parFmts, ", ");
        out << ")";
        out << (isAbstract ? ";" : " { }");
        out << FormatMethodSpecial(m);
        out << "\n";
    }

    out << "}";
    out << "\n";
    out << "\n";
}

void RunDumper() {
    using namespace Api;

    Log::Debug("running...");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto extDirS = GetAppExtDir();
    if (extDirS.empty()) {
        Log::Error("cannot obtain external files dir");
        return;
    }
    Log::Debug("obtained external files dir: {}", extDirS);

    std::filesystem::path extDir(extDirS);

    std::map<std::string, std::string> symMap;

    auto mapFileP = extDir / "symbol_map.txt";

    if (std::filesystem::exists(mapFileP)) {
        Log::Debug("symbol_map.txt found");

        std::ifstream file(mapFileP);

        std::string line;
        while (std::getline(file, line)) {
            auto pos = line.find(' ');
            if (pos == std::string::npos)
                continue;

            auto key   = line.substr(0, pos);
            auto value = line.substr(pos + 1);

            symMap[key] = value;
        }

        Log::Debug("loaded {:d} keys", symMap.size());
    }

    auto libRes = LibResolver();
    auto res = SymApiResolver(symMap);

    bool init = InitApi(libRes, res);
    if (!init) {
        Log::Error("initialization failed");
        return;
    }

    libBase = libRes.GetBaseAddress();

    Domain* dmn = Domain::GetInstance();
    if (!dmn) {
        Log::Error("cannot obtain domain");
        return;
    }
    Log::Debug("domain: 0x{:X}", (uintptr_t) dmn);

    Thread* thr = Thread::Attach(dmn);
    if (!thr) {
        Log::Error("cannot attach thread");
        return;
    }
    Log::Debug("thread: 0x{:X}", (uintptr_t) thr);

    Image* corlib = Image::GetCorlib();
    if (!corlib) {
        Log::Error("cannot obtain mscorlib.dll");
    } else {
        Log::Debug("corlib: 0x{:X}", (uintptr_t) corlib);

        InitReflection(corlib);
        InitReflectionAtt(corlib);
    }

    if (getUnityVer) {
        auto verS = (String*) getUnityVer->Invoke(nullptr);
        auto ver = verS ? U16ToU8(verS->GetChars()) : "null";
        Log::Debug("unity version: {}", ver);
    }

    auto nm = corlib->GetName();
    Log::Debug("corlib: {}", nm);

    auto asms = dmn->GetAssemblies();
    Log::Debug("assemblies: {:d}", asms.size());

    std::filesystem::path dir = std::filesystem::path(extDirS) / "runtime-dumper";
    if (std::filesystem::exists(dir)) {
        std::filesystem::remove_all(dir);
        Log::Debug("cleaned dir");
    }
    
    if (!std::filesystem::create_directories(dir)) {
        Log::Debug("cannot create dir, {}", dir.c_str());
        return;
    }

    Log::Debug("dumping...");
    auto start = std::chrono::steady_clock::now();

    std::ofstream out(dir / "dump.cs", std::ios::trunc);

    for (auto a : asms) {
        Image* img = a->GetImage();
        
        out << "// Assembly: ";
        out << img->GetName();
        out << "\n";
    }
    out << "\n";
    out << "\n";

    for (auto a : asms) {
        Image* img = a->GetImage();

        auto klasses = img->GetClasses();
        for (auto k : klasses) {
            DoClass(k, libRes, out);
        }

        out.flush();
    }
    out.close();

    auto end = std::chrono::steady_clock::now();
    float durSec = std::chrono::duration<float>(end - start).count();
    Log::Debug("dump success ({:.3f}s)", durSec);
    Log::Debug("output dir: {}", dir.c_str());

    Log::Debug("detaching il2cpp...");
    thr->Detach();

    Log::Debug("all finished!");
}