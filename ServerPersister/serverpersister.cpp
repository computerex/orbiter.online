#define STRICT
#define ORBITER_MODULE
#define _CRT_SECURE_NO_WARNINGS

#include <Shlwapi.h>
#include <orbitersdk.h>
#include "curl.h"
#include <map>
#include <string>
#include <rapidjson\document.h>
#include <rapidjson\writer.h>
#include <rapidjson\stringbuffer.h>
#include <iostream>
#include <vector>
#include <io.h>
#include <mutex>

using namespace std;
using namespace rapidjson;

class SimpleVesselState
{
public:
	bool landed;
	string refplanet, name, className;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz, heading,
		retro, hover, main, mjd, accx, accy, accz;
};

map<string, SimpleVesselState> vesselList, serverVesselList;
vector<string> masterConfigFileList;
mutex stateLock;
double last_update_time=0;
string persistID = "";
thread *serverThread;

void ScanConfigDir(const char *ppath, vector<string>& configFileList)
{
	struct _finddata_t fdata;
	long fh;
	char cbuf[MAX_PATH], path[MAX_PATH];

	strcpy(path, ppath);
	PathCombine(path, path, "*.*");
	// scan for subdirectories
	if ((fh = _findfirst(path, &fdata)) != -1) {
		do {
			std::string paths(fdata.name);
			int pos = 0;
			PathCombine(cbuf, ppath, fdata.name);
			if ((fdata.attrib & _A_SUBDIR) && fdata.name[0] != '.') {
				ScanConfigDir(cbuf, configFileList);
			}
			else if (fdata.name[0] != '.' && (pos = paths.find_last_of(".")) != std::string::npos) {
				configFileList.push_back(paths.substr(0, pos));
			}
		} while (!_findnext(fh, &fdata));
		_findclose(fh);
	}
}

bool isVesselCompatible(std::string className)
{
	if (masterConfigFileList.size() == 0) {
		ScanConfigDir("Config\\Vessels", masterConfigFileList);
	}
	int classNameLength = className.length();
	for (unsigned int i = 0; i < masterConfigFileList.size(); i++)
	{
		if (masterConfigFileList[i].length() == classNameLength &&
			_strnicmp(masterConfigFileList[i].c_str(), className.c_str(), classNameLength) == 0) {
			return true;
		}
	}
	return false;
}

map<string, SimpleVesselState> parseVesselStates(string teleJson) {
	Document d;
	d.Parse(teleJson.c_str());
	map<string, SimpleVesselState> newVesselList;
	if (d.IsArray()) {
		for (unsigned int i = 0; i < d.Size(); i++)
		{
			string name = d[i]["name"].GetString();
			SimpleVesselState s = SimpleVesselState();
			s.name = name;
			s.refplanet = d[i]["refplanet"].GetString();
			s.landed = d[i]["landed"].GetBool();
			s.lon = d[i]["lon"].GetDouble();
			s.lat = d[i]["lat"].GetDouble();
			s.rposx = d[i]["rposx"].GetDouble();
			s.rposy = d[i]["rposy"].GetDouble();
			s.rposz = d[i]["rposz"].GetDouble();
			s.rvelx = d[i]["rvelx"].GetDouble();
			s.rvely = d[i]["rvely"].GetDouble();
			s.rvelz = d[i]["rvelz"].GetDouble();
			s.arotx = d[i]["arotx"].GetDouble();
			s.aroty = d[i]["aroty"].GetDouble();
			s.arotz = d[i]["arotz"].GetDouble();
			s.heading = d[i]["heading"].GetDouble();
			s.className = d[i]["className"].GetString();
			s.retro = d[i]["retro"].GetDouble();
			s.hover = d[i]["hover"].GetDouble();
			s.main = d[i]["main"].GetDouble();
			s.mjd = d[i]["mjd"].GetDouble();
			s.accx = d[i]["accx"].GetDouble();
			s.accy = d[i]["accy"].GetDouble();
			s.accz = d[i]["accz"].GetDouble();
			newVesselList[name] = s;
		}
	}
	return newVesselList;
}

