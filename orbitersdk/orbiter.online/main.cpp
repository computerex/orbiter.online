#define STRICT
#define ORBITER_MODULE

#include <orbitersdk.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <algorithm>
#include <string>
#include <curl\curl.h>
#include "curl.h"

#include "rapidjson\document.h"
#include "rapidjson\writer.h"
#include "rapidjson\stringbuffer.h"
#include <iostream>
#include <vector>
#include <stdio.h>
#include <io.h>
#include "OrbiterOnlineMfd.h"
#include <unordered_map>

int mode;

using namespace std;
using namespace rapidjson;

bool first = true;
string content="";

class telemetry
{
public:
	bool landed;
	string planet;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz;
};

class base
{
public:
	int id;
	bool landed;
	std::string planet;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz;
	string basedesc, baseurl, name;
	string created;
	VECTOR3 gpos;
	bool contact;
};
vector<base> bases;
vector<string> configFileList;
vector<string> globalVesselList;

void trafficController();
string username, password;
int userid = -1;
DWORD lastTick = 0;
string teleRequest = "";
CRITICAL_SECTION cs;

void ScanConfigDir(const char *ppath);
bool isVesselCompatible(std::string className);
void login();
std::unordered_map<int, double> mjdList;
std::unordered_map<int, std::string> usernameList;

void proc()
{
	bool skipprune = false;
	string req;
	EnterCriticalSection(&cs);
	req = teleRequest;
	LeaveCriticalSection(&cs);
	string resp = curl_post("http://orbiter.online/posttele", req);

	Document d;
	d.Parse(resp.c_str());
	vector<int> idlist;
	if (d.IsArray()){
		for (int i = 0; i < d.Size(); i++)
		{
			int id = d[i]["id"].GetInt();
			//if (id == userid) continue;
			idlist.push_back(i);
		}
	}
	else if (d.IsObject() && d["error"] != NULL && d["error"].GetInt() == -1){ // logged out
		// login
		login();
	}
	char buffer[256];
	// update vessels in orbiter
	EnterCriticalSection(&cs);
	VECTOR3 gpos;
	std::vector<std::string> localVesselList;
	for (int i = 0; i < idlist.size(); i++){
		int inx = idlist[i];
		int id = d[inx]["id"].GetInt();
		// check if vessel with this name exists
		sprintf(buffer, "ORP_%d", id);
		OBJHANDLE h = oapiGetVesselByName(buffer);
		VESSELSTATUS2 vs;
		memset(&vs, 0, sizeof(VESSELSTATUS2));
		vs.version = 2;
		VESSEL *v = NULL;
		if (oapiIsVessel(h))
		{
			v = oapiGetVesselInterface(h);
			v->GetStatusEx(&vs);
		}
		vs.status = (int)(d[inx]["ld"].GetBool());
		vs.rbody = oapiGetGbodyByName((char*)d[inx]["pl"].GetString());
		vs.arot.x = d[inx]["arx"].GetDouble(); vs.arot.y = d[inx]["ary"].GetDouble(); vs.arot.z = d[inx]["arz"].GetDouble();
		vs.rvel.x = d[inx]["rvx"].GetDouble(); vs.rvel.y = d[inx]["rvy"].GetDouble(); vs.rvel.z = d[inx]["rvz"].GetDouble();
		vs.rpos.x = d[inx]["rpx"].GetDouble(); vs.rpos.y = d[inx]["rpy"].GetDouble(); vs.rpos.z = d[inx]["rpz"].GetDouble();
		vs.surf_lat = d[inx]["lat"].GetDouble();
		vs.surf_lng = d[inx]["lon"].GetDouble();
		vs.surf_hdg = d[inx]["h"].GetDouble();
		mjdList[id] = d[inx]["mjd"].GetDouble();
		//master->Local2Rel(vs.rpos, gpos);
		//vs.rpos = gpos;
		if (v == NULL){
			std::string classname = "Deltaglider";
			std::string reqclass = d[inx]["cn"].GetString();
			if (isVesselCompatible(reqclass))
				classname = reqclass;
			h = oapiCreateVesselEx(buffer, classname.c_str(), &vs);
			if (oapiIsVessel(h)){
				v = oapiGetVesselInterface(h);
				globalVesselList.push_back(buffer);
				skipprune = true;
				char cbuf[512];
				sprintf(cbuf, "http://orbiter.online/userInfo/%d", id);
				std::string url = cbuf;
				std::string response = curl_get(url);
				Document d;
				d.Parse(response.c_str());
				if (d.IsObject())
				{
					usernameList[id] = d["username"].GetString();
				}
			}
		}
		else{
			v->DefSetStateEx(&vs);
			localVesselList.push_back(buffer);
		}
		v->SetThrusterGroupLevel(THGROUP_MAIN, d[inx]["tm"].GetDouble());
		v->SetThrusterGroupLevel(THGROUP_HOVER, d[inx]["th"].GetDouble());
		v->SetThrusterGroupLevel(THGROUP_RETRO, d[inx]["tr"].GetDouble());
	}
	// remove vessels that spawned away
	if (!skipprune){
		for (vector<std::string>::iterator it = globalVesselList.begin();
			it != globalVesselList.end();)
		{
			if (std::find(localVesselList.begin(), localVesselList.end(), std::string(it->c_str())) == localVesselList.end()){
				OBJHANDLE h = oapiGetVesselByName((char*)it->c_str());
				if (oapiIsVessel(h))
					oapiDeleteVessel(h);
				it = globalVesselList.erase(it);
			}
			else
				++it;
		}
	}
	LeaveCriticalSection(&cs);
}

