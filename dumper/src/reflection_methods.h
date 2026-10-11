#pragma once
#include "api/method.h"
#include "api/class.h"
#include "api/class.h"

namespace ReflectionMethods {
    inline struct {
        Api::Method* get_unityVersion{};
    } C_UnityEngine;

    inline struct {
        Api::Method* GetValue;
    } C_Array;

    inline struct {
        Api::Method* get_Count{};
    } C_ICollection;

    inline struct {
        Api::Class* klass{};
        Api::Method* get_Item{};
    } C_IList;

    inline struct {
        Api::Class* klass{};
        Api::Method* GetGenericArguments{};
        Api::Method* GetField{};
        Api::Method* GetProperty{};
        Api::Method* GetInterfaces{};
    } C_Type;

    inline struct {
        Api::Method* GetGenericArguments{};
        Api::Method* GetParameters{};
    } C_MethodBase;

    inline struct {
        Api::Method* get_Name{};
    } C_MemberInfo;

    inline struct {
        Api::Method* GetCustomAttributesM{};
        Api::Method* GetCustomAttributesP{};
        Api::Method* get_AttributeType{};
        Api::Method* get_ConstructorArguments{};
        Api::Method* get_NamedArguments{};
    } C_CustomAttributeData;

    inline struct {
        Api::Method* get_MemberName{};
        Api::Method* get_MemberInfo{};
        Api::Method* get_TypedValue{};
    } C_CustomAttributeNamedArgument;

    inline struct {
        Api::Class* klass{};
        Api::Method* get_Value{};
    } C_CustomAttributeTypedArgument;

    inline struct ParameterInfo {
        Api::Method* get_Attributes{};
        Api::Method* get_Name{};
        Api::Method* get_Type{};
        Api::Method* get_DefaultValue{};
    } C_ParameterInfo;
}