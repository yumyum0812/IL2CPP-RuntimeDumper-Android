#include "api/functions.h"
#include "api/assembly.h"
#include "api/consts.h"
#include "api/thread.h"
#include "api/domain.h"
#include "api/string.h"
#include "api/array.h"
#include "jni_util/android_util.h"
#include "jni_util/jni_util.h"
#include "log.h"

#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>


namespace Dumper {
    using ApiResolver = Api::Functions::ApiResolver;
    using SymApiResolver = Api::Functions::SymApiResolver;

    class RuntimeDumper {
        uintptr_t libBase;

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
            {Api::TypeAttributes::NESTED_FAM_AND_ASSEM, "internal"},
            {Api::TypeAttributes::NESTED_ASSEMBLY, "internal"},
            {Api::TypeAttributes::NESTED_PRIVATE, "private"},
            {Api::TypeAttributes::NESTED_FAMILY, "protected"},
            {Api::TypeAttributes::NESTED_FAM_OR_ASSEM, "protected internal"},
        };

        inline const static std::unordered_map<int, std::string> methodAccessors = {
            {Api::MethodAttributes::PUBLIC, "public"},
            {Api::MethodAttributes::PRIVATE, "private"},
            {Api::MethodAttributes::ASSEM, "internal"},
            {Api::MethodAttributes::FAMILY, "protected"},
            {Api::MethodAttributes::FAM_OR_ASSEM, "protected internal"},
            {Api::MethodAttributes::FAM_AND_ASSEM, "private protected"},
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

            auto app = AndroidUtil::GetApplication(env);
            if (!app) {
                Log::Error("dumper: jni: cannot obtain Application");
                return "";
            }
            Log::Debug("dumper: jni: obtained Application");

            auto extDir = AndroidUtil::GetExternalFilesPath(env, app);
            if (extDir.empty()) {
                Log::Error("dumper: jni: cannot obtain externalFilesDir");
                return "";
            }

            Log::Debug("dumper: jni: detaching JNI...");
            return extDir;
        }

        std::string FormatTypeName(Api::Type* typ, bool def = false) {
            auto klass = Api::Class::FromType(typ);
            auto tp = typ->GetType();

            std::string res;

            auto dt = klass->GetDeclaringType();
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
                std::string ps;
                for (int i = 0; i < len; i++) {
                    auto p = arrGetValue->Invoke(args, i);
                    auto pt = Api::Class::FromSystemType(p);
                    if (i >= 1) ps += ", ";
                    ps += FormatTypeName(pt->GetType(), def);
                }
                
                return purename + "<" + ps + ">";
            }

            return res + klass->GetName();;
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

            std::ostringstream out;

            int flags = met->GetFlags();

            int acc = flags & MEMBER_ACCESS_MASK;
            if (methodAccessors.contains(acc)) {
                out << methodAccessors.at(acc) << " ";
            }

            if (flags & STATIC)
                out << "static ";
            
            if (flags & ABSTRACT) {
                out << "abstract ";
                if ((flags & VTABLE_LAYOUT_MASK) == REUSE_SLOT)
                    out << "override ";

            } else if (flags & FINAL) {
                if ((flags & VTABLE_LAYOUT_MASK) == REUSE_SLOT)
                    out << "sealed override ";

            } else if (flags & VIRTUAL) {
                if ((flags & VTABLE_LAYOUT_MASK) == NEW_SLOT)
                    out << "virtual ";
                else
                    out << "override ";
            }
            
            if (flags & PINVOKE_IMPL)
                out << "extern ";

