#include "json/JSONDecoder.h"
#include "thirdParty/cjson/cJSON.h"
#include <assert.h>

namespace struct2x {

    JSONDecoder::JSONDecoder(const char* sz)
        : _root(cJSON_Parse(sz))
        , _cur(_root) {
    }


    JSONDecoder::~JSONDecoder() {
        if (_root)
            cJSON_Delete(_root);
    }

    JSONDecoder& JSONDecoder::operator >>(std::vector<int32_t>& value) {
        if (!value.empty()) value.clear();
        cJSON *c = _cur->child;
        while (c) {
            value.push_back(c->valueint);
            c = c->next;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::operator >>(std::vector<float>& value) {
        if (!value.empty()) value.clear();
        cJSON *c = _cur->child;
        while (c) {
            value.push_back((float)c->valuedouble);
            c = c->next;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::operator >>(std::vector<double>& value) {
        if (!value.empty()) value.clear();
        cJSON *c = _cur->child;
        while (c) {
            value.push_back(c->valuedouble);
            c = c->next;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::operator >>(std::vector<std::string>& value) {
        if (!value.empty()) value.clear();
        cJSON *c = _cur->child;
        while (c) {
            value.push_back(c->valuestring);
            c = c->next;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, bool& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            if (item->type == cJSON_False)
                value = false;
            else
                value = true;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, uint32_t& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valueint;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, int32_t& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valueint;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, uint64_t& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valuedouble;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, int64_t& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valuedouble;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, float& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = (float)item->valuedouble;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, double& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valuedouble;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::string& value, bool* pHas) {
        if (cJSON * item = cJSON_GetObjectItem(_cur, sz)) {
            value = item->valuestring;
            if (pHas) *pHas = true;
        }
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<bool>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back((bool)c->valueint);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<uint32_t>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back(c->valueint);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<int32_t>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back(c->valueint);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<uint64_t>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back((uint64_t)c->valuedouble);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<int64_t>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back((int64_t)c->valuedouble);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<float>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back((float)c->valuedouble);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<double>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back(c->valuedouble);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    JSONDecoder& JSONDecoder::convert(const char* sz, std::vector<std::string>& value, bool* pHas) {
        if (!value.empty()) value.clear();
        cJSON* curItem = cur();
        if (getObject(sz)) {
            for (cJSON *c = _cur->child; c; c = c->next) {
                value.push_back(c->valuestring);
            }
            if (pHas) *pHas = true;
        }
        cur(curItem);
        return *this;
    }

    bool JSONDecoder::getObject(const char* sz) {
        if (cJSON *fmt = cJSON_GetObjectItem(_cur, sz)) {
            assert(fmt);
            _cur = fmt;
            return true;
        }
        return false;
    }

    int32_t JSONDecoder::getArraySize()const {
        return cJSON_GetArraySize(_cur);
    }

    void JSONDecoder::getArrayItem(int32_t i) {
        cJSON *fmt = cJSON_GetArrayItem(_cur, i);
        assert(fmt);
        _cur = fmt;
    }

    int32_t JSONDecoder::getMapSize() const {
        return getArraySize();
    }

    const char* JSONDecoder::getChildName(int32_t i) const {
        cJSON *fmt = _cur->child;
        for (int32_t index = 0; index < i; ++index) {
            fmt = fmt->next;
        }
        assert(fmt);
        return fmt->string;
    }

}