void postTele(string tele)
{
	EnterCriticalSection(&cs);
	teleRequest = tele;
	LeaveCriticalSection(&cs);
	DWORD tid;
	CreateThread(NULL, //Choose default security
		0, //Default stack size
		(LPTHREAD_START_ROUTINE)&proc,
		//Routine to execute
		0, //Thread parameter
		0, //Immediately run the thread
		&tid//Thread Id
		);
}

bool castaway = false;
OBJHANDLE lastFocus = NULL;

void login()
{
	FILE * f = fopen("orbiter.online.cfg", "r");
	if (f != 0){
		char buffer[256];
		// username
		fgets(buffer, 255, f);
		username = buffer;
		username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());
		// password
		fgets(buffer, 255, f);
		password = buffer;
		password.erase(std::remove(password.begin(), password.end(), '\n'), password.end());
		fclose(f);
		sprintf(buffer, "{\"username\":\"%s\", \"password\":\"%s\"}", username.c_str(), password.c_str());
		string response = curl_post("http://orbiter.online/login", buffer);
		if (response.find("error") != std::string::npos){
		}
		else {
			Document d;
			d.Parse(response.c_str());
			if (d.IsObject()){
				userid = d["user"].GetInt();
			}
		}
	}
}

DLLCLBK void opcPreStep(double simt, double simdt, double mjd)
{

	/*if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_MENU))
	{
		castaway = true;
		VESSEL * v = oapiGetFocusInterface();
		VECTOR3 pos;
		VESSELSTATUS2 vs;
		memset(&vs, 0, sizeof(VESSELSTATUS2));
		vs.version = 2;
		v->GetStatusEx(&vs);
		vs.status = 0;
		v->Local2Rel(_V(0, 10, 10), pos);
		vs.rpos = pos;
		oapiCreateVesselEx("foobar", "shuttlepb", &vs);
	}
	if (castaway)
		sprintf(oapiDebugString(), "casting away");
	return;*/
	if (first)
	{	
		char root[MAX_PATH], vessels[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, root);
		PathCombine(vessels, root, "Config");
		PathCombine(vessels, vessels, "Vessels");
		ScanConfigDir(vessels);
		InitializeCriticalSection(&cs);
		content = curl_get("http://orbiter.online/bases");
		Document d;
		d.Parse(content.c_str());
		if (d.IsArray()){
			for (int i = 0; i < d.Size(); i++)
			{
				base base;
				base.id = d[i]["id"].GetInt();
				base.landed = (bool)d[i]["landed"].GetInt();
				base.planet = d[i]["planet"].GetString();
				base.lon = d[i]["lon"].IsNull() ? 0 : d[i]["lon"].GetDouble();
				base.lat = d[i]["lat"].IsNull() ? 0 : d[i]["lat"].GetDouble();
				base.rposx = d[i]["rposx"].IsNull() ? 0 : d[i]["rposx"].GetDouble();
				base.rposy = d[i]["rposy"].IsNull() ? 0 : d[i]["rposy"].GetDouble();
				base.rposz = d[i]["rposz"].IsNull() ? 0 : d[i]["rposz"].GetDouble();
				base.rvelx = d[i]["rvelx"].IsNull() ? 0 : d[i]["rvelx"].GetDouble();
				base.rvely = d[i]["rvely"].IsNull() ? 0 : d[i]["rvely"].GetDouble();
				base.rvelz = d[i]["rvelz"].IsNull() ? 0 : d[i]["rvelz"].GetDouble();
				base.arotx = d[i]["arotx"].IsNull() ? 0 : d[i]["arotx"].GetDouble();
				base.aroty = d[i]["aroty"].IsNull() ? 0 : d[i]["aroty"].GetDouble();
				base.arotz = d[i]["arotz"].IsNull() ? 0 : d[i]["arotz"].GetDouble();
				base.created = d[i]["created"].GetString();
				base.basedesc = d[i]["basedesc"].GetString();
				base.name = d[i]["name"].GetString();
				base.baseurl = d[i]["baseurl"].IsNull() ? "" : d[i]["baseurl"].GetString();
				base.contact = false;
				bases.push_back(base);
			}
		}
		// spawn bases
		for (int i = 0; i < bases.size(); i++)
		{
			auto base = bases[i];
			VESSELSTATUS2 vs;
			memset(&vs, 0, sizeof(VESSELSTATUS2));
			auto v = oapiGetFocusInterface();
			v->GetStatusEx(&vs);
			vs.arot.x = base.arotx; vs.arot.y = base.aroty; vs.arot.z = base.arotz;
			vs.rpos.x = base.rposx; vs.rpos.y = base.rposy; vs.rpos.z = base.rposz;
			vs.surf_lat = base.lat * RAD;
			vs.surf_lng = base.lon * RAD;
			vs.surf_hdg = 0;
			if (base.landed)
				vs.status = 1;
			else
				vs.status = 0;
			OBJHANDLE planet = oapiGetGbodyByName((char*)base.planet.c_str());
			vs.rbody = planet;
			vs.flag = VS_FUELRESET | VS_THRUSTRESET;
			vs.version = 2;
			OBJHANDLE h = oapiGetVesselByName((char*)base.name.c_str());
			if (oapiIsVessel(h))
				oapiDeleteVessel(h);
			h = oapiCreateVesselEx(base.name.c_str(), "PreludeIII", &vs);
			if (oapiIsVessel(h)){
				v = oapiGetVesselInterface(h);
				v->SetEnableFocus(false);
				v->GetGlobalPos(bases[i].gpos);
			}

		}
		
		login();
		first = false;
	}
	if (userid == -1){
		sprintf(oapiDebugString(), "Orbiter Online: Unable to login, make sure username/password are correct then retry.", userid);
	}
	trafficController();

	if (userid != -1){
		DWORD tick = GetTickCount();
		if (tick - lastTick > 1000)
		{
			VESSEL * v = oapiGetFocusInterface();
			
			VESSELSTATUS2 vs;
			memset(&vs, 0, sizeof(VESSELSTATUS2));
			vs.version = 2;
			v->GetStatusEx(&vs);
			char buffer[256];
			double tm, tr, th;
			vs.rbody = v->GetSurfaceRef();
			if (vs.rbody != 0)
				oapiGetObjectName(vs.rbody, buffer, 256);
			else
				memset(buffer, 0, 256);
			//bool landed;
			//string planet;
			//double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz;
			VECTOR3 pos;
			pos = vs.rpos;
			Document d;
			d.SetObject();
			if (vs.status == 1)
			{
				pos.x = pos.y = pos.z = vs.rvel.x = vs.rvel.y = vs.rvel.z = vs.arot.x = vs.arot.y = vs.arot.z = tm = th = tr = 0;
			}else{
				tm = v->GetThrusterGroupLevel(THGROUP_MAIN);
				th = v->GetThrusterGroupLevel(THGROUP_HOVER);
				tr = v->GetThrusterGroupLevel(THGROUP_RETRO);
			}
			d.AddMember("ld", vs.status==1, d.GetAllocator());
			Value valr((char*)buffer, d.GetAllocator());
			d.AddMember("pl", valr, d.GetAllocator());
			d.AddMember("lon", vs.surf_lng, d.GetAllocator());
			d.AddMember("lat", vs.surf_lat, d.GetAllocator());
			d.AddMember("rpx", pos.x, d.GetAllocator());
			d.AddMember("rpy", pos.y, d.GetAllocator());
			d.AddMember("rpz", pos.z, d.GetAllocator());
			d.AddMember("rvx", vs.rvel.x, d.GetAllocator());
			d.AddMember("rvy", vs.rvel.y, d.GetAllocator());
			d.AddMember("rvz", vs.rvel.z, d.GetAllocator());
			d.AddMember("arx", vs.arot.x, d.GetAllocator());
			d.AddMember("ary", vs.arot.y, d.GetAllocator());
			d.AddMember("arz", vs.arot.z, d.GetAllocator());
			d.AddMember("h", vs.surf_hdg, d.GetAllocator());
			d.AddMember("tm", tm, d.GetAllocator());
			d.AddMember("th", th, d.GetAllocator());
			d.AddMember("tr", tr, d.GetAllocator());
			valr = Value((char*)v->GetClassNameA(), d.GetAllocator());
			d.AddMember("cn", valr, d.GetAllocator());
			d.AddMember("mjd", oapiGetSimMJD(), d.GetAllocator());
			GenericStringBuffer<UTF8<>> sbuf;
			Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);

			d.Accept(writer);
			std::string str = sbuf.GetString();
			postTele(str);

			// check if we entered the SOI of a base
			v = oapiGetFocusInterface();
			if (v != NULL) {
				OBJHANDLE h = v->GetHandle();
				if (oapiIsVessel(h)){
					if (h != lastFocus){
						lastFocus = h;
						for (int i = 0; i < bases.size(); i++)
						{
							bases[i].contact = false;
						}
					}
					else{
						VECTOR3 pos; v->GetGlobalPos(pos);
						for (int i = 0; i < bases.size(); i++)
						{
							OBJHANDLE base = oapiGetVesselByName((char*)bases[i].name.c_str());
							if (oapiIsVessel(base)){
								VESSEL * hv = oapiGetVesselInterface(base);
								v->GetRelativePos(base, pos);
								double distance = length(pos);
								if (distance < 100){
									if (!bases[i].contact){
										if (simt > 15){
											Document d;
											d.SetObject();
											d.AddMember("baseid", bases[i].id, d.GetAllocator());
											sbuf.Clear();
											Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);
											d.Accept(writer);
											str = sbuf.GetString();
											curl_post("http://orbiter.online/post_visit", str);
											bases[i].contact = true;
											break;
										}
										else{
											bases[i].contact = true;
										}
									}
								}
								else{
									bases[i].contact = false;
								}
							}
						}
					}
				}
			}
			lastTick = tick;
		}
	}
}

