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
	//curl_clean();
}


class SimpleVesselState
{
public:
	bool landed;
	string refplanet, name, className, persisterId;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz, heading,
		retro, hover, main, mjd, accx, accy, accz, angx, angy, angz, propMass;
};

class ReconState {
public:
	string reference, target, persisterId;
	double targetPosX, targetPosY, targetPosZ, mjd;
	double targetVelX, targetVelY, targetVelZ;
	double orientX, orientY, orientZ;
};

class DockEvent {
public:
	string reference, target, persisterId;
	UINT refPort, targetPort;
};

map<string, SimpleVesselState> vesselList, serverVesselList;
vector<ReconState> reconOutStates, reconInStates;
vector<DockEvent> dockEventsIn, dockEventsOut;
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
			s.angx = d[i]["angx"].GetDouble();
			s.angy = d[i]["angy"].GetDouble();
			s.angz = d[i]["angz"].GetDouble();
			s.propMass = d[i]["propMass"].GetDouble();
			s.persisterId = d[i]["persisterId"].GetString();
			newVesselList[name] = s;
		}
	}
	return newVesselList;
}

vector<ReconState> parseReconStates(string teleJson) {
	Document d;
	d.Parse(teleJson.c_str());
	vector<ReconState> states;
	if (d.IsArray()) {

		ReconState rs;
		for (unsigned int i = 0; i < d.Size(); i++)
		{
			rs.reference = d[i]["reference"].GetString();
			rs.target = d[i]["target"].GetString();
			rs.targetPosX = d[i]["targetPosX"].GetDouble();
			rs.targetPosY = d[i]["targetPosY"].GetDouble();
			rs.targetPosZ = d[i]["targetPosZ"].GetDouble();
			rs.targetVelX = d[i]["targetVelX"].GetDouble();
			rs.targetVelY = d[i]["targetVelY"].GetDouble();
			rs.targetVelZ = d[i]["targetVelZ"].GetDouble();
			rs.orientX = d[i]["orientX"].GetDouble();
			rs.orientY = d[i]["orientY"].GetDouble();
			rs.orientZ = d[i]["orientZ"].GetDouble();
			rs.persisterId = d[i]["persisterId"].GetString();
			rs.mjd = d[i]["mjd"].GetDouble();
			states.push_back(rs);
		}
	}
	return states;
}

vector<DockEvent> parseDockEvents(string teleJson) {
	Document d;
	d.Parse(teleJson.c_str());
	vector<DockEvent> states;
	if (d.IsArray()) {

		DockEvent rs;
		for (unsigned int i = 0; i < d.Size(); i++)
		{
			rs.reference = d[i]["reference"].GetString();
			rs.target = d[i]["target"].GetString();
			rs.refPort = d[i]["refPort"].GetUint();
			rs.targetPort= d[i]["targetPort"].GetUint();
			rs.persisterId = d[i]["persisterId"].GetString();
			states.push_back(rs);
		}
	}
	return states;
}

double slowUpdateLastsyst = 0;

bool areDocked(OBJHANDLE a, OBJHANDLE b) {
	if (!oapiIsVessel(a) || !oapiIsVessel(b)) return false;
	VESSEL* va = oapiGetVesselInterface(a);
	VESSEL* vb = oapiGetVesselInterface(b);

	for (unsigned int i = 0; i < va->DockCount(); i++) {
		DOCKHANDLE d = va->GetDockHandle(i);
		if (va->GetDockStatus(d) == b) return true;
	}
	return false;
}

map<string, double> lastReconUpdateForVessel;
map<string, double> lastThrusterFireTime;
double lastThrusterFireTimeCheckTime = 0;
OBJHANDLE lastStepDockedVessel[100]; // up to 100 docking ports are synchronized

bool isVesselActive(VESSEL* v) {
	string name = v->GetName();
	if (lastThrusterFireTime.count(name) == 0) return false;
	double syst = oapiGetSysTime();
	if ((syst - lastThrusterFireTime[name]) >= 30) return false;
	return true;
}

bool shouldSendReconState(VESSEL* focus, VESSEL* target) {
	double pe;
	focus->GetPeDist(pe);
	double refSize = oapiGetSize(focus->GetSurfaceRef());
	pe -= refSize;
	bool focusIsActive = isVesselActive(focus);
	//bool targetIsActive = isVesselActive(target);
	return (focusIsActive && pe > 0);
}

