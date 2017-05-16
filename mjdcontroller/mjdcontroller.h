#ifndef _H_OICOM_CRUISE_MFD_H_
#define _H_OICOM_CRUISE_MFD_H_

#include <orbitersdk.h>

class MJDControllerMFD: public MFD {
public:
	MJDControllerMFD(DWORD w, DWORD h, VESSEL *vessel);
	~MJDControllerMFD();
	void Update (HDC hDC);
	static int MsgProc (UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam);
	bool ConsumeKeyBuffered(DWORD key);
	bool ConsumeButton(int bt, int event);
	char *ButtonLabel(int bt);
	int  ButtonMenu(const MFDBUTTONMENU **menu) const;
private:
	DWORD width, height;
	std::string req;
};

#endif