OBJHANDLE findClosestSurfaceBase(OBJHANDLE vessel)
{
	VESSEL * v = oapiGetVesselInterface(vessel);
	OBJHANDLE refBody = v->GetSurfaceRef();
	int numBases = oapiGetBaseCount(refBody);
	VECTOR3 a, b;
	oapiGetGlobalPos(vessel, &a);
	double minDistance=AU, distance;
	OBJHANDLE closestBase = NULL;
	char name[256];
	for (int index = 0; index < numBases; index++)
	{
		OBJHANDLE base = oapiGetBaseByIndex(refBody, index);
		oapiGetObjectName(base, name, 255);
		oapiGetGlobalPos(base, &b);
		distance = dist(a, b);
		if (distance < minDistance || closestBase == NULL){
			minDistance = distance;
			closestBase = base;
		}
	}
	int vesselCount = oapiGetVesselCount();
	char* className;
	for (int index = 0; index < vesselCount; index++)
	{
		OBJHANDLE h = oapiGetVesselByIndex(index);
		v = oapiGetVesselInterface(h);
		className = v->GetClassName();
		if (className != NULL){
			if (_strnicmp(className, "preludeIII", 10) == 0){
				v->GetGlobalPos(b);
				distance = dist(a, b);
				if (distance < minDistance || closestBase == NULL){
					minDistance = distance;
					closestBase = h;
				}
			}
		}
	}
	return closestBase;
}