string getTele(map<string, SimpleVesselState> vessels) {
	Document d;
	d.SetArray();
	Document::AllocatorType& a = d.GetAllocator();
	for (map<string, SimpleVesselState>::iterator it = vessels.begin(); it != vessels.end(); ++it)
	{
		Value v("vessel");
		v.SetObject();
		SimpleVesselState s = it->second;
		if (s.landed)
		{
			s.rposx = s.rposy = s.rposz = s.rvelx = s.rvely = s.rvelz = s.arotx = s.aroty = s.arotz = s.main = s.retro = s.hover = 0;
		}
		v.AddMember("landed", it->second.landed, a);
		Value valr(s.refplanet.c_str(), a);
		v.AddMember("refplanet", valr, a);
		v.AddMember("lon", s.lon, a);
		v.AddMember("lat", s.lat, a);
		v.AddMember("rposx", s.rposx, a);
		v.AddMember("rposy", s.rposy, a);
		v.AddMember("rposz", s.rposz, a);
		v.AddMember("rvelx", s.rvelx, a);
		v.AddMember("rvely", s.rvely, a);
		v.AddMember("rvelz", s.rvelz, a);
		v.AddMember("arotx", s.arotx, a);
		v.AddMember("aroty", s.aroty, a);
		v.AddMember("arotz", s.arotz, a);
		v.AddMember("heading", s.heading, a);
		v.AddMember("main", s.main, a);
		v.AddMember("hover", s.hover, a);
		v.AddMember("retro", s.retro, a);
		v.AddMember("mjd", s.mjd, a);
		v.AddMember("accx", s.accx, a);
		v.AddMember("accy", s.accy, a);
		v.AddMember("accz", s.accz, a);
		valr = Value(s.className.c_str(), a);
		v.AddMember("className", valr, a);
		valr = Value(s.name.c_str(), a);
		v.AddMember("name", valr, a);
		d.PushBack(v, a);
	}
	GenericStringBuffer<UTF8<>> sbuf;
	Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);
	d.Accept(writer);
	std::string str = sbuf.GetString();
	return str;
}

void proc()
{
	while (true) {
		stateLock.lock();
		map<string, SimpleVesselState> vesselStates = vesselList;
		stateLock.unlock();
		string teleStr = getTele(vesselStates);
		string resp = curl_post("http://orbiter.world/posttele?pid=" + persistID, teleStr);
		map<string, SimpleVesselState> newVesselList = parseVesselStates(resp);

		stateLock.lock();
		serverVesselList = newVesselList;
		stateLock.unlock();

		Sleep(1000);
	}
}
void checkgoogle()
{
	//string req = curl_get("http://www.google.com");
}
DLLCLBK void InitModule(HINSTANCE hDLL)
{
	//gcheck.join();
}

DLLCLBK void ExitModule(HINSTANCE hDLL)
{
}

DLLCLBK void opcCloseRenderViewport()
{
	//curl_post("http://orbiter.world/persister/exit?pid=" + persistID, "{}");
}

string getPersistIDFromDisk() {
	FILE* file = fopen("persistid", "r");
	if (file == 0) return "";
	char buffer[512];
	fgets(buffer, 512, file);
	fclose(file);
	return buffer;
}

