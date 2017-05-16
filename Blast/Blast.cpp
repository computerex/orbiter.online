#define STRICT
#define ORBITER_MODULE

#include "Blast.h"
#include <vector>

VECTOR3 negv(VECTOR3 a)
{
	return _V(-a.x,-a.y,-a.z);
}
VECTOR3 norv(VECTOR3 &a)
{
	double l = length(a);
	return _V(a.x/l, a.y/l, a.z/l);
}
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

BLAST::BLAST(OBJHANDLE hVessel, int flightmodel) : VESSEL2(hVessel, flightmodel)
{
	m_DRange=1; 
	m_Blown =false;
	m_vel = 5;
	m_CreationTime=oapiGetSimTime();
	m_ExplosionTime=m_DRange;
}
int BLAST::clbkConsumeBufferedKey(DWORD key, bool down, char *kstate)
{
	if (!down) return 0;

	if (key==EVENT_RANGE_SET)
	{
		auto targetRange = *(TargetRange*)kstate;
		this->m_DRange = targetRange.range;
		this->m_vel = targetRange.vel;
		strcpy(target, targetRange.target);
		ClearThrusterDefinitions();
		PROPELLANT_HANDLE hpr = CreatePropellantResource(1);
		THRUSTER_HANDLE    th = CreateThruster(_V(0,0,0),_V(0,1,0),1e0,hpr,500e50);
		COLOUR4 col_d = { 0.9f,0.8f,1.0f,0.0f };
		COLOUR4 col_s = { 1.9f,0.8f,1.0f,0.0f };
		COLOUR4 col_a = { 0.0f,0.0f,0.0f,0.0f };
		AddPointLight(_V(0, 0, 0), m_DRange*10, 0.0, 0.0, 2e-3, col_d, col_s, col_a);
		AddExhaust(th, 1, m_DRange/2);
		CreateThrusterGroup(&th, 1, THGROUP_MAIN);
		SetThrusterGroupLevel(THGROUP_MAIN,1);
		return 1;
	}
	return 0;
}
void BLAST::clbkPostStep(double simt, double simdt, double mjd)
{
}
void BLAST::clbkPreStep(double simt, double simdt, double mjd)
{
	double lifetime = (simt - m_CreationTime);
	VECTOR3 fVector;
	GetWeightVector(fVector);
	AddForce(negv(fVector), _V(0, 0, 0));
	//Physics();
	OBJHANDLE target = oapiGetObjectByName(this->target);
	if (target == NULL) { 
		if (lifetime > 1) {
			oapiDeleteVessel(GetHandle());
		}
		return; 
	}
	char buffer[256];
	oapiGetObjectName(target, buffer, 256);
	VECTOR3 g, local, l, rp,rv;
	oapiGetGlobalPos(target, &g);
	Global2Local(g, l);
	GetRelativePos(target, rp);
	VECTOR3 n, vel;
	
	//Collision detection with target supporting "shoot-through"
	GetRelativeVel(target, n);
	n = n*simdt;											//Propagate along relative velocity trajectory
	GetGlobalPos(l);
	g -= l;
	double r = length(n);									//Detection range
	double t = max(min(dotp(g, n) / r / r, 1), 0);				//"Periapsis" point along the trajectory
	double s = oapiGetSize(target);
	r = length(-g + n*t);									//Line-point distance
	if (length(rp) < m_DRange || r<m_DRange || (-0.5<l.x && l.x<0.5 && -0.5<l.y && l.y<0.5 && -0.5<l.z && l.z<0.5))
	{
		//oapiDeleteVessel(target);
		Damage(target);
		oapiDeleteVessel(GetHandle());
		return;
	}

	MATRIX3 m;
	GetRotationMatrix(m);
	l = _V(m.m13, m.m23, m.m33);			
	
	GetRelativeVel(target, rv);
	if (dotp(g, n) < 0 && lifetime > 1) { strcpy(this->target, ""); return; }	//TODO: make missile targeting angle a server-set constant
																		//Head towards target
	//Current direction vector
	normalise(g);										//Target direction vector, also	Matrix Z vector	
	
	normalise(n = crossp(_V(m.m12, m.m22, m.m32), g));	//Matrix X vector
	normalise(l = -crossp(n, g));							//Matrix Y vector
	SetRotationMatrix(_M(n.x, l.x, g.x, n.y, l.y, g.y, n.z, l.z, g.z));
	VESSELSTATUS vs;
	memset(&vs, 0, sizeof(VESSELSTATUS));
	GetStatus(vs);
	//GlobalRot(_V(0, 0, 0), vel);
	//vs.rvel += vel;
	Local2Rel(_V(0, 0, m_vel*simdt), vs.rpos);
	DefSetState(&vs);
	
	/*
	if (Blow())
	{
		m_Blown=true;
		m_ExplosionTime=oapiGetSimTime();
		SetThrusterGroupLevel(THGROUP_MAIN,0);
		Damage();
	}
	if (m_Blown)
	{
		if (m_ExplosionTime+5 < oapiGetSimTime())
		{
			oapiDeleteVessel(GetHandle());
		}
	}*/
}
void BLAST::clbkSetClassCaps(FILEHANDLE cfg)
{
	SetReentryTexture(NULL);
	SetEnableFocus(false);
}
bool BLAST::Blow()
{
	if (m_Blown)
		return false;
	if (GroundContact())
		return true;
	if (ClosestVessel() <= m_DRange)
		return true;
	if (m_CreationTime+20 < oapiGetSimTime())
		return true;
	return false;
}
double BLAST::ClosestVessel()
{
	std::vector<double> dist;
	OBJHANDLE hVes;
	VECTOR3 pos;
	VESSEL * vessel;
	for (int i = 0; i < oapiGetVesselCount(); i++)
	{
		hVes=oapiGetVesselByIndex(i);
		vessel=oapiGetVesselInterface(hVes);
		if (oapiGetFocusInterface()->GetHandle() == hVes)
			continue;
		if (vessel->GetClassName())
			if (!strcmp(vessel->GetClassName(),"Blast"))
				continue;
		GetRelativePos(hVes,pos);
		dist.push_back(length(pos));
	}
	while(true)
	{
		bool done = true;
		for (int i = 1; i < dist.size(); i++)
		{
			if (dist[i] < dist[i-1])
			{
				double temp = dist[i];
				dist[i]=dist[i-1];
				dist[i-1]=temp;
				done=false;
			}
		}
		if (done)
			break;
	}
	return dist[0];
}
void BLAST::Damage(OBJHANDLE hTarget)
{
	VESSEL * target=oapiGetVesselInterface(hTarget);
	VESSELSTATUS vs;
	target->GetStatus(vs);
	vs.status=0;
	target->Local2Rel(_V(1, 1, 1), vs.rpos);
	target->DefSetState(&vs);
	VECTOR3 rpos;
	GetRelativePos(GetHandle(),rpos);
	VECTOR3 angvel;
	target->GetAngularVel(angvel);
	angvel.x+=0.1;
	angvel.z-=0.1;
	target->SetAngularVel(angvel);
}
void BLAST::Damage()
{
	double dist = 0;
	OBJHANDLE hVes=NULL;
	VECTOR3 pos;
	for (int i = 0; i < oapiGetVesselCount(); i++)
	{
		hVes=oapiGetVesselByIndex(i);
		GetRelativePos(hVes, pos);
		if (length(pos) <= m_DRange)
			Damage(hVes);
	}
}
DLLCLBK VESSEL *ovcInit (OBJHANDLE hvessel, int flightmodel)
{
	return new BLAST (hvessel, flightmodel);
}
DLLCLBK void ovcExit (VESSEL *vessel)
{
	if (vessel) delete (BLAST*)vessel;
}

