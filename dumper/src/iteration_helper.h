#pragma once
#include "dumper_log.h"
#include "finder.h"
#include "api/object.h"


namespace IterationHelper {
    inline void IterateIList(Api::Object* obj, std::string_view tName, std::function<void(Api::Object* v)> iter) {
        Api::Class* klass = obj->GetClass();

        Api::Method* vGetCount = Finder::FindMethodI(klass, "get_Count", "System.Int32", {});
        if (!vGetCount) return;

        Api::Method* vGetItem = Finder::FindMethodI(klass, "get_Item", tName, {"System.Int32"});
        if (!vGetItem) return;

        vGetCount = obj->GetVirtualMethod(vGetCount);
        vGetItem = obj->GetVirtualMethod(vGetItem);

        if (!vGetCount) {
            DumperLog::Warn("get_Count is null for {}", klass->GetName());
            return;
        }
        if (!vGetItem) {
            DumperLog::Warn("get_Item is null for {}", klass->GetName());
            return;
        }

        Api::Object* countBox = vGetCount->Invoke(obj);
        if (!countBox)
            return;

        int count = *(int*) countBox->Unbox();
        for (int i = 0; i < count; i++) {
            Api::Object* v = vGetItem->Invoke(obj, i);
            iter(v);
        }
    }
}