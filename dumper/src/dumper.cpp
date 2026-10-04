#include <chrono>
#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
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
#include "utils/str_util.h"
#include "resolver.h"
#include "log.h"
#include "finder.h"
#include "formats.h"

using namespace Api;
namespace fs = std::filesystem;


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


uintptr_t libBase = 0;
size_t structBoxSize = 0;

struct {
    struct {
        Method* get_unityVersion{};
    } UnityEngine;
    struct {
        Method* GetValue;
    } Array;
    struct {
        Method* get_Count{};
    } ICollection;
    struct {
        Class* klass{};
        Method* get_Item{};
    } IList;
    struct {
        Class* klass{};
        Method* GetGenericArguments{};
        Method* GetField{};
        Method* GetProperty{};
    } Type;
    struct {
        Method* GetGenericArguments{};
        Method* GetParameters{};
    } MethodBase;
    struct {
        Method* get_Name{};
    } MemberInfo;
    struct {
        Method* GetCustomAttributesM{};
        Method* GetCustomAttributesP{};
        Method* get_AttributeType{};
        Method* get_ConstructorArguments{};
        Method* get_NamedArguments{};
    } CustomAttributeData;
    struct {
        Method* get_MemberName{};
        Method* get_MemberInfo{};
        Method* get_TypedValue{};
    } CustomAttributeNamedArgument;
    struct {
        Class* klass{};
        Method* get_Value{};
    } CustomAttributeTypedArgument;
    struct ParameterInfo {
        Method* get_Attributes{};
        Method* get_Name{};
        Method* get_Type{};
        Method* get_DefaultValue{};
    } ParameterInfo;
} refl;


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

std::string FormatTypeName(Type* t, bool forDefinition = false, bool forTypeof = false) {
    if (!t) return "?";

    auto klass = Class::FromType(t);
    auto tp = t->GetTypeEnum();

    std::string res;

    auto outer = klass->GetDeclaringClass();
    if (outer)
        res += FormatTypeName(outer->GetType(), forDefinition) + ".";
    
    if (typeSuffixes.contains(tp)) {
        auto ec = klass->GetElementClass();
        if (ec && ec != klass)
            return res + FormatTypeName(ec->GetType(), forDefinition) + typeSuffixes.at(tp);
    }
    
    if (!forDefinition && typeAliases.contains(tp)) {
        return typeAliases.at(tp);
    }

    auto name = klass->GetName();
    auto isGeneric = klass->IsGeneric();
    auto isInflated = klass->IsInflated();

    if (refl.Array.GetValue && refl.Type.GetGenericArguments && (isGeneric || isInflated)) {
        auto to = t->GetObject();
        if (to) {
            auto getGensV = to->GetVirtualMethod(refl.Type.GetGenericArguments);
            if (getGensV) {
                auto args = (Array*) getGensV->Invoke(to);
                if (args) {
                    std::string pureName = StrUtil::SubBefore(name, "`");

                    int len = args->GetLength();

                    if (!isInflated && forTypeof)
                        return StrUtil::SubBefore(name, "`") + "<" + StrUtil::Repeat(",", len - 1) +  ">";

                    std::vector<std::string> ps;
                    for (int i = 0; i < len; i++) {
                        auto p = refl.Array.GetValue->Invoke(args, i);
                        auto pt = Class::FromSystemType(p);
                        auto ptC = pt->GetType();
                        ps.push_back(FormatTypeName(ptC, forDefinition));
                    }
                    
                    return pureName + "<" + StrUtil::Join(ps, ", ") + ">";
                }
            }
        }
    }

    return res + name;
}

std::string FormatTypeType(Class* klass) {
    bool isValueType = klass->IsValueType();
    bool isEnum = klass->IsEnum();
    int flags = klass->GetFlags();
    
    using namespace TypeAttributes;

    std::string out;

    bool isAbstract  = (flags & ABSTRACT) != 0;
    bool isFinal     = (flags & SEALED) != 0;
    bool isInterface = (flags & INTERFACE) != 0;

    if (isAbstract && isFinal)
        out += "static ";
    else if (isInterface && isAbstract)
        out += "abstract ";
    else if (!isValueType && !isEnum && isFinal)
        out += "sealed ";

    if (flags & INTERFACE)
        out += "interface ";
    else if (isEnum)
        out += "enum ";
    else if (isValueType)
        out += "struct ";
    else
        out += "class ";

    return out;
}

std::string FormatTypeAccessor(Class* klass) {
    int flags = klass->GetFlags();

    using namespace TypeAttributes;

    const static std::map<int, std::string> accessMap = {
        {PUBLIC,               "public"},
        {NESTED_PUBLIC,        "public"},
        {NOT_PUBLIC,           "internal"},
        {NESTED_FAM_AND_ASSEM, "private protected"},
        {NESTED_ASSEMBLY,      "internal"},
        {NESTED_PRIVATE,       "private"},
        {NESTED_FAMILY,        "protected"},
        {NESTED_FAM_OR_ASSEM,  "protected internal"},
    };

    std::string outl;

    int acc = flags & VISIBILITY_MASK;
    if (accessMap.contains(acc)) {
        return accessMap.at(acc) + " ";
    }

    return "";
}

