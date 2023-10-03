#include "D3D9DebugUtils.h"


ods_buf::~ods_buf() {
	sync();
}

int ods_buf::sync() {
#ifdef WIN32
	//Output the string
	OutputDebugStringW(str().c_str());
#else
	//Add non-win32 debug output code here
#endif

		//Clear the buffer
	str(std::basic_string<TCHAR>());

	return 0;
}

ods_stream::ods_stream() : std::basic_ostream<TCHAR, std::char_traits<TCHAR> >(new ods_buf()) {}

ods_stream::~ods_stream() {
	delete rdbuf();
}

std::basic_string<TCHAR> HexString(DWORD data, DWORD numBits) {
	std::basic_ostringstream<TCHAR> strHexNum;

	strHexNum << std::hex;
	strHexNum.fill('0');
	strHexNum << std::uppercase;
	strHexNum << std::setw(((numBits + 3) & -4) / 4);
	strHexNum << data;

	return strHexNum.str();
}
