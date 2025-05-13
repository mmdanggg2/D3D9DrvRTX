#include "D3D9DebugUtils.h"
#include "Core.h"

ods_buf::~ods_buf() {
	sync();
}

int ods_buf::sync() {
#ifdef WIN32
	//Output the string
	OutputDebugString(str().c_str());
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

ods_stream dout;

std::basic_string<TCHAR> HexString(DWORD data, DWORD numBits) {
	std::basic_ostringstream<TCHAR> strHexNum;

	strHexNum << std::hex;
	strHexNum.fill('0');
	strHexNum << std::uppercase;
	strHexNum << std::setw(((numBits + 3) & -4) / 4);
	strHexNum << data;

	return strHexNum.str();
}

#if UTGLR_ENABLE_CLASS_EXPORT
bool exportPackage(const FString& exportPackageName) {
	// You should put what class headers you do have in /Package/Inc/*.h or classes will not have the #include statements
	// Load in the Editor package, specifically ClassExporterH which ExportToFile will find and use to export .h files
	UObject::StaticLoadClass(UExporter::StaticClass(), NULL, TEXT("Editor.ClassExporterH"), NULL, LOAD_NoFail, NULL);
	// Grab the package to export
	UObject* exportPackage = UObject::LoadPackage(NULL, *exportPackageName, LOAD_NoFail);
	for (TObjectIterator<UClass> classIter = TObjectIterator<UClass>(); classIter; ++classIter) {
		UClass* cls = *classIter;
		UObject* outer = cls->GetOuter();
		if (outer != exportPackage) continue;  // Ignore if not the package we want
		// Seems to be unavailable // if (!cls->ScriptText) continue;  // Ignore classes with no uc script.
		if (!(cls->GetFlags() & RF_Native) || (cls->ClassFlags & CLASS_NoExport)) continue;  // Ignore if not native
		cls->SetFlags(RF_TagExp);  // Set this as an object to export
	}
	FString exportPath = FString(TEXT("../")) + exportPackageName + TEXT("/Inc/") + exportPackageName + TEXT("Classes.h");
	return UExporter::ExportToFile(UObject::StaticClass(), NULL, *exportPath, false, false);
}
#endif

const TCHAR* dereferenceFName(const FName& name) {
	__try {
		if (const_cast<FName&>(name).IsValid()) {
			return name.operator*();
		} else {
			return nullptr;
		}
	} __except (true) {  // Eat EVERY exception, what could go wrong?
		return nullptr;
	}
}