std::string FormatMethodAccessor(Method* met) {
    int flags = met->GetFlags();
    
    using namespace MethodAttributes;

    int acc = flags & MEMBER_ACCESS_MASK;

    const static std::map<int, std::string> accessMap = {
        {PUBLIC,         "public"},
        {PRIVATE,        "private"},
        {FAM_AND_ASSEM,  "private protected"},
        {FAM_OR_ASSEM,   "protected internal"},
        {ASSEM,          "internal"},
        {FAMILY,         "protected"}
    };

    bool isReuse    = (flags & VTABLE_LAYOUT_MASK) == REUSE_SLOT;
    bool isStatic   = (flags & STATIC)       != 0;
    bool isAbstract = (flags & ABSTRACT)     != 0;
    bool isVirtual  = (flags & VIRTUAL)      != 0;
    bool isFinal    = (flags & FINAL)        != 0;
    bool isExtern   = (flags & PINVOKE_IMPL) != 0;

    std::string out;

    auto it = accessMap.find(acc);
    if (it != accessMap.end()) {
        out += it->second + " ";
    }

    bool renderAbstract = isAbstract;
    bool renderSealed   = isReuse   &&  isFinal && !isAbstract;
    bool renderVirtual  = isVirtual && !isReuse && !isAbstract;
    bool renderOverride = isVirtual &&  isReuse && !isAbstract;
    bool renderStatic   = isStatic;
    bool renderExtern   = isExtern;

    if (renderAbstract) out += "abstract ";
    if (renderSealed)   out += "sealed ";
    if (renderVirtual)  out += "virtual ";
    if (renderOverride) out += "override ";
    if (renderStatic)   out += "static ";
    if (renderExtern)   out += "extern ";

    return out;
}

std::string FormatFieldAccessor(Field* fld) {
    using namespace FieldAttributes;

    std::string out;

    int flags = fld->GetFlags();
    int acc = flags & FIELD_ACCESS_MASK;

    const static std::map<int, std::string> accessMap = {
        {FieldAttributes::PUBLIC,        "public"},
        {FieldAttributes::PRIVATE,       "private"},
        {FieldAttributes::FAMILY,        "protected"},
        {FieldAttributes::ASSEMBLY,      "internal"},
        {FieldAttributes::FAM_OR_ASSEM,  "protected internal"},
        {FieldAttributes::FAM_AND_ASSEM, "private protected"},
    };
    
    bool isConst    = (flags & LITERAL)   != 0;
    bool isStatic   = (flags & STATIC)    != 0;
    bool isReadonly = (flags & INIT_ONLY) != 0;

    if (accessMap.contains(acc)) {
        out += accessMap.at(acc);
        out += " ";
    }

    bool renderConst    = isConst;
    bool renderStatic   = isStatic   && !isConst;
    bool renderReadonly = isReadonly && !isStatic;
    
    if (renderConst)    out += "const ";
    if (renderStatic)   out += "static ";
    if (renderReadonly) out += "readonly ";

    return out;
}

std::string FormatParamType(Type* typ) {
    int attrs = typ->GetAttrs();
    bool isByRef = typ->IsByRef();

    using namespace ParameterAttributes;

    std::string out;

    bool isOut = (attrs & OUT) != 0;
    bool isIn  = (attrs & IN)  != 0;

    if (isByRef) {
        if (isOut & isIn)
            out += "ref ";
        else if (isOut)
            out += "out ";
        else if (isIn)
            out += "in ";
            
    } else {
        if (isIn)  out += "[In] ";
        if (isOut) out += "[Out] ";
    }
    
    return out;
}

void ObjListToVector(Object* obj, std::string_view tName, std::function<void(Object* v)> iter) {
    Class* klass = obj->GetClass();

    Method* vGetCount = Finder::FindMethodI(klass, "get_Count", "System.Int32", {});
    if (!vGetCount) return;

    Method* vGetItem = Finder::FindMethodI(klass, "get_Item", tName, {"System.Int32"});
    if (!vGetItem) return;

    vGetCount = obj->GetVirtualMethod(vGetCount);
    vGetItem = obj->GetVirtualMethod(vGetItem);

    if (!vGetCount) {
        Log::Warn("get_Count is null for {}", FormatTypeName(klass->GetType()));
        return;
    }
    if (!vGetItem) {
        Log::Warn("get_Item is null for {}", FormatTypeName(klass->GetType()));
        return;
    }

    Object* countBox = vGetCount->Invoke(obj);
    if (!countBox)
        return;

    int count = *(int*) countBox->Unbox();
    for (int i = 0; i < count; i++) {
        Object* v = vGetItem->Invoke(obj, i);
        iter(v);
    }
}

