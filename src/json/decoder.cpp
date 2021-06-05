#include "json/decoder.h"
#include <assert.h>


namespace serialize {

    Handler::Handler(void* pStruct, const functionValueMgr& mgr) :_pStruct(pStruct), _converter(NULL), _mgr(&mgr) {
    }

    size_t Handler::offsetByValue(const void* cValue) const {
        return (const uint8_t*)cValue - (uint8_t*)_pStruct;
    }

    bool Handler::Key(const char* sz, unsigned length) {
        uint32_t nSize = _mgr->set.size();
        for (uint32_t idx = 0; idx < nSize; ++idx) {
            const function_value& item = _mgr->set.at(idx);
            if (strlen(item.first) == length && strncmp(sz, item.first, length) == 0) {
                _converter = &item.second;
                break;
            }
        }

        return true;
    }

    bool Handler::Value(const char* sz, unsigned length) {
        if (_converter) {
            (*_converter)((uint8_t*)_pStruct, sz, length);
            _converter = NULL;
        }
        return true;
    }

    /*------------------------------------------------------------------------------*/
    static bool Consume(StringStream& is, const char expect) {
        if (is.Peek() == expect) {
            is.Take();
            return true;
        }
        else
            return false;
    }

    static void SkipWhitespace(StringStream& is) {
        for (;;) {
            const char c = is.Peek();
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                is.Take();
            }
            else {
                break;
            }
        }
    }

    GenericReader::GenericReader() :_result(true) {
    }

    bool GenericReader::Parse(StringStream& is, BaseHandler& handler) {
        if (is.Peek() != '\0') {
            ParseValue(is, handler, false);
        }
        return _result;
    }

    const char* GenericReader::getError()const {
        return _strError.c_str();
    }
    void GenericReader::setError(const char* sz) {
        _result = false;
        _strError.append(sz);
    }

    void GenericReader::ParseValue(StringStream& is, BaseHandler& handler, bool bSkipObj) {
        void(GenericReader::*set[])(StringStream&, BaseHandler&) = { &GenericReader::ParseObject, &GenericReader::ParseObjectAsStr };
        switch (is.Peek()) {
        case 'n': ParseNull(is, handler); break;
        case 't': ParseTrue(is, handler); break;
        case 'f': ParseFalse(is, handler); break;
        case '"': ParseString(is, handler); break;
        case '{': (this->*set[bSkipObj])(is, handler); break;
        case '[': ParseArray(is, handler); break;
        default:
            ParseNumber(is, handler);
            break;
        }
        if (!_result)
            return;
    }

    void GenericReader::ParseKey(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == '\"');
        is.Take();  // Skip '\"'
        const char* szStart = is.Strart();

        for (; is.Peek() != '\0'; is.Take()) {
            if (is.Peek() == '\"') {
                handler.Key(szStart, is.Strart() - szStart);
                is.Take();  // Skip '\"'
                return;
            }
        }
        setError("KeyInvalid");
    }

    void GenericReader::ParseNull(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == 'n');
        is.Take();

        if (Consume(is, 'u') && Consume(is, 'l') && Consume(is, 'l')) {
            handler.Value("", 0);
        }
        else {
            setError("ValueInvalid");
        }
    }

    void GenericReader::ParseTrue(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == 't');
        const char* szStart = is.Strart();
        is.Take();

        if (Consume(is, 'r') && Consume(is, 'u') && Consume(is, 'e')) {
            handler.Value(szStart, is.Strart() - szStart);
        }
        else {
            setError("ValueInvalid");
        }
    }

    void GenericReader::ParseFalse(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == 'f');
        const char* szStart = is.Strart();
        is.Take();

        if (Consume(is, 'a') && Consume(is, 'l') && Consume(is, 's') && Consume(is, 'e')) {
            handler.Value(szStart, is.Strart() - szStart);
        }
        else {
            setError("ValueInvalid");
        }
    }

    void GenericReader::ParseString(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == '\"');
        is.Take();  // Skip '\"'
        const char* szStart = is.Strart();

        for (; is.Peek() != '\0'; is.Take()) {
            if (is.Peek() == '\"' && is.Second2Last() != '\\') {
                handler.Value(szStart, is.Strart() - szStart);
                is.Take();  // Skip '\"'
                return;
            }
        }
        setError("ValueInvalid");
    }

    void GenericReader::ParseArray(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == '[');
        is.Take();  // Skip '['
        const char* szStart = is.Strart();

        for (uint32_t nCount = 1; is.Peek() != '\0'; is.Take()) {
            if (is.Peek() == ']') {
                --nCount;
                if (!nCount) {
                    handler.Value(szStart, is.Strart() - szStart);
                    is.Take();  // Skip ']'
                    return;
                }
            }
            else if (is.Peek() == '[')
                ++nCount;
        }
        setError("ValueArrayInvalid");
    }

    void GenericReader::ParseNumber(StringStream& is, BaseHandler& handler) {
        const char* szStart = is.Strart();

        for (; is.Peek() != '\0'; is.Take()) {
            if (is.Peek() == '-' || (is.Peek() >= '0' && is.Peek() <= '9')) {
                continue;
            }
            else {
                handler.Value(szStart, is.Strart() - szStart);
                return;
            }
        }
        setError("ValueInvalid");
    }

    void GenericReader::ParseObject(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == '{');
        const char* szStart = is.Strart();
        is.Take();  // Skip '{'

        SkipWhitespace(is);
        if (is.Peek() == '}') {
            handler.Value(szStart, is.Strart() - szStart + 1);// empty object
            return;
        }

        for (;!is.isEnd();) {
            if (is.Peek() != '"') {
                setError("ObjectMissName");
                return;
            }
            ParseKey(is, handler);

            SkipWhitespace(is);
            if (!Consume(is, ':')) {
                setError("ObjectMissColon");
                return;
            }

            SkipWhitespace(is);
            ParseValue(is, handler, true);

            SkipWhitespace(is);
            switch (is.Peek()) {
            case ',':
                is.Take();
                SkipWhitespace(is);
                break;
            case '}':
                is.Take();
                //setError("Termination");
                return;
            default:
                setError("ObjectMissCommaOrCurlyBracket");
                break; // This useless break is only for making warning and coverage happy
            }
        }
    }

    void GenericReader::ParseObjectAsStr(StringStream& is, BaseHandler& handler) {
        assert(is.Peek() == '{');
        const char* szStart = is.Strart();
        is.Take();  // Skip '{'

        for (uint32_t nCount = 1; is.Peek() != '\0'; is.Take()) {
            if (is.Peek() == '}') {
                --nCount;
                if (!nCount) {
                    handler.Value(szStart, is.Strart() - szStart + 1);
                    is.Take();  // Skip '\"'
                    return;
                }
            }
            else if (is.Peek() == '{')
                ++nCount;
        }
        setError("ValueObjectInvalid");
    }
    /*------------------------------------------------------------------------------*/

    void JSONDecoder::decodeValue(bool& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        if (strncmp("true", sz, length) == 0) {
            value = true;
        }
        else if (strncmp("false", sz, length) == 0) {
            value = false;
        }
        else {
            assert(false);
        }
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(int32_t& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        bool bMinus = false;
        if (sz[0] == '-') {
            bMinus = true;
        }
        for (uint32_t idx = bMinus; idx < length; ++idx) {
            value *= 10;
            value += sz[idx] - '0';
        }
        if (bMinus) value = 0 - value;
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(uint32_t& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        for (uint32_t idx = 0; idx < length; ++idx) {
            value *= 10;
            value += sz[idx] - '0';
        }
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(int64_t& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        bool bMinus = false;
        if (sz[0] == '-') {
            bMinus = true;
        }
        for (uint32_t idx = bMinus; idx < length; ++idx) {
            value *= 10;
            value += sz[idx] - '0';
        }
        if (bMinus) value = 0 - value;
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(uint64_t& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        for (uint32_t idx = 0; idx < length; ++idx) {
            value *= 10;
            value += sz[idx] - '0';
        }
        if (pHas) *pHas = true;
    }

    static inline double decimal(uint8_t n, uint32_t num) {
        double db = 0.0f;
        while (num--)
            db = n / 10;
        return db;
    }

    void JSONDecoder::decodeValue(float& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        bool bMinus = false;
        if (sz[0] == '-') {
            bMinus = true;
        }
        for (uint32_t idx = bMinus, bFlag = false, num = 0; idx < length; ++idx) {
            const char& c = sz[idx];
            if (c == '.') {
                bFlag = true;
            }
            uint8_t n = c - '0';
            if (!bFlag) {
                value *= 10;
                value += n;
            }
            else {
                ++num;
                value += decimal(n, num);
            }
        }
        if (bMinus) value = 0 - value;
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(double& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value = 0;
        for (uint32_t idx = 0, bFlag = false, num = 0; idx < length; ++idx) {
            const char& c = sz[idx];
            if (c == '.') {
                bFlag = true;
            }
            uint8_t n = c - '0';
            if (!bFlag) {
                value *= 10;
                value += n;
            }
            else {
                ++num;
                value += decimal(n, num);
            }
        }
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeValue(std::string& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;
        value.clear();
        value.append(sz, length);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<bool>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                bool v = false;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        bool v = false;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<int32_t>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                int32_t v = 0;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        int32_t v = 0;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<uint32_t>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                uint32_t v = 0;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        uint32_t v = 0;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<int64_t>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                int64_t v = 0;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        int64_t v = 0;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<uint64_t>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                uint64_t v = 0;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        uint64_t v = 0;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<float>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                float v = 0.0f;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        float v = 0.0f;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<double>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == ',') {
                double v = 0.0f;
                decodeValue(v, sz + n, idx - n, NULL);
                value.push_back(v);
                bFlag = true;
            }
            else {
                if (bFlag) {
                    n = idx;
                    bFlag = false;
                }
            }
        }
        double v = 0.0f;
        decodeValue(v, sz + n, length - n, NULL);
        value.push_back(v);
        if (pHas) *pHas = true;
    }

    void JSONDecoder::decodeArray(std::vector<std::string>& value, const char* sz, uint32_t length, bool* pHas) {
        if (!length) return;

        uint32_t n = 0;
        for (uint32_t idx = 0, bFlag = true; idx < length; ++idx) {
            const char c = sz[idx];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            if (c == '"') {
                if (bFlag) {
                    n = idx + 1;
                    bFlag = false;
                } else {
                    std::string v;
                    decodeValue(v, sz + n, idx - n, NULL);
                    value.push_back(v);
                    bFlag = true;
                }
            }
            else {
            }
        }
        if (pHas) *pHas = true;
    }

}