void updateOrbiterVessels(map<string, SimpleVesselState> vessels, 
	vector<ReconState> reconIn, 
	vector<ReconState>& reconOut,
	vector<DockEvent>& dockEventsIn, 
	vector<DockEvent>& dockEventsOut
	)
{
	double syst = oapiGetSysTime();
	double simdt = oapiGetSimStep();
	double mjd = oapiGetSimMJD();
	bool didSlowUpdate = false;
	VESSEL* focus = oapiGetFocusInterface();
	OBJHANDLE focushandle = focus->GetHandle();
	VECTOR3 gpos, lpos, focusgpos;
	double pe;
	focus->GetPeDist(pe);
	pe -= oapiGetSize(focus->GetSurfaceRef());
	focus->GetGlobalPos(focusgpos);
	for (auto it = vessels.begin(); it != vessels.end(); ++it) {
		if (it->second.persisterId == persisterId) continue;
		OBJHANDLE h = oapiGetVesselByName((char*)it->first.c_str());
		// skip global coord updates for vessels which have had a recon update in past 3 seconds
		if (lastReconUpdateForVessel.count(it->first) > 0) {
			if (lastReconUpdateForVessel[it->first] < 3) continue;
		}
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
		}
		SimpleVesselState state = it->second;

		if (state.landed) {
			memset(&vs, 0, sizeof(vs));
			vs.version = 2;
			vs.arot.x = 10;
			vs.rbody = oapiGetGbodyByName((char*)state.refplanet.c_str());
			vs.status = 1;
		}
		else {
			vs.rbody = oapiGetGbodyByName((char*)state.refplanet.c_str());
			vs.arot.x = state.arotx; vs.arot.y = state.aroty; vs.arot.z = state.arotz;
			vs.rvel.x = state.rvelx; vs.rvel.y = state.rvely; vs.rvel.z = state.rvelz;
			vs.rpos.x = state.rposx; vs.rpos.y = state.rposy; vs.rpos.z = state.rposz;
			vs.status = 0;
		}
		double dt = ( mjd - state.mjd) * 60 * 60 * 24;
		vs.surf_lat = state.lat;
		vs.surf_lng = state.lon;
		vs.surf_hdg = state.heading;
		
		if (v == NULL) {
			std::string classname = "Deltaglider";
			if (isVesselCompatible(state.className))
				classname = state.className;
			h = oapiCreateVesselEx(state.name.c_str(), classname.c_str(), &vs);
			v = oapiGetVesselInterface(h);
			//((VESSEL2*)v)->clbkConsumeBufferedKey(OAPI_KEY_G, true, NULL);
			continue;
		}
		if (it->second.className == "Blast") continue;
		if (dt > 3) continue;
		VECTOR3 oldGpos, oldLpos, displacement, orientation;
		v->GetGlobalPos(oldGpos);
		focus->Global2Local(oldGpos, oldLpos);
		v->DefSetStateEx(&vs);
		v->SetThrusterGroupLevel(THGROUP_MAIN, state.main);
		v->SetThrusterGroupLevel(THGROUP_HOVER, state.hover);
		v->SetThrusterGroupLevel(THGROUP_RETRO, state.retro);
		v->ActivateNavmode(NAVMODE_KILLROT);
		if (!state.landed) {
			v->GetStatusEx(&vs);
			VECTOR3 vel, rel, rvel, lvel;
			MATRIX3 R;
			v->GlobalRot(_V(state.accx*dt, state.accy*dt, state.accz*dt), vel);
			vs.rvel += vel;
			vs.rpos += vs.rvel * dt;
			v->DefSetStateEx(&vs);
			v->SetAngularVel(_V(state.angx, state.angy, state.angz));
			v->SetFuelMass(state.propMass);
			if (length(oldLpos) < 10000 && shouldSendReconState(focus, v)) {
				v->GetGlobalPos(gpos);
				focus->Global2Local(gpos, lpos);
				displacement = lpos - oldLpos;
				v->GetRelativeVel(focus->GetHandle(), rvel);
				v->GetRotationMatrix(R);
				lvel = tmul(R, rvel);
				if ((length(displacement) > length(rvel)*simdt)) {
					VECTOR3 avg = (lpos + oldLpos) / 2.0;
					avg = oldLpos;
					focus->Local2Rel(avg, rel);
					vs.rpos = rel;
					v->DefSetStateEx(&vs);
					// create recon event
					//avg *= -1;
					//avg.z *= -1;
					v->Global2Local(focusgpos, avg);
					ReconState rs;
					rs.mjd = mjd;
					rs.reference = v->GetName();
					rs.target = focus->GetName();
					rs.targetPosX = avg.x;
					rs.targetPosY = avg.y;
					rs.targetPosZ = avg.z;
					rs.targetVelX = lvel.x;
					rs.targetVelY = lvel.y;
					rs.targetVelZ = lvel.z;
					focus->GetGlobalOrientation(orientation);
					rs.orientX = orientation.x;
					rs.orientY = orientation.y;
					rs.orientZ = orientation.z;
					rs.persisterId = persisterId;
					reconOut.push_back(rs);
				}
			}
			else {
				/*v->Global2Local(oldGpos, oldLpos);
				VECTOR3 acc, force;
				v->GetForceVector(force);
				acc = force / v->GetMass();
				if (length(oldLpos)) {
					v->GetStatusEx(&vs);
					v->Local2Rel(oldLpos, vs.rpos);
					v->DefSetStateEx(&vs);
				}*/
			}
		}
	}
	OBJHANDLE ref, target;
	double dt;
	VECTOR3 pos, vel;
	VESSEL* v;
	VESSELSTATUS vs;
	for (unsigned int i = 0; i < reconIn.size(); i++) {
		auto s = reconIn[i];
		if (s.persisterId == persisterId) continue;
		
		dt = (mjd - s.mjd) * 60 * 60 * 24;
		ref = oapiGetVesselByName((char*)s.reference.c_str());
		if (!oapiIsVessel(ref)) continue;
		if (focushandle != ref) continue;
		target = oapiGetVesselByName((char*)s.target.c_str());
		if (!oapiIsVessel(target)) continue;
		v = oapiGetVesselInterface(ref);
		v->Local2Rel(_V(s.targetPosX, s.targetPosY, s.targetPosZ), pos);
		v->GlobalRot(_V(s.targetVelX, s.targetVelY, s.targetVelZ), vel);
		v = oapiGetVesselInterface(target);
		memset(&vs, 0, sizeof(VESSELSTATUS));
		v->GetStatus(vs);
		vs.rpos = pos;
		v->DefSetState(&vs);
		// TODO- decide whether we should do this
		v->GetStatus(vs);
		vs.rpos += vel * dt;
		v->DefSetState(&vs);
		lastReconUpdateForVessel[s.target] = syst;
		v->SetGlobalOrientation(_V(s.orientX, s.orientY, s.orientZ));
	}
	for (unsigned int i = 0; i < dockEventsIn.size(); i++) {
		auto s = dockEventsIn[i];
		if (s.persisterId == persisterId) continue;
		OBJHANDLE ref = oapiGetVesselByName((char*)s.reference.c_str());
		if (!oapiIsVessel(ref)) continue;
		OBJHANDLE target = oapiGetVesselByName((char*)s.target.c_str());
		if (!oapiIsVessel(target)) continue;
		VESSEL* vr = oapiGetVesselInterface(ref);
		vr->Dock(target, s.refPort, s.targetPort, 1); // teleport target for docking
	}
	for (unsigned int i = 0; i < focus->DockCount(); i++) {
		OBJHANDLE dockedVessel = focus->GetDockStatus(focus->GetDockHandle(i));
		if (oapiIsVessel(dockedVessel) && !oapiIsVessel(lastStepDockedVessel[i])) {
			DockEvent event;
			VESSEL* refv = oapiGetVesselInterface(dockedVessel);
			event.persisterId = persisterId;
			event.reference = refv->GetName();
			for (unsigned int j = 0; j < refv->DockCount(); j++) {
				if (refv->GetDockStatus(refv->GetDockHandle(j)) == focushandle) {
					event.refPort = j;
					break;
				}
			}
			event.target = focus->GetName();
			event.targetPort = i;
			dockEventsOut.push_back(event);
		}
		lastStepDockedVessel[i] = dockedVessel;
	}
	if (didSlowUpdate) {
		slowUpdateLastsyst = syst;
	}
}
DLLCLBK void opcFocusChanged(OBJHANDLE hGainsFocus, OBJHANDLE hLosesFocus) {
	for (int i = 0; i < 100; i++) {
		lastStepDockedVessel[i] = NULL;
	}
}
string serializeReconStates(vector<ReconState> states)
{
	Document d;
	d.SetArray();
	Document::AllocatorType& a = d.GetAllocator();
	for (unsigned int i = 0; i < states.size(); i++)
	{
		Value v("state");
		v.SetObject();
		ReconState s = states[i];
		v.AddMember("mjd", s.mjd, a);
		Value valr(s.reference.c_str(), a);
		v.AddMember("reference", valr, a);
		valr = Value(s.target.c_str(), a);
		v.AddMember("target", valr, a);
		v.AddMember("targetPosX", s.targetPosX, a);
		v.AddMember("targetPosY", s.targetPosY, a);
		v.AddMember("targetPosZ", s.targetPosZ, a);
		v.AddMember("targetVelX", s.targetVelX, a);
		v.AddMember("targetVelY", s.targetVelY, a);
		v.AddMember("targetVelZ", s.targetVelZ, a);
		v.AddMember("orientX", s.orientX, a);
		v.AddMember("orientY", s.orientY, a);
		v.AddMember("orientZ", s.orientZ, a);
		valr = Value(persisterId.c_str(), a);
		v.AddMember("persisterId", valr, a);
		d.PushBack(v, a);
	}
	GenericStringBuffer<UTF8<>> sbuf;
	Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);
	d.Accept(writer);
	std::string str = sbuf.GetString();
	return str;
}

