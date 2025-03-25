#include "windows.h"

#include "RTXLevelProperties.h"
#include "D3D9DebugUtils.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <fstream>

using json = nlohmann::json;

double floored_mod(double a, double b) {
	return a - b * std::floor(a / b);
}

void RTXAnchor::Tick(float deltaTime) {
	rotation += angularVelocity * deltaTime;
	if (rotation.X > 360.0f || rotation.X < 0.0f) {
		rotation.X = floored_mod(rotation.X, 360.0);
	}
	if (rotation.Y > 360.0f || rotation.Y < 0.0f) {
		rotation.Y = floored_mod(rotation.Y, 360.0);
	}
	if (rotation.Z > 360.0f || rotation.Z < 0.0f) {
		rotation.Z = floored_mod(rotation.Z, 360.0);
	}
}

void RTXAnchorLinear::Tick(float deltaTime) {
	RTXAnchor::Tick(deltaTime);
	pathLocation += speed * deltaTime;
	if (pathLocation > pathLength || pathLocation < 0) {
		pathLocation = floored_mod(pathLocation, pathLength);
	}
	location = pathStart + pathDirection * pathLocation;
}

void RTXAnchorPingPong::Tick(float deltaTime) {
	RTXAnchorLinear::Tick(deltaTime);
	location = pathStart + pathDirection * (pathLocation < pathLengthReal ? pathLocation : pathLength - pathLocation);
}

std::wstring s2ws(const std::string& str) {
	if (str.empty()) {
		return L"";
	}

	// Calculate the required buffer size
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	if (size_needed <= 0) {
		throw std::runtime_error("MultiByteToWideChar failed");
	}

	// Allocate the buffer
	std::wstring result(size_needed - 1, 0); // -1 because size_needed includes null terminator

	// Perform the conversion
	int chars_converted = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size_needed);
	if (chars_converted <= 0) {
		throw std::runtime_error("MultiByteToWideChar failed");
	}

	return result;
}

std::string ws2s(const std::wstring& wstr) {
	if (wstr.empty()) {
		return "";
	}

	// Calculate the required buffer size
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	if (size_needed <= 0) {
		throw std::runtime_error("WideCharToMultiByte failed");
	}

	// Allocate the buffer
	std::string result(size_needed - 1, 0); // -1 because size_needed includes null terminator

	// Perform the conversion
	int bytes_written = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size_needed, NULL, NULL);
	if (bytes_written <= 0) {
		throw std::runtime_error("WideCharToMultiByte failed");
	}

	return result;
}

void from_json(const json& j, FVector& p) {
	j.at(0).get_to(p.X);
	j.at(1).get_to(p.Y);
	j.at(2).get_to(p.Z);
}

