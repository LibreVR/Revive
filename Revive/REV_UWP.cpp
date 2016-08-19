#include "REV_UWP.h"
#include <Windows.h>
#include <appmodel.h>

std::wstring PackageFullNameFromFamilyName(const wchar_t* familyName)
{
	std::wstring fullName;
	UINT32 count = 0;
	UINT32 length = 0;

	// First call gets the count and length; PACKAGE_FILTER_HEAD tells Windows to query Application Packages
	LONG status = FindPackagesByPackageFamily(familyName, PACKAGE_FILTER_HEAD, &count, nullptr, &length, nullptr, nullptr);
	if (status == ERROR_SUCCESS || status != ERROR_INSUFFICIENT_BUFFER)
		return fullName;

	PWSTR * fullNames = (PWSTR *)malloc(count * sizeof(*fullNames));
	PWSTR buffer = (PWSTR)malloc(length * sizeof(WCHAR));
	UINT32 * properties = (UINT32 *)malloc(count * sizeof(*properties));

	// Second call gets all fullNames
	// buffer and properties are needed even though they're never used
	status = FindPackagesByPackageFamily(familyName, PACKAGE_FILTER_HEAD, &count, fullNames, &length, buffer, properties);
	if (status == ERROR_SUCCESS)
		fullName = std::wstring(fullNames[0]); // Get the first activatable package found; usually there is only one anyway

	if (properties != nullptr)
		free(properties);
	if (buffer != nullptr)
		free(buffer);
	if (fullNames != nullptr)
		free(fullNames);

	return fullName;
}