template <typename Getter>
std::string FormatObjectCustom(int typeEnum, Getter&& getter) {
    using namespace TypeEnum;

    std::string out;

    auto appendInteger = [&]<std::integral T>() {
        T v = getter.template operator()<T, true>();
        out += Formats::FormatInteger<T>(v);
    };

    auto appendFloat = [&]<std::floating_point T>() {
        T v = getter.template operator()<T, true>();
        out += Formats::FormatFloat<T>(v);
    };

    switch (typeEnum) {
        case CHAR: {
            uint16_t v = getter.template operator()<uint16_t, true>();
            out += Formats::FormatCharLiteral(v);
        } break;

        case BOOLEAN: {
            bool value = getter.template operator()<bool, true>();
            out += value ? "true" : "false";
        } break;

        case STRING: {
            String* value = getter.template operator()<String*, false>();

            if (!value)
                return "null";

            auto u16s = value->GetChars();
            std::string s = StrUtil::U16ToU8(u16s);
            out += Formats::FormalStringLiteral(s);
        } break;

        case I1: appendInteger.template operator()<int8_t>(); break;
        case U1: appendInteger.template operator()<uint8_t>(); break;
        case I2: appendInteger.template operator()<int16_t>(); break;
        case U2: appendInteger.template operator()<uint16_t>(); break;
        case I4: appendInteger.template operator()<int32_t>(); break;
        case U4: appendInteger.template operator()<uint32_t>(); break;
        case I8: appendInteger.template operator()<int64_t>(); break;
        case U8: appendInteger.template operator()<uint64_t>(); break;
        case I:  appendInteger.template operator()<intptr_t>(); break;
        case U:  appendInteger.template operator()<uintptr_t>(); break;
        case R4: appendFloat.template operator()<float>(); break;
        case R8: appendFloat.template operator()<double>(); break;
    }

    if (out.empty())
        return "?";

    return out;
}

std::string FormatObject(Object* obj) {
    using namespace TypeEnum;

    if (!obj)
        return "null";

    auto klass = obj->GetClass();
    if (!klass) return "?";

    auto type = klass->GetType();
    if (!type) return "?";

    if (refl.Type.klass && refl.Type.klass->IsAssignableFrom(klass)) {
        auto tName = Class::FromSystemType(obj)->GetType();
        return "typeof(" + FormatTypeName(tName, false, true) + ")";
    }

    if (klass->IsEnum())
        type = klass->GetEnumBaseType();

    return FormatObjectCustom(
        type->GetTypeEnum(),
        [&]<typename T, bool unboxed>() -> T {
            if constexpr (!unboxed) {
                return (T) obj;
            }
            return *(T*) obj->Unbox();
        }
    );
}

std::string FormatFieldValue(Field* fld) {
    using namespace FieldAttributes;

    int flags = fld->GetFlags();
    bool isConst = (flags & LITERAL) != 0;

    if (!isConst)
        return "";

    auto type = fld->GetType();
    auto klass = Class::FromType(type);

    if (klass->IsEnum())
        type = klass->GetEnumBaseType();

    return " = " + FormatObjectCustom(
        type->GetTypeEnum(),
        [&]<typename T, bool>() -> T {
            return fld->GetStaticValue<T>();
        }
    );
}

std::string FormatAttrTyped(Object* typed) {
    if (!typed) return "?";

    void* typedUnbox = typed->Unbox();
    if (!typedUnbox) return "?";

    Object* v = refl.CustomAttributeTypedArgument.get_Value->Invoke(typedUnbox);
    if (!v) return "null";

    Class* klass = v->GetClass();
    if (!klass) return "?";

    if (refl.IList.klass && refl.IList.get_Item && refl.ICollection.get_Count && refl.IList.klass->IsAssignableFrom(klass)) {
        Method* vGetItem = v->GetVirtualMethod(refl.IList.get_Item);
        if (!vGetItem) return "?";

        Method* vGetCount = v->GetVirtualMethod(refl.ICollection.get_Count);
        if (!vGetCount) return "?";

        Object* countO = vGetCount->Invoke(v);
        if (!countO) return "?";

        int count = *(int*) countO->Unbox();

        std::vector<std::string> valueFmts;
        for (int i = 0; i < count; i++) {
            Object* vv = vGetItem->Invoke(v, i);
            Class* k = vv ? vv->GetClass() : nullptr;
            
            if (k && refl.CustomAttributeTypedArgument.klass->IsAssignableFrom(k))
                valueFmts.push_back(FormatAttrTyped(vv));
            else
                valueFmts.push_back(FormatObject(vv));
        }

        return "new[] {" + StrUtil::Join(valueFmts, ", ") + "}";
    };

    return FormatObject(v);
}

