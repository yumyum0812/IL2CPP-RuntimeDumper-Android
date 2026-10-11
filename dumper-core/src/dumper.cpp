#include <chrono>
#include <concepts>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <dobby.h>
#include <KittyMemory.hpp>

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
#include "dumper_log.h"
#include "formats.h"
#include "reflection_methods.h"
#include "reflection_initer.h"
#include "iteration_helper.h"
#include "config.h"
#include "environment.h"


using namespace Api;
using namespace ReflectionMethods;
namespace fs = std::filesystem;

static constexpr auto INDENT = "    ";


static const std::unordered_map<int, std::string> typeAliases = {
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
    {TypeEnum::I, "nint"},
    {TypeEnum::U, "nuint"},
    {TypeEnum::OBJECT, "object"},
};

static const std::unordered_map<int, std::string> typeSuffixes = {
    {TypeEnum::SZARRAY, "[]"},
    {TypeEnum::PTR, "*"},
};


static Config::DumperConfig config;
static std::filesystem::path outDir;
static LibResolver libRes;

static uintptr_t libBase = 0;
static size_t structBoxSize = 0;


static std::string FormatTypeName(Type* t, bool useRealName = false, bool forTypeof = false) {
    if (!t) return "?";

    auto klass = Class::FromType(t);
    auto tp = t->GetTypeEnum();

    std::string res;

    if (!useRealName) {
        if (auto it = typeAliases.find(tp); it != typeAliases.end()) {
            return it->second;
        }
    }

    auto outer = klass->GetDeclaringClass();
    if (outer)
        res += FormatTypeName(outer->GetType(), useRealName) + ".";
    
    if (typeSuffixes.contains(tp)) {
        auto ec = klass->GetElementClass();
        if (ec && ec != klass)
            return res + FormatTypeName(ec->GetType(), useRealName) + typeSuffixes.at(tp);
    }


    auto name = klass->GetName();
    auto isGeneric = klass->IsGeneric();
    auto isInflated = klass->IsInflated();

    if (C_Array.GetValue && C_Type.GetGenericArguments && (isGeneric || isInflated)) {
        auto to = t->GetObject();
        if (to) {
            auto getGensV = to->GetVirtualMethod(C_Type.GetGenericArguments);
            if (getGensV) {
                auto args = (Array*) getGensV->Invoke(to);
                if (args) {
                    std::string pureName = StrUtil::SubBefore(name, "`");

                    int len = args->GetLength();

                    if (!isInflated && forTypeof)
                        return StrUtil::SubBefore(name, "`") + "<" + StrUtil::Repeat(",", len - 1) +  ">";

                    std::vector<std::string> ps;
                    for (int i = 0; i < len; i++) {
                        auto p = C_Array.GetValue->Invoke(args, i);
                        auto pt = Class::FromSystemType(p);
                        auto ptC = pt->GetType();
                        ps.push_back(FormatTypeName(ptC, useRealName));
                    }
                    
                    return pureName + "<" + StrUtil::Join(ps, ", ") + ">";
                }
            }
        }
    }

    return res + name;
}

