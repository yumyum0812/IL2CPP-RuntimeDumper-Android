#pragma once
#include "api/method.h"
#include "api/class.h"
#include "api/class.h"

namespace ReflectionMethods {
    struct {
        Api::Method* get_unityVersion{};
    } C_UnityEngine;

    struct {
        Api::Method* GetValue;
    } C_Array;

    struct {
        Api::Method* get_Count{};
    } C_ICollection;

    struct {
        Api::Class* klass{};
        Api::Method* get_Item{};
    } C_IList;

    struct {
        Api::Class* klass{};
        Api::Method* GetGenericArguments{};
        Api::Method* GetField{};
        Api::Method* GetProperty{};
        Api::Method* GetInterfaces{};
    } C_Type;

    struct {
        Api::Method* GetGenericArguments{};
        Api::Method* GetParameters{};
    } C_MethodBase;

    struct {
        Api::Method* get_Name{};
    } C_MemberInfo;

    struct {
        Api::Method* GetCustomAttributesM{};
        Api::Method* GetCustomAttributesP{};
        Api::Method* get_AttributeType{};
        Api::Method* get_ConstructorArguments{};
        Api::Method* get_NamedArguments{};
    } C_CustomAttributeData;

    struct {
        Api::Method* get_MemberName{};
        Api::Method* get_MemberInfo{};
        Api::Method* get_TypedValue{};
    } C_CustomAttributeNamedArgument;

    struct {
        Api::Class* klass{};
        Api::Method* get_Value{};
    } C_CustomAttributeTypedArgument;

    struct ParameterInfo {
        Api::Method* get_Attributes{};
        Api::Method* get_Name{};
        Api::Method* get_Type{};
        Api::Method* get_DefaultValue{};
    } C_ParameterInfo;
}