bool CanDoAttr() {
    return refl.CustomAttributeData.get_AttributeType &&
        refl.CustomAttributeData.get_ConstructorArguments &&
        refl.CustomAttributeData.get_NamedArguments &&
        refl.CustomAttributeTypedArgument.get_Value &&
        refl.CustomAttributeNamedArgument.get_TypedValue &&
        (refl.CustomAttributeNamedArgument.get_MemberName || (refl.CustomAttributeNamedArgument.get_MemberInfo && refl.MemberInfo.get_Name));
}

void DoAttr(Object* attrs, std::ostream& out, int ind, bool nl, const std::vector<std::pair<std::string, std::string>>& excludes = {}) {
    if (!CanDoAttr()) return;

    ObjListToVector(attrs, "System.Reflection.CustomAttributeData", [&] (Object* attr) {
        if (!attr) return;

        Object* attrTO = refl.CustomAttributeData.get_AttributeType->Invoke(attr);
        if (!attrTO) return;

        auto attrC = Class::FromSystemType(attrTO);
        if (!attrC) return;
        
        auto attrNs = attrC->GetNamespace();
        auto attrTypeName = attrC->GetName();

        if (std::ranges::any_of(excludes, [&](const auto& e) {
            return e.first == attrNs && e.second == attrTypeName;
        })) {
            return;
        }

        StrUtil::EraseSuffix(attrTypeName, "Attribute");

        out << StrUtil::Repeat(INDENT, ind);
        out << "[";
        out << attrTypeName;

        Object* constrsO = refl.CustomAttributeData.get_ConstructorArguments->Invoke(attr);
        Object* namedsO = refl.CustomAttributeData.get_NamedArguments->Invoke(attr);

        std::vector<std::string> argStrs;
        if (constrsO) {
            ObjListToVector(constrsO, "System.Reflection.CustomAttributeTypedArgument", [&] (Object* typed) {
                argStrs.push_back(FormatAttrTyped(typed));
            });
        }
        if (namedsO) {
            ObjListToVector(namedsO, "System.Reflection.CustomAttributeNamedArgument", [&] (Object* named) {
                if (!named) return;

                void* namedUnbox = named->Unbox();
                if (!namedUnbox) return;

                String* memberNameS = [&]() -> String* {
                    if (refl.CustomAttributeNamedArgument.get_MemberName) {
                        return (String*) refl.CustomAttributeNamedArgument.get_MemberName->Invoke(namedUnbox);
                    }

                    Object* info = refl.CustomAttributeNamedArgument.get_MemberInfo->Invoke(namedUnbox);
                    if (!info) return nullptr;

                    Method* vGetName = info->GetVirtualMethod(refl.MemberInfo.get_Name);
                    if (!vGetName) return nullptr;

                    return (String*) vGetName->Invoke(info);
                }();

                auto memberName = memberNameS ? StrUtil::U16ToU8(memberNameS->GetChars()) : "?";

                Object* typed = refl.CustomAttributeNamedArgument.get_TypedValue->Invoke(namedUnbox);
                argStrs.push_back(memberName + " = " + FormatAttrTyped(typed));
            });
        }

        if (!argStrs.empty()) {
            out << "(";
            out << StrUtil::Join(argStrs, ", ");
            out << ")";
        }
        out << "]";
        out << (nl ? "\n" : " ");
    });
}

void DoMemberForAttr(Object* memberInfo, std::ostream& out, int ind = 1) {
    if (!memberInfo) return;
    if (!CanDoAttr()) return;

    if (!refl.CustomAttributeData.GetCustomAttributesM) return;

    Object* attrs = refl.CustomAttributeData.GetCustomAttributesM->Invoke(nullptr, memberInfo);
    if (!attrs) return;
    
    DoAttr(attrs, out, ind, true);
}

void DoParamAttr(Object* paramInfo, std::ostream& out, const std::vector<std::pair<std::string, std::string>>& excludes = {}) {
    if (!paramInfo) return;
    if (!CanDoAttr()) return;

    Object* attrs = refl.CustomAttributeData.GetCustomAttributesP->Invoke(nullptr, paramInfo);
    if (!attrs) return;

    DoAttr(attrs, out, 0, false, excludes);
}

