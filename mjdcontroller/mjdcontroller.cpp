#define STRICT
#define ORBITER_MODULE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <orbitersdk.h>
#include "mjdcontroller.h"
#include "curl.h"
#include "miniPID.h"
#include <thread>
#include <string>
#include <map>
#include <rapidjson\document.h>
#include <rapidjson\writer.h>
#include <rapidjson\stringbuffer.h>
using namespace std;
using namespace rapidjson;

int mode;

double last_update_simt = oapiGetSimTime();
double real_mjd = 0;
MiniPID mjdcontroller(1, 0, 500);
DWORD l_tick_count = GetTickCount();
CRITICAL_SECTION mjdlock;
bool first = false;
std::thread *t1;
string persisterId = "";

void mjd_sync() {
	while (true) {
		DWORD tickCount = GetTickCount();
		std::string req = curl_get("http://orbiter.world/mjd");
		if (req == "") {
			continue;
		}
		double mjd = atof(req.c_str());
		EnterCriticalSection(&mjdlock);
		real_mjd = mjd;
		LeaveCriticalSection(&mjdlock);
		Sleep(3000);
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

DLLCLBK void InitModule (HINSTANCE hDLL)
{
	curl_init();
	static char *name = "MJD controller";
	MFDMODESPECEX spec;
	spec.name    = name;
	spec.key     = OAPI_KEY_C;
	spec.context = NULL;
	spec.msgproc = MJDControllerMFD::MsgProc;
	mode = oapiRegisterMFDMode (spec);
}

class SimpleVesselState
{
public:
	bool landed;
	string refplanet, name, className, persisterId;
	double lon, lat, rposx, rposy, rposz, rvelx, rvely, rvelz, arotx, aroty, arotz, heading,
		retro, hover, main, mjd, accx, accy, accz, angx, angy, angz, propMass;
};

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
	//curl_clean();
	oapiUnregisterMFDMode (mode);
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
		valr = Value(s.className.c_str(), a);
		v.AddMember("className", valr, a);
		valr = Value(s.name.c_str(), a);
		v.AddMember("name", valr, a);
		valr = Value(persisterId.c_str(), a);
		v.AddMember("persisterId", valr, a);
		v.AddMember("mjd", s.mjd, a);
		v.AddMember("accx", s.accx, a);
		v.AddMember("accy", s.accy, a);
		v.AddMember("accz", s.accz, a);
		v.AddMember("angx", s.angx, a);
		v.AddMember("angy", s.angy, a);
		v.AddMember("angz", s.angz, a);
		v.AddMember("propMass", s.propMass, a);
		d.PushBack(v, a);
	}
	GenericStringBuffer<UTF8<>> sbuf;
	Writer<GenericStringBuffer<UTF8<>>> writer(sbuf);
	d.Accept(writer);
	std::string str = sbuf.GetString();
	return str;
}

