#ifndef __ORBITER_ONLINE_MFD_H
#define __ORBITER_ONLINE_MFD_H
#include <Windows.h>
#include <Orbitersdk.h>

class OrbiterOnlineMFD : public MFD {
public:
	OrbiterOnlineMFD(DWORD w, DWORD h, VESSEL *vessel);
	~OrbiterOnlineMFD();
	bool ConsumeKeyBuffered(DWORD key);
	bool ConsumeButton(int bt, int event);
	char *ButtonLabel(int bt);
	int  ButtonMenu(const MFDBUTTONMENU **menu) const;
	void Update(HDC hDC);
	static int MsgProc(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam);

	int page;
	int mfd;
};

#endif