#include <iostream>
#include <fstream>
#include <Windows.h>
#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>


#define ASSERT(expression) if ((expression) == false) { ::MessageBoxA(nullptr, "Assertion failed!", "ASSERT", MB_OK); std::cout << "Assertion Failed in File: " << __FILE__ << " Line:" << __LINE__; ::DebugBreak; }


using uint8 = uint8_t;
using int16 = int16_t;
using int32 = int32_t;
using uint64 = uint64_t;

class BinaryFile;
class BinaryParser;
class JSONFile;


class BinaryFile
{
	friend BinaryParser;

public:
	BinaryFile(const std::string& fileName)
	{
		std::ifstream ifs(fileName, std::fstream::binary | std::fstream::ate);
		ASSERT(ifs.is_open());
		const uint64 count = ifs.tellg();
		_bytes.resize(count);
		ifs.seekg(std::fstream::beg);
		ifs.read((char*)&_bytes[0], count);
	}
private:
	std::vector<byte> _bytes;
};

class BinaryParser
{
public:
	BinaryParser(const BinaryFile& binaryFile) : _binaryFile{ binaryFile } { __noop; }
	template<typename T>
	T readBinary()
	{
		const uint64 at = _at;
		_at += sizeof(T);
		return *reinterpret_cast<const T*>(&_binaryFile._bytes[at]);
	}
private:
	const BinaryFile& _binaryFile;
	uint64 _at = 0;
};


class JSONValueID
{
	friend JSONFile;
	uint64 _raw = 0;
};

enum class JSONValueType
{
	// { NameValuePairs }
	Object,

	// [ NameValuePairs ]
	Array,

	// "name": value
	NameValuePair,

	// value
	Value
};

class JSONValue
{
public:
	using InternalValueType = std::variant<std::string, int32, float>;
private:
	friend JSONFile;
	std::string _name;
	InternalValueType _value;
private:
	JSONValueType _type = JSONValueType::Object;
	JSONValueID _ID;
	JSONValueID _parentID;
	std::vector<JSONValueID> _childIDs;
	uint8 _depth = 0;
};

class JSONFile
{
public:
	JSONFile(const std::string& fileName) : _fileName{ fileName }
	{
		_jsonValues.push_back(JSONValue());
	}
	JSONValueID getRootJSONValueID() const { return _jsonValues[0]._ID; }
	JSONValueID createJSONValue(const JSONValueID& parentID, const std::string& name, const JSONValueType type)
	{
		if (_nameChecker.find(name) != _nameChecker.end())
		{
			ASSERT(false);
			return getRootJSONValueID();
		}
		JSONValue jsonValue;
		{
			JSONValue& parentJSONValue = getJSONValue(parentID);
			jsonValue._name = name;
			jsonValue._type = type;
			jsonValue._ID._raw = _jsonValues.size();
			jsonValue._parentID = parentID;
			jsonValue._depth = parentJSONValue._depth + 1;
			parentJSONValue._childIDs.push_back(jsonValue._ID);
		}
		_jsonValues.push_back(jsonValue);
		return jsonValue._ID;
	}
	void pushNameValuePair(const JSONValueID& parentID, const std::string& name, const JSONValue::InternalValueType& value)
	{
		const JSONValueID nameValuePairID = createJSONValue(parentID, name, JSONValueType::NameValuePair);
		getJSONValue(nameValuePairID)._value = value;
	}
	void pushNameValuePairObject(const JSONValueID& parentID, const std::string& name, const JSONValue::InternalValueType& value)
	{
		const JSONValueID objectID = createJSONValue(parentID, "", JSONValueType::Object);
		const JSONValueID nameValuePairID = createJSONValue(objectID, name, JSONValueType::NameValuePair);
		getJSONValue(nameValuePairID)._value = value;
	}
	void pushValue(const JSONValueID& parentID, const JSONValue::InternalValueType& value)
	{
		const JSONValueID valueID = createJSONValue(parentID, "", JSONValueType::Value);
		getJSONValue(valueID)._value = value;
	}
	void save() const
	{
		using namespace std;
		string buffer;
		ofstream ofs{ _fileName };
		ASSERT(ofs.is_open());
		save_write_JSONValue(ofs, getRootJSONValueID());
	}

private:
	void save_write_JSONValue(std::ofstream& ofs, const JSONValueID& id) const
	{
		using namespace std;
		const JSONValue& jsonValue = getJSONValue(id);
		for (uint64 depthIter = 0; depthIter < jsonValue._depth; ++depthIter)
		{
			ofs.put('\t');
		}

		if (jsonValue._name.empty() == false)
		{
			ofs.put('\"');
			ofs.write(jsonValue._name.c_str(), jsonValue._name.size());
			ofs.put('\"');
			ofs.put(':');
			ofs.put(' ');
		}

		const JSONValue& parentJSONValue = getJSONValue(jsonValue._parentID);
		if (jsonValue._type == JSONValueType::Object || jsonValue._type == JSONValueType::Array)
		{
			if (jsonValue._type == JSONValueType::Object)
			{
				ofs.put('{');
			}
			else
			{
				ofs.put('[');
			}
			ofs.put('\n');
		}

		string buffer;
		const size_t i = jsonValue._value.index();
		switch (i)
		{
		case 0:
			buffer = get<string>(jsonValue._value);
			if (buffer.empty() == false)
			{
				ofs.put('\"');
				ofs.write(buffer.c_str(), buffer.size());
				ofs.put('\"');
			}
			break;
		case 1:
			buffer = to_string(get<int32>(jsonValue._value));
			ofs.write(buffer.c_str(), buffer.size());
			break;
		case 2:
			buffer = to_string(get<float>(jsonValue._value));
			ofs.write(buffer.c_str(), buffer.size());
			break;
		default:
			break;
		}

		for (const JSONValueID& childID : jsonValue._childIDs)
		{
			save_write_JSONValue(ofs, childID);
		}

		if (jsonValue._type == JSONValueType::Object || jsonValue._type == JSONValueType::Array)
		{
			for (uint64 depthIter = 0; depthIter < jsonValue._depth; ++depthIter)
			{
				ofs.put('\t');
			}
			if (jsonValue._type == JSONValueType::Object)
			{
				ofs.put('}');
			}
			else
			{
				ofs.put(']');
			}
		}

		if (jsonValue._depth != 0)
		{
			if (jsonValue._ID._raw != parentJSONValue._childIDs.back()._raw)
			{
				ofs.put(',');
			}
			ofs.put('\n');
		}
	}
	JSONValue& getJSONValue(const JSONValueID& id) { return _jsonValues[id._raw]; }
	const JSONValue& getJSONValue(const JSONValueID& id) const { return _jsonValues[id._raw]; }

private:
	std::string _fileName;
	std::vector<JSONValue> _jsonValues;
	std::unordered_map<std::string, bool> _nameChecker;
};


