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
#include "api/consts.h"
#include "api/thread.h"
#include "api/domain.h"
#include "api/string.h"
#include "api/array.h"
#include "api/property.h"

#include "jni_util.h"
#include "log.h"


namespace Dumper {
    using ApiResolver = Api::Functions::ApiResolver;
    using SymApiResolver = Api::Functions::SymApiResolver;

    class RuntimeDumper {
        inline constexpr static auto INDENT = "    ";
        
        uintptr_t libBase = 0;

        size_t structBoxSize = 0;
        
        Api::Class* typeKlass = nullptr;
        Api::Method* arrGetValue = nullptr;
        Api::Method* tGetGenericArgs = nullptr;
        Api::Method* tGetField = nullptr;
        Api::Method* tGetProp = nullptr;
        Api::Method* miGetAttrs = nullptr;
        Api::Method* mGetGenericArgs = nullptr;
        Api::Method* getUnityVer = nullptr;

        struct {
            Api::Method* getAttType = nullptr;
            Api::Method* getConstructorArgs = nullptr;
            Api::Method* getNamedArgs = nullptr;
        } attM;
        struct {
            Api::Method* getValue = nullptr;
        } attTypedArgM;
        struct {
            Api::Method* getName = nullptr;
            Api::Method* getTypedValue = nullptr;
        } attNamedArgM;

        inline const static std::unordered_map<int, std::string> typeAliases = {
            {Api::TypeEnum::VOID, "void"},
            {Api::TypeEnum::BOOLEAN, "bool"},
            {Api::TypeEnum::CHAR, "char"},
            {Api::TypeEnum::I1, "sbyte"},
            {Api::TypeEnum::U1, "byte"},
            {Api::TypeEnum::I2, "short"},
            {Api::TypeEnum::U2, "ushort"},
            {Api::TypeEnum::I4, "int"},
            {Api::TypeEnum::U4, "uint"},
            {Api::TypeEnum::I8, "long"},
            {Api::TypeEnum::U8, "ulong"},
            {Api::TypeEnum::R4, "float"},
            {Api::TypeEnum::R8, "double"},
            {Api::TypeEnum::STRING, "string"},
            {Api::TypeEnum::TYPEDBYREF, "TypedReference"},
            {Api::TypeEnum::I, "IntPtr"},
            {Api::TypeEnum::U, "UIntPtr"},
            {Api::TypeEnum::OBJECT, "object"},
        };

        inline const static std::unordered_map<int, std::string> typeSuffixes = {
            {Api::TypeEnum::SZARRAY, "[]"},
            {Api::TypeEnum::PTR, "*"},
        };

        inline const static std::unordered_map<int, std::string> classAccessors = {
            {Api::TypeAttributes::PUBLIC, "public"},
            {Api::TypeAttributes::NESTED_PUBLIC, "public"},
            {Api::TypeAttributes::NOT_PUBLIC, "internal"},
            {Api::TypeAttributes::NESTED_FAM_AND_ASSEM, "private protected"},
            {Api::TypeAttributes::NESTED_ASSEMBLY, "internal"},
            {Api::TypeAttributes::NESTED_PRIVATE, "private"},
            {Api::TypeAttributes::NESTED_FAMILY, "protected"},
            {Api::TypeAttributes::NESTED_FAM_OR_ASSEM, "protected internal"},
        };

        inline const static std::unordered_map<int, std::string> fieldAccessors = {
            {Api::FieldAttributes::PUBLIC, "public"},
            {Api::FieldAttributes::PRIVATE, "private"},
            {Api::FieldAttributes::FAMILY, "protected"},
            {Api::FieldAttributes::ASSEMBLY, "internal"},
            {Api::FieldAttributes::FAM_OR_ASSEM, "protected internal"},
            {Api::FieldAttributes::FAM_AND_ASSEM, "private protected"},
        };

        inline const static std::unordered_map<int, std::string> methodAccessors = {
            {Api::MethodAttributes::PUBLIC, "public"},
            {Api::MethodAttributes::PRIVATE, "private"},
            {Api::MethodAttributes::ASSEM, "internal"},
            {Api::MethodAttributes::FAMILY, "protected"},
            {Api::MethodAttributes::FAM_OR_ASSEM, "protected internal"},
            {Api::MethodAttributes::FAM_AND_ASSEM, "private protected"},
        };