bool first = false;
DLLCLBK void opcPreStep(double simt, double simdt, double mjd) {
	if (!first) {
		first = true;
		persistID = getPersistIDFromDisk();
		if (persistID == "") {
			persistID = curl_get("http://orbiter.world/persister/register");
			FILE* persistFile = fopen("persistid", "w");
			fputs(persistID.c_str(), persistFile);
			fclose(persistFile);
		}
		serverThread = new thread(proc);
	}
	double syst = oapiGetSysTime();
	if (syst > last_update_time + 1) {
		// update state vectors
		stateLock.lock();
		for (map<string, SimpleVesselState>::iterator it = vesselList.begin(); it != vesselList.end(); it++) {
			SimpleVesselState s = it->second;
			OBJHANDLE ves = oapiGetVesselByName((char*)s.name.c_str());
			if (!oapiIsVessel(ves)) {
				continue;
			}
			VESSEL * v = oapiGetVesselInterface(ves);
			VESSELSTATUS2 vs;
			memset(&vs, 0, sizeof(VESSELSTATUS2));
			vs.version = 2;
			v->GetStatusEx(&vs);
			char buffer[256];
			vs.rbody = v->GetSurfaceRef();
			if (vs.rbody != 0)
				oapiGetObjectName(vs.rbody, buffer, 256);
			else
				memset(buffer, 0, 256);
			s.arotx = vs.arot.x;
			s.aroty = vs.arot.y;
			s.arotz = vs.arot.z;
			s.className = v->GetClassNameA();
			s.heading = vs.surf_hdg;
			s.mjd = mjd;
			s.hover = v->GetThrusterGroupLevel(THGROUP_HOVER);
			s.landed = 0;
			s.lat = vs.surf_lat;
			s.lon = vs.surf_lng;
			s.main = v->GetThrusterGroupLevel(THGROUP_MAIN);
			s.name = it->first;
			s.refplanet = buffer;
			s.retro = v->GetThrusterGroupLevel(THGROUP_RETRO);
			s.rposx = vs.rpos.x;
			s.rposy = vs.rpos.y;
			s.rposz = vs.rpos.z;
			s.rvelx = vs.rvel.x;
			s.rvely = vs.rvel.y;
			s.rvelz = vs.rvel.z;
			VECTOR3 force, acc;
			v->GetForceVector(force);
			acc = force / v->GetMass();
			s.accx = acc.x;
			s.accy = acc.y;
			s.accz = acc.z;
			vesselList[s.name] = s;
		}
		map<string, SimpleVesselState> newVesselList = serverVesselList;

		for (auto it = newVesselList.begin(); it != newVesselList.end(); ++it)
		{
			OBJHANDLE ves = oapiGetVesselByName((char*)it->first.c_str());
			VESSELSTATUS2 vs;
			memset(&vs, 0, sizeof(VESSELSTATUS2));
			vs.version = 2;
			VESSEL *v = NULL;
			SimpleVesselState state = it->second;
			if (!oapiIsVessel(ves)) {
				vs.status = (int)(state.landed);
				vs.rbody = oapiGetGbodyByName((char*)state.refplanet.c_str());
				vs.arot.x = state.arotx; vs.arot.y = state.aroty; vs.arot.z = state.arotz;
				vs.rvel.x = state.rvelx; vs.rvel.y = state.rvely; vs.rvel.z = state.rvelz;
				vs.rpos.x = state.rposx; vs.rpos.y = state.rposy; vs.rpos.z = state.rposz;
				vs.surf_lat = state.lat;
				vs.surf_lng = state.lon;
				vs.surf_hdg = state.heading;
				std::string classname = "Deltaglider";
				if (isVesselCompatible(state.className))
					classname = state.className;
				ves = oapiCreateVesselEx(state.name.c_str(), classname.c_str(), &vs);
				v = oapiGetVesselInterface(ves);
			}
			else {
				v = oapiGetVesselInterface(ves);
				v->GetStatusEx(&vs);
				char buffer[256];
				vs.rbody = v->GetSurfaceRef();
				if (vs.rbody != 0)
					oapiGetObjectName(vs.rbody, buffer, 256);
				else
					memset(buffer, 0, 256);
				state.arotx = vs.arot.x;
				state.aroty = vs.arot.y;
				state.arotz = vs.arot.z;
				state.className = v->GetClassNameA();
				state.heading = vs.surf_hdg;
				state.mjd = mjd;
				state.hover = v->GetThrusterGroupLevel(THGROUP_HOVER);
				state.landed = 0;
				state.lat = vs.surf_lat;
				state.lon = vs.surf_lng;
				state.main = v->GetThrusterGroupLevel(THGROUP_MAIN);
				state.refplanet = buffer;
				state.retro = v->GetThrusterGroupLevel(THGROUP_RETRO);
				state.rposx = vs.rpos.x;
				state.rposy = vs.rpos.y;
				state.rposz = vs.rpos.z;
				state.rvelx = vs.rvel.x;
				state.rvely = vs.rvel.y;
				state.rvelz = vs.rvel.z;
				VECTOR3 acc, force;
				v->GetForceVector(force);
				acc = force / v->GetMass();
				state.accx = acc.x;
				state.accy = acc.y;
				state.accz = acc.z;
			}
			vesselList[state.name] = state;
		}

		map<string, SimpleVesselState> localVessels = vesselList;
		stateLock.unlock();

		// delete vessels that no longer should be here
		for (auto it = localVessels.begin(); it != localVessels.end(); ++it)
		{
			if (newVesselList.count(it->first) == 0) {
				OBJHANDLE v = oapiGetVesselByName((char*)it->first.c_str());
				if (oapiIsVessel(v)) {
					oapiDeleteVessel(v);
				}
			}
		}
		last_update_time = syst;
	}
}

/*
DLLCLBK void opcPreStep(double simt, double simdt, double mjd) {
	if (!first) {
		first = true;
		req = curl_get("https://www.google.com");
	}

	if (oapiGetSysTime() > 5 && !save) {
		save = true;
		oapiSaveScenario("savestate", "");
		FILE* file = fopen("Scenarios\\savestate.scn", "r");
		if (file == 0) return;
		fseek(file, 0, SEEK_END);
		long fsize = ftell(file);
		fseek(file, 0, SEEK_SET);  //same as rewind(f);

		char *string = new char[fsize + 1];
		fread(string, fsize, 1, file);
		fclose(file);
		req = string;
		delete[] string;

		std::string className, vesselName;
		VESSEL* v = oapiGetFocusInterface();
		vesselName = v->GetName();
		className = v->GetClassNameA();
		std::string lookFor = vesselName + ":" + className;
		std::string until = "END\n";
		auto p1 = req.find(lookFor);
		auto p2 = req.find(until, p1);
		if (p2 == std::string::npos) {
			req = "p2 is null";
		}
		else {
			req = req.substr(p1, p2 - p1);
		}
	}

	sprintf(oapiDebugString(), "%s", req.substr(0, 100).c_str());
}
*/