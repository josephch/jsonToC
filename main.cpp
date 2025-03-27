#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "cJSON/cJSON.h"

struct JsonField {
	std::string name;
	std::string type;
	int size;
};

std::vector<JsonField> fields;
std::map<std::string, std::vector<JsonField>> structs;

void parseJson(const cJSON *json, const std::string &structName) {
	std::vector<JsonField> &fieldList = structs[structName];

	cJSON *item = json->child;
	while (item) {
		JsonField field;
		field.name = item->string;

		if (cJSON_IsString(item)) {
			field.type = "char";
			field.size = strlen(item->valuestring) + 1;
		} else if (cJSON_IsNumber(item)) {
			field.type = "int";
			field.size = 0;
		} else if (cJSON_IsObject(item)) {
			std::string nestedStruct = structName + "_" + field.name;
			parseJson(item, nestedStruct);
			field.type = nestedStruct;
			field.size = 0;
		}

		fieldList.push_back(field);
		item = item->next;
	}
}

void writeStructs(std::ofstream &file) {
	// Gather all #defines
	file << "/* String Length Defines */\n";
	for (const auto &structPair : structs) {
		for (const auto &field : structPair.second) {
			if (field.type == "char") {
				std::string defineName = structPair.first + "_" + field.name + "_LEN";
				std::transform(defineName.begin(), defineName.end(), defineName.begin(), ::toupper);
				file << "#define " << defineName << " " << field.size << "\n";
			}
		}
	}
	file << "\n";

	// Write structs in order
	for (const auto &structPair : structs) {
		file << "typedef struct " << structPair.first << " {\n";
		for (const auto &field : structPair.second) {
			if (field.type == "char") {
				std::string defineName = structPair.first + "_" + field.name + "_LEN";
				std::transform(defineName.begin(), defineName.end(), defineName.begin(), ::toupper);
				file << "    " << field.type << " " << field.name << "[" << defineName << "];\n";
			} else {
				file << "    " << field.type << " " << field.name << ";\n";
			}
		}
		file << "} " << structPair.first << ";\n\n";
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <json_input> <output_file>\n";
		return 1;
	}

	std::string jsonInput = argv[1];
	std::string outputFilename = argv[2];
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

	parseJson(json, "RootStruct");

	std::ofstream file(outputFilename);
	if (!file) {
		std::cerr << "Error opening output file\n";
		return 1;
	}

	writeStructs(file);

	file.close();
	cJSON_Delete(json);

	std::cout << "C structure written to " << outputFilename << "\n";
	return 0;
}