        inline const static std::unordered_map<char, std::string> charEscapes = {
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


        static std::string GetAppExtDir() {
            JavaVM* vm = JniUtil::GetJavaVM();
            if (!vm) {
                Log::Error("dumper: jni: cannot obtain JavaVM");
                return "";
            }
            Log::Debug("dumper: jni: obtained JavaVM");

            auto att = JniUtil::JniAttach(vm);
            JNIEnv* env = att.Attach();
            if (!env) {
                Log::Error("dumper: jni: cannot attach JNI");
                return "";
            }
            Log::Debug("dumper: jni: attached JNI");

            auto app = JniUtil::GetApplication(env);
            if (!app) {
                Log::Error("dumper: jni: cannot obtain Application");
                return "";
            }
            Log::Debug("dumper: jni: obtained Application");

            auto extDir = JniUtil::GetExternalFilesPath(env, app);
            if (extDir.empty()) {
                Log::Error("dumper: jni: cannot obtain externalFilesDir");
                return "";
            }

            Log::Debug("dumper: jni: detaching JNI...");
            return extDir;
        }

        std::string FormatTypeName(Api::Type* typ, bool def = false) {
            auto klass = Api::Class::FromType(typ);
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
                auto args = (Api::Array*) getGensV->Invoke(to);
                
                std::string pureName = klass->GetName();
                size_t pos = pureName.find('`');
                pureName = (pos != std::string::npos) ? pureName.substr(0, pos) : pureName;

                int len = args->GetLength();
                std::vector<std::string> ps;
                for (int i = 0; i < len; i++) {
                    auto p = arrGetValue->Invoke(args, i);
                    auto pt = Api::Class::FromSystemType(p);
                    auto ptC = pt->GetType();
                    ps.push_back(FormatTypeName(ptC, def));
                }
                
                return pureName + "<" + JoinStr(ps, ", ") + ">";
            }

            return res + klass->GetName();
        }

