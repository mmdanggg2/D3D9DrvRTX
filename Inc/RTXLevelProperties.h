#pragma once

#include "Engine.h"

#include <string>
#include <vector>
#include <memory>

class RTXAnchor {
protected:
	std::string name;
	size_t hash;
	FVector location;
	FVector rotation;
	FVector angularVelocity;

public:
	RTXAnchor(std::string name, FVector location, FVector startRot, FVector rotationRate)
		: name(name), location(location), rotation(startRot), angularVelocity(rotationRate) {
		hash = std::hash<std::string>()(name);
	}

	virtual void Tick(float deltaTime);

	const FVector& getLocation() const {
		return location;
	}
	const FVector& getRotation() const {
		return rotation;
	}
	size_t getHash() const {
		return hash;
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
	RTXAnchorLinear(std::string name, FVector startLoc, FVector startRot, FVector rotationRate, FVector endLoc, float speed)
		: RTXAnchor(name, startLoc, startRot, rotationRate), pathStart(startLoc), pathDirection(endLoc - startLoc), speed(speed) {
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
	RTXAnchorPingPong(std::string name, FVector startLoc, FVector startRot, FVector rotationRate, FVector endLoc, float speed) 
		: RTXAnchorLinear(name, startLoc, startRot, rotationRate, endLoc, speed), pathLengthReal(pathLength){
		pathLength = pathLength * 2;
	}

	void Tick(float deltaTime) override;
};

void loadLevelJson(const std::string& levelName, std::vector<std::unique_ptr<RTXAnchor>>& anchors);
