#ifndef __TEST_TEST1__H_
#define __TEST_TEST1__H_

#include "serialize.h"

#include<stdint.h>
#include <string>
#include <vector>
#include <map>

namespace testStruct {

    enum EnumType {
        ET1,
        ET2,
        ET3,
    };

    struct struExample {
        struExample() : id(), has_id(false), has_str(false), f(), db() {}
        EnumType id;
        bool has_id;
        std::string str;
        bool has_str;
        float f;
        double db;
        std::vector<int32_t> v;

        template<typename T>
        void serialize(T& t) {
            t & SERIALIZATION(1, id, &has_id) & SERIALIZATION(2, str, &has_str) & SERIALIZATION(3, f) & SERIALIZATION(4, db) &SERIALIZATION(5, v);
        }
    };

    struct struExamples {
        struExamples() {}
        std::vector<struExample> v;
        std::map<int32_t, struExample> m;

        template<typename T>
        void serialize(T& t) {
            t & SERIALIZATION(1, v) & SERIALIZATION(2, m);
        }
    };

}

#endif