            return out.str();
        }

        static std::string FormatFieldAccessor(Api::Field* fld) {
            using namespace Api::FieldAttributes;

            std::string out;

            int flags = fld->GetFlags();
            switch (flags & FIELD_ACCESS_MASK) {
                case PRIVATE:
                    out += "private ";
                    break;
                case PUBLIC:
                    out += "public ";
                    break;
                case FAMILY:
                    out += "protected ";
                    break;
                case ASSEMBLY:
                case FAM_AND_ASSEM:
                    out += "internal ";
                    break;
                case FAM_OR_ASSEM:
                    out += "protected internal ";
                    break;
            }

            if (flags & LITERAL) {
                out += "const ";
            } else {
                if (flags & STATIC) {
                    out += "static ";
                }
                if (flags & INIT_ONLY) {
                    out += "readonly ";
                }
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
                if (isOut && !isIn)
                    out += "out ";
                else if (isIn && !isOut)
                    out += "in ";
                else
                    out += "ref ";
            } else {
                if (isIn)
                    out += "[In] ";
                if (isOut)
                    out += "[Out] ";
            }
            return out;
        }

        static Api::Method* FindMethod(Api::Class* klass, const std::string& name, const std::string& rt, const std::vector<std::string>& params) {
            auto mets = klass->GetMethods();
            for (auto m : mets) {
                if (m->GetName() != name) continue;
                if (m->GetReturnType()->GetName() != rt) continue;

                auto pc = m->GetParamCount();
                if (pc != params.size()) continue;

                bool ok = true;
                for (int i = 0; i < pc; i++) {
                    auto c = m->GetParam(i);
                    if (c->GetName() != params[i]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) return m;
            }
            return nullptr;
        }

        void InitReflection(Api::Image* corlib) {
            auto typeKlass = corlib->FindClass("System", "RuntimeType");
            if (!typeKlass) {
                Log::Warn("System.RuntimeType not found");
            } else {
                rtGetGenericArgs = FindMethod(typeKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("System.RuntimeType.GetGenericArguments: 0x{:X}", (uintptr_t) rtGetGenericArgs);
            }

            auto methodKlass = corlib->FindClass("System.Reflection", "RuntimeMethodInfo");
            if (!methodKlass) {
                Log::Warn("System.Reflection.RuntimeMethodInfo not found");
            } else {
                rmGetGenericArgs = FindMethod(methodKlass, "GetGenericArguments", "System.Type[]", {});
                Log::Debug("System.Reflection.RuntimeMethodInfo.GetGenericArguments: 0x{:X}", (uintptr_t) rmGetGenericArgs);
            }

            auto arrKlass = corlib->FindClass("System", "Array");
            if (!arrKlass) {
                Log::Warn("System.Array not found");
            } else {
                arrGetValue = FindMethod(arrKlass, "GetValue", "System.Object", {"System.Int32"});
                Log::Debug("System.Array.GetValue: 0x{:X}", (uintptr_t) arrGetValue);
            }
        }

        static std::string FormalStringLiteral(const std::string& input) {
            std::string result;
            for (char c : input) {
                switch (c) {
                    case '\\': result += "\\\\"; continue;
                    case '"':  result += "\\\""; continue;
                    case '\'': result += "\\\'"; continue;
                    case '\0': result += "\\0";  continue;
                    case '\a': result += "\\a";  continue;
                    case '\f': result += "\\f";  continue;
                    case '\n': result += "\\n";  continue;
                    case '\r': result += "\\r";  continue;
                    case '\t': result += "\\t";  continue;
                    case '\v': result += "\\v";  continue;
                }
                if (std::iscntrl(c)) {
                    result += "\\x" + std::to_string(static_cast<int>(static_cast<unsigned char>(c)));
                    continue;
                }
                result += c;
            }
            return "\"" + result + "\"";
        }

        static std::string Utf16ToUtf8(const std::u16string& u16str) {
            std::u8string u8str = std::filesystem::path(u16str).u8string();
            std::string utf8str(reinterpret_cast<const char*>(u8str.data()), u8str.length());
            return utf8str;
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

            switch (type->GetType()) {
                case Api::TypeEnum::CHAR: {
                    uint16_t by = fld->GetStaticValue<uint16_t>();
                    out += std::to_string(static_cast<int>(by));
                } break;
                case Api::TypeEnum::BOOLEAN: {
                    bool value = fld->GetStaticValue<bool>();
                    out += value ? "true" : "false";
                } break;
                case Api::TypeEnum::I1: {
                    int8_t value = fld->GetStaticValue<int8_t>();
                    out += std::to_string(static_cast<int>(value));
                } break;
                case Api::TypeEnum::U1: {
                    uint8_t value = fld->GetStaticValue<uint8_t>();
                    out += std::to_string(static_cast<int>(value));
                } break;
                case Api::TypeEnum::I2: {
                    int16_t value = fld->GetStaticValue<int16_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::U2: {
                    uint16_t value = fld->GetStaticValue<uint16_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::I4: {
                    int32_t value = fld->GetStaticValue<int32_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::U4: {
                    uint32_t value = fld->GetStaticValue<uint32_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::I8: {
                    int64_t value = fld->GetStaticValue<int64_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::U8: {
                    uint64_t value = fld->GetStaticValue<uint64_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::I: {
                    intptr_t value = fld->GetStaticValue<uintptr_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::U: {
                    uintptr_t value = fld->GetStaticValue<uintptr_t>();
                    out += std::to_string(value);
                } break;
                case Api::TypeEnum::R4: {
                    float value = fld->GetStaticValue<float>();
                    out += std::format("{}", value);
                } break;
                case Api::TypeEnum::R8: {
                    double value = fld->GetStaticValue<double>();
                    out += std::format("{}", value);
                } break;
                case Api::TypeEnum::STRING: {
                    auto* value = fld->GetStaticValue<Api::String*>();
                    if (!value) out += "null";
                    auto u16s = value->ToU16String();
                    auto s = Utf16ToUtf8(u16s);
                    out += FormalStringLiteral(s);
                } break;
                case Api::TypeEnum::CLASS:
                case Api::TypeEnum::OBJECT: {
                    auto* value = fld->GetStaticValue<Api::Object*>();
                    if (!value) out += "null";
                } break;
            }

            if (!out.empty()) {
                out = " = " + out;
            }

            return out;
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

            for (auto a : asms) {
                Image* img = a->GetImage();
                if (!img) continue;

                std::ofstream out(dir / (img->GetName() + ".cs"), std::ios::trunc);

                auto clss = img->GetClasses();
                for (auto c : clss) {
                    out << std::format("// Namespace: {}\n", c->GetNamespace());

                    std::string ext;
                    auto par = c->GetParent();
                    if (c->IsEnum()) {
                        par = Api::Class::FromType(c->GetEnumBaseType());
                    }
                    if (par) {
                        int tp = par->GetType()->GetType();
                        if (tp == Api::TypeEnum::VALUETYPE || tp == Api::TypeEnum::ENUM || tp == Api::TypeEnum::OBJECT) {
                            par = nullptr;
                        }
                    }
                    if (par) {
                        ext += " : ";
                        ext += FormatTypeName(par->GetType());
                        if (c->GetName() == "FreeRunOnline") Log::Debug("{}", ext);
                    }

                    auto ints = c->GetInterfaces();
                    if (!ints.empty()) {
                        ext += ext.empty() ? " : " : ", ";
                    }
                    for (int i = 0; i < ints.size(); i++) {
                        if (i >= 1) {
                            ext += ", ";
                        }
                        ext += FormatTypeName(ints[i]->GetType());
                    }
                    ext += " ";

                    out << std::format("{}{}{}{}{{\n", FormatTypeAccessor(c), FormatTypeType(c), FormatTypeName(c->GetType(), true), ext);

                    auto flds = c->GetFields();
                    for (auto f : flds) {
                        auto t = f->GetType();
                        auto ff = f->GetFlags();

                        std::string offstr = "";
                        if (!(ff & Api::FieldAttributes::LITERAL) && !(ff & Api::FieldAttributes::STATIC)) {
                            offstr = std::format(" // 0x{:X}", f->GetOffset());
                        }
                        out << std::format("    {}{} {}{};{}\n", FormatFieldAccessor(f), FormatTypeName(t), f->GetName(), FormatFieldValue(f), offstr);
                    }

                    auto mtds = c->GetMethods();
                    for (auto m : mtds) {
                        auto t = m->GetReturnType();

                        auto count = m->GetParamCount();
                        std::string ps;
                        for (int i = 0; i < count; i++) {
                            auto pt = m->GetParam(i);
                            auto pn = m->GetParamName(i);

                            if (i >= 1) ps += ", ";
                            ps += std::format("{}{} {}", FormatParamType(pt), FormatTypeName(pt), pn);
                        }

                        uintptr_t va = 0;
                        uintptr_t rva = 0;
                        uintptr_t off = 0;
                        if (!m->IsGeneric()) {
                            va = (uintptr_t) m->GetPointer();
                            if (va != UINTPTR_MAX && va != 0) {
                                rva = va - libBase;
                                off = libRes.RVAToOffset(rva);
                            }
                        }

                        out << "\n";
                        out << std::format("    // RVA: 0x{:X}, Offset: 0x{:X}, VA: 0x{:X}\n", rva, off, va);
                        out << std::format("    {}{} {}({}) {{ }}\n", FormatMethodAccessor(m), FormatTypeName(t), m->GetName(), ps);
                    }

                    out << "}\n";
                    out << "\n";
                }

                out.flush();
                out.close();
            }

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