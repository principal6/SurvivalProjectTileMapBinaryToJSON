#include <iostream>
#include <fstream>
#include <Windows.h>
#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>


#define ASSERT(expression) if ((expression) == false) { ::MessageBoxA(nullptr, "Assertion failed!", "ASSERT", MB_OK); std::cout << "Assertion Failed in File: " << __FILE__ << " Line:" << __LINE__ << "\n"; ::DebugBreak(); }
#define STRING_AND_NAME(var) #var, var


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
	T read()
	{
		const uint64 at = _at;
		_at += sizeof(T);
		return *reinterpret_cast<const T*>(&_binaryFile._bytes[at]);
	}
	template<typename T>
	const T* read(uint64 count)
	{
		const uint64 at = _at;
		_at += sizeof(T) * count;
		return reinterpret_cast<const T*>(&_binaryFile._bytes[at]);
	}
	bool can_read(uint64 count) const
	{
		return _at + count <= _binaryFile._bytes.size();
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
		JSONValue& parentJSONValue = getJSONValue(parentID);
		if (name.empty() == false && parentJSONValue._childIDs.empty() == false)
		{
			for (const JSONValueID& childID : parentJSONValue._childIDs)
			{
				const JSONValue& child = getJSONValue(childID);
				if (child._name == name)
				{
					ASSERT(false);
					return getRootJSONValueID();
				}
			}
		}
		JSONValue jsonValue;
		{
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
		writeJSONValue(ofs, getRootJSONValueID());
	}

private:
	void writeJSONValue_processEscape(std::string& value) const
	{
		int32 at = static_cast<int32>(value.size()) - 1;
		while (at > 0)
		{
			if (value[at] == '\\' && (at == 0 || value[at - 1] != '\\'))
			{
				value.insert(value.begin() + at, '\\');
			}
			--at;
		}
	}
	void writeJSONValue(std::ofstream& ofs, const JSONValueID& id) const
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
				writeJSONValue_processEscape(buffer);
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
			writeJSONValue(ofs, childID);
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
		const int32 map_type_or_map_width = binaryParser.read<int32>();
		ASSERT(map_type_or_map_width == -1);

		const int32 tile_width = binaryParser.read<int32>();
		const int32 tile_height = binaryParser.read<int32>();
		const int32 map_width = binaryParser.read<int32>();
		const int32 map_height = binaryParser.read<int32>();
		const int32 unknown_width = binaryParser.read<int32>();
		const int32 unknown_height = binaryParser.read<int32>();
		const int16 tile_count = binaryParser.read<int16>();
		const int16 layer_count = binaryParser.read<int16>();
		const int16 map_unknown_0 = binaryParser.read<int16>();
		const int16 minimap_file_count = binaryParser.read<int16>();
		const int16 map_unknown_1 = binaryParser.read<int16>();
		const int16 map_unknown_2 = binaryParser.read<int16>();

		JSONFile jsonFile{ "map.json" };
		{
			const JSONValueID metaDataID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "meta_data", JSONValueType::Object);
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_type_or_map_width));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(tile_width));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(tile_height));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_width));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_height));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(unknown_width));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(unknown_height));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(tile_count));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(layer_count));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_unknown_0));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(minimap_file_count));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_unknown_1));
			jsonFile.pushNameValuePair(metaDataID, STRING_AND_NAME(map_unknown_2));
		}
		{
			const JSONValueID tileFileNamesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "tile_filenames", JSONValueType::Array);
			for (int16 tileIndex = 0; tileIndex < tile_count; ++tileIndex)
			{
				const char* const tileFileName = binaryParser.read<char>(MAX_PATH);
				jsonFile.pushValue(tileFileNamesID, tileFileName);
			}
		}
		{
			const JSONValueID layerFileNamesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "layer_filenames", JSONValueType::Array);
			for (int16 layerIndex = 0; layerIndex < layer_count; ++layerIndex)
			{
				const char* const layerFileName = binaryParser.read<char>(MAX_PATH);
				jsonFile.pushValue(layerFileNamesID, layerFileName);
			}
		}
		{
			const JSONValueID layerColorKeysID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "layer_colorkeys", JSONValueType::Array);
			for (int16 layerIndex = 0; layerIndex < layer_count; ++layerIndex)
			{
				const int32 colorKey = binaryParser.read<int32>();
				jsonFile.pushValue(layerColorKeysID, colorKey);
			}
		}
		{
			const JSONValueID minimapFileNamesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "minimap_filenames", JSONValueType::Array);
			for (int16 minimapFileIndex = 0; minimapFileIndex < minimap_file_count; ++minimapFileIndex)
			{
				const char* const minimapFileName = binaryParser.read<char>(MAX_PATH);
				jsonFile.pushValue(minimapFileNamesID, minimapFileName);
			}
		}
		{
			const JSONValueID mapAreasID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "map_areas", JSONValueType::Array);
			const int32 mapAreaCount = binaryParser.read<int32>();
			for (int32 mapAreaIndex = 0; mapAreaIndex < mapAreaCount; ++mapAreaIndex)
			{
				const int32 area_type = binaryParser.read<int32>();
				const int32 x0 = binaryParser.read<int32>();
				const int32 y0 = binaryParser.read<int32>();
				const int32 x1 = binaryParser.read<int32>();
				const int32 y1 = binaryParser.read<int32>();

				const JSONValueID mapAreaID = jsonFile.createJSONValue(mapAreasID, "", JSONValueType::Object);
				jsonFile.pushNameValuePair(mapAreaID, STRING_AND_NAME(area_type));
				jsonFile.pushNameValuePair(mapAreaID, STRING_AND_NAME(x0));
				jsonFile.pushNameValuePair(mapAreaID, STRING_AND_NAME(y0));
				jsonFile.pushNameValuePair(mapAreaID, STRING_AND_NAME(x1));
				jsonFile.pushNameValuePair(mapAreaID, STRING_AND_NAME(y1));
			}
		}
		{
			const JSONValueID placedTilesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "placed_tiles", JSONValueType::Array);
			const int32 placedTileCount = binaryParser.read<int32>();
			for (int32 placedTileIndex = 0; placedTileIndex < placedTileCount; ++placedTileIndex)
			{
				const int32 x = binaryParser.read<int32>();
				const int32 y = binaryParser.read<int32>();
				const int32 tile_index = binaryParser.read<int32>();

				const JSONValueID placedTileID = jsonFile.createJSONValue(placedTilesID, "", JSONValueType::Object);
				jsonFile.pushNameValuePair(placedTileID, STRING_AND_NAME(x));
				jsonFile.pushNameValuePair(placedTileID, STRING_AND_NAME(y));
				jsonFile.pushNameValuePair(placedTileID, STRING_AND_NAME(tile_index));
			}
		}
		{
			const JSONValueID placedLayersID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "placed_layers", JSONValueType::Array);
			const int32 placedLayerCount = binaryParser.read<int32>();
			for (int32 placedLayerIndex = 0; placedLayerIndex < placedLayerCount; ++placedLayerIndex)
			{
				const int32 x = binaryParser.read<int32>();
				const int32 y = binaryParser.read<int32>();
				const int16 unknown_0 = binaryParser.read<int16>();
				const int16 unknown_1 = binaryParser.read<int16>();
				const int16 unknown_2 = binaryParser.read<int16>();
				const int16 unknown_3 = binaryParser.read<int16>();
				const int32 unknown_4 = binaryParser.read<int32>();
				const int16 layer_index = binaryParser.read<int16>();
				const uint8 unknown_5 = binaryParser.read<uint8>();
				const uint8 unknown_6 = binaryParser.read<uint8>();
				const int32 unknown_7 = binaryParser.read<int32>();
				const int32 unknown_8 = binaryParser.read<int32>();
				const int32 unknown_9 = binaryParser.read<int32>();
				const int32 unknown_10 = binaryParser.read<int32>();
				const int32 unknown_11 = binaryParser.read<int32>();
				const int32 unknown_12 = binaryParser.read<int32>();

				const JSONValueID placedLayerID = jsonFile.createJSONValue(placedLayersID, "", JSONValueType::Object);
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(x));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(y));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_0));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_1));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_2));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_3));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_4));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(layer_index));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_5));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_6));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_7));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_8));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_9));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_10));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_11));
				jsonFile.pushNameValuePair(placedLayerID, STRING_AND_NAME(unknown_12));
			}
		}
		{
			const JSONValueID unknownValuesID = jsonFile.createJSONValue(jsonFile.getRootJSONValueID(), "unknown_values", JSONValueType::Array);
			while (binaryParser.can_read(4))
			{
				const int32 unknown_value = binaryParser.read<int32>();
				jsonFile.pushValue(unknownValuesID, unknown_value);
			}
		}
		jsonFile.save();
		break;
	}
	return 0;
}