OBJHANDLE resetTrafficBase(VESSEL * focus)
{
	OBJHANDLE closestBase = findClosestSurfaceBase(focus->GetHandle());
	// check if the traffic base exists
	OBJHANDLE hTrafficBase = oapiGetVesselByName("PTraffic");
	if (closestBase == NULL){
		//if (oapiIsVessel(h)){
		//	oapiDeleteVessel(h);
		//}
		return NULL;
	}
	// calculate position of base
	double lon, lat, rad;
	if (oapiIsVessel(closestBase)){
		VESSEL * vesselBase = oapiGetVesselInterface(closestBase);
		vesselBase->GetEquPos(lon, lat, rad);
	}
	else {
		oapiGetBaseEquPos(closestBase, &lon, &lat, &rad);
	}
	VESSELSTATUS2 vs;
	memset(&vs, 0, sizeof(VESSELSTATUS2));
	focus->GetStatusEx(&vs);
	vs.version = 2;
	vs.status = 1;
	vs.rbody = focus->GetSurfaceRef();
	vs.surf_hdg = 0;
	vs.surf_lat = lat;
	vs.surf_lng = lon;
	if (!oapiIsVessel(hTrafficBase))
	{
		oapiCreateVesselEx("PTraffic", "preludeIII", &vs);
	}
	else{
		// move the traffic base
		VESSEL * trafficBase = oapiGetVesselInterface(hTrafficBase);
		trafficBase->GetStatusEx(&vs);
		vs.surf_lat = lat;
		vs.surf_lng = lon;
		vs.version = 2;
		vs.rbody = focus->GetSurfaceRef();
		trafficBase->DefSetStateEx(&vs);
	}
	return closestBase;
}

