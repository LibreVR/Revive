#include "SessionDetails.h"

#include <Windows.h>
#include <Shlwapi.h>

SessionDetails::HackInfo SessionDetails::m_known_hacks[] = {
	{ "drt.exe", HACK_WAIT_IN_TRACKING_STATE, true },
	{ "ultrawings.exe", HACK_FAKE_PRODUCT_NAME, true },
};

SessionDetails::SessionDetails()
{
	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	char* filename = PathFindFileNameA(filepath);

	for (auto& hack : m_known_hacks)
	{
		if (_stricmp(filename, hack.m_filename) == 0)
			m_hacks.emplace(hack.m_hack, hack);
	}
}

SessionDetails::~SessionDetails()
{
}

bool SessionDetails::UseHack(Hack hack)
{
	auto it = m_hacks.find(hack);
	if (it == m_hacks.end())
		return false;
	return it->second.m_usehack;
}