bool Persist(void *id, char *str, void *data)
{
	OBJHANDLE ves = oapiGetVesselByName((char*)str);
	if (!oapiIsVessel(ves))
		return false;
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
	SimpleVesselState s;
	s.arotx = vs.arot.x;
	s.aroty = vs.arot.y;
	s.arotz = vs.arot.z;
	s.className = v->GetClassNameA();
	s.persisterId = persisterId;
	s.heading = vs.surf_hdg;
	s.mjd = oapiGetSimMJD();
	s.hover = v->GetThrusterGroupLevel(THGROUP_HOVER);
	s.landed = 0;
	s.lat = vs.surf_lat;
	s.lon = vs.surf_lng;
	s.main = v->GetThrusterGroupLevel(THGROUP_MAIN);
	s.name = v->GetName();
	s.refplanet = buffer;
	s.retro = v->GetThrusterGroupLevel(THGROUP_RETRO);
	s.rposx = vs.rpos.x;
	s.rposy = vs.rpos.y;
	s.rposz = vs.rpos.z;
	s.rvelx = vs.rvel.x;
	s.rvely = vs.rvel.y;
	s.rvelz = vs.rvel.z;
	VECTOR3 force, avel;
	v->GetForceVector(force);
	v->GetAngularVel(avel);
	VECTOR3 acc = force / v->GetMass();
	s.accx = acc.x;
	s.accy = acc.y;
	s.accz = acc.z;
	s.angx = avel.x;
	s.angy = avel.y;
	s.angz = avel.z;
	s.propMass = v->GetFuelMass();

	map<string, SimpleVesselState> states;
	string name = v->GetName();
	states[name] = s;
	string json = getTele(states);
	curl_post("http://orbiter.world/persist?pid=" + persisterId, json);
	return true;
}
double oldRMjd = 0;
double last_updt_syst = 0;
DLLCLBK void opcPreStep (double simt, double simdt, double mjd)
{
	if (!first) {
		first = true;
		persisterId = getpersisterIdFromDisk();
		if (persisterId == "") {
			persisterId = curl_get("http://orbiter.world/persister/register");
			FILE* persistFile = fopen("persisterId", "w");
			fputs(persisterId.c_str(), persistFile);
			fclose(persistFile);
		}
		InitializeCriticalSection(&mjdlock);
		t1 = new std::thread(mjd_sync);
		return;
	}
	
	EnterCriticalSection(&mjdlock);
	double rmjd = real_mjd;
	LeaveCriticalSection(&mjdlock);

	if (rmjd == 0) { oapiSetTimeAcceleration(1.0); return; }

	double syst = oapiGetSysTime();
	if (rmjd != oldRMjd) {
		last_updt_syst = syst;
	}

	double delta = rmjd - mjd;
	delta *= 60 * 60 * 24;
	delta += syst - last_updt_syst;
	if (fabs(delta) < 1)
	{
		oapiSetTimeAcceleration(1.0);
	}else 
		oapiSetTimeAcceleration(delta + 1);
	oldRMjd = rmjd;
	//oapiSetTimeAcceleration(acc);
	//sprintf(oapiDebugString(), "delta: %f acc: %f", delta, acc);
}

MJDControllerMFD::MJDControllerMFD (DWORD w, DWORD h, VESSEL *vessel)
: MFD (w, h, vessel)
{
	width=w/35;
	height=h/24;
	last_update_simt = oapiGetSysTime();
}

MJDControllerMFD::~MJDControllerMFD ()
{
}


// message parser
int MJDControllerMFD::MsgProc (UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case OAPI_MSG_MFD_OPENEDEX: {
		MFDMODEOPENSPEC *ospec = (MFDMODEOPENSPEC*)wparam;
		return (int)(new MJDControllerMFD (ospec->w, ospec->h, (VESSEL*)lparam));
		}
	}
	return 0;
}

bool MJDControllerMFD::ConsumeKeyBuffered(DWORD key)
{
	switch (key) {
		case OAPI_KEY_P: {
			oapiOpenInputBox("What vessel do you want to persist?: ", Persist, 0, 20, (void*)this);
			return true;
		}
	}
	return false;
}

bool MJDControllerMFD::ConsumeButton(int bt, int event)
{
	if (!(event & PANEL_MOUSE_LBDOWN)) return false;
	static const DWORD btkey[1] = { OAPI_KEY_P };
	if (bt < 1) return ConsumeKeyBuffered(btkey[bt]);
	else return false;
}

char * MJDControllerMFD::ButtonLabel(int bt)
{

	char *label[1] = { "PER" };
	return (bt < 1 ? label[bt] : 0);
}

int MJDControllerMFD::ButtonMenu(const MFDBUTTONMENU ** menu) const
{
	static const MFDBUTTONMENU mnu[1] = {
		{ "Persist vessel", 0, 'P' }
	};
	if (menu) *menu = mnu;
	return 1;
}

void MJDControllerMFD::Update (HDC hDC)
{
	Title (hDC, "MJD Controller");

	int len;
	char buffer[256];
	int y = height*2;

	len = sprintf(buffer, "MJD: %f", oapiGetSimMJD());
	TextOut(hDC, width, y, buffer, len);
	y+=height*1;

	double rmjd = 0;
	EnterCriticalSection(&mjdlock);
	rmjd = real_mjd;
	LeaveCriticalSection(&mjdlock);
	len = sprintf(buffer, "rel: %f", real_mjd);
	TextOut(hDC, width, y, buffer, len);
	y += height * 1;

}
