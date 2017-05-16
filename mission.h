#ifndef _H_COMPUTEREX_MISSION_H_WE_ARE_BACK_BABY_
#define _H_COMPUTEREX_MISSION_H_WE_ARE_BACK_BABY_

#include <string>
#include <string.h>
#include <vector>
using namespace std;

class Waypoint {
public:
	Waypoint(const char* waypointName, const char* waypointType,const char* referenceName, vector<string> tokens) {
		name = waypointName;
		type = waypointType;
	}
	bool IsAtWaypoint(OBJHANDLE hVessel) {

	}
private:
	std::string name, refName, type;
};

class Mission {
public:
	Mission(const char* mission_name) {

	}
private:
};

#endif