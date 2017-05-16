#define STRICT
#define ORBITER_MODULE

#include <orbitersdk.h>
#include <curl.h>
#include "mission.h"
#include "miniPID.h"
#include "rapidjson\rapidjson.h"

double z = 0;
double tlon = -80.675 * RAD;
double tlat = 28.528 * RAD;
int zcnt = 0;
bool first = false;
std::string html;
DLLCLBK void opcPreStep(double simt, double simdt, double mjd) {
	if (!first) {
		first = true;
		html = curl_get("https://www.google.com");
	}
}

/*

class TargetRange {
public:
	TargetRange(const char* target, double range, double vel) {
		strcpy(this->target, target);
		this->range = range;
		this->vel = vel;
	}
	char target[256];
	double range, vel;
};

void destroyglnt(double simt, double simdt, OBJHANDLE self);

DLLCLBK void opcPreStep(double simt, double simdt, double mjd) {
	OBJHANDLE destroyer = oapiGetVesselByName("destroyer");
	if (!first) {
		first = true;
		if (oapiIsVessel(destroyer)) {
			oapiDeleteVessel(destroyer);
		}
		
		OBJHANDLE ref = oapiGetVesselByName("GL-01");
		if (!oapiIsVessel(ref)) return;
		VESSELSTATUS vs;
		memset(&vs, 0, sizeof(VESSELSTATUS));
		VESSEL * refv = oapiGetVesselInterface(ref);
		refv->GetStatus(vs);
		refv->Local2Rel(_V(20, 0, 0), vs.rpos);
		VECTOR3 vel;
		refv->GlobalRot(_V(0, 0, 0), vel);
		vs.rvel += vel;
		oapiCreateVessel("destroyer", "Deltaglider", vs);
	}
	destroyglnt(simt, simdt, oapiGetVesselByName("GL-01"));
	VESSEL * v = oapiGetFocusInterface();
	
	z += simdt * 8000;
	z = 0;
	double lon, lat, rad, refSize;
	OBJHANDLE wheel = oapiGetVesselByName("wheel");
	if (!oapiIsVessel(wheel)) {
		VESSELSTATUS vs;
		oapiGetFocusInterface()->GetStatus(vs);
		wheel = oapiCreateVessel("wheel", "bigwheel", vs);
	}
	v = oapiGetVesselInterface(wheel);
	OBJHANDLE surfRef = v->GetSurfaceRef();
	refSize = oapiGetSize(surfRef);

	oapiGetEquPos(v->GetHandle(), &lon, &lat, &rad);


	VESSELSTATUS2 vs;
	memset(&vs, 0, sizeof(VESSELSTATUS2));
	vs.version = 2;
	v->GetStatusEx(&vs);


	vs.surf_lat = tlat;
	vs.surf_lng = tlon;
	vs.status = 1;
	v->DefSetStateEx(&vs);

	v->GetStatusEx(&vs);
	v->Local2Rel(_V(0, 50000, z), vs.rpos);
	vs.status = 0;
	v->DefSetStateEx(&vs);
	VECTOR3 arot;
	arot.data[0] = 0;
	arot.data[1] = RAD * 90;
	arot.data[2] = 0;
	v->SetGlobalOrientation(arot);
	if (z > 2000) {

		v->GetStatusEx(&vs);
		tlat = vs.surf_lat;
		tlon = vs.surf_lng;

		z = 0;
		zcnt++;
	}
	//Collision detection with target supporting "shoot-through"
	//v->GetRelativeVel(target, n);
	//n = n*simdt;											//Propagate along relative velocity trajectory
	//v->GetGlobalPos(l);
	//g -= l;
	//double r = length(n);									//Detection range
	//double t = max(min(dotp(g, n) / r / r, 1), 0);				//"Periapsis" point along the trajectory
	//double s = oapiGetSize(target);
	//r = length(-g + n*t);									//Line-point distance
	/*
	MATRIX3 m;
	v->GetRotationMatrix(m);
	l = _V(m.m13, m.m23, m.m33);							//Current direction vector
	normalise(g);										//Target direction vector, also	Matrix Z vector	

	VECTOR3 b = l - g;
	double rr = acos(dotp(l, g));
	normalise(n = crossp(_V(m.m12, m.m22, m.m32), g));	//Matrix X vector
	normalise(l = -crossp(n, g));							//Matrix Y vector
	double rrx = acos(dotp(n, g));
	double rry = acos(dotp(l, g));*/

	//Head towards target
	
	
	//SetRotationMatrix(_M(n.x, l.x, g.x, n.y, l.y, g.y, n.z, l.z, g.z));
			//
	/*
}

double last_fire_time = 0;
MiniPID pitchAng(1, 0, 1000), pitchRCS(1, 0.0, 1000);
MiniPID yawAng(1, 0, 1000), yawRCS(1, 0.0, 1000);
MiniPID updownVel(1, 0, 100), updownRCS(1, 0, 100);
MiniPID leftrightVel(1, 0, 100), leftrightRCS(1, 0, 100);
double cnt = 0;
void destroyglnt(double simt, double simdt, OBJHANDLE self) {

	OBJHANDLE target = oapiGetVesselByName("GL-NT");
	if (!oapiIsVessel(self) || !oapiIsVessel(target)) return;
	VESSEL* v = oapiGetVesselInterface(self);
	VESSEL* tv = oapiGetVesselInterface(target);

	VECTOR3 g, l, n, local, avel, rvel, vgpos, prograde;
	oapiGetGlobalPos(target, &g);
	oapiGetGlobalPos(v->GetHandle(), &vgpos);
	vgpos -= g;
	v->GetRelativeVel(target, rvel);
	v->HorizonInvRot(rvel, prograde);
	v->Global2Local(g, local);
	double localScalar = length(local);
	VECTOR3 localxy = _V(local.x, local.y, 0);
	local /= length(local);
	v->GetAngularVel(avel);

	double angX = pitchAng.getOutput(local.y, 0);
	double rcsx = pitchRCS.getOutput(avel.x, angX);

	double angY = yawAng.getOutput(local.x, 0);
	double rcsy = yawRCS.getOutput(avel.y, angY);

	double vely = updownVel.getOutput(vgpos.y, 0);
	double rcsupdown = updownRCS.getOutput(prograde.y, 0);

	double velx = leftrightVel.getOutput(vgpos.x, 0);
	double rcsleftright = leftrightRCS.getOutput(prograde.x, 0);
	double firingRate = localScalar * 0.001;
	if (localScalar > 1000) {
		firingRate = 5;
	}
	if (length(localxy) > 40) {
		firingRate = 5;
	}
	if ((simt - last_fire_time) > firingRate) {
		VESSELSTATUS vs;
		v->GetStatus(vs);
		vs.status = 0;
		VECTOR3 rofs;
		v->GlobalRot(_V(0, 0, 500), rofs);
		vs.rvel += rofs;
		char name[256];
		sprintf(name, "blast%.0f", cnt);
		OBJHANDLE hVessel = oapiCreateVessel(name, "Blast", vs);
		VESSEL2 * blaster = (VESSEL2*)oapiGetVesselInterface(hVessel);
		TargetRange tr("GL-NT", 5, 500);

		blaster->clbkConsumeBufferedKey(300, true, (char *)&tr);
		last_fire_time = simt;
	}
	
	/*
	if (rcsupdown > 0) {
		v->SetThrusterGroupLevel(THGROUP_ATT_UP, rcsupdown);
		v->SetThrusterGroupLevel(THGROUP_ATT_DOWN, 0);
	}
	else {
		v->SetThrusterGroupLevel(THGROUP_ATT_UP, 0);
		v->SetThrusterGroupLevel(THGROUP_ATT_DOWN, -rcsupdown);
	}

	if (rcsleftright > 0) {
		v->SetThrusterGroupLevel(THGROUP_ATT_LEFT, 0);
		v->SetThrusterGroupLevel(THGROUP_ATT_RIGHT, rcsleftright);
	}
	else {
		v->SetThrusterGroupLevel(THGROUP_ATT_LEFT, -rcsleftright);
		v->SetThrusterGroupLevel(THGROUP_ATT_RIGHT, 0);
	}
	if (length(avel) > 0.5) {
		v->ActivateNavmode(NAVMODE_KILLROT);
		rcsx = rcsy = 0;
	}
	else if (length(avel) < 0.1) {
		v->DeactivateNavmode(NAVMODE_KILLROT);
	}
	if (rcsx > 0) {
		v->SetThrusterGroupLevel(THGROUP_ATT_PITCHUP, 0);
		v->SetThrusterGroupLevel(THGROUP_ATT_PITCHDOWN, rcsx);
	}
	else {
		v->SetThrusterGroupLevel(THGROUP_ATT_PITCHUP, -rcsx);
		v->SetThrusterGroupLevel(THGROUP_ATT_PITCHDOWN, 0);
	}
	if (rcsy > 0) {
		v->SetThrusterGroupLevel(THGROUP_ATT_YAWLEFT, rcsy);
		v->SetThrusterGroupLevel(THGROUP_ATT_YAWRIGHT, 0);
	}
	else {
		v->SetThrusterGroupLevel(THGROUP_ATT_YAWLEFT, 0);
		v->SetThrusterGroupLevel(THGROUP_ATT_YAWRIGHT, -rcsy);
	}
	sprintf(oapiDebugString(), "length(local): %.2f length(localxy): %.2f fRate: %.2f", localScalar, length(localxy), firingRate);
}
*/