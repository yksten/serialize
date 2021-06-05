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

    static unsigned parse_hex4(const char *str) {
        unsigned h=0;
        if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
        h=h<<4;str++;
        if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
        h=h<<4;str++;
        if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
        h=h<<4;str++;
        if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
        return h;
    }

    /* Parse the input text into an unescaped cstring, and populate item. */
    static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
    static bool parse_string(std::string& strValue, const char* str, uint32_t length) {
        if (!length) { return false; }    /* not a string! */
        const char *ptr = str; unsigned uc, uc2;
        for (uint32_t i = 0; i < length;++i) {
            if (ptr[i]!='\\') {
                strValue.append(1, ptr[i]);
            } else {
                ++i;
                switch (ptr[i]) {
                    case 'b': strValue.append(1, '\b');    break;
                    case 'f': strValue.append(1, '\f');    break;
                    case 'n': strValue.append(1, '\n');    break;
                    case 'r': strValue.append(1, '\r');    break;
                    case 't': strValue.append(1, '\t');    break;
                    case 'u':     /* transcode utf16 to utf8. */
                        uc=parse_hex4(ptr+i+1);i+=4;    /* get the unicode char. */
                        if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)    break;    /* check for invalid.    */
                        /* UTF16 surrogate pairs.    */
                        if (uc>=0xD800 && uc<=0xDBFF) {
                            if (ptr[i+1]!='\\' || ptr[i+2]!='u')    break;    /* missing second-half of surrogate.    */
                            uc2=parse_hex4(ptr+i+3);i+=6;
                            if (uc2<0xDC00 || uc2>0xDFFF)        break;    /* invalid second-half of surrogate.    */
                            uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
                        }

                        if (uc<0x80) {
                            strValue.append(1, uc | firstByteMark[1]);
                        } else if (uc<0x800) {
                            strValue.append(1, '\0');
                            strValue.append(1, (uc | 0x80) & 0xBF);
                            uc >>= 6;
                        } else if (uc<0x10000) {
                            strValue.append(2, '\0');
                            strValue.append(1, (uc | 0x80) & 0xBF);
                            uc >>= 6;
                        } else {
                            strValue.append(3, '\0');
                            strValue.append(1, (uc | 0x80) & 0xBF);
                            uc >>= 6;
                        }
                        break;
                    default:  strValue.append(1, ptr[i]); break;
                }
            }
        }
        return true;
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
        bool ret = parse_string(value, sz, length);
        if (pHas) *pHas = ret;
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
