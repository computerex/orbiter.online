#ifndef _H_BLASTER_FOR_BLASTER_MFD_H_
#define _H_BLASTER_FOR_BLASTER_MFD_H_

#include <orbitersdk.h>

#define EVENT_RANGE_SET 300

class BLAST : public VESSEL2
{
public:
	BLAST(OBJHANDLE hVessel, int flightmodel);
	void   clbkSetClassCaps(FILEHANDLE cfg);
	int    clbkConsumeBufferedKey (DWORD key, bool down, char *kstate);
	void   clbkPreStep(double simt, double simdt, double mjd);
	void   clbkPostStep(double simt, double simdt, double mjd);
	bool   Blow();
	double ClosestVessel();
	void   Damage(OBJHANDLE hTarget);
	void   Damage();
	void Physics();
	void deleteMyself();
private:
	double m_DRange;
	double m_vel;
	bool   m_Blown;
	double m_CreationTime;
	double m_ExplosionTime;
	char target[256];
	bool isLocal;
};
#endif