string serializeDockEvents(vector<DockEvent> states)
{
	Document d;
	d.SetArray();
	Document::AllocatorType& a = d.GetAllocator();
	for (unsigned int i = 0; i < states.size(); i++)
	{
		Value v("dockEvents");
		v.SetObject();
		DockEvent s = states[i];
		v.AddMember("refPort", s.refPort, a);
		v.AddMember("targetPort", s.targetPort, a);
		Value valr(s.reference.c_str(), a);
		v.AddMember("reference", valr, a);
		valr = Value(s.target.c_str(), a);
		v.AddMember("target", valr, a);
		valr = Value(persisterId.c_str(), a);
		v.AddMember("persisterId", valr, a);
		d.PushBack(v, a);
	}
	GenericStringBuffer<UTF8<>> sbuf;
	Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);
	d.Accept(writer);
	std::string str = sbuf.GetString();
	return str;
}

void teleproc()
{
	string resp = curl_get("http://localhost:5000/tele");
	map<string, SimpleVesselState> newVesselList = parseVesselStates(resp);
	stateLock.lock();
	serverVesselList = newVesselList;
	stateLock.unlock();
}

void reconproc()
{
	stateLock.lock();
	vector<ReconState> reconStatesOut = reconOutStates;
	reconOutStates.clear();
	stateLock.unlock();
	string resp = curl_post("http://localhost:5000/recon", serializeReconStates(reconStatesOut));
	vector<ReconState> reconIn = parseReconStates(resp);
	stateLock.lock();
	reconInStates = reconIn;
	stateLock.unlock();
}