        static std::string FormatTypeType(Api::Class* klass) {
            using namespace Api::TypeAttributes;

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

        static std::string FormatTypeAccessor(Api::Class* klass) {
            using namespace Api::TypeAttributes;
            int flags = klass->GetFlags();

            int vis = flags & VISIBILITY_MASK;
            if (classAccessors.contains(vis)) {
                return classAccessors.at(vis) + " ";
            }

            return {};
        }

        static std::string FormatMethodAccessor(Api::Method* met) {
            using namespace Api::MethodAttributes;

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

        static std::string FormatFieldAccessor(Api::Field* fld) {
            using namespace Api::FieldAttributes;

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

        static std::string FormatParamType(Api::Type* typ) {
            using namespace Api::ParameterAttributes;

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

        Api::Method* FindMethod(Api::Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
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

        Api::Method* FindMethodP(Api::Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
            auto m = FindMethod(klass, name, rtName, pNames);
            if (m) return m;

            auto par = klass->GetParent();
            return FindMethod(par, name, rtName, pNames);
        }

        Api::Method* FindMethodI(Api::Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
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

        void RemoveSuffix(std::string& str, std::string_view suff) {
            if (str.ends_with(suff))
                str.erase(str.size() - suff.size());
        }

        std::vector<Api::Object*> ObjListToVector(Api::Object* obj) {
            auto klass = obj->GetClass();

            auto getCount = FindMethodI(klass, "get_Count", "System.Int32", {});
            if (!getCount) return {};

            auto getItem = FindMethodI(klass, "get_Item", "", {"System.Int32"});
            if (!getItem) return {};

            getCount = obj->GetVirtualMethod(getCount);
            getItem = obj->GetVirtualMethod(getItem);

            if (!getCount) {
                Log::Warn("dumper: get_Count is null for {}", FormatTypeName(klass->GetType()));
                return {};
            }
            if (!getItem) {
                Log::Warn("dumper: get_Item is null for {}", FormatTypeName(klass->GetType()));
                return {};
            }

            auto countObj = getCount->Invoke(obj);
            if (!countObj)
                return {};

            int count = *reinterpret_cast<int*>(countObj->Unbox());

            std::vector<Api::Object*> vec;
            vec.reserve(count);

            for (int i = 0; i < count; i++) {
                auto v = getItem->Invoke(obj, i);
                vec.push_back(v);
            }

            return vec;
        }

        void InitReflection(Api::Image* corlib) {
            auto vtKlass = corlib->FindClass("System", "Object");
            if (!vtKlass) {
                Log::Warn("dumper: Object not found");
            } else {
                structBoxSize = vtKlass->GetInstanceSize();
                Log::Debug("dumper: object box size: 0x{:X}", structBoxSize);
            }

            typeKlass = corlib->FindClass("System", "Type");
            if (!typeKlass) {
                Log::Warn("dumper: class Type not found");
            } else {
                tGetGenericArgs = FindMethod(typeKlass, "GetGenericArguments", "System.Type[]", {});
                if (!tGetGenericArgs)
                    Log::Warn("dumper: method Type.GetGenericArguments not found");

                tGetField = FindMethod(typeKlass, "GetField", "System.Reflection.FieldInfo", {"System.String"});
                if (!tGetField)
                    Log::Warn("dumper: method Type.GetField not found");

                tGetProp = FindMethod(typeKlass, "GetProperty", "System.Reflection.PropertyInfo", {"System.String"});
                if (!tGetProp)
                    Log::Warn("dumper: method Type.GetProperty not found");
            }

            auto methodKlass = corlib->FindClass("System.Reflection", "MethodBase");
            if (!methodKlass) {
                Log::Warn("dumper: MethodBase not found");
            } else {
                mGetGenericArgs = FindMethod(methodKlass, "GetGenericArguments", "System.Type[]", {});
                if (!tGetGenericArgs)
                    Log::Warn("dumper: method MethodBase.GetGenericArguments not found");
            }

            auto arrKlass = corlib->FindClass("System", "Array");
            if (!arrKlass) {
                Log::Warn("dumper: System.Array not found");
            } else {
                arrGetValue = FindMethod(arrKlass, "GetValue", "System.Object", {"System.Int32"});
                if (!arrGetValue)
                    Log::Warn("dumper: method Array.GetValue not found");
            }

            auto ueImg = FindImage("UnityEngine.CoreModule.dll");
            if (!ueImg) {
                Log::Warn("dumper: UnityEngine.CoreModule.dll not found");
            } else {
                auto c = ueImg->FindClass("UnityEngine", "Application");
                if (!c) {
                    Log::Warn("dumper: Application not found");
                } else {
                    getUnityVer = FindMethod(c, "get_unityVersion", "System.String", {});
                    if (!getUnityVer) {
                        Log::Warn("dumper: UnityEngine.get_unityVersion not found");
                    }
                }
            }
        }

        Api::Image* FindImage(const std::string& name) {
            auto domain = Api::Domain::GetInstance();
            auto asms = domain->GetAssemblies();
            for (auto a : asms) {
                auto img = a->GetImage();
                if (img->GetName() == name) return img;
            }
            return nullptr;
        }

        void InitReflectionAtt(Api::Image* corlib) {
            auto memberInfoC = corlib->FindClass("System.Reflection", "MemberInfo");
            if (!memberInfoC) {
                Log::Warn("dumper: class MemberInfo not found");
                return;
            }

            miGetAttrs = FindMethod(
                memberInfoC,
                "GetCustomAttributesData",
                "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
                {}
            );
            if (!miGetAttrs) {
                Log::Warn("dumper: method MemberInfo.GetCustomAttributesData not found");
                return;
            }

            auto attC = corlib->FindClass("System.Reflection", "CustomAttributeData");
            {
                if (!attC) {
                    Log::Warn("dumper: class CustomAttributeData not found");
                    return;
                }
                
                attM.getAttType = FindMethod(
                    attC,
                    "get_AttributeType",
                    "System.Type",
                    {}
                );
                if (!attM.getAttType) {
                    Log::Warn("dumper: method CustomAttributeData.get_AttributeType not found");
                    return;
                }

                attM.getConstructorArgs = FindMethod(
                    attC,
                    "get_ConstructorArguments",
                    "System.Collections.Generic.IList<System.Reflection.CustomAttributeTypedArgument>",
                    {}
                );
                if (!attM.getConstructorArgs) {
                    Log::Warn("dumper: method CustomAttributeData.get_ConstructorArguments not found");
                    return;
                }

                attM.getNamedArgs = FindMethod(
                    attC,
                    "get_NamedArguments",
                    "System.Collections.Generic.IList<System.Reflection.CustomAttributeNamedArgument>",
                    {}
                );
                if (!attM.getNamedArgs) {
                    Log::Warn("dumper: method CustomAttributeData.get_NamedArguments not found");
                    return;
                }
            }

            auto typedArgC = corlib->FindClass("System.Reflection", "CustomAttributeTypedArgument");
            if (!typedArgC) {
                Log::Warn("dumper: method CustomAttributeTypedArgument not found");
                return;
            }

            attTypedArgM.getValue = FindMethod(
                typedArgC,
                "get_Value",
                "System.Object",
                {}
            );
            if (!attTypedArgM.getValue) {
                Log::Warn("dumper: method CustomAttributeTypedArgument.get_Value not found");
                return;
            }

            auto namedArgC = corlib->FindClass("System.Reflection", "CustomAttributeNamedArgument");
            if (!namedArgC) {
                Log::Warn("dumper: class CustomAttributeNamedArgument not found");
                return;
            }

            attNamedArgM.getName = FindMethod(
                namedArgC,
                "get_MemberName",
                "System.String",
                {}
            );
            if (!attNamedArgM.getName) {
                Log::Warn("dumper: method CustomAttributeNamedArgument.get_MemberName not found");
                return;
            }

            attNamedArgM.getTypedValue = FindMethod(
                namedArgC,
                "get_TypedValue",
                "System.Reflection.CustomAttributeTypedArgument",
                {}
            );
            if (!attNamedArgM.getTypedValue) {
                Log::Warn("dumper: method CustomAttributeNamedArgument.get_TypedValue not found");
                return;
            }
        }

        static std::string FormalStringLiteral(const std::string& input) {
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
        static std::string FormatInteger(T value) {
            return std::format("{:d}", value);
        }

        template <std::floating_point T>
        static std::string FormatFloat(T value) {
            auto s = std::format("{}", value);
            if (!s.empty() &&
                std::all_of(s.begin() + (s.front() == '-' ? 1 : 0), s.end(), [](unsigned char c) {
                    return std::isdigit(c);
                })) {
                s += ".0";
            }
            return s;
        }

        static std::string U16ToU8(const std::u16string& u16str) {
            std::u8string u8str = std::filesystem::path(u16str).u8string();
            std::string utf8str(reinterpret_cast<const char*>(u8str.data()), u8str.length());
            return utf8str;
        }

        template <std::integral T>
        static std::string FormatOffset(T value) {
            if (value < 0) {
                using U = std::make_unsigned_t<T>;
                const auto magnitude = U{0} - static_cast<U>(value);
                return std::format("-0x{:X}", magnitude);
            }
            return std::format("0x{:X}", value);
        }

        std::string FormatClassSpecial(Api::Class* klass) {
            // using namespace Api::TypeAttributes;
            // int flags = klass->GetFlags();
            // if (flags & SPECIAL_NAME) {
            //     return " /* ★ */";
            // }
            return "";
        }

        std::string FormatMethodSpecial(Api::Method* met) {
            // using namespace Api::MethodAttributes;
            // int flags = met->GetFlags();
            // if (flags & SPECIAL_NAME) {
            //     return " /* ★ */";
            // }
            return "";
        }

        std::string FormatFieldSpecial(Api::Field* fld) {
            // using namespace Api::FieldAttributes;
            // int flags = fld->GetFlags();
            // if (flags & SPECIAL_NAME) {
            //     return " /* ★ */";
            // }
            return "";
        }

        std::string FormatFieldValue(Api::Field* fld) {
            using namespace Api::FieldAttributes;
            using namespace Api::TypeEnum;

            int flags = fld->GetFlags();
            bool isConst = (flags & LITERAL) != 0;

            if (!isConst) return "";

            auto type = fld->GetType();
            auto klass = Api::Class::FromType(type);
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
                case Api::TypeEnum::CHAR: {
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

                case Api::TypeEnum::BOOLEAN: {
                    bool value = fld->GetStaticValue<bool>();
                    out += value ? "true" : "false";
                } break;

                case Api::TypeEnum::STRING: {
                    auto* value = fld->GetStaticValue<Api::String*>();
                    if (!value) out += "null";

                    auto u16s = value->GetChars();
                    auto s = U16ToU8(u16s);
                    out += FormalStringLiteral(s);
                } break;

                case Api::TypeEnum::CLASS:
                case Api::TypeEnum::OBJECT: {
                    auto* value = fld->GetStaticValue<Api::Object*>();
                    if (!value) out += "null";
                } break;

                case Api::TypeEnum::I1: appendInteger.operator()<int8_t>(); break;
                case Api::TypeEnum::U1: appendInteger.operator()<uint8_t>(); break;
                case Api::TypeEnum::I2: appendInteger.operator()<int16_t>(); break;
                case Api::TypeEnum::U2: appendInteger.operator()<uint16_t>(); break;
                case Api::TypeEnum::I4: appendInteger.operator()<int32_t>(); break;
                case Api::TypeEnum::U4: appendInteger.operator()<uint32_t>(); break;
                case Api::TypeEnum::I8: appendInteger.operator()<int64_t>(); break;
                case Api::TypeEnum::U8: appendInteger.operator()<uint64_t>(); break;
                case Api::TypeEnum::I:  appendInteger.operator()<intptr_t>(); break;
                case Api::TypeEnum::U:  appendInteger.operator()<uintptr_t>(); break;
                case Api::TypeEnum::R4: appendFloat.operator()<float>(); break;
                case Api::TypeEnum::R8: appendFloat.operator()<double>(); break;
            }

            if (!out.empty()) {
                out = " = " + out;
            }

            return out;
        }

        std::string FormatObject(Api::Object* obj) {
            using namespace Api::TypeEnum;

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
                case Api::TypeEnum::CHAR: {
                    uint16_t v = *(uint16_t*) obj->Unbox();;
                    
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

                case Api::TypeEnum::BOOLEAN: {
                    bool value = *(bool*) obj->Unbox();;
                    out += value ? "true" : "false";
                } break;

                case Api::TypeEnum::STRING: {
                    auto* value = (Api::String*) obj;
                    auto u16s = value->GetChars();
                    auto s = U16ToU8(u16s);
                    out += FormalStringLiteral(s);
                } break;

                case Api::TypeEnum::I1: appendInteger.operator()<int8_t>(); break;
                case Api::TypeEnum::U1: appendInteger.operator()<uint8_t>(); break;
                case Api::TypeEnum::I2: appendInteger.operator()<int16_t>(); break;
                case Api::TypeEnum::U2: appendInteger.operator()<uint16_t>(); break;
                case Api::TypeEnum::I4: appendInteger.operator()<int32_t>(); break;
                case Api::TypeEnum::U4: appendInteger.operator()<uint32_t>(); break;
                case Api::TypeEnum::I8: appendInteger.operator()<int64_t>(); break;
                case Api::TypeEnum::U8: appendInteger.operator()<uint64_t>(); break;
                case Api::TypeEnum::I:  appendInteger.operator()<intptr_t>(); break;
                case Api::TypeEnum::U:  appendInteger.operator()<uintptr_t>(); break;
                case Api::TypeEnum::R4: appendFloat.operator()<float>(); break;
                case Api::TypeEnum::R8: appendFloat.operator()<double>(); break;
                case Api::TypeEnum::CLASS: {
                    if (typeKlass && typeKlass->IsAssignableFrom(obj->GetClass())) {
                        auto tName = Api::Class::FromSystemType(obj)->GetType();
                        return "typeof(" + FormatTypeName(tName) + ")";
                    }
                }break;
            }

            if (out.empty())
                return "?";

            return out;
        }

        static std::string JoinStr(const std::vector<std::string>& vars, const std::string& delim) {
            std::string out;
            for (int i = 0; i < vars.size(); i++) {
                if (i >= 1) out += delim;
                out += vars[i];
            }
            return out;
        }

        void DoAttr(Api::Object* memberInfo, std::ofstream& out, int ind = 1) {
            if (!memberInfo) return;

            if (!miGetAttrs ||
                !attM.getAttType || !attM.getConstructorArgs || !attM.getNamedArgs ||
                !attTypedArgM.getValue || !attNamedArgM.getTypedValue ||
                !attNamedArgM.getName
            ) return;

            Api::Method* vm = memberInfo->GetVirtualMethod(miGetAttrs);
            if (!vm) return;

            Api::Object* attrs = vm->Invoke(memberInfo);
            if (!attrs) return;
            
            auto attrsV = ObjListToVector(attrs);
            for (Api::Object* attr : attrsV) {
                if (!attr) continue;

                Api::Object* attTypeO = attM.getAttType->Invoke(attr);
                if (!attTypeO) continue;

                auto attC = Api::Class::FromSystemType(attTypeO);
                if (!attC) continue;
                
                auto attTypeName = attC->GetName();
                RemoveSuffix(attTypeName, "Attribute");

                for (int i = 0; i < ind; i++) {
                    out << INDENT;
                }
                out << "[";
                out << attTypeName;

                Api::Object* constrs = attM.getConstructorArgs->Invoke(attr);
                if (!constrs) continue;

                Api::Object* nameds = attM.getNamedArgs->Invoke(attr);
                if (!nameds) continue;

                auto constrsV = ObjListToVector(constrs);
                auto namedsV = ObjListToVector(nameds);

                std::vector<std::string> argStrs;
                for (Api::Object* typed : constrsV) {
                    if (!typed) continue;

                    void* typedUnbox = typed->Unbox();
                    if (!typedUnbox) continue;

                    Api::Object* v = attTypedArgM.getValue->Invoke(typedUnbox);
                    argStrs.push_back(FormatObject(v));
                }
                for (Api::Object* named : namedsV) {
                    if (!named) continue;

                    void* namedUnbox = named->Unbox();
                    if (!namedUnbox) continue;

                    Api::String* memberNameS = (Api::String*) attNamedArgM.getName->Invoke(namedUnbox);
                    auto memberName = memberNameS ? U16ToU8(memberNameS->GetChars()) : "null";

                    Api::Object* typed = attNamedArgM.getTypedValue->Invoke(namedUnbox);
                    if (!typed) continue;

                    void* typedUnbox = typed->Unbox();
                    if (!typedUnbox) continue;

                    Api::Object* v = attTypedArgM.getValue->Invoke(typedUnbox);
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

        void DoClass(Api::Class* klass, Api::Functions::LibraryResolver& libRes, std::ofstream& out) {
            out << "// Namespace: ";
            out << klass->GetNamespace();
            out << "\n";

            std::vector<std::string> extNames;

            auto type = klass->GetType();
            bool isValueType = klass->IsValueType();
            bool isEnum = klass->IsEnum();

            auto pKlass = klass->GetParent();
            if (isEnum) {
                pKlass = Api::Class::FromType(klass->GetEnumBaseType());

            } else if (isValueType) {
                pKlass = nullptr;

            } else if (pKlass) {
                auto pType = pKlass->GetType();
                if (pType->GetTypeEnum() == Api::TypeEnum::OBJECT)
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

            Api::Object* to = type->GetObject();
            DoAttr(to, out, 0);

            out << FormatTypeAccessor(klass) + FormatTypeType(klass) + FormatTypeName(klass->GetType(), true) + ext + " {" + FormatClassSpecial(klass);
            out << "\n";

            auto fields = klass->GetFields();
            for (auto f : fields) {
                if (tGetField) {
                    auto fName = f->GetName();
                    auto fNameS = Api::String::New(fName.c_str());
                    auto gf = to->GetVirtualMethod(tGetField);
                    auto fo = gf->Invoke(to, fNameS);
                    if (fo) {
                        DoAttr(fo, out);
                    }
                }

                auto fType = f->GetType();
                auto fFlag = f->GetFlags();
                bool isConst = fFlag & Api::FieldAttributes::LITERAL;
                bool isStatic = fFlag & Api::FieldAttributes::STATIC;
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
                    auto pNameS = Api::String::New(pName.c_str());
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
                        rva = va - libBase;
                        off = libRes.RVAToOffset(rva);
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
                    auto args = (Api::Array*) getGensV->Invoke(to);
                    int len = args->GetLength();

                    std::vector<std::string> pNames;
                    for (int i = 0; i < len; i++) {
                        auto p = arrGetValue->Invoke(args, i);
                        auto pt = Api::Class::FromSystemType(p);

                        pNames.push_back(FormatTypeName(pt->GetType()));
                    }
                    
                    out << "<";
                    out << JoinStr(pNames, ", ");
                    out << ">";
                }

                out << "(";
                out << JoinStr(parFmts, ", ");
                out << ") { }";
                out << FormatMethodSpecial(m);
                out << "\n";
            }

            out << "}";
            out << "\n";
            out << "\n";
        }

    public:
        void Run() {
            using namespace Api;

            Log::Debug("dumper: running...");

            auto extDirS = GetAppExtDir();
            if (extDirS.empty()) {
                Log::Debug("dumper: cannot obtain external files dir");
                return;
            }
            Log::Debug("dumper: obtained external files dir: {}", extDirS);

            std::filesystem::path extDir(extDirS);

            std::map<std::string, std::string> symMap;

            auto mapFileP = extDir / "symbol_map.txt";
            if (std::filesystem::exists(mapFileP)) {
                Log::Debug("dumper: symbol_map.txt was found");

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

                Log::Debug("dumper: loaded {:d} keys", symMap.size());
            }

            auto libRes = Functions::LibraryResolver();
            auto res = SymApiResolver(symMap);

            bool init = Api::Functions::InitApi(libRes, res);
            if (!init) {
                Log::Error("dumper: initialization failed");
                return;
            }

            Domain* dmn = Domain::GetInstance();
            if (!dmn) {
                Log::Error("dumper: cannot obtain domain");
                return;
            }
            Log::Debug("dumper: domain: 0x{:X}", (uintptr_t) dmn);

            Thread* thr = Thread::Attach(dmn);
            if (!thr) {
                Log::Error("dumper: cannot attach thread");
                return;
            }
            Log::Debug("dumper: thread: 0x{:X}", (uintptr_t) thr);

            Image* corlib = Image::GetCorlib();
            if (!corlib) {
                Log::Error("dumper: cannot obtain mscorlib.dll");
                return;
            }
            Log::Debug("dumper: corlib: 0x{:X}", (uintptr_t) corlib);

            InitReflection(corlib);
            InitReflectionAtt(corlib);

            if (getUnityVer) {
                auto verS = (Api::String*) getUnityVer->Invoke(nullptr);
                auto ver = verS ? U16ToU8(verS->GetChars()) : "null";
                Log::Debug("dumper: unity version: {}", ver);
            }

            auto nm = corlib->GetName();
            Log::Debug("dumper: corlib: {}", nm);

            auto asms = dmn->GetAssemblies();
            Log::Debug("dumper: assemblies: {:d}", asms.size());

            std::filesystem::path dir = std::filesystem::path(extDirS) / "runtime-dumper";
            if (std::filesystem::exists(dir)) {
                std::filesystem::remove_all(dir);
                Log::Debug("dumper: cleaned dir");
            }
            
            if (!std::filesystem::create_directories(dir)) {
                Log::Debug("dumper: cannot create dir, {}", dir.c_str());
                return;
            }

            Log::Debug("dumper: dumping...");
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
            Log::Debug("dumper: dump success ({:.3f}s)", durSec);
            Log::Debug("dumper: output dir: {}", dir.c_str());

            Log::Debug("dumper: detaching il2cpp...");
            thr->Detach();

            Log::Debug("dumper: all finished!");
        }
    };
}