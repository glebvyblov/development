#include <unordered_set>
#include <library/cpp/string_utils/base64/base64.h> 
#include <util/string/escape.h>
#include <util/string/printf.h>
#include <util/charset/utf8.h>
#include <util/stream/str.h>
#include "json.h"

void TProtoToJson::EscapeJsonString(IOutputStream& os, const TString& s) {
    const char* b = s.begin();
    const char* e = s.end();
    const char* p = b;
    while (p < e) {
        size_t len = 1;
        RECODE_RESULT result = GetUTF8CharLen(
                    len,
                    reinterpret_cast<const unsigned char*>(p),
                    reinterpret_cast<const unsigned char*>(e)
                    );
        if (result == RECODE_OK && len != 1) {
            len = std::min<decltype(len)>(len, e - p);
            os.Write(p, len);
            p += len;
        } else {
            char c = *p;
            if (c < '\x20') {
                os << Sprintf("\\u%04x", int(c));
            } else if (c == '"') {
                os << "\\\"";
            } else if (c == '\\') {
                os << "\\\\";
            } else {
                os << c;
            }
            ++p;
        }
    }
}

TString TProtoToJson::EscapeJsonString(const TString& s) {
    TStringStream str;
    EscapeJsonString(str, s);
    return str.Str();
}

void TProtoToJson::ProtoToJson(IOutputStream& to, const ::google::protobuf::EnumValueDescriptor* descriptor, const TJsonSettings& jsonSettings) {
    if (jsonSettings.EnumAsNumbers) {
        to << descriptor->number();
    } else {
        const ::google::protobuf::EnumValueDescriptor* trueDescriptor = descriptor->type()->FindValueByNumber(descriptor->number());
        if (trueDescriptor == nullptr) {
            to << descriptor->number();
        } else {
            to << '"';
            EscapeJsonString(to, trueDescriptor->name());
            to << '"';
        }
    }
}

void TProtoToJson::ProtoToJson(IOutputStream& to, const ::google::protobuf::Message& protoFrom, const TJsonSettings& jsonSettings) {
    to << '{';
    ProtoToJsonInline(to, protoFrom, jsonSettings);
    to << '}';
}