void DoClass(Class* klass, LibResolver& libRes, std::ofstream& out) {
    out << "// Namespace: ";
    out << klass->GetNamespace();
    out << "\n";

    std::vector<std::string> extNames;

    auto type = klass->GetType();
    if (!type) return;

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

    Object* tO = type->GetObject();
    if (!tO) return;
    DoMemberForAttr(tO, out, 0);

    std::string ext;
    if (!extNames.empty()) {
        ext = " : " + StrUtil::Join(extNames, ", ");
    }

    out << FormatTypeAccessor(klass) + FormatTypeType(klass) + FormatTypeName(klass->GetType(), true) + ext + " {";
    out << "\n";

    auto fields = klass->GetFields();
    for (auto f : fields) {
        if (refl.Type.GetField) [&]{
            auto fName = f->GetName();
            auto fNameS = String::New(fName.c_str());
            auto vGetField = tO->GetVirtualMethod(refl.Type.GetField);
            if (!vGetField) return;

            auto fO = vGetField->Invoke(tO, fNameS);
            DoMemberForAttr(fO, out);
        }();

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
            out << Formats::FormatOffset(off - pad);
        }
        
        out << "\n";
    }

    auto props = klass->GetProperties();
    
    if (!fields.empty() && !props.empty())
        out << "\n";
    
    for (auto p : props) {
        auto pName = p->GetName();

        if (refl.Type.GetField) [&]{
            auto pNameO = String::New(pName.c_str());
            auto vGetProperty = tO->GetVirtualMethod(refl.Type.GetProperty);
            if (!vGetProperty) return;

            auto pO = vGetProperty->Invoke(tO, pNameO, BindingFlags::DECLARED_ONLY);
            DoMemberForAttr(pO, out);
        }();

        auto fGet = p->GetGetMethod();
        auto fSet = p->GetSetMethod();

        auto fEith = fGet ? fGet : fSet;
        if (!fEith) continue;

        out << INDENT;
        out << FormatMethodAccessor(fEith) + FormatTypeName(fEith->GetReturnType()) + " " + pName;
        out << " { ";
        if (fGet) out << "get; ";
        if (fSet) out << "set; ";
        out << "}";
        out << "\n";
    }
    
    auto methods = klass->GetMethods();
    for (auto m : methods) {
        auto mName = m->GetName();
        auto mObj = m->GetObject();
        auto mRT = m->GetReturnType();
        bool isGeneric = m->IsGeneric();
        bool isInflated = m->IsInflated();
        bool isAbstract = (m->GetFlags() & MethodAttributes::ABSTRACT) != 0;

        if (!mObj) continue;
        if (!mRT) continue;

        std::vector<std::string> parFmts;

        if (
            refl.Array.GetValue &&
            refl.MethodBase.GetParameters &&
            refl.ParameterInfo.get_DefaultValue &&
            refl.ParameterInfo.get_Name &&
            refl.ParameterInfo.get_Type &&
            refl.ParameterInfo.get_Attributes
        ) [&]{
            auto vGetParams = mObj->GetVirtualMethod(refl.MethodBase.GetParameters);
            if (!vGetParams) return;

            auto arr = (Array*) vGetParams->Invoke(mObj);
            if (!arr) return;

            int len = arr->GetLength();
            int lenI = m->GetParamCount();
            if (lenI < len) return;

            for (int i = 0; i < len; i++) {
                auto obj = refl.Array.GetValue->Invoke(arr, i);
                if (!obj) continue;

                auto vGetName = obj->GetVirtualMethod(refl.ParameterInfo.get_Name);
                auto vGetType = obj->GetVirtualMethod(refl.ParameterInfo.get_Type);
                auto vGetDefV = obj->GetVirtualMethod(refl.ParameterInfo.get_DefaultValue);
                auto vGetAttrs = obj->GetVirtualMethod(refl.ParameterInfo.get_Attributes);
                if (!vGetName) continue;
                if (!vGetType) continue;
                if (!vGetDefV) continue;
                if (!vGetAttrs) continue;

                auto type = m->GetParam(i);
                if (!type) continue;

                auto isByRef = type->IsByRef();

                auto nameO = (String*) vGetName->Invoke(obj);
                auto name = nameO ? StrUtil::U16ToU8(nameO->GetChars()) : "?";

                auto attrsO = vGetAttrs->Invoke(obj);
                int attrs = *(int*) attrsO->Unbox();

                bool hasDefValue = attrs & ParameterAttributes::HAS_DEFAULT;

                std::string s;
                std::ostringstream atts;
                std::vector<std::pair<std::string, std::string>> excs;
                if (isByRef) {
                    excs.push_back({"System.Runtime.InteropServices", "InAttribute"});
                    excs.push_back({"System.Runtime.InteropServices", "OutAttribute"});
                }
                if (hasDefValue) {
                    excs.push_back({"System.Runtime.InteropServices", "OptionalAttribute"});
                }
                DoParamAttr(obj, atts, excs);
                s += atts.str();

                bool isIn = (attrs & ParameterAttributes::IN) != 0;
                bool isOut = (attrs & ParameterAttributes::OUT) != 0;
                if (isByRef) {
                    if (isIn && isOut) {
                        s += "ref ";
                    } else if (isIn) {
                        s += "in ";
                    } else if (isOut) {
                        s += "out ";
                    }
                }

                s += FormatTypeName(type);
                s += " ";
                s += name;

                if (hasDefValue) {
                    auto defO = vGetDefV->Invoke(obj);
                    s += " = " + FormatObject(defO);
                }

                parFmts.push_back(s);
            }
        }(); else {
            auto count = m->GetParamCount();
            for (int i = 0; i < count; i++) {
                auto pt = m->GetParam(i);
                auto pn = m->GetParamName(i);

                parFmts.push_back(FormatParamType(pt) + FormatTypeName(pt) + " " + pn);
            }
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
        out << std::format("// RVA: {} Offset: {} VA: {}", Formats::FormatOffset(rva), Formats::FormatOffset(off), Formats::FormatOffset(rva));
        out << "\n";

        DoMemberForAttr(mObj, out);

        out << INDENT;
        out << FormatMethodAccessor(m);
        out << FormatTypeName(mRT);
        out << " ";
        out << mName;
        
        if ((isInflated || isGeneric) && refl.Array.GetValue && refl.MethodBase.GetGenericArguments) [&]{
            auto to = m->GetObject();
            if (!to) return;

            auto vGetGenArgs = to->GetVirtualMethod(refl.MethodBase.GetGenericArguments);
            if (!vGetGenArgs) return;

            auto args = (Array*) vGetGenArgs->Invoke(to);
            if (!args) return;

            int len = args->GetLength();

            std::vector<std::string> pNames;
            for (int i = 0; i < len; i++) {
                auto p = refl.Array.GetValue->Invoke(args, i);
                auto pt = Class::FromSystemType(p);

                pNames.push_back(FormatTypeName(pt->GetType()));
            }
            
            out << "<";
            out << StrUtil::Join(pNames, ", ");
            out << ">";
        }();

        out << "(";
        out << StrUtil::Join(parFmts, ", ");
        out << ")";
        out << (isAbstract ? ";" : " { }");
        out << "\n";
    }

    out << "}";
    out << "\n";
    out << "\n";

    out.flush();
}