static std::string FormatTypeType(Class* klass) {
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

static std::string FormatTypeAccessor(Class* klass) {
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

static std::string FormatMethodAccessor(Method* met) {
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

static std::string FormatFieldAccessor(Field* fld) {
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

static std::string FormatParamType(Type* typ) {
    int attrs = typ->GetAttrs();
    bool isByRef = typ->IsByRef();

    using namespace ParameterAttributes;

    std::string out;

    bool isOut = (attrs & OUT) != 0;
    bool isIn  = (attrs & IN)  != 0;

    if (isByRef) {
        if (isOut && !isIn)
            out += "out ";
        else if (isIn && !isOut)
            out += "in ";
        else
            out += "ref ";
            
    } else {
        if (isIn)  out += "[In] ";
        if (isOut) out += "[Out] ";
    }
    
    return out;
}



template <typename Getter>
static std::string FormatObject(int typeEnum, Getter&& getter) {
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

static std::string FormatObjectValue(Object* obj) {
    using namespace TypeEnum;

    if (!obj)
        return "null";

    auto klass = obj->GetClass();
    if (!klass) return "?";

    auto type = klass->GetType();
    if (!type) return "?";

    if (C_Type.klass && C_Type.klass->IsAssignableFrom(klass)) {
        auto tName = Class::FromSystemType(obj)->GetType();
        return "typeof(" + FormatTypeName(tName, false, true) + ")";
    }

    if (klass->IsEnum())
        type = klass->GetEnumBaseType();

    return FormatObject(
        type->GetTypeEnum(),
        [&]<typename T, bool unboxed>() -> T {
            if constexpr (!unboxed) {
                return (T) obj;
            }
            return *(T*) obj->Unbox();
        }
    );
}

static std::string FormatFieldValue(Field* fld) {
    using namespace FieldAttributes;

    int flags = fld->GetFlags();
    bool isConst = (flags & LITERAL) != 0;

    if (!isConst)
        return "";

    auto type = fld->GetType();
    auto klass = Class::FromType(type);

    if (klass->IsEnum())
        type = klass->GetEnumBaseType();

    return " = " + FormatObject(
        type->GetTypeEnum(),
        [&]<typename T, bool>() -> T {
            return fld->GetStaticValue<T>();
        }
    );
}

static std::string FormatAttrTyped(Object* typed) {
    if (!typed) return "?";

    void* typedUnbox = typed->Unbox();
    if (!typedUnbox) return "?";

    Object* v = C_CustomAttributeTypedArgument.get_Value->Invoke(typedUnbox);
    if (!v) return "null";

    Class* klass = v->GetClass();
    if (!klass) return "?";

    if (C_IList.klass && C_IList.get_Item && C_ICollection.get_Count && C_IList.klass->IsAssignableFrom(klass)) {
        Method* vGetItem = v->GetVirtualMethod(C_IList.get_Item);
        if (!vGetItem) return "?";

        Method* vGetCount = v->GetVirtualMethod(C_ICollection.get_Count);
        if (!vGetCount) return "?";

        Object* countO = vGetCount->Invoke(v);
        if (!countO) return "?";

        int count = *(int*) countO->Unbox();

        std::vector<std::string> valueFmts;
        for (int i = 0; i < count; i++) {
            Object* vv = vGetItem->Invoke(v, i);
            Class* k = vv ? vv->GetClass() : nullptr;
            
            if (k && C_CustomAttributeTypedArgument.klass->IsAssignableFrom(k))
                valueFmts.push_back(FormatAttrTyped(vv));
            else
                valueFmts.push_back(FormatObjectValue(vv));
        }

        return "new[] {" + StrUtil::Join(valueFmts, ", ") + "}";
    };

    return FormatObjectValue(v);
}

static bool CanDumpAttrs() {
    return C_CustomAttributeData.get_AttributeType &&
        C_CustomAttributeData.get_ConstructorArguments &&
        C_CustomAttributeData.get_NamedArguments &&
        C_CustomAttributeTypedArgument.get_Value &&
        C_CustomAttributeNamedArgument.get_TypedValue &&
        (
            C_CustomAttributeNamedArgument.get_MemberName ||
            (
                C_CustomAttributeNamedArgument.get_MemberInfo &&
                C_MemberInfo.get_Name
            )
        );
}

static void DumpAttrs(Object* attrsList, std::ostream& out, int indents, bool newLine, const std::vector<std::pair<std::string, std::string>>& excludes = {}) {
    if (!CanDumpAttrs()) return;

    IterationHelper::IterateIList(attrsList, "System.Reflection.CustomAttributeData", [&] (Object* attr) {
        if (!attr) return;

        Object* attrTO = C_CustomAttributeData.get_AttributeType->Invoke(attr);
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

        out << StrUtil::Repeat(INDENT, indents);
        out << "[";
        out << attrTypeName;

        Object* constrsO = C_CustomAttributeData.get_ConstructorArguments->Invoke(attr);
        Object* namedsO = C_CustomAttributeData.get_NamedArguments->Invoke(attr);

        std::vector<std::string> argStrs;
        if (constrsO) {
            IterationHelper::IterateIList(constrsO, "System.Reflection.CustomAttributeTypedArgument", [&] (Object* typed) {
                argStrs.push_back(FormatAttrTyped(typed));
            });
        }
        if (namedsO) {
            IterationHelper::IterateIList(namedsO, "System.Reflection.CustomAttributeNamedArgument", [&] (Object* named) {
                if (!named) return;

                void* namedUnbox = named->Unbox();
                if (!namedUnbox) return;

                String* memberNameS = [&]() -> String* {
                    if (C_CustomAttributeNamedArgument.get_MemberName) {
                        return (String*) C_CustomAttributeNamedArgument.get_MemberName->Invoke(namedUnbox);
                    }

                    Object* info = C_CustomAttributeNamedArgument.get_MemberInfo->Invoke(namedUnbox);
                    if (!info) return nullptr;

                    Method* vGetName = info->GetVirtualMethod(C_MemberInfo.get_Name);
                    if (!vGetName) return nullptr;

                    return (String*) vGetName->Invoke(info);
                }();

                auto memberName = memberNameS ? StrUtil::U16ToU8(memberNameS->GetChars()) : "?";

                Object* typed = C_CustomAttributeNamedArgument.get_TypedValue->Invoke(namedUnbox);
                argStrs.push_back(memberName + " = " + FormatAttrTyped(typed));
            });
        }

        if (!argStrs.empty()) {
            out << "(";
            out << StrUtil::Join(argStrs, ", ");
            out << ")";
        }
        out << "]";
        out << (newLine ? "\n" : " ");
    });
}

static void DumpMemberAttrs(Object* memberInfo, std::ostream& out, int ind = 1) {
    if (!memberInfo) return;
    if (!CanDumpAttrs()) return;

    if (!C_CustomAttributeData.GetCustomAttributesM) return;

    Object* attrs = C_CustomAttributeData.GetCustomAttributesM->Invoke(nullptr, memberInfo);
    if (!attrs) return;
    
    DumpAttrs(attrs, out, ind, true);
}

static void DumpParamAttrs(Object* paramInfo, std::ostream& out, const std::vector<std::pair<std::string, std::string>>& excludes = {}) {
    if (!paramInfo) return;
    if (!CanDumpAttrs()) return;

    if (!C_CustomAttributeData.GetCustomAttributesP) return;

    Object* attrs = C_CustomAttributeData.GetCustomAttributesP->Invoke(nullptr, paramInfo);
    if (!attrs) return;

    DumpAttrs(attrs, out, 0, false, excludes);
}

static void DumpClass(Class* klass, std::ofstream& out) {
    if (config.classBlacklist.contains(klass->GetName())) return;
    
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
    DumpMemberAttrs(tO, out, 0);

    std::string ext;
    if (!extNames.empty()) {
        ext = " : " + StrUtil::Join(extNames, ", ");
    }

    out << FormatTypeAccessor(klass) + FormatTypeType(klass) + FormatTypeName(klass->GetType(), true) + ext + "\n{";
    out << "\n";

    auto fields = klass->GetFields();
    for (auto f : fields) {
        if (C_Type.GetField) [&]{
            auto fName = f->GetName();
            auto fNameS = String::New(fName.c_str());
            auto vGetField = tO->GetVirtualMethod(C_Type.GetField);
            if (!vGetField) return;

            auto fO = vGetField->Invoke(tO, fNameS);
            DumpMemberAttrs(fO, out);
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

        if (C_Type.GetField) [&]{
            auto pNameO = String::New(pName.c_str());
            auto vGetProperty = tO->GetVirtualMethod(C_Type.GetProperty);
            if (!vGetProperty) return;

            auto pO = vGetProperty->Invoke(tO, pNameO, BindingFlags::DECLARED_ONLY | BindingFlags::PUBLIC | BindingFlags::NON_PUBLIC | BindingFlags::STATIC | BindingFlags::INSTANCE);
            DumpMemberAttrs(pO, out);
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
            C_Array.GetValue &&
            C_MethodBase.GetParameters &&
            C_ParameterInfo.get_DefaultValue &&
            C_ParameterInfo.get_Name &&
            C_ParameterInfo.get_Type &&
            C_ParameterInfo.get_Attributes
        ) [&]{
            auto vGetParams = mObj->GetVirtualMethod(C_MethodBase.GetParameters);
            if (!vGetParams) return;

            auto arr = (Array*) vGetParams->Invoke(mObj);
            if (!arr) return;

            int len = arr->GetLength();
            int lenI = m->GetParamCount();
            if (lenI < len) return;

            for (int i = 0; i < len; i++) {
                auto obj = C_Array.GetValue->Invoke(arr, i);
                if (!obj) continue;

                auto vGetName = obj->GetVirtualMethod(C_ParameterInfo.get_Name);
                auto vGetType = obj->GetVirtualMethod(C_ParameterInfo.get_Type);
                auto vGetDefV = obj->GetVirtualMethod(C_ParameterInfo.get_DefaultValue);
                auto vGetAttrs = obj->GetVirtualMethod(C_ParameterInfo.get_Attributes);
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
                DumpParamAttrs(obj, atts, excs);
                s += atts.str();

                if (isByRef) {
                    bool isIn = (attrs & ParameterAttributes::IN) != 0;
                    bool isOut = (attrs & ParameterAttributes::OUT) != 0;
                    
                    if (!isIn && !isOut) {
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
                    s += " = " + FormatObjectValue(defO);
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

        DumpMemberAttrs(mObj, out);

        out << INDENT;
        out << FormatMethodAccessor(m);
        out << FormatTypeName(mRT);
        out << " ";
        out << mName;
        
        if ((isInflated || isGeneric) && C_Array.GetValue && C_MethodBase.GetGenericArguments) [&]{
            auto to = m->GetObject();
            if (!to) return;

            auto vGetGenArgs = to->GetVirtualMethod(C_MethodBase.GetGenericArguments);
            if (!vGetGenArgs) return;

            auto args = (Array*) vGetGenArgs->Invoke(to);
            if (!args) return;

            int len = args->GetLength();

            std::vector<std::string> pNames;
            for (int i = 0; i < len; i++) {
                auto p = C_Array.GetValue->Invoke(args, i);
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

static void DumpAssembly(Assembly* a, std::ofstream& out) {
    Image* img = a->GetImage();
    if (!img) return;
    
    DumperLog::Debug("dumping assembly: {}", img->GetName());

    auto klasses = img->GetClasses();
    for (auto k : klasses) {
        DumpClass(k, out);
    }

    out.flush();
}

static void DumpAssemblies(const std::vector<Assembly*> asms, std::ofstream& out) {
    for (auto a : asms) {
        Image* img = a->GetImage();
        if (!img) continue;
        
        out << "// Assembly: ";
        out << img->GetName();
        out << "\n";
    }
    out << "\n";
    out << "\n";
    out.flush();

    for (auto a : asms) {
        DumpAssembly(a, out);
        out.flush();
    }
}

static void DumperMain() {
    Domain* domain = Domain::GetInstance();
    if (!domain) {
        DumperLog::Error("domain got null");
        return;
    }
    DumperLog::Debug("domain: 0x{:X}", (uintptr_t) domain);

    Thread* thr = Thread::GetCurrent();
    bool attached = thr != nullptr;
    if (thr) {
        DumperLog::Debug("already attached to il2cpp, skipping attach");
    } else {
        DumperLog::Debug("not attached to il2cpp, attaching...");

        thr = Thread::Attach(domain);
        if (!thr) {
            DumperLog::Error("attach returned null");
            return;
        }
        DumperLog::Error("attached to thread");
    }
    DumperLog::Debug("thread: 0x{:X}", (uintptr_t) thr);

    Image* corlib = Image::GetCorlib();
    if (!corlib) {
        DumperLog::Error("cannot obtain corlib");
    } else {
        auto nm = corlib->GetName();
        DumperLog::Debug("corlib: {}", nm);

        ReflectionIniter::InitReflection(corlib);
        ReflectionIniter::InitUnityReflection();
        structBoxSize = ReflectionIniter::GetBoxSize(corlib);
        
        DumperLog::Debug("initialized reflection methods");
    }

    if (C_UnityEngine.get_unityVersion) {
        auto verO = (String*) C_UnityEngine.get_unityVersion->Invoke(nullptr);
        auto ver = verO ? StrUtil::U16ToU8(verO->GetChars()) : "null";
        DumperLog::Debug("unity version: {}", ver);
    }

    auto asms = domain->GetAssemblies();
    DumperLog::Debug("assemblies: {:d}", asms.size());

    if (config.filterAssembly) {
        std::erase_if(asms, [&](Assembly* item) {
            Image* img = item->GetImage();
            if (!img) return true;

            return config.assmblyFilter.find(img->GetName()) == config.assmblyFilter.end();
        });

        DumperLog::Debug("filtered assemblies: {:d}", asms.size());
    }

    DumperLog::Debug("dumping...");
    auto startTime = std::chrono::steady_clock::now();

    if (!config.classBlacklist.empty()) {
        DumperLog::Debug("blacklisted classes: {}", StrUtil::Join(config.classBlacklist, ", "));
    }

    if (!config.splitDump) {
        std::ofstream out(outDir / "dump.cs", std::ios::trunc);

        DumpAssemblies(asms, out);
    } else {
        fs::path dumpDir = outDir / "DumpCs";

        for (auto a : asms) {
            Image* img = a->GetImage();
            if (!img) continue;

            if (!fs::exists(dumpDir) && !fs::create_directories(dumpDir)) {
                DumperLog::Debug("cannot create dir: {}", dumpDir.c_str());
                continue;
            }

            std::string asmName = img->GetName();
            StrUtil::EraseSuffix(asmName, ".dll");
            
            std::ofstream out(dumpDir / (asmName + ".cs"), std::ios::trunc);
            DumpAssemblies({a}, out);
        }
    }

    auto finishTime = std::chrono::steady_clock::now();
    float durationSec = std::chrono::duration<float>(finishTime - startTime).count();
    DumperLog::Debug("dump success ({:.3f}s)", durationSec);
    DumperLog::Debug("output dir: {}", outDir.c_str());

    if (!attached) {
        DumperLog::Debug("detaching il2cpp...");
        thr->Detach();
    }

    DumperLog::Debug("all finished!");
    DumperLog::CloseFile();
}


static Object* (* Old_RuntimeInvoke)(Method* method, void* obj, void** args, Object** exc);
static Object* Hook_RuntimeInvoke(Method* method, void* obj, void** args, Object** exc) {
    static bool dumpOnce = true;
    if (dumpOnce) {
        dumpOnce = false;
        DumperMain();
    }
    return Old_RuntimeInvoke(method, obj, args, exc);
}


static std::string GetAppExtDir() {
    JavaVM* vm = JniUtil::GetJavaVM();
    if (!vm) {
        DumperLog::Error("cannot obtain JavaVM");
        return "";
    }

    auto att = JniUtil::JniAttach(vm);
    JNIEnv* env = att.Attach();
    if (!env) {
        DumperLog::Error("cannot attach JNI");
        return "";
    }

    jobject app = nullptr;
    int msElapsed = 0;
    while (!(app = JniUtil::GetApplication(env))) {
        if (msElapsed >= 10'000) {
            DumperLog::Error("cannot obtain Applicaion object for 10 seconds");
            return "";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        msElapsed += 100;
    }

    auto extDir = JniUtil::GetExternalFilesPath(env, app);
    if (extDir.empty()) {
        DumperLog::Error("failed externalFilesDir()");
        return "";
    }

    return extDir;
}

void RunDumper() {
    using namespace Api;

    DumperLog::Debug("running... ({})", ARCH_NAME);

    auto extDirS = GetAppExtDir();
    if (extDirS.empty()) {
        DumperLog::Error("cannot obtain external files dir");
        return;
    }
    DumperLog::Debug("obtained external files dir: {}", extDirS);
    fs::path extDir(extDirS);

    auto dumperDir = fs::path(extDirS) / "RuntimeDumper";
    outDir = dumperDir / "Dump";
    // if (fs::exists(outDir)) {
    //     fs::remove_all(outDir);
    //     DumperLog::Debug("cleaned dir");
    // }
    
    if (!fs::exists(outDir) && !fs::create_directories(outDir)) {
        DumperLog::Error("cannot create dir: {}", outDir.c_str());
        return;
    }

    fs::path logPath = outDir / "log.txt";
    DumperLog::SetFile(logPath);
    DumperLog::Debug("initialized log file");

    try {
        config = Config::DumperConfig::Load(dumperDir / "dumper_config.json");
    } catch (const std::exception& e) {
        DumperLog::Debug("failed to load dumper config: {}", e.what());
        return;
    }
    DumperLog::Debug("loaded dumper config");

    if (config.noDump) {
        DumperLog::Debug("no dumping is enabled, aborting...");
        return;
    }

    libRes = LibResolver(config.libName);
    ApiResolver res;
    try {
        res = Config::LoadApiResolver(dumperDir);
    } catch (const std::runtime_error& e) {
        DumperLog::Error("failed to create api resovler: {}", e.what());
        return;
    }

    std::vector<Config::PatchInfo> patches;
    try {
        patches = Config::LoadPatches(dumperDir);
    } catch (const std::runtime_error& e) {
        DumperLog::Error("failed to load patches: {}", e.what());
        return;
    }

    bool init = InitApi(libRes, res);
    if (!init) {
        DumperLog::Error("api init failed");
        return;
    }

    libBase = libRes.GetBaseAddress();
    DumperLog::Debug("libil2cpp base: 0x{:X}", libBase);

    DumperLog::Debug("waiting... ({:d}s)", config.waitTime);
    std::this_thread::sleep_for(std::chrono::seconds(config.waitTime));

    for (const auto& p : patches) {
        DumperLog::Debug("writing patch: 0x{:X}, {} bytes", p.offset, p.bytes.size());
        KittyMemory::memWrite(libBase + p.offset, p.bytes.data(), p.bytes.size());
        DumperLog::Debug("wrote {} bytes", p.bytes.size());
    }

    if (config.triggerByHook) {
        DumperLog::Debug("trigger by hook is enabled");
        DumperLog::Debug("hooking il2cpp_runtime_invoke...");
        DobbyHook((void*) Api::Functions::runtime_invoke.GetPointer(), (void*) Hook_RuntimeInvoke, (void**) &Old_RuntimeInvoke);
        DumperLog::Debug("hooked il2cpp_runtime_invoke");
    } else {
        DumperMain();
    }
}