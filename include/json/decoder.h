#ifndef __JSON_DECODER_H__
#define __JSON_DECODER_H__
#include "serialize.h"
#include <string>
#include <map>


namespace serialize {

    struct converter {
        typedef void(*function_type)(void*, const char*, uint32_t, bool*);
        converter(function_type func, size_t value, size_t has) :_func(func), _value(value), _has(has) {}
        void operator()(uint8_t* pStruct, const char* cValue, uint32_t length) const {
            (*_func)((pStruct + _value), cValue, length, (bool*)(_has ? pStruct + _has : NULL));
        }
        template<typename T>
        static converter bind(void(*f)(T&, const char*, uint32_t, bool*), size_t value, size_t has) {
            return converter(function_type(f), value, has);
        }

        template<typename T>
        static converter bindArray(void(*f)(std::vector<T>&, const char*, uint32_t, bool*), size_t value, size_t has) {
            return converter(function_type(f), value, has);
        }
    private:
        function_type _func;
        size_t _value;
        size_t _has;
    };

    class StringStream {
        typedef const char Ch;
        Ch* _src;
        Ch* _end;
        uint32_t _length;
    public:
        StringStream(Ch* src, uint32_t length) : _src(src), _length(length) {}
        // Read
        Ch Peek() { if (isEnd()) return '\0';  return *_src; }
        Ch Take() { --_length; return *_src++; }
        Ch* Strart() const { return _src; }
        bool isEnd() const { return (_length == 0); }
    };

    class EXPORTAPI BaseHandler {
    public:
        virtual ~BaseHandler() {}
        virtual bool Key(const char* sz, unsigned length) = 0;
        virtual bool Value(const char* sz, unsigned length) = 0;
    };

    typedef std::pair<const char*, converter> function_value;

    struct functionValueMgr {
        functionValueMgr() :pStruct(NULL) {}
        functionValueMgr(const functionValueMgr& that) :pStruct(NULL), set(that.set) {}
        void* pStruct;
        std::vector<function_value> set;
    };

    class EXPORTAPI Handler :public BaseHandler {
    protected:
        void* _pStruct;
        const converter* _converter;
        const functionValueMgr* _mgr;
    public:
        Handler(void* pStruct, const functionValueMgr& mgr);
        size_t offsetByValue(const void* cValue) const;
        virtual bool Key(const char* sz, unsigned length);
        virtual bool Value(const char* sz, unsigned length);
    };

    class EXPORTAPI GenericReader {
        bool _result;
        std::string _strError;
    public:
        GenericReader();
        bool Parse(StringStream& is, BaseHandler& handler);
        const char* getError()const;
    private:
        void setError(const char* sz);
        void ParseValue(StringStream& is, BaseHandler& handler, bool bSkipObj);
        void ParseKey(StringStream& is, BaseHandler& handler);
        void ParseNull(StringStream& is, BaseHandler& handler);
        void ParseTrue(StringStream& is, BaseHandler& handler);
        void ParseFalse(StringStream& is, BaseHandler& handler);
        void ParseString(StringStream& is, BaseHandler& handler);
        void ParseNumber(StringStream& is, BaseHandler& handler);
        void ParseArray(StringStream& is, BaseHandler& handler);
        void ParseObject(StringStream& is, BaseHandler& handler);
        void ParseObjectAsStr(StringStream& is, BaseHandler& handler);
    };

    class EXPORTAPI JSONDecoder {
        StringStream _str;
        functionValueMgr* _mgr;
    public:
        JSONDecoder(const char* sz, uint32_t length) :_str(sz, length), _mgr(NULL) {}
        JSONDecoder(const std::string& str) :_str(str.c_str(), str.length()), _mgr(NULL) {}
        ~JSONDecoder() {}

        template<typename T>
        functionValueMgr getSet(T& value) {
            functionValueMgr mgr;
            mgr.pStruct = &value;
            _mgr = &mgr;
            internal::serializeWrapper(*this, value);
            return mgr;
        }

        template<typename T>
        bool operator >> (T& value) {
            static functionValueMgr mgr = getSet(value);
            Handler handler(&value, mgr);
            return GenericReader().Parse(_str, handler);
        }

        template<typename T>
        JSONDecoder& operator&(serializeItem<T> value) {
            return convert(value.name, value.value);
        }