void BLAST::Physics()
{
	if (oapiGetTimeAcceleration() > 10)
		return;
	VECTOR3 spdShipVector;
	GetShipAirspeedVector(spdShipVector);
	VECTOR3 ForceVectorNewton;
	VECTOR3 RadiusVectorMeter;
	int RadiusVectorMultiplier = GetMass();
	ForceVectorNewton.x = 0;
	RadiusVectorMeter.x = 0;
	ForceVectorNewton.y = 0;
	RadiusVectorMeter.y = 0;
	ForceVectorNewton.z = 0;
	RadiusVectorMeter.z = 0;
	if (GetAltitude() < 100)
	{
		ForceVectorNewton.x = 0;
		RadiusVectorMeter.x = 0;
		ForceVectorNewton.z = 0;
		RadiusVectorMeter.z = 0;
		ForceVectorNewton.y = 0;
		RadiusVectorMeter.y = 0;
	}
	else
	{
		if (spdShipVector.x != 0)
		{
			RadiusVectorMeter.x = ((spdShipVector.x - spdShipVector.x - spdShipVector.x) * 10);
			ForceVectorNewton.x = ((RadiusVectorMeter.x / 10) * RadiusVectorMultiplier);
		}
		if (spdShipVector.y != 0)
		{
			RadiusVectorMeter.y = ((spdShipVector.y - spdShipVector.y - spdShipVector.y) * 10);
			ForceVectorNewton.y = ((RadiusVectorMeter.y / 10) * RadiusVectorMultiplier);
		}
	}
	AddForce(ForceVectorNewton, RadiusVectorMeter);
}