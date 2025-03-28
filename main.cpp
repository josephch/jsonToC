#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "cJSON/cJSON.h"

struct JsonField {
	std::string name;
	std::string variableName;
	std::string type;
	int size;
};

std::vector<JsonField> fields;
std::map<std::string, std::vector<JsonField>> structs;

std::string getVariableName(const char *itemName) {
	std::string ret = itemName;
	std::replace(ret.begin(), ret.end(), ' ', '_');
	if (isdigit(itemName[0])) {
		ret = std::string("N") + itemName;
	}
	return ret;
}

void parseJson(const cJSON *json, const std::string &structName) {
	std::vector<JsonField> &fieldList = structs[structName];

	cJSON *item = json->child;
	while (item) {
		JsonField field;
		field.name = item->string;
		field.variableName = getVariableName(item->string);

		if (cJSON_IsString(item)) {
			field.type = "char";
			field.size = strlen(item->valuestring) + 1;
		} else if (cJSON_IsNumber(item)) {
			field.type = "int";
			field.size = 0;
		} else if (cJSON_IsObject(item)) {
			std::string nestedStruct = structName + "_" + field.name;
			parseJson(item, nestedStruct);
			field.type = getVariableName(nestedStruct.c_str());
			field.size = 0;
		} else {
			std::cerr << "type not supported\n";
			abort();
		}

		fieldList.push_back(field);
		item = item->next;
	}
}

void writeStructs(std::ofstream &file) {
	file << "#include \"cJSON.h\"\n";
	file << "#include <string.h>\n\n";
	for (const auto &structPair : structs) {
		for (const auto &field : structPair.second) {
			if (field.type == "char") {
				std::string defineName = structPair.first + "_" + field.name + "_LEN";
				std::replace(defineName.begin(), defineName.end(), ' ', '_');
				std::transform(defineName.begin(), defineName.end(), defineName.begin(), ::toupper);
				file << "#define " << defineName << " " << field.size << "\n";
			}
		}
	}
	file << "\n";

	for (auto structPair = structs.rbegin(); structPair != structs.rend(); ++structPair) {
		std::string structureName = getVariableName(structPair->first.c_str());
		file << "typedef struct " << structureName << " {\n";
		for (const auto &field : structPair->second) {
			if (field.type == "char") {
				std::string defineName = structPair->first + "_" + field.name + "_LEN";
				std::replace(defineName.begin(), defineName.end(), ' ', '_');
				std::transform(defineName.begin(), defineName.end(), defineName.begin(), ::toupper);
				file << "    " << field.type << " " << field.variableName << "[" << defineName << "];\n";
			} else {
				file << "    " << field.type << " " << field.variableName << ";\n";
			}
		}
		file << "} " << std::move(structureName) << ";\n\n";
	}
}

void writeParsingFunctions(std::ofstream &file) {
	for (auto structPair = structs.rbegin(); structPair != structs.rend(); ++structPair) {
		std::string structName = getVariableName(structPair->first.c_str());

		file << "void parse_" << structName << "(" << structName << " *obj, cJSON *json) {\n";
		file << "    if (!obj || !json) return;\n\n";
		file << "    cJSON *item;\n";
		for (const auto &field : structPair->second) {
			if (field.type == "char") {
				std::string defineName = structName + "_" + field.name + "_LEN";
				std::replace(defineName.begin(), defineName.end(), ' ', '_');
				std::transform(defineName.begin(), defineName.end(), defineName.begin(), ::toupper);
				file << "    item = cJSON_GetObjectItem(json, \"" << field.name << "\");\n";
				file << "    if (cJSON_IsString(item) && (item->valuestring != NULL)) {\n";
				file << "        strncpy(obj->" << field.variableName << ", item->valuestring, " << defineName << " - 1);\n";
				file << "        obj->" << field.variableName << "[" << defineName << " - 1] = '\\0';\n";
				file << "    }\n\n";
			} else if (field.type == "int") {
				file << "    item = cJSON_GetObjectItem(json, \"" << field.name << "\");\n";
				file << "    if (cJSON_IsNumber(item)) {\n";
				file << "        obj->" << field.variableName << " = item->valueint;\n";
				file << "    }\n\n";
			} else {
				file << "    item = cJSON_GetObjectItem(json, \"" << field.name << "\");\n";
				file << "    if (cJSON_IsObject(item)) {\n";
				file << "        parse_" << field.type << "(&(obj->" << field.variableName << "), item);\n";
				file << "    }\n\n";
			}
		}
		file << "}\n\n";
	}
}

int main(int argc, char *argv[]) {
	if (argc < 5) {
		std::cerr << "Usage: " << argv[0] << " <json_input> <root name> <skip-level> <output_file>\n";
		return 1;
	}

	std::string jsonInput = argv[1];
	std::string rootName = argv[2];
	int skipLevel = std::stoi(argv[3]);
	std::string outputFilename = argv[4];
	std::ifstream jsonFile(jsonInput);
	if (!jsonFile) {
		std::cerr << "Error opening input JSON file." << std::endl;
		return 1;
	}

	std::stringstream buffer;
	buffer << jsonFile.rdbuf();
	jsonFile.close();

	cJSON *json = cJSON_Parse(buffer.str().c_str());
	if (!json) {
		std::cerr << "Error parsing JSON " << jsonInput << "\n";
		return 1;
	}

	while (skipLevel) {
		json = json->child;
		skipLevel--;
	}

	parseJson(json, rootName);

	std::ofstream file(outputFilename);
	if (!file) {
		std::cerr << "Error opening output file\n";
		return 1;
	}

	writeStructs(file);
	writeParsingFunctions(file);

	file.close();
	cJSON_Delete(json);

	std::cout << "C code written to " << outputFilename << "\n";
	return 0;
}