        template<typename T>
        JSONDecoder& convert(const char* sz, T& value, bool* pHas = NULL) {
            size_t offset = ((uint8_t*)(&value)) - (uint8_t*)_mgr->pStruct;
            size_t has = pHas ? ((uint8_t*)(pHas)) - (uint8_t*)_mgr->pStruct : 0;
            _mgr->set.push_back(function_value(sz, converter::bind<typename internal::TypeTraits<T>::Type>(&JSONDecoder::decodeValue, offset, has)));
            return *this;
        }

        template<typename T>
        JSONDecoder& convert(const char* sz, std::vector<T>& value, bool* pHas = NULL) {
            size_t offset = ((uint8_t*)(&value)) - (uint8_t*)_mgr->pStruct;
            size_t has = pHas ? ((uint8_t*)(pHas)) - (uint8_t*)_mgr->pStruct : 0;
            _mgr->set.push_back(function_value(sz, converter::bindArray<typename internal::TypeTraits<T>::Type>(&JSONDecoder::decodeArray, offset, has)));
            return *this;
        }
        template<typename K, typename V>
        JSONDecoder& convert(const char* sz, std::map<K, V>& value, bool* pHas = NULL) {
            size_t offset = ((uint8_t*)(&value)) - (uint8_t*)_mgr->pStruct;
            size_t has = pHas ? ((uint8_t*)(pHas)) - (uint8_t*)_mgr->pStruct : 0;
            _mgr->set.push_back(function_value(sz, converter::bind<std::map<K, V> >(&JSONDecoder::decodeMap, offset, has)));
            return *this;
        }
    private:
        static void decodeValue(bool& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(int32_t& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(uint32_t& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(int64_t& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(uint64_t& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(float& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(double& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeValue(std::string& value, const char* sz, uint32_t length, bool* pHas);

        static void decodeArray(std::vector<bool>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<int32_t>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<uint32_t>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<int64_t>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<uint64_t>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<float>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<double>& value, const char* sz, uint32_t length, bool* pHas);
        static void decodeArray(std::vector<std::string>& value, const char* sz, uint32_t length, bool* pHas);

        template<typename T>
        static void decodeValue(T& value, const char* cValue, uint32_t length, bool* pHas) {
            if (length > 2) {
                JSONDecoder decoder(cValue, length);
                if (!decoder.operator>>(value))
                    return;
                if (pHas) *pHas = true;
            }
        }

        template<typename T>
        static void decodeArray(std::vector<T>& value, const char* cValue, uint32_t length, bool* pHas) {
            if (length) {
                value.clear();
                std::vector<const char*> stack;
                for (uint32_t idx = 0, nCount = 0; idx < length; ++idx) {
                    const char c = cValue[idx];
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        continue;
                    }
                    if (c == '{') {
                        stack.push_back(cValue + idx);
                        ++nCount;
                    }
                    else if (c == '}') {
                        --nCount;
                        const char* szBin = stack[stack.size() - 1];
                        stack.erase(stack.begin() + stack.size() - 1);
                        if (!nCount) {
                            T temp = T();
                            JSONDecoder decoder(szBin, (cValue + idx) - szBin + 1);
                            if (decoder.operator>>(temp))
                                value.push_back(temp);
                        }
                    }
                }
                if (pHas) *pHas = true;
            }
        }

        template<typename K, typename V>
        class MapHandler : public BaseHandler {
            K _key;
            V _value;
            std::map<K, V>& _map;
        public:
            MapHandler(std::map<K, V>& map) :_key(), _value(), _map(map) {}
            bool Key(const char* sz, unsigned length) {
                decodeValue(_key, sz, length, NULL);
                return true;
            }
            bool Value(const char* sz, unsigned length) {
                decodeValue(_value, sz, length, NULL);
                _map.insert(std::pair<K, V>(_key, _value));
                return true;
            }
        };

        template<typename K, typename V>
        static void decodeMap(std::map<K, V>& value, const char* cValue, uint32_t length, bool* pHas) {
            if (length > 2) {
                StringStream str(cValue, length);
                MapHandler<K, V> handler(value);
                GenericReader().Parse(str, handler);

                if (pHas) *pHas = true;
            }
        }
    };

}


#endif