Class* FindReflClass(Image* img, const std::string& ns, const std::string& name) {
    Class* ret = img->FindClass(ns, name);
    if (!ret)
        Log::Warn("class {}.{} not found", ns, name);

    return ret;
}

void FindReflMethod(Class* klass, std::string_view name, std::string_view rt, const std::vector<std::string_view>& params, Method** method, std::string_view debugSuffix = {}) {
    if(!(*method = Finder::FindMethod(klass, name, rt, params)))
        Log::Warn("method {}.{}{} not found", klass->GetName(), name, debugSuffix);
}

void InitUnityReflection() {
    Image* image = Finder::FindImage("UnityEngine.CoreModule.dll");
    if (!image) {
        Log::Warn("image UnityEngine.CoreModule.dll not found");
        return;
    }

    Class* appC = FindReflClass(image, "UnityEngine", "Application");
    if (!appC) return;

    FindReflMethod(appC,
        "get_unityVersion",
        "System.String",
        {},
        &refl.UnityEngine.get_unityVersion
    );
}

void InitBoxSize(Image* corlib) {
    Class* objC = FindReflClass(corlib, "System", "Object");
    if (!objC) return;

    structBoxSize = objC->GetInstanceSize();
    Log::Debug("object box size: 0x{:X}", structBoxSize);
}

void InitReflection(Image* corlib) {
    InitBoxSize(corlib);

    refl.Type.klass = FindReflClass(corlib, "System", "Type");
    if (refl.Type.klass) {
        FindReflMethod(
            refl.Type.klass,
            "GetGenericArguments",
            "System.Type[]",
            {},
            &refl.Type.GetGenericArguments
        );
        FindReflMethod(
            refl.Type.klass,
            "GetField",
            "System.Reflection.FieldInfo",
            {"System.String"},
            &refl.Type.GetField
        );
        FindReflMethod(
            refl.Type.klass,
            "GetProperty",
            "System.Reflection.PropertyInfo",
            {"System.String", "System.Reflection.BindingFlags"},
            &refl.Type.GetProperty
        );
    }

    Class* methodC = FindReflClass(corlib, "System.Reflection", "MethodBase");
    if (methodC) {
        FindReflMethod(
            methodC,
            "GetGenericArguments",
            "System.Type[]",
            {},
            &refl.MethodBase.GetGenericArguments
        );
        FindReflMethod(
            methodC,
            "GetParameters",
            "System.Reflection.ParameterInfo[]",
            {},
            &refl.MethodBase.GetParameters
        );
    }

    Class* parC = FindReflClass(corlib, "System.Reflection", "ParameterInfo");
    if (parC) {
        FindReflMethod(
            parC,
            "get_Attributes",
            "System.Reflection.ParameterAttributes",
            {},
            &refl.ParameterInfo.get_Attributes
        );
        FindReflMethod(
            parC,
            "get_DefaultValue",
            "System.Object",
            {},
            &refl.ParameterInfo.get_DefaultValue
        );
        FindReflMethod(
            parC,
            "get_Name",
            "System.String",
            {},
            &refl.ParameterInfo.get_Name
        );
        FindReflMethod(
            parC,
            "get_ParameterType",
            "System.Type",
            {},
            &refl.ParameterInfo.get_Type
        );
    }

    Class* arrC = FindReflClass(corlib, "System", "Array");
    if (arrC) {
        FindReflMethod(
            arrC,
            "GetValue",
            "System.Object",
            {"System.Int32"},
            &refl.Array.GetValue
        );
    }

    Class* iColl = FindReflClass(corlib, "System.Collections", "ICollection");
    if (iColl) {
        FindReflMethod(
            iColl,
            "get_Count",
            "System.Int32",
            {},
            &refl.ICollection.get_Count
        );
    }
    
    refl.IList.klass = FindReflClass(corlib, "System.Collections", "IList");
    if (refl.IList.klass) {
        FindReflMethod(
            refl.IList.klass,
            "get_Item",
            "System.Object",
            {"System.Int32"},
            &refl.IList.get_Item
        );
    }

    Class* memberInfoC = FindReflClass(corlib, "System.Reflection", "MemberInfo");
    if (memberInfoC) {
        FindReflMethod(
            memberInfoC,
            "get_Name",
            "System.String",
            {},
            &refl.MemberInfo.get_Name
        );
    }

    InitUnityReflection();
}

