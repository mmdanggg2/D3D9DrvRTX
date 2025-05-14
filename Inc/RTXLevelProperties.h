#pragma once

#include "Engine.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class RTXAnchor {
protected:
	std::string name;
	uint32_t hash;
	FVector location;
	FVector rotation;
	FVector scale;
	FVector angularVelocity;
	bool pausable;

public:
	RTXAnchor(
		std::string name,
		FVector location,
		FVector startRot,
		FVector scale,
		FVector rotationRate,
		bool pausable
	);

	virtual void Tick(float deltaTime);

	const FVector& getLocation() const {
		return location;
	}
	const FVector& getRotation() const {
		return rotation;
	}
	const FVector& getScale() const {
		return scale;
	}
	uint32_t getHash() const {
		return hash;
	}
	bool isPausable() const {
		return pausable;
	}
};

class RTXAnchorLinear : public RTXAnchor {
protected:
	FVector pathStart;
	FVector pathDirection;
	float pathLength;
	float speed;
	float pathLocation = 0;
public:
	RTXAnchorLinear(
		std::string name,
		FVector startLoc,
		FVector startRot,
		FVector scale,
		FVector rotationRate,
		bool pausable,
		FVector endLoc,
		float speed
	) : RTXAnchor(
		name,
		startLoc,
		startRot,
		scale,
		rotationRate,
		pausable
	), pathStart(startLoc), pathDirection(endLoc - startLoc), speed(speed) {
		pathLength = pathDirection.Size();
		pathDirection.Normalize();
	}
	using RTXAnchor::RTXAnchor;

	void Tick(float deltaTime) override;
};

class RTXAnchorPingPong : public RTXAnchorLinear {
protected:
	float pathLengthReal;

public:
	RTXAnchorPingPong(
		std::string name,
		FVector startLoc,
		FVector startRot,
		FVector scale,
		FVector rotationRate,
		bool pausable,
		FVector endLoc,
		float speed
	) : RTXAnchorLinear(
		name,
		startLoc,
		startRot,
		scale,
		rotationRate,
		pausable,
		endLoc,
		speed
	), pathLengthReal(pathLength){
		pathLength = pathLength * 2;
	}

	void Tick(float deltaTime) override;
};

typedef std::vector<std::unique_ptr<RTXAnchor>> RTXAnchors;
typedef std::unordered_map<std::string, std::string> RTXConfigVars;

std::unordered_set<std::wstring> getHashTexBlacklist();
void loadLevelJson(const TCHAR* levelName, RTXAnchors& anchors, RTXConfigVars& remixConfigVaraibles);
