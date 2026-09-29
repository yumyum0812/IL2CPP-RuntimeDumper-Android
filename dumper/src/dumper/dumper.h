#include <chrono>
#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "api/functions.h"
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

            if (arrGetValue && rtGetGenericArgs && (klass->IsGeneric()) || klass->IsInflated()) {
                auto to = typ->GetObject();
                auto args = (Api::Array*) rtGetGenericArgs->Invoke(to);
                
                std::string purename = klass->GetName();
                size_t pos = purename.find('`');
                purename = (pos != std::string::npos) ? purename.substr(0, pos) : purename;

                int len = args->GetLength();
                std::vector<std::string> ps;
                for (int i = 0; i < len; i++) {
                    auto p = arrGetValue->Invoke(args, i);
                    auto pt = Api::Class::FromSystemType(p);
                    ps.push_back(FormatTypeName(pt->GetType(), def));
                }
                
                return purename + "<" + JoinStr(ps, ",") + ">";
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

        static Api::Method* FindMethod(Api::Class* klass, const std::string& name, const std::string& rtName, const std::vector<std::string>& pNames) {
            auto methods = klass->GetMethods();

            for (auto m : methods) {
                if (m->GetName() != name) continue;

                auto rt = m->GetReturnType();
                if (!rt || rt->GetName() != rtName) continue;

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

        void InitReflection(Api::Image* corlib) {
            auto vtKlass = corlib->FindClass("System", "ValueType");
            if (!vtKlass) {
                Log::Warn("dumper: System.ValueType not found");
            } else {
                structBoxSize = vtKlass->GetInstanceSize();
                Log::Debug("dumper: struct box size: 0x{:X}", structBoxSize);
            }

            auto typeKlass = corlib->FindClass("System", "RuntimeType");
            if (!typeKlass) {
                Log::Warn("dumper: System.RuntimeType not found");
            } else {
                rtGetGenericArgs = FindMethod(typeKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("dumper: System.RuntimeType.GetGenericArguments: 0x{:X}", (uintptr_t) rtGetGenericArgs);
            }

            auto methodKlass = corlib->FindClass("System.Reflection", "RuntimeMethodInfo");
            if (!methodKlass) {
                Log::Warn("dumper: System.Reflection.RuntimeMethodInfo not found");
            } else {
                rmGetGenericArgs = FindMethod(methodKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("dumper: System.Reflection.RuntimeMethodInfo.GetGenericArguments: 0x{:X}", (uintptr_t) rmGetGenericArgs);
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
            auto typeE = type->GetTypeEnum();
            auto klass = Api::Class::FromType(type);
            if (klass->IsEnum()) {
                type = klass->GetEnumBaseType();
            }

            std::string out;

            auto appendInteger = [&]<std::integral T>() {
                T v = fld->GetStaticValue<T>();
                out += std::format("{:d}", v);
            };

            auto appendFloat = [&]<std::floating_point T>() {
                T v = fld->GetStaticValue<T>();
                out += FormatFloat<T>(v);
            };

            switch (typeE) {
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

                out << INDENT;
                out << FormatFieldAccessor(f);
                out << FormatTypeName(fType);
                out << " ";
                out << f->GetName();
                out << FormatFieldValue(f);
                out << ";";

                if (!(fFlag & Api::FieldAttributes::LITERAL) && !(fFlag & Api::FieldAttributes::STATIC)) {
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
                out << std::format("// RVA: {} Offset: {}", FormatOffset(rva), FormatOffset(off));
                out << "\n";

                out << INDENT;
                out << FormatMethodAccessor(m);
                out << FormatTypeName(rt);
                out << " ";
                out << mName;
                
                if ((isInflated || isGeneric) && arrGetValue && rmGetGenericArgs) {
                    auto to = m->GetObject();
                    auto args = (Api::Array*) rmGetGenericArgs->Invoke(to);
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

            Image* corlib = Domain::GetCorlib();
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

            std::filesystem::path dir = std::filesystem::path(extDir) / "dump";
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