std::string toLower(const std::string& s) {
	std::string lower = s;
	std::transform(s.begin(), s.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return lower;
}

std::string normalize_level_name(const std::string& input) {
	// 1. Replace backslashes with forward slashes
	std::string s = input;
	std::string::size_type pos = 0;
	while ((pos = s.find('\\', pos)) != std::string::npos) {
		s.replace(pos, 1, "/");
		pos++;
	}

	// 2. Split by '/' and take the last element (filename)
	std::string filename;
	size_t last_slash = s.find_last_of('/');
	if (last_slash != std::string::npos) {
		filename = s.substr(last_slash + 1);
	}
	else {
		filename = s;
	}

	std::string result;
	// 3. Split the filename by '.' and remove the last element (extension)
	if (filename.find('.') != std::string::npos) {
		std::vector<std::string> parts;
		size_t start = 0, end;
		while ((end = filename.find('.', start)) != std::string::npos) {
			parts.push_back(filename.substr(start, end - start));
			start = end + 1;
		}
		// 3b. Join the parts with '.'
		for (size_t i = 0; i < parts.size(); i++) {
			if (i > 0) result += '.';
			result += parts[i];
		}
	}
	else {
		result = filename;
	}


	// 4. Convert to lowercase
	return toLower(result);
}

void loadAnchorsArray(json& anchorsArr, const std::string& levelName, RTXAnchors& anchors) {
	for (json anchorObj : anchorsArr) {
		if (!anchorObj.is_object()) {
			debugf(TEXT("Error RTXAnchor '%s' in level '%s' is not an object!"), s2ws(anchorObj.dump()).c_str(), s2ws(levelName).c_str());
			continue;
		}

#define GET_ANCHOR_MEMBER(KEY, VAR) \
		try {\
			anchorObj.at(#KEY).get_to(VAR);\
		}\
		catch (const json::exception& e) {\
			debugf(TEXT("Error on RTXAnchor '%s' in level '%s', failed to parse '"#KEY"': %s"), s2ws(anchorObj.dump()).c_str(), s2ws(levelName).c_str(), s2ws(e.what()).c_str());\
			anchorError = true;\
		}

#define GET_ANCHOR_MEMBER_OPTIONAL(KEY, VAR) \
		if (anchorObj.contains(#KEY)) {\
			GET_ANCHOR_MEMBER(KEY, VAR);\
		}

		bool anchorError = false;
		std::string name;
		GET_ANCHOR_MEMBER(name, name);
		std::string animType{ "linear" };
		GET_ANCHOR_MEMBER_OPTIONAL(anim_type, animType);
		FVector startLoc;
		GET_ANCHOR_MEMBER(start_loc, startLoc);
		FVector startRot{};
		GET_ANCHOR_MEMBER_OPTIONAL(start_rot, startRot);
		FVector rotationRate{};
		GET_ANCHOR_MEMBER_OPTIONAL(rotation_rate, rotationRate);
		FVector scale{ 1, 1, 1 };
		GET_ANCHOR_MEMBER_OPTIONAL(scale, scale);
		if (animType == "static") {
			if (anchorError) continue;
			anchors.push_back(std::make_unique<RTXAnchor>(name, startLoc, startRot, scale, rotationRate));
		}
		else if (animType == "linear" || animType == "ping-pong") {
			FVector endLoc;
			GET_ANCHOR_MEMBER(end_loc, endLoc);
			float speed = 0.0f;
			GET_ANCHOR_MEMBER(speed, speed);
			if (anchorError) continue;
			if (animType == "ping-pong") {
				anchors.push_back(std::make_unique<RTXAnchorPingPong>(name, startLoc, startRot, scale, rotationRate, endLoc, speed));
			}
			else {
				anchors.push_back(std::make_unique<RTXAnchorLinear>(name, startLoc, startRot, scale, rotationRate, endLoc, speed));
			}
		}
		else {
			debugf(TEXT("Unknown anim_type on RTXAnchor '%s' in level '%s': %s"), s2ws(anchorObj.dump()).c_str(), s2ws(levelName).c_str(), s2ws(animType).c_str());
		}
#undef GET_ANCHOR_MEMBER_OPTIONAL
#undef GET_ANCHOR_MEMBER
	}
}

void loadConfigVars(json& configVars, const std::string& levelName, RTXConfigVars& remixConfigVariables) {
	for (const auto& [key, value] : configVars.items()) {
		if (!value.is_string()) {
			debugf(NAME_Warning, TEXT("RTX config var '%s' in level '%s' is not a string!"), s2ws(key).c_str(), s2ws(levelName).c_str());
			continue;
		}
		remixConfigVariables[key] = value;
	}
}

void loadLevelJson(const TCHAR* rawLevelName, RTXAnchors& anchors, RTXConfigVars& remixConfigVariables) {
	std::wstring fileName = L"D3D9DrvRTX_level_properties.json";
	std::ifstream file = std::ifstream(fileName);
	if (!file.is_open()) {
		debugf(TEXT("Could not open RTX level properties file '%s'!"), fileName.c_str());
		return;
	}
	std::string levelName = normalize_level_name(ws2s(rawLevelName));
	try {
		json configJson = json::parse(file);
		json levelObj;
		for (const auto& [key, val] : configJson.items()) {
			if (toLower(key) == levelName) {
				levelObj = val;
				break;
			}
		}
		if (levelObj.is_null()) {
			debugf(TEXT("Level '%s' was not found in RTX level properties, attempting to use '_default_level'"), s2ws(levelName).c_str());
			levelObj = configJson["_default_level"];
			if (levelObj.is_null()) {
				debugf(TEXT("No '_default_level' properties were found in RTX level properties."));
				return;
			}
		}
		if (!levelObj.is_object()) {
			debugf(TEXT("Level '%s' properties value is not a json object!"), s2ws(levelName).c_str());
			return;
		}
		json& anchorsArr = levelObj["anchors"];
		if (anchorsArr.is_array()) {
			loadAnchorsArray(anchorsArr, levelName, anchors);
		}
		json& configVars = levelObj["config_vars"];
		if (configVars.is_object()) {
			loadConfigVars(configVars, levelName, remixConfigVariables);
		}
	}
	catch (const json::exception& e) {
		debugf(TEXT("Error loading rtx level properties for level '%s': %s"), s2ws(levelName).c_str(), s2ws(e.what()).c_str());
	}
}