void dockproc()
{
	stateLock.lock();
	vector<DockEvent> outDockEvents = dockEventsOut;
	dockEventsOut.clear();
	stateLock.unlock();
	string resp = curl_post("http://localhost:5000/dock", serializeDockEvents(outDockEvents));
	vector<DockEvent> dockIn = parseDockEvents(resp);
	stateLock.lock();
	dockEventsIn = dockIn;
	stateLock.unlock();
}
void proc()
{
	Sleep(1000 * 5);
	while (true) {
		thread tele(teleproc);
		thread recon(reconproc);
		thread dock(dockproc);
		tele.join();
		recon.join();
		dock.join();
		Sleep(SHORT_POLL * 1000.);
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
			persisterId = curl_get("http://localhost:5000/persister/register");
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
		vector<ReconState> newReconEvents, reconOut;
		vector<DockEvent> newDockEvents, dockOut;
		stateLock.lock();
		newVesselList = serverVesselList;
		newReconEvents = reconInStates;
		newDockEvents = dockEventsIn;
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
		updateOrbiterVessels(vesselList, newReconEvents, reconOut, newDockEvents, dockOut);
		stateLock.lock();
		reconOutStates = reconOut;
		dockEventsOut = dockOut;
		stateLock.unlock();
		last_update_time = syst;
	}
	VESSEL* focus = oapiGetFocusInterface();
	string name = focus->GetName();
	if (lastThrusterFireTime.count(name) == 0)
		lastThrusterFireTime[name] = 0;
	if (lastThrusterFireTimeCheckTime + 0.1 < syst) {
		for (unsigned int i = 0; i < focus->GetThrusterCount(); i++) {
			if (focus->GetThrusterLevel(focus->GetThrusterHandleByIndex(i)) > 0) {
				lastThrusterFireTime[name] = syst;
				break;
			}
		}
		lastThrusterFireTimeCheckTime = syst;
	}
}