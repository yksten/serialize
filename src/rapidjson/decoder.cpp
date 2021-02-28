#include "rapidjson/decoder.h"
//#include "thirdParty/rapidjson/reader.h"


namespace serialize {

    void rapidJsonConverterMgr::setStruct(void* value, void* owner) {
        if (value && owner) {
            _owner = (convertHandler*)owner;
            _owner->_stackStruct.push_back(_owner->_struct);
            _owner->_struct = (uint8_t*)value;
        }
    }

    /*------------------------------------------------------------------------------*/

    bool convertHandler::Null() {
        return true;
    }

    bool convertHandler::Bool(bool b) {
        if (_converter) {
            (*_converter)(_struct, &b);
        }
        return true;
    }

    bool convertHandler::Int(int i) {
        if (_converter) {
            (*_converter)(_struct, &i);
        }
        return true;
    }

    bool convertHandler::Uint(unsigned u) {
        if (_converter) {
            (*_converter)(_struct, &u);
        }
        return true;
    }

    bool convertHandler::Int64(int64_t i) {
        if (_converter) {
            (*_converter)(_struct, &i);
        }
        return true;
    }

    bool convertHandler::Uint64(uint64_t u) {
        if (_converter) {
            (*_converter)(_struct, &u);
        }
        return true;
    }

    bool convertHandler::Double(double d) {
        if (_converter) {
            (*_converter)(_struct, &d);
        }
        return true;
    }

    bool convertHandler::RawNumber(const char* str, unsigned length, bool copy) {
        return true;
    }

    bool convertHandler::String(const char* str, unsigned length, bool copy) {
        if (_converter) {
            std::string strValue(str, length);
            (*_converter)(_struct, &strValue);
        }
        return true;
    }

    bool convertHandler::Key(const char* str, unsigned length, bool copy) {
        if (_mgr->isMap()) {
            const rapidJsonConverter* tempConverter = _converter;
            uint8_t* tempStruct = _struct;
            _converter->setKey(str);

            rapidJsonConverterMgr* mgr = (rapidJsonConverterMgr*)_converter->getConvert(_struct, this);
            _converter = &mgr->find(str)->second;
            _converter->setLast(tempConverter, resetStruct(&_struct, tempStruct));
        } else {
            rapidJsonConverterMgr::const_iterator it = _mgr->find(str);
            if (it != _mgr->end()) {
                _converter = &it->second;
            } else {
                _converter = NULL;
            }
        }
        return true;
    }

    bool convertHandler::StartObject() {
        if (_converter) {
            _stackFunction.push_back(_converter);

            _stackMgr.push_back(_mgr);
            _mgr = (rapidJsonConverterMgr*)_converter->getConvert(_struct, this);
        }
        return true;
    }

    bool convertHandler::EndObject(unsigned memberCount) {
        if (!_stackMgr.empty()) {
            bool bIsMap = _mgr->isMap();
            _mgr = _stackMgr.back();
            _stackMgr.erase(_stackMgr.begin() + _stackMgr.size() - 1);

            _converter = _stackFunction.back();
            _stackFunction.erase(_stackFunction.begin() + _stackFunction.size() - 1);

            if (!bIsMap || memberCount) {
                _struct = _stackStruct.back();
                _stackStruct.erase(_stackStruct.begin() + _stackStruct.size() - 1);
            }
        }
        return true;
    }

    bool convertHandler::StartArray() {
        return true;
    }

    bool convertHandler::EndArray(unsigned elementCount) {
        if (!elementCount) {
            _converter->clear(_struct);
        }
        return true;
    }

    /*------------------------------------------------------------------------------*/

    void rapidjsonDecoder::decodeValue(const char* sz, bool& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<bool>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, uint32_t& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<uint32_t>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, int32_t& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<int32_t>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, uint64_t& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<uint64_t>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, int64_t& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<int64_t>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, float& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<float>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, double& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<double>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::string& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bind<std::string>(&serialize::rapidjsonDecoder::convertValue, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<bool>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<bool>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<uint32_t>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<uint32_t>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<int32_t>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<int32_t>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<uint64_t>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<uint64_t>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<int64_t>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<int64_t>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<float>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<float>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<double>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<double>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

    void rapidjsonDecoder::decodeValue(const char* sz, std::vector<std::string>& value, bool* pHas) {
        size_t offset = ((uint8_t*)(&value)) - _mgr->getStruct();
        size_t has = pHas ? ((uint8_t*)(pHas)) - _mgr->getStruct() : 0;
        _mgr->insert(std::pair<std::string, rapidJsonConverter>(sz, rapidJsonConverterMgr::bindArray<std::string>(&serialize::rapidjsonDecoder::convertArray, offset, has)));
    }

}