void InitReflectionAtt(Image* corlib) {
    Class* customAttC = FindReflClass(corlib, "System.Reflection", "CustomAttributeData");
    if (customAttC) {
        FindReflMethod(
            customAttC,
            "GetCustomAttributes",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
            {"System.Reflection.MemberInfo"},
            &refl.CustomAttributeData.GetCustomAttributesM,
            "(M)"
        );

        FindReflMethod(
            customAttC,
            "GetCustomAttributes",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
            {"System.Reflection.ParameterInfo"},
            &refl.CustomAttributeData.GetCustomAttributesP,
            "(P)"
        );

        FindReflMethod(
            customAttC,
            "get_AttributeType",
            "System.Type",
            {},
            &refl.CustomAttributeData.get_AttributeType
        );

        FindReflMethod(
            customAttC,
            "get_ConstructorArguments",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeTypedArgument>",
            {},
            &refl.CustomAttributeData.get_ConstructorArguments
        );

        FindReflMethod(
            customAttC,
            "get_NamedArguments",
            "System.Collections.Generic.IList<System.Reflection.CustomAttributeNamedArgument>",
            {},
            &refl.CustomAttributeData.get_NamedArguments
        );
    }

    refl.CustomAttributeTypedArgument.klass = FindReflClass(
        corlib,
        "System.Reflection",
        "CustomAttributeTypedArgument"
    );
    if (refl.CustomAttributeTypedArgument.klass) {
        FindReflMethod(
            refl.CustomAttributeTypedArgument.klass,
            "get_Value",
            "System.Object",
            {},
            &refl.CustomAttributeTypedArgument.get_Value
        );
    }

    Class* namedArgC = FindReflClass(
        corlib,
        "System.Reflection",
        "CustomAttributeNamedArgument"
    );
    if (namedArgC) {
        FindReflMethod(
            namedArgC,
            "get_MemberName",
            "System.String",
            {},
            &refl.CustomAttributeNamedArgument.get_MemberName
        );

        FindReflMethod(
            namedArgC,
            "get_MemberInfo",
            "System.Reflection.MemberInfo",
            {},
            &refl.CustomAttributeNamedArgument.get_MemberInfo
        );

        FindReflMethod(
            namedArgC,
            "get_TypedValue",
            "System.Reflection.CustomAttributeTypedArgument",
            {},
            &refl.CustomAttributeNamedArgument.get_TypedValue
        );
    }
}

std::unique_ptr<ApiResolver> GetApiResolver(const fs::path& extDir) {
    auto mapFileP = extDir / "symbol_map.txt";
    auto rvaFileP = extDir / "rva_map.txt";

    if (fs::exists(mapFileP)) {
        std::map<std::string, std::string> symMap;
        Log::Debug("symbol_map.txt found");

        std::ifstream mapF(mapFileP);

        std::string line;
        while (std::getline(mapF, line)) {
            std::istringstream ss(line);

            std::string key;
            std::string value;
            ss >> key >> value;

            if (key.empty()) {
                Log::Debug("key is empty");
                continue;
            }
            if (value.empty()) {
                Log::Debug("value is empty");
                continue;
            }

            symMap[key] = value;
        }

        Log::Debug("loaded {:d} keys", symMap.size());
        return std::make_unique<SymApiResolver>(symMap);
    }

    if (fs::exists(rvaFileP)) {
        std::map<std::string, size_t> offMap;
        Log::Debug("rva_map.txt found");

        std::ifstream mapF(rvaFileP);

        std::string line;
        while (std::getline(mapF, line)) {
            std::istringstream ss(line);

            std::string key;
            std::string value;
            ss >> key >> value;

            if (key.empty()) {
                Log::Debug("key is empty");
                continue;
            }
            if (value.empty()) {
                Log::Debug("value is empty");
                continue;
            }

            try {
                auto offset = static_cast<size_t>(std::stoull(value, nullptr, 16));
                offMap[key] = offset;
            } catch (const std::exception& e) {
                Log::Debug("invalid offset for {}: {}", key, value);
                return nullptr;
            }
        }

        Log::Debug("loaded {:d} keys", offMap.size());
        return std::make_unique<RvaApiResolver>(offMap);
    }

    return std::make_unique<SymApiResolver>();
}


