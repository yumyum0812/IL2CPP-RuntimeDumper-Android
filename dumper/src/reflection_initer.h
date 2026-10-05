#pragma once
#include "api/class.h"
#include "api/image.h"
#include "api/method.h"
#include "log.h"
#include "reflection_methods.h"
#include "finder.h"


namespace ReflectionIniter {
    Api::Class* FindReflClass(Api::Image* img, const std::string& ns, const std::string& name) {
        Api::Class* ret = img->FindClass(ns, name);
        if (!ret)
            Log::Warn("class {}.{} not found", ns, name);

        return ret;
    }

    void FindReflMethod(Api::Class* klass, std::string_view name, std::string_view rt, const std::vector<std::string_view>& params, Api::Method** method, std::string_view debugSuffix = {}) {
        if(!(*method = Finder::FindMethod(klass, name, rt, params)))
            Log::Warn("method {}.{}{} not found", klass->GetName(), name, debugSuffix);
    }

    void InitUnityReflection() {
        using namespace ReflectionMethods;

        Api::Image* image = Finder::FindImage("UnityEngine.CoreModule.dll");
        if (!image) {
            Log::Warn("image UnityEngine.CoreModule.dll not found");
            return;
        }

        Api::Class* appC = FindReflClass(image, "UnityEngine", "Application");
        if (!appC) return;

        FindReflMethod(appC,
            "get_unityVersion",
            "System.String",
            {},
            &C_UnityEngine.get_unityVersion
        );
    }

    int GetBoxSize(Api::Image* corlib) {
        Api::Class* objC = FindReflClass(corlib, "System", "Object");
        if (!objC) return 0;

        int sz = objC->GetInstanceSize();
        Log::Debug("object box size: 0x{:X}", sz);
        return sz;
    }

    void InitReflection(Api::Image* corlib) {
        using namespace ReflectionMethods;

        C_Type.klass = FindReflClass(corlib, "System", "Type");
        if (C_Type.klass) {
            FindReflMethod(
                C_Type.klass,
                "GetGenericArguments",
                "System.Type[]",
                {},
                &C_Type.GetGenericArguments
            );
            FindReflMethod(
                C_Type.klass,
                "GetField",
                "System.Reflection.FieldInfo",
                {"System.String"},
                &C_Type.GetField
            );
            FindReflMethod(
                C_Type.klass,
                "GetProperty",
                "System.Reflection.PropertyInfo",
                {"System.String", "System.Reflection.BindingFlags"},
                &C_Type.GetProperty
            );
        }

        Api::Class* methodC = FindReflClass(corlib, "System.Reflection", "MethodBase");
        if (methodC) {
            FindReflMethod(
                methodC,
                "GetGenericArguments",
                "System.Type[]",
                {},
                &C_MethodBase.GetGenericArguments
            );
            FindReflMethod(
                methodC,
                "GetParameters",
                "System.Reflection.ParameterInfo[]",
                {},
                &C_MethodBase.GetParameters
            );
        }

        Api::Class* parC = FindReflClass(corlib, "System.Reflection", "ParameterInfo");
        if (parC) {
            FindReflMethod(
                parC,
                "get_Attributes",
                "System.Reflection.ParameterAttributes",
                {},
                &C_ParameterInfo.get_Attributes
            );
            FindReflMethod(
                parC,
                "get_DefaultValue",
                "System.Object",
                {},
                &C_ParameterInfo.get_DefaultValue
            );
            FindReflMethod(
                parC,
                "get_Name",
                "System.String",
                {},
                &C_ParameterInfo.get_Name
            );
            FindReflMethod(
                parC,
                "get_ParameterType",
                "System.Type",
                {},
                &C_ParameterInfo.get_Type
            );
        }

        Api::Class* arrC = FindReflClass(corlib, "System", "Array");
        if (arrC) {
            FindReflMethod(
                arrC,
                "GetValue",
                "System.Object",
                {"System.Int32"},
                &C_Array.GetValue
            );
        }

        Api::Class* iColl = FindReflClass(corlib, "System.Collections", "ICollection");
        if (iColl) {
            FindReflMethod(
                iColl,
                "get_Count",
                "System.Int32",
                {},
                &C_ICollection.get_Count
            );
        }
        
        C_IList.klass = FindReflClass(corlib, "System.Collections", "IList");
        if (C_IList.klass) {
            FindReflMethod(
                C_IList.klass,
                "get_Item",
                "System.Object",
                {"System.Int32"},
                &C_IList.get_Item
            );
        }

        Api::Class* memberInfoC = FindReflClass(corlib, "System.Reflection", "MemberInfo");
        if (memberInfoC) {
            FindReflMethod(
                memberInfoC,
                "get_Name",
                "System.String",
                {},
                &C_MemberInfo.get_Name
            );
        }

        Api::Class* customAttC = FindReflClass(corlib, "System.Reflection", "CustomAttributeData");
        if (customAttC) {
            FindReflMethod(
                customAttC,
                "GetCustomAttributes",
                "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
                {"System.Reflection.MemberInfo"},
                &C_CustomAttributeData.GetCustomAttributesM,
                "(M)"
            );

            FindReflMethod(
                customAttC,
                "GetCustomAttributes",
                "System.Collections.Generic.IList<System.Reflection.CustomAttributeData>",
                {"System.Reflection.ParameterInfo"},
                &C_CustomAttributeData.GetCustomAttributesP,
                "(P)"
            );

            FindReflMethod(
                customAttC,
                "get_AttributeType",
                "System.Type",
                {},
                &C_CustomAttributeData.get_AttributeType
            );

            FindReflMethod(
                customAttC,
                "get_ConstructorArguments",
                "System.Collections.Generic.IList<System.Reflection.CustomAttributeTypedArgument>",
                {},
                &C_CustomAttributeData.get_ConstructorArguments
            );

            FindReflMethod(
                customAttC,
                "get_NamedArguments",
                "System.Collections.Generic.IList<System.Reflection.CustomAttributeNamedArgument>",
                {},
                &C_CustomAttributeData.get_NamedArguments
            );
        }

        C_CustomAttributeTypedArgument.klass = FindReflClass(
            corlib,
            "System.Reflection",
            "CustomAttributeTypedArgument"
        );
        if (C_CustomAttributeTypedArgument.klass) {
            FindReflMethod(
                C_CustomAttributeTypedArgument.klass,
                "get_Value",
                "System.Object",
                {},
                &C_CustomAttributeTypedArgument.get_Value
            );
        }

        Api::Class* namedArgC = FindReflClass(
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
                &C_CustomAttributeNamedArgument.get_MemberName
            );

            FindReflMethod(
                namedArgC,
                "get_MemberInfo",
                "System.Reflection.MemberInfo",
                {},
                &C_CustomAttributeNamedArgument.get_MemberInfo
            );

            FindReflMethod(
                namedArgC,
                "get_TypedValue",
                "System.Reflection.CustomAttributeTypedArgument",
                {},
                &C_CustomAttributeNamedArgument.get_TypedValue
            );
        }
    }
}