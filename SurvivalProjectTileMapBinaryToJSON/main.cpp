#include <iostream>
#include <fstream>
#include <Windows.h>
#include <filesystem>
#include <vector>


#define ASSERT(expression) if ((expression) == false) { ::MessageBoxA(nullptr, "Assertion failed!", "ASSERT", MB_OK); std::cout << "Assertion Failed in File: " << __FILE__ << " Line:" << __LINE__; ::DebugBreak; }


using int16 = int16_t;
using int32 = int32_t;
using uint64 = uint64_t;

class BinaryFile;
class BinaryParser;

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

struct JSONObject
{
	uint64 _depth = 0;
	std::string _declarationName;
	std::string _value;
};

class JSONFile
{
public:
	JSONFile(const std::string& fileName) : _fileName{ fileName } { __noop; }

	std::vector<JSONObject> jsonObjects;
	const std::string& _fileName;
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
		ASSERT(mapTypeOrMapWidth == -11);

		const int32 tileWidth = binaryParser.readBinary<int32>();
		const int32 tileHeight = binaryParser.readBinary<int32>();
		int32 mapWidth = binaryParser.readBinary<int32>();
		int32 mapHeight = binaryParser.readBinary<int32>();
		int32 unknownWidth = binaryParser.readBinary<int32>();
		int32 unknownHeight = binaryParser.readBinary<int32>();
		int16 tileCount = binaryParser.readBinary<int16>();
		int16 layerCount = binaryParser.readBinary<int16>();
		int16 mapUnknown0 = binaryParser.readBinary<int16>();
		int16 minimapFileCount = binaryParser.readBinary<int16>();
		int16 mapUnknown1 = binaryParser.readBinary<int16>();
		int16 mapUnknown2 = binaryParser.readBinary<int16>();

		break;
	}
	return 0;
}