OBJHANDLE lastCameraFocus = NULL;
OBJHANDLE lastClosestBase= NULL;
void trafficController()
{
	VESSEL * focus = oapiGetFocusInterface();
	OBJHANDLE focusH = focus->GetHandle();
	OBJHANDLE closestBase = findClosestSurfaceBase(focusH);
	if (lastCameraFocus != focusH || lastClosestBase != closestBase){
		lastCameraFocus = focusH;
		lastClosestBase = resetTrafficBase(focus);
	}
}

bool isVesselCompatible(std::string className)
{
	int classNameLength = className.length();
	for (int i = 0; i < configFileList.size(); i++)
	{
		if (_strnicmp(configFileList[i].c_str(), className.c_str(), min(configFileList[i].length(), classNameLength)) == 0){
			return true;
		}
	}
	return false;
}
void ScanConfigDir(const char *ppath)
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
				ScanConfigDir(cbuf);
			}
			else if ( fdata.name[0] != '.' && (pos=paths.find_last_of(".")) != std::string::npos ){
				configFileList.push_back(paths.substr(0, pos));
			}
		} while (!_findnext(fh, &fdata));
		_findclose(fh);
	}
}

DLLCLBK void InitModule(HINSTANCE hDLL)
{
	static char *name = "Orbiter Online MFD";
	MFDMODESPECEX spec;
	spec.name = name;
	spec.key = OAPI_KEY_O;
	spec.context = NULL;
	spec.msgproc = OrbiterOnlineMFD::MsgProc;
	mode = oapiRegisterMFDMode(spec);
}