void TProtoToJson::ProtoToJsonInline(IOutputStream& to, const ::google::protobuf::Message& protoFrom, const TJsonSettings& jsonSettings) {
    // TODO: get rid of "copy-paste"
    using namespace ::google::protobuf;
    const Reflection& reflectionFrom = *protoFrom.GetReflection();
    const Descriptor& descriptorFrom = *protoFrom.GetDescriptor();
    bool hasAnyField = false;
    int fieldCount = descriptorFrom.field_count();
    for (int idxField = 0; idxField < fieldCount; ++idxField) {
        const FieldDescriptor* fieldFrom = descriptorFrom.field(idxField);
        auto ir = jsonSettings.FieldRemapper.find(fieldFrom);
        if (ir != jsonSettings.FieldRemapper.end()) {
            TStringStream remapTo;
            ir->second(remapTo, protoFrom, jsonSettings);
            if (remapTo.Size() > 0) {
                if (hasAnyField) {
                    to << ',';
                }
                to << remapTo.Str();
                hasAnyField = true;
            }
        } else {
            if (fieldFrom->is_repeated()) {
                if (!jsonSettings.EmptyRepeated && reflectionFrom.FieldSize(protoFrom, fieldFrom) == 0) {
                    continue;
                }
            } else {
                if (!reflectionFrom.HasField(protoFrom, fieldFrom)) {
                    continue;
                }
            }
            if (hasAnyField) {
                to << ',';
            }
            hasAnyField = true;
            TString name;
            if (jsonSettings.NameGenerator) {
                name = jsonSettings.NameGenerator(*fieldFrom);
            } else {
                name = fieldFrom->name();
            }
            to << '"' << name << "\":";
            FieldDescriptor::CppType type = fieldFrom->cpp_type();
            if (fieldFrom->is_repeated()) {
                int size = reflectionFrom.FieldSize(protoFrom, fieldFrom);
                if (fieldFrom->is_map()) {
                    const FieldDescriptor* keyDescriptor = nullptr;
                    const FieldDescriptor* valueDescriptor = nullptr;
                    FieldDescriptor::CppType keyType;
                    FieldDescriptor::CppType valueType;
                    const Reflection* itemReflection = nullptr;
                    to << '{';
                    for (int i = 0; i < size; ++i) {
                        const Message& item = reflectionFrom.GetRepeatedMessage(protoFrom, fieldFrom, i);
                        if (i == 0) {
                            itemReflection = item.GetReflection();
                            std::vector<const FieldDescriptor*> fields;
                            itemReflection->ListFields(item, &fields);
                            if (fields.size() == 2) {
                                keyDescriptor = fields[0];
                                valueDescriptor = fields[1];
                                keyType = keyDescriptor->cpp_type();
                                valueType = valueDescriptor->cpp_type();
                            } else {
                                break;
                            }
                        } else {
                            to << ',';
                        }
                        to << '"';
                        switch (keyType) {
                        case FieldDescriptor::CPPTYPE_INT32:
                            to << itemReflection->GetInt32(item, keyDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_INT64:
                            to << itemReflection->GetInt64(item, keyDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_UINT32:
                            to << itemReflection->GetUInt32(item, keyDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_UINT64:
                            to << itemReflection->GetUInt64(item, keyDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_DOUBLE:
                            {
                                double value = itemReflection->GetDouble(item, keyDescriptor);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_FLOAT:
                            {
                                float value = itemReflection->GetFloat(item, keyDescriptor);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_BOOL:
                            to << (itemReflection->GetBool(item, keyDescriptor) ? "true" : "false");
                            break;
                        case FieldDescriptor::CPPTYPE_ENUM:
                            to << itemReflection->GetEnum(item, keyDescriptor)->name();
                            break;
                        case FieldDescriptor::CPPTYPE_STRING:
                            if (fieldFrom->type() == FieldDescriptor::TYPE_BYTES) {
                                to << Base64Encode(itemReflection->GetString(item, keyDescriptor));
                            } else {
                                EscapeJsonString(to, itemReflection->GetString(item, keyDescriptor));
                            }
                            break;
                        default:
                            to << "null";
                            break;
                        }
                        to << '"';
                        to << ':';
                        switch (valueType) {
                        case FieldDescriptor::CPPTYPE_INT32:
                            to << itemReflection->GetInt32(item, valueDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_INT64:
                            if (jsonSettings.UI64AsString) {
                                to << '"';
                                to << itemReflection->GetInt64(item, valueDescriptor);
                                to << '"';
                            } else {
                                to << itemReflection->GetInt64(item, valueDescriptor);
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_UINT32:
                            to << itemReflection->GetUInt32(item, valueDescriptor);
                            break;
                        case FieldDescriptor::CPPTYPE_UINT64:
                            if (jsonSettings.UI64AsString) {
                                to << '"';
                                to << itemReflection->GetUInt64(item, valueDescriptor);
                                to << '"';
                            } else {
                                to << itemReflection->GetUInt64(item, valueDescriptor);
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_DOUBLE:
                            {
                                double value = itemReflection->GetDouble(item, valueDescriptor);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_FLOAT:
                            {
                                float value = itemReflection->GetFloat(item, valueDescriptor);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_BOOL:
                            to << (itemReflection->GetBool(item, valueDescriptor) ? "true" : "false");
                            break;
                        case FieldDescriptor::CPPTYPE_ENUM:
                            ProtoToJson(to, itemReflection->GetEnum(item, valueDescriptor), jsonSettings);
                            break;
                        case FieldDescriptor::CPPTYPE_STRING:
                            to << '"';
                            if (fieldFrom->type() == FieldDescriptor::TYPE_BYTES) {
                                to << Base64Encode(itemReflection->GetString(item, valueDescriptor));
                            } else {
                                EscapeJsonString(to, itemReflection->GetString(item, valueDescriptor));
                            }
                            to << '"';
                            break;
                        case FieldDescriptor::CPPTYPE_MESSAGE:
                            ProtoToJson(to, itemReflection->GetMessage(item, valueDescriptor), jsonSettings);
                            break;
                        }
                    }
                    to << '}';
                } else {
                    to << '[';
                    for (int i = 0; i < size; ++i) {
                        if (i != 0) {
                            to << ',';
                        }
                        switch (type) {
                        case FieldDescriptor::CPPTYPE_INT32:
                            to << reflectionFrom.GetRepeatedInt32(protoFrom, fieldFrom, i);
                            break;
                        case FieldDescriptor::CPPTYPE_INT64:
                            // JavaScript could not handle large numbers (bigger than 2^53)
                            if (jsonSettings.UI64AsString) {
                                to << '"';
                                to << reflectionFrom.GetRepeatedInt64(protoFrom, fieldFrom, i);
                                to << '"';
                            } else {
                                to << reflectionFrom.GetRepeatedInt64(protoFrom, fieldFrom, i);
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_UINT32:
                            to << reflectionFrom.GetRepeatedUInt32(protoFrom, fieldFrom, i);
                            break;
                        case FieldDescriptor::CPPTYPE_UINT64:
                            if (jsonSettings.UI64AsString) {
                                to << '"';
                                to << reflectionFrom.GetRepeatedUInt64(protoFrom, fieldFrom, i);
                                to << '"';
                            } else {
                                to << reflectionFrom.GetRepeatedUInt64(protoFrom, fieldFrom, i);
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_DOUBLE:
                            {
                                double value = reflectionFrom.GetRepeatedDouble(protoFrom, fieldFrom, i);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_FLOAT:
                            {
                                float value = reflectionFrom.GetRepeatedFloat(protoFrom, fieldFrom, i);
                                if (isnan(value)) {
                                    to << jsonSettings.NaN;
                                } else {
                                    to << value;
                                }
                            }
                            break;
                        case FieldDescriptor::CPPTYPE_BOOL:
                            to << (reflectionFrom.GetRepeatedBool(protoFrom, fieldFrom, i) ? "true" : "false");
                            break;
                        case FieldDescriptor::CPPTYPE_ENUM:
                            ProtoToJson(to, reflectionFrom.GetRepeatedEnum(protoFrom, fieldFrom, i), jsonSettings);
                            break;
                        case FieldDescriptor::CPPTYPE_STRING:
                            to << '"';
                            if (fieldFrom->type() == FieldDescriptor::TYPE_BYTES) {
                                to << Base64Encode(reflectionFrom.GetRepeatedString(protoFrom, fieldFrom, i));
                            } else {
                                EscapeJsonString(to, reflectionFrom.GetRepeatedString(protoFrom, fieldFrom, i));
                            }
                            to << '"';
                            break;
                        case FieldDescriptor::CPPTYPE_MESSAGE:
                            ProtoToJson(to, reflectionFrom.GetRepeatedMessage(protoFrom, fieldFrom, i), jsonSettings);
                            break;
                        }
                    }
                    to << ']';
                }
            } else {
                switch (type) {
                case FieldDescriptor::CPPTYPE_INT32:
                    to << reflectionFrom.GetInt32(protoFrom, fieldFrom);
                    break;
                case FieldDescriptor::CPPTYPE_INT64:
                    if (jsonSettings.UI64AsString) {
                        to << '"';
                        to << reflectionFrom.GetInt64(protoFrom, fieldFrom);
                        to << '"';
                    } else {
                        to << reflectionFrom.GetInt64(protoFrom, fieldFrom);
                    }
                    break;
                case FieldDescriptor::CPPTYPE_UINT32:
                    to << reflectionFrom.GetUInt32(protoFrom, fieldFrom);
                    break;
                case FieldDescriptor::CPPTYPE_UINT64:
                    if (jsonSettings.UI64AsString) {
                        to << '"';
                        to << reflectionFrom.GetUInt64(protoFrom, fieldFrom);
                        to << '"';
                    } else {
                        to << reflectionFrom.GetUInt64(protoFrom, fieldFrom);
                    }
                    break;
                case FieldDescriptor::CPPTYPE_DOUBLE:
                    {
                        double value = reflectionFrom.GetDouble(protoFrom, fieldFrom);
                        if (isnan(value)) {
                            to << jsonSettings.NaN;
                        } else {
                            to << value;
                        }
                    }
                    break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                    {
                        float value = reflectionFrom.GetFloat(protoFrom, fieldFrom);
                        if (isnan(value)) {
                            to << jsonSettings.NaN;
                        } else {
                            to << value;
                        }
                    }
                    break;
                case FieldDescriptor::CPPTYPE_BOOL:
                    to << (reflectionFrom.GetBool(protoFrom, fieldFrom) ? "true" : "false");
                    break;
                case FieldDescriptor::CPPTYPE_ENUM:
                    ProtoToJson(to, reflectionFrom.GetEnum(protoFrom, fieldFrom), jsonSettings);
                    break;
                case FieldDescriptor::CPPTYPE_STRING:
                    to << '"';
                    if (fieldFrom->type() == FieldDescriptor::TYPE_BYTES) {
                        to << Base64Encode(reflectionFrom.GetString(protoFrom, fieldFrom));
                    } else {
                        EscapeJsonString(to, reflectionFrom.GetString(protoFrom, fieldFrom));
                    }
                    to << '"';
                    break;
                case FieldDescriptor::CPPTYPE_MESSAGE:
                    ProtoToJson(to, reflectionFrom.GetMessage(protoFrom, fieldFrom), jsonSettings);
                    break;
                }
            }
        }
    }
}

void TProtoToJson::ProtoToJsonSchema(IOutputStream& to, const TJsonSettings& jsonSettings, const ::google::protobuf::Descriptor* descriptor, std::unordered_set<const ::google::protobuf::Descriptor*>& descriptors) {
    using namespace ::google::protobuf;
    if (descriptor == nullptr) {
        return;
    }
    to << '{';
    to << "\"type\":\"object\",";
    to << "\"title\":\"";
    EscapeJsonString(to, descriptor->name());
    to << '"';
    int fields = descriptor->field_count();
    if (fields > 0) {
        to << ",\"properties\":{";
        int oneofFields = descriptor->oneof_decl_count();
        for (int idx = 0; idx < oneofFields; ++idx) {
            const OneofDescriptor* fieldDescriptor = descriptor->oneof_decl(idx);
            if (idx != 0) {
                to << ',';
            }
            to << '"';
            //TString name;
            if (jsonSettings.NameGenerator) {
                // TODO
            }
            EscapeJsonString(to, fieldDescriptor->name());
            to << "\":";
            to << "{\"type\":\"oneOf\"}";
        }
        for (int idx = 0; idx < fields; ++idx) {
            const FieldDescriptor* fieldDescriptor = descriptor->field(idx);
            if (idx != 0 || oneofFields != 0) {
                to << ',';
            }
            to << '"';
            TString name;
            if (jsonSettings.NameGenerator) {
                name = jsonSettings.NameGenerator(*fieldDescriptor);
            } else {
                name = fieldDescriptor->name();
            }
            EscapeJsonString(to, name);
            to << "\":";
            if (fieldDescriptor->is_repeated()) {
                to << "{\"type\":\"array\",\"items\":";
            }
            if (fieldDescriptor->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
                if (descriptors.insert(descriptor).second) {
                    ProtoToJsonSchema(to, jsonSettings, fieldDescriptor->message_type(), descriptors);
                } else {
                    to << "{}";
                }
            } else {
                to << "{\"type\":\"";
                switch (fieldDescriptor->cpp_type()) {
                case FieldDescriptor::CPPTYPE_INT32:
                case FieldDescriptor::CPPTYPE_UINT32:
                case FieldDescriptor::CPPTYPE_ENUM:
                    to << "integer";
                    break;
                case FieldDescriptor::CPPTYPE_STRING:
                case FieldDescriptor::CPPTYPE_INT64:
                case FieldDescriptor::CPPTYPE_UINT64:
                    to << "string"; // because of JS compatibility (JavaScript could not handle large numbers (bigger than 2^53))
                    break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                case FieldDescriptor::CPPTYPE_DOUBLE:
                    to << "number";
                    break;
                case FieldDescriptor::CPPTYPE_BOOL:
                    to << "boolean";
                    break;
                case FieldDescriptor::CPPTYPE_MESSAGE:
                    to << "object";
                    break;
                };
                to << '"';
                to << '}';
            }
            if (fieldDescriptor->is_repeated()) {
                to << '}';
            }
        }
        to << '}';
    }
    to << '}';
}

void TProtoToJson::ProtoToJsonSchema(IOutputStream& to, const TJsonSettings& jsonSettings, const ::google::protobuf::Descriptor* descriptor) {
    std::unordered_set<const ::google::protobuf::Descriptor*> descriptors;
    ProtoToJsonSchema(to, jsonSettings, descriptor, descriptors);
}
