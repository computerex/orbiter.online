#define STRICT
#define ORBITER_MODULE
#define _CRT_SECURE_NO_WARNINGS

#define LONG_POLL 1
#define SHORT_POLL 1

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

DLLCLBK void InitModule(HINSTANCE hDLL)
{
	curl_init();
}

DLLCLBK void ExitModule(HINSTANCE hDLL)
{
	curl_clean();
}


class SimpleVesselState
{
public:
	bool landed;
	string refplanet, name, className, persisterId;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz, heading,
		retro, hover, main, mjd, accx, accy, accz;
};

map<string, SimpleVesselState> vesselList, serverVesselList;
vector<string> masterConfigFileList;
double last_update_time = -300, initTime = 0;
mutex stateLock;
thread* serverThread;
bool first = false;
string persisterId = "";

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
			s.mjd = d[i]["mjd"].GetDouble();
			s.className = d[i]["className"].GetString();
			s.retro = d[i]["retro"].GetDouble();
			s.hover = d[i]["hover"].GetDouble();
			s.main = d[i]["main"].GetDouble();
			s.accx = d[i]["accx"].GetDouble();
			s.accy = d[i]["accy"].GetDouble();
			s.accz = d[i]["accz"].GetDouble();
			s.persisterId = d[i]["persisterId"].GetString();
			newVesselList[name] = s;
		}
	}
	return newVesselList;
}
double slowUpdateLastsyst = 0;
class SimpleAverageState {
public:
	VECTOR3 rpos;
	int samples;
	SimpleAverageState::SimpleAverageState() {
		samples = 0;
		rpos = _V(0, 0, 0);
	}
};
map<string, vector<VECTOR3> > stateAverages;
void updateOrbiterVessels(map<string, SimpleVesselState> vessels)
{
	double syst = oapiGetSysTime();
	bool didSlowUpdate = false;
	for (auto it = vessels.begin(); it != vessels.end(); ++it) {
		if (it->second.persisterId == persisterId) continue;
		OBJHANDLE h = oapiGetVesselByName((char*)it->first.c_str());
		VESSELSTATUS2 vs, vsold;
		memset(&vs, 0, sizeof(VESSELSTATUS2));
		vs.version = 2;
		memset(&vsold, 0, sizeof(VESSELSTATUS2));
		vsold.version = 2;
		VESSEL *v = NULL;
		if (oapiIsVessel(h))
		{
			v = oapiGetVesselInterface(h);
			double pe;
			
			v->GetPeDist(pe);
			pe -= oapiGetSize(v->GetSurfaceRef());
			if (pe > 0) {
				if (slowUpdateLastsyst + LONG_POLL > syst) {
					continue;
				}
				else {
					didSlowUpdate = true;
				}
			}
			v->GetStatusEx(&vs);
		}
		else {
			//oapiGetFocusInterface()->GetStatusEx(&vs);
		}
		SimpleVesselState state = it->second;
		vs.status = state.landed ? 1 : 0;
		vs.rbody = oapiGetGbodyByName((char*)state.refplanet.c_str());
		vs.arot.x = state.arotx; vs.arot.y = state.aroty; vs.arot.z = state.arotz;
		vs.rvel.x = state.rvelx; vs.rvel.y = state.rvely; vs.rvel.z = state.rvelz;
		vs.rpos.x = state.rposx; vs.rpos.y = state.rposy; vs.rpos.z = state.rposz;

		double dt = (oapiGetSimMJD() - state.mjd) * 60 * 60 * 24;
		vs.surf_lat = state.lat;
		vs.surf_lng = state.lon;
		vs.surf_hdg = state.heading;
		//vs.rpos += vs.rvel * dt;
		if (v == NULL) {
			std::string classname = "Deltaglider";
			if (isVesselCompatible(state.className))
				classname = state.className;
			h = oapiCreateVesselEx(state.name.c_str(), classname.c_str(), &vs);
			v = oapiGetVesselInterface(h);
		}
		v->DefSetStateEx(&vs);
		v->GetStatusEx(&vs);

		VECTOR3 vel;
		v->GlobalRot(_V(state.accx*dt, state.accy*dt, state.accz*dt), vel);
		vs.rvel += vel;
		vs.rpos += vs.rvel * dt;
		/*
		if (stateAverages.count(state.name) == 0) {
			vector<VECTOR3> pos;
			pos.push_back(vs.rpos);
			stateAverages[state.name] = pos;
		}
		else {
			auto avgState = stateAverages[state.name];
			avgState.push_back(vs.rpos);
		}
		auto avgState = stateAverages[state.name];
		vector<VECTOR3> st(avgState.end() - 5, avgState.end());
		VECTOR3 rpos = _V(0, 0, 0);
		for (int i = 0; i < st.size(); i++)
			rpos += st[i];
		if (st.size() >= 50)
			vs.rpos = rpos/st.size();*/
		v->DefSetStateEx(&vs);
		/*
		if (avgState.size() > 200) {
			vector<VECTOR3> st(avgState.end() - 5, avgState.end());
			avgState = st;
		}*/
		v->SetThrusterGroupLevel(THGROUP_MAIN, state.main);
		v->SetThrusterGroupLevel(THGROUP_HOVER, state.hover);
		v->SetThrusterGroupLevel(THGROUP_RETRO, state.retro);
		v->ActivateNavmode(NAVMODE_KILLROT);
	}
	if (didSlowUpdate) {
		slowUpdateLastsyst = syst;
	}
}

void proc()
{
	while (true) {
		string resp = curl_get("http://orbiter.world/tele");
		map<string, SimpleVesselState> newVesselList = parseVesselStates(resp);

		stateLock.lock();
		serverVesselList = newVesselList;
		stateLock.unlock();

		Sleep(SHORT_POLL * 1);
	}
}

string getpersisterIdFromDisk() {
	FILE* file = fopen("persisterId", "r");
	if (file == 0) return "";
	char buffer[512];
	fgets(buffer, 512, file);
	fclose(file);
	return buffer;
}
bool persisterInit = false;

DLLCLBK void opcPreStep(double simt, double simdt, double mjd) {
	if (!persisterInit) {
		persisterInit = true;
		persisterId = getpersisterIdFromDisk();
		if (persisterId == "") {
			persisterId = curl_get("http://orbiter.world/persister/register");
			FILE* persistFile = fopen("persisterId", "w");
			fputs(persisterId.c_str(), persistFile);
			fclose(persistFile);
		}
	}
	double syst = oapiGetSysTime();
	if (!first && oapiGetTimeAcceleration() == 1 && syst > 5) {
		first = true;
		serverThread = new thread(proc);
		initTime = syst;
	}
	
	if ((syst > last_update_time + SHORT_POLL) && first && (syst-initTime) > 2) {
		map<string, SimpleVesselState> newVesselList;
		stateLock.lock();
		newVesselList = serverVesselList;
		stateLock.unlock();
		if (newVesselList.size() > 0) {
			for (auto it = vesselList.begin(); it != vesselList.end(); ++it)
			{
				if (newVesselList.count(it->first) == 0) {
					OBJHANDLE v = oapiGetVesselByName((char*)it->first.c_str());
					if (oapiIsVessel(v)) {
						oapiDeleteVessel(v);
					}
				}
			}
		}
		vesselList = newVesselList;
		updateOrbiterVessels(vesselList);
		last_update_time = syst;
	}
}