DLLCLBK void ExitModule(HINSTANCE hDLL)
{
	oapiUnregisterMFDMode(mode);
	curl_clean();
}


OrbiterOnlineMFD::OrbiterOnlineMFD(DWORD w, DWORD h, VESSEL *vessel)
	: MFD(w, h, vessel)
{
}

OrbiterOnlineMFD::~OrbiterOnlineMFD()
{
}

// message parser
int OrbiterOnlineMFD::MsgProc(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case OAPI_MSG_MFD_OPENEDEX: {
		MFDMODEOPENSPEC *ospec = (MFDMODEOPENSPEC*)wparam;
		return (int)(new OrbiterOnlineMFD(ospec->w, ospec->h, (VESSEL*)lparam));
	}
	}
	return 0;
}
bool SyncMJD(void *id, char *str, void *data)
{
	double mjd = 0.0;
	int userid = atoi(str);
	std::unordered_map<int, double>::iterator it;
	if ((it=mjdList.find(userid)) == mjdList.end())
	{
		return false;
	}
	else {
		oapiSetSimMJD(it->second);
		return true;
	}
}

bool OrbiterOnlineMFD::ConsumeKeyBuffered(DWORD key)
{
	switch (key) {
		case OAPI_KEY_S:{
			oapiOpenInputBox("Enter target to sync MJD to:", SyncMJD, 0, 20, (void*)this);
			return true;
		}
	}
	return false;
}

bool OrbiterOnlineMFD::ConsumeButton(int bt, int event)
{
	if (!(event & PANEL_MOUSE_LBDOWN)) return false;
	static const DWORD btkey[1] = { OAPI_KEY_S };
	if (bt < 1) return ConsumeKeyBuffered(btkey[bt]);
	else return false;
}

char *OrbiterOnlineMFD::ButtonLabel(int bt)
{
	char *label[1] = { "SYC" };
	return (bt < 1 ? label[bt] : 0);
}

int OrbiterOnlineMFD::ButtonMenu(const MFDBUTTONMENU **menu) const
{
	static const MFDBUTTONMENU mnu[1] = {
		{ "Select a target to sync to", 0, 'S' }
	};
	if (menu) *menu = mnu;
	return 1;
}

void OrbiterOnlineMFD::Update(HDC hDC)
{
	Title(hDC, "Orbiter Online MFD");

	int y = 20;
	char buffer[256];
	int len;
	for (std::unordered_map<int, double>::iterator iter = mjdList.begin(); iter != mjdList.end(); ++iter)
	{
		int userid = iter->first;
		double mjd = iter->second;
		unordered_map<int, std::string>::iterator it;
		if ((it = usernameList.find(userid)) != usernameList.end())
			len = sprintf(buffer, "%s (%d) - %f", it->second.c_str(), userid, mjd);
		else
			len = sprintf(buffer, "%d - %f", userid, mjd);
		TextOut(hDC, 10, y, buffer, len);
		y += 20;
	}
}