int main()
{
	using std::cin;
	using std::cout;
	using namespace std;

	char mapFileName[MAX_PATH]{};
	while (true)
	{
		cout << "> Type map file name (e.g. forest1_2.map)\n";
		cout << "> ";
		cin >> mapFileName;
		if (filesystem::exists(mapFileName) == false)
		{
			cout << "> Couldn't fine the map named \"" << mapFileName << "\"\n";
			continue;
		}

		const BinaryFile binaryFile{ mapFileName };
		BinaryParser binaryParser{ binaryFile };
		const int32 mapTypeOrMapWidth = binaryParser.readBinary<int32>();
		ASSERT(mapTypeOrMapWidth == -1);

		const int32 tileWidth = binaryParser.readBinary<int32>();
		const int32 tileHeight = binaryParser.readBinary<int32>();
		const int32 mapWidth = binaryParser.readBinary<int32>();
		const int32 mapHeight = binaryParser.readBinary<int32>();
		const int32 unknownWidth = binaryParser.readBinary<int32>();
		const int32 unknownHeight = binaryParser.readBinary<int32>();
		const int16 tileCount = binaryParser.readBinary<int16>();
		const int16 layerCount = binaryParser.readBinary<int16>();
		const int16 mapUnknown0 = binaryParser.readBinary<int16>();
		const int16 minimapFileCount = binaryParser.readBinary<int16>();
		const int16 mapUnknown1 = binaryParser.readBinary<int16>();
		const int16 mapUnknown2 = binaryParser.readBinary<int16>();

#define STRING_AND_VARIABLE(valueWithName) #valueWithName, valueWithName
		JSONFile jsonFile{ "map.json" };
		{
			const JSONValueID metaDataID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "meta_data", JSONValueType::Object);
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapTypeOrMapWidth));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(tileWidth));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(tileHeight));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapWidth));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapHeight));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(unknownWidth));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(unknownHeight));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(tileCount));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(layerCount));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapUnknown0));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(minimapFileCount));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapUnknown1));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_VARIABLE(mapUnknown2));
		}
		{
			const JSONValueID tileFileNamesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "tile_filenames", JSONValueType::Array);
			//jsonFile.pushNameValuePairObject(tileFileNamesID, "fileName", "TEST0.bmp");
			//jsonFile.pushNameValuePairObject(tileFileNamesID, "fileName", "TEST1.bmp");
			jsonFile.pushValue(tileFileNamesID, "TEST0.bmp");
			jsonFile.pushValue(tileFileNamesID, "TEST1.bmp");
		}
		jsonFile.save();
		break;
	}
	return 0;
}
