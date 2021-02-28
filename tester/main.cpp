#include <iostream>
#include <assert.h>

#include "testStruct.h"

#include "json/encoder.h"
#include "json/decoder.h"

struct testStru {
    testStru() :i(0), db(0.0) {}
    int i;
    double db;

    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, i, db);
    }
};

struct struInfo {
    struInfo() :no(99) {}
    int no;
    testStru ts;
    std::vector<testStru> vts;
    std::vector<bool> v;
    std::map<int, int> m;

    bool operator ==(const struInfo& in)const {
        if (no != in.no)
            return false;
        if (v != in.v)
            return false;
        return true;
    }

    bool operator !=(const struInfo& in)const {
        return !operator==(in);
    }

    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, no, ts, vts, v, m);
    }
};

struct struItem {
    int id;
    std::string str;
    struInfo info;
    std::vector<std::string> v;
    std::vector<struInfo> v2;
    std::map<std::string, int> m;
    std::map<std::string, struInfo> m2;

    bool operator ==(const struItem& in) {
        if (id != in.id)
            return false;
        if (str != in.str)
            return false;
        if (v != in.v)
            return false;
        if (v2 != in.v2)
            return false;
        if (m != in.m)
            return false;
        if (m2 != in.m2)
            return false;
        return true;
    }

    template<typename T>
    void serialize(T& t)
    {
        SERIALIZE(t, id, str, info, v, v2, m, m2);
    }
};

//VISITSTRUCT(struItem, id, str, info, v, v2, m, m2)
//template<typename T>
//void serialize(T& t, struItem& item)
//{
//    NISERIALIZE(t, item, id, str, info, v, v2, m, m2);
//}


void testStructFunc() {
    struItem item;
    item.id = 1;
    item.str = "asdfgh";
    item.info.no = 99;
    item.v.push_back("11");
    item.v.push_back("22");
    struInfo info;
    item.v2.push_back(info);
    info.no = 68;
    item.v2.push_back(info);
    item.m["1"] = 11;
    item.m["2"] = 22;
    item.m2["1"] = info;
    info.no = 992;
    item.m2["2"] = info;

    //serialize::CJSONEncoder jr;
    //jr << item;
    //std::string str;
    //jr.toString(str);

    std::string str;
    serialize::JSONEncoder encoder2(str);
    encoder2 << item;
    //encoder.toString(str);

    serialize::JSONDecoder jw(str.c_str());
    struItem item2;
    jw >> item2;
    bool b = (item == item2);
    assert(b);
}

struct myStruct {
    std::vector<std::vector<int> > vec;
    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, vec);
    }
};


enum EnumType {
    ET1,
    ET2,
    ET3,
};

struct struExampleEnum {
    struExampleEnum() : id(), has_id(false), has_str(false), f(), db(), e(ET3) {}
    int32_t id;
    bool has_id;
    std::string str;
    bool has_str;
    float f;
    double db;
    std::vector<int32_t> v;
    EnumType e;

    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, id, str, f, db, v);
        //SERIALIZE(t, v);
    }
};

struct intXy {
    int32_t x;
    int32_t y;
    intXy(int32_t a = 0, int32_t b = 0) :x(a), y(b) {}

    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, x, y);
    }
};

struct arrayXy {
    std::vector<intXy> arr;
    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, arr);
    }
};

struct vecObject {
    std::vector<arrayXy> vec;
    template<typename T>
    void serialize(T& t) {
        SERIALIZE(t, vec);
    }
};

int main(int argc, char* argv[]) {
    //vecObject v;
    //arrayXy a;
    //a.arr.push_back(intXy(1, 1));
    //a.arr.push_back(intXy(2, 2));
    //v.vec.push_back(a);

    //std::string json;
    //bool bE = serialize::JSONEncoder(json) << v;

    //bool bD = serialize::JSONDecoder(json) >> v;

    //return 0;
    //struExampleEnum item;
    //item.e = ET2;
    //serialize::CJSONDecoder jw("{\"id\":10,\"str\":\"qa\",\"f\":11.0,\"db\":12.0,\"e\":2}");
    //jw >> item;

    //item.v.push_back(15); item.v.push_back(35);
    //std::string mpBuf;
    //serialize::MPEncoder mpe(mpBuf);
    //mpe << item;

    //for (size_t i = 0; i < mpBuf.size(); ++i)
    //    printf("%02x ", 0xff & mpBuf[i]);
    //printf("\n");

    //serialize::MPDecoder mpd((const uint8_t*)mpBuf.data(), mpBuf.size());
    //struExampleEnum item2;
    //mpd >> item2;

    struItem ins;
	std::string strJson("{\"id\":-11,\"str\":\"struct2json\",\"info\":{\"no\":99,\"v\":[false,true],\"m\":{}},\"v\":[\"false\",false],\"v2\":[],\"m\":{},\"m2\":{}}");
    serialize::JSONDecoder decoder(strJson);
    bool bDecode = decoder >> ins;

    std::string str;
    serialize::JSONEncoder encoder(str);
    bool bEncode = encoder << ins;

    return 0;
}