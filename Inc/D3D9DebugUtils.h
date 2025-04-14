#pragma once

#include <windows.h>

#include <iostream>
#include <sstream>
#include <iomanip>

class ods_buf : public std::basic_stringbuf<TCHAR, std::char_traits<TCHAR> > {
public:
	virtual ~ods_buf();

protected:
	int sync();
};

class ods_stream : public std::basic_ostream<TCHAR, std::char_traits<TCHAR> > {
public:
	ods_stream();
	~ods_stream();
};

extern ods_stream dout;

std::basic_string<TCHAR> HexString(DWORD data, DWORD numBits = 4);

bool exportPackage(const class FString& exportPackageName);