std::map<uintptr_t, uintptr_t> GetStaticWriteMap(const fs::path& extDir) {
    auto mapFileP = extDir / "static_write_map.txt";

    if (fs::exists(mapFileP)) {
        std::map<uintptr_t, uintptr_t> out;
        Log::Debug("static_write_map.txt found");

        std::ifstream mapF(mapFileP);

        std::string line;
        while (std::getline(mapF, line)) {
            std::istringstream ss(line);

            std::string key;
            std::string value;
            ss >> key >> value;

            if (key.empty()) {
                Log::Debug("key is empty");
                continue;
            }
            if (value.empty()) {
                Log::Debug("value is empty");
                continue;
            }

            size_t offset = 0;
            try {
                offset = static_cast<size_t>(std::stoull(key, nullptr, 16));
            } catch (const std::exception& e) {
                Log::Debug("invalid write offset: {}", key);
                return {};
            }

            uintptr_t val = 0;
            try {
                val = static_cast<size_t>(std::stoull(value, nullptr, 16));
            } catch (const std::exception& e) {
                Log::Debug("invalid write value for 0x{:X}: {}", offset, value);
                return {};
            }

            out[offset] = val;
        }

        Log::Debug("loaded {:d} write offsets", out.size());
        return out;
    }
    return {};
}


void RunDumper() {
    using namespace Api;

    Log::Debug("sleeping...");

    std::this_thread::sleep_for(std::chrono::seconds(4));

    Log::Debug("running...");

    auto extDirS = GetAppExtDir();
    if (extDirS.empty()) {
        Log::Error("cannot obtain external files dir");
        return;
    }
    Log::Debug("obtained external files dir: {}", extDirS);
    fs::path extDir(extDirS);

    fs::path dir = fs::path(extDirS) / "runtime-dumper";
    if (fs::exists(dir)) {
        fs::remove_all(dir);
        Log::Debug("cleaned dir");
    }
    
    if (!fs::create_directories(dir)) {
        Log::Debug("cannot create dir, {}", dir.c_str());
        return;
    }

    fs::path logPath = dir / "log.txt";
    Log::SetFile(logPath);

    Log::Debug("initialized log file");

    auto libRes = LibResolver();
    auto res = GetApiResolver(extDir);
    auto staticWriteMap = GetStaticWriteMap(extDir);

    if (!res) {
        Log::Error("cannot create api resolver");
        return;
    }

    bool init = InitApi(libRes, *res);
    if (!init) {
        Log::Error("initialization failed");
        return;
    }

    libBase = libRes.GetBaseAddress();

    for (auto [offset, value] : staticWriteMap) {
        Log::Debug("writing offset 0x{:X}: 0x{:X}", offset, value);
        *(uintptr_t*)(libBase + offset) = value;
        Log::Debug("wrote value to offset 0x{:X}", offset);
    }

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
        Log::Error("cannot obtain corlib");
    } else {
        Log::Debug("corlib: 0x{:X}", (uintptr_t) corlib);

        InitReflection(corlib);
        InitReflectionAtt(corlib);
    }
    Log::Debug("initialized reflection methods");

    if (refl.UnityEngine.get_unityVersion) {
        auto verO = (String*) refl.UnityEngine.get_unityVersion->Invoke(nullptr);
        auto ver = verO ? StrUtil::U16ToU8(verO->GetChars()) : "null";
        Log::Debug("unity version: {}", ver);
    }

    auto nm = corlib->GetName();
    Log::Debug("corlib: {}", nm);

    auto asms = dmn->GetAssemblies();
    Log::Debug("assemblies: {:d}", asms.size());

    Log::Debug("dumping...");
    auto start = std::chrono::steady_clock::now();

    std::ofstream out(dir / "dump.cs", std::ios::trunc);

    for (auto a : asms) {
        Image* img = a->GetImage();
        if (!img) continue;
        
        out << "// Assembly: ";
        out << img->GetName();
        out << "\n";
    }
    out << "\n";
    out << "\n";

    for (auto a : asms) {
        Image* img = a->GetImage();
        if (!img) continue;
        
        Log::Debug("dumping assembly: {}", img->GetName());

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
    Log::CloseFile();
}