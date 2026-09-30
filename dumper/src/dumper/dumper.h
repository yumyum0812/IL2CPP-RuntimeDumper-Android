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
        Api::Method* arrGetValue = nullptr;
        Api::Method* rtGetGenericArgs = nullptr;
        Api::Method* rmGetGenericArgs = nullptr;

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
            {'\n', "\\\n"},
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

            if (arrGetValue && rtGetGenericArgs && (klass->IsGeneric() || klass->IsInflated())) {
                auto to = typ->GetObject();
                auto getGensV = to->GetVirtualMethod(rtGetGenericArgs);
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

        Api::Method* FindMethod(Api::Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames, bool ancestor = false) {
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

            if (ancestor) {
                auto par = klass->GetParent();
                auto inters = klass->GetInterfaces();
                inters.push_back(par);
                
                for (auto p : inters) {
                    if (p && p != klass) {
                        auto met = FindMethod(p, name, rtName, pNames, true);
                        if (met) return met;
                    }
                }
            }

            return nullptr;
        }

        std::vector<Api::Object*> ObjListToVector(Api::Object* obj) {
            auto klass = obj->GetClass();

            // auto getCount = FindMethod(klass, "get_Count", "System.Int32", {}, true);
            auto getItem = FindMethod(klass, "get_Item", "", {"System.Int32"}, true);

            auto cor = Api::Image::GetCorlib();
            auto k1 = cor->FindClass("System.Collections", "ICollection`1");
            auto gc1 = FindMethod(k1, "get_Item", "System.Int32", {}, false);

            auto getCount = obj->GetVirtualMethod(gc1);

            // auto getItem = FindMethod(klass, "get_Item", "", {"System.Int32"}, true);

            if (!getCount) {
                Log::Warn("test: get_Count is null for {}", FormatTypeName(klass->GetType()));
                return {};
            }
            if (!getItem) {
                Log::Warn("test: get_Item is null for {}", FormatTypeName(klass->GetType()));
                return {};
            }

            auto countObj = getCount->Invoke(obj);
            if (!countObj) {
                Log::Error("getCount->Invoke returned null: {}, {}", FormatTypeName(klass->GetType()), getCount->GetReturnType()->GetName());
                Log::Debug("rva: {} ", getCount ? FormatOffset(uintptr_t(getCount->methodPtr) - libBase) : "?");
                return {};
            }
            auto unboxed = countObj->Unbox();
            if (!unboxed) {
                Log::Error("Count Unbox returned null");
                return {};
            }

            int count = *reinterpret_cast<int*>(unboxed);

            std::vector<Api::Object*> vec;
            vec.reserve(count);

            for (int i = 0; i < count; i++) {
                vec.push_back(getItem->Invoke(obj, i));
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

            auto typeKlass = corlib->FindClass("System", "Type");
            if (!typeKlass) {
                Log::Warn("dumper: Type not found");
            } else {
                rtGetGenericArgs = FindMethod(typeKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("dumper: Type.GetGenericArguments: 0x{:X}", (uintptr_t) rtGetGenericArgs);
            }

            auto methodKlass = corlib->FindClass("System.Reflection", "MethodBase");
            if (!methodKlass) {
                Log::Warn("dumper: MethodBase not found");
            } else {
                rmGetGenericArgs = FindMethod(methodKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("dumper: MethodBase.GetGenericArguments: 0x{:X}", (uintptr_t) rmGetGenericArgs);
            }

            auto arrKlass = corlib->FindClass("System", "Array");
            if (!arrKlass) {
                Log::Warn("dumper: System.Array not found");
            } else {
                arrGetValue = FindMethod(arrKlass, "GetValue", "System.Object", {"System.Int32"});
                Log::Debug("dumper: System.Array.GetValue: 0x{:X}", (uintptr_t) arrGetValue);
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

        std::string FormatValue(Api::Object* obj) {
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
            }

            if (out.empty()) {
                out += "?";
            }

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

            out << FormatTypeAccessor(klass) + FormatTypeType(klass) + FormatTypeName(klass->GetType(), true) + ext + " {" + FormatClassSpecial(klass);
            out << "\n";

            auto fields = klass->GetFields();
            for (auto f : fields) {
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

                auto o = m->GetObject();
                auto methodKlass = o->GetClass();

                // auto mtd = FindMethod(methodKlass, "GetCustomAttributesData", "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>", {}, true);
                // if (mtd) {
                //     auto lst = mtd->Invoke(o);
                //     auto vec = ObjListToVector(lst);

                //     if (!vec.empty()) {
                //         static auto corlib = Api::Image::GetCorlib();
                //         auto cadC = corlib->FindClass("System.Reflection", "CustomAttributeData");
                //         auto mtdCAD = FindMethod(cadC, "get_AttributeType", "System.Type", {}, true);

                //         for (int i = 0; i < vec.size(); i++) {
                //             auto attData = vec[i];
                //             if (!attData) continue;

                //             auto attType = mtdCAD->Invoke(attData);
                //             if (!attType) continue;

                //             auto dataC = attData->GetClass();
                //             if (!dataC) continue;

                //             auto getCAs = FindMethod(dataC, "get_ConstructorArguments", "System.Collections.Generic.IList<System.Reflection.CustomAttributeTypedArgument>", {}, true);
                //             if (!getCAs) continue;

                //             auto getNAs = FindMethod(dataC, "get_NamedArguments", "System.Collections.Generic.IList<System.Reflection.CustomAttributeNamedArgument>", {}, true);
                //             if (!getNAs) continue;

                //             auto cataC = corlib->FindClass("System.Reflection", "CustomAttributeTypedArgument");
                //             if (!cataC) continue;

                //             auto cataV = FindMethod(cataC, "get_Value", "System.Object", {}, true);
                //             if (!cataV) continue;

                //             auto canaC = corlib->FindClass("System.Reflection", "CustomAttributeNamedArgument");
                //             if (!canaC) continue;

                //             auto canaMN = FindMethod(canaC, "get_MemberName", "System.String", {}, true);
                //             if (!canaMN) continue;

                //             auto canaTV = FindMethod(canaC, "get_TypedValue", "System.Reflection.CustomAttributeTypedArgument", {}, true);
                //             if (!canaTV) continue;

                //             auto cas = getCAs->Invoke(attData);
                //             if (!cas) continue;

                //             auto nas = getNAs->Invoke(attData);
                //             if (!nas) continue;

                //             auto casV = ObjListToVector(cas);
                //             auto nasV = ObjListToVector(nas);

                //             std::vector<std::string> argstrs;
                //             for (int i = 0; i < casV.size(); i++) {
                //                 auto cata = casV[i]->Unbox();
                //                 if (!cata) continue;

                //                 auto aV = cataV->Invoke(cata);
                //                 argstrs.push_back(FormatValue(aV));
                //             }
                //             for (int i = 0; i < nasV.size(); i++) {
                //                 auto cana = nasV[i]->Unbox();
                //                 if (!cana) continue;

                //                 auto memberNameS = (Api::String*) canaMN->Invoke(cana);
                //                 auto memberName = U16ToU8(memberNameS->GetChars());

                //                 auto cata = canaTV->Invoke(cana)->Unbox();
                //                 auto aV = cataV->Invoke(cata);

                //                 argstrs.push_back(memberName + " = " + FormatValue(aV));
                //             }

                //             auto attC = Api::Class::FromSystemType(attType);
                //             auto attTypeName = attC->GetName();
                //             if (attTypeName.ends_with("Attribute"))
                //                 attTypeName.erase(attTypeName.size() - (sizeof("Attribute") - 1));

                //             out << INDENT;
                //             out << "[";
                //             out << attTypeName;
                //             if (!argstrs.empty()) {
                //                 out << "(";
                //                 out << JoinStr(argstrs, ", ");
                //                 out << ")";
                //             }
                //             out << "]";
                //             out << "\n";
                //         }
                //     }
                // }

                out << INDENT;
                out << FormatMethodAccessor(m);
                out << FormatTypeName(rt);
                out << " ";
                out << mName;
                
                if ((isInflated || isGeneric) && arrGetValue && rmGetGenericArgs) {
                    auto to = m->GetObject();
                    auto getGensV = to->GetVirtualMethod(rmGetGenericArgs);
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

            auto libRes = Functions::LibraryResolver();
            auto res = SymApiResolver();

            bool init = Api::Functions::InitApi(libRes, res);
            if (!init) {
                Log::Debug("dumper: initialization failed");
                return;
            }

            libBase = libRes.GetBaseAddress();
            Log::Debug("dumper: base: {:#x}", libBase);
            if (libBase == 0) {
                Log::Debug("dumper: warning: base address got null");
            }

            Domain* dmn = Domain::GetInstance();
            Log::Debug("dumper: domain: 0x{:X}", (uintptr_t) dmn);

            Thread* thr = Thread::Attach(dmn);
            Log::Debug("dumper: thread: 0x{:X}", (uintptr_t) thr);

            Image* corlib = Image::GetCorlib();
            Log::Debug("dumper: corlib: 0x{:X}", (uintptr_t) corlib);

            InitReflection(corlib);

            auto nm = corlib->GetName();
            Log::Debug("dumper: corlib: {}", nm);

            auto asms = dmn->GetAssemblies();
            Log::Debug("dumper: assemblies: {:d}", asms.size());

            auto extDir = GetAppExtDir();
            if (extDir.empty()) {
                Log::Debug("dumper: cannot obtain external files dir");
                return;
            }
            Log::Debug("dumper: obtained external files dir: {}", extDir);

            std::filesystem::path dir = std::filesystem::path(extDir) / "runtime-dumper";
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