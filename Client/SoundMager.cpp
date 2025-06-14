#include "SoundMager.h"
#pragma comment(lib, "../x64/Debug/fmod_vc.lib")

using namespace std;

int CSound_Manager::Initialize()
{
	// ���带 ����ϴ� ��ǥ��ü�� �����ϴ� �Լ�
	FMOD::System_Create(&m_pSystem);

	m_pSystem->init(MAXCHANNEL, FMOD_INIT_NORMAL, nullptr);

	if (m_pSystem == nullptr)
		return -1;

	LoadSoundFile();

	return 0;
}

void CSound_Manager::PlaySound(const TCHAR* pSoundKey, UINT eID, float fVolume)
{
	map<TCHAR*, FMOD::Sound*>::iterator iter;

	// iter = find_if(m_mapSound.begin(), m_mapSound.end(), CTag_Finder(pSoundKey));
	iter = find_if(m_Sounds.begin(), m_Sounds.end(),
		[&](auto& iter)->bool
		{
			return !lstrcmp(pSoundKey, iter.first);
		});

	if (iter == m_Sounds.end())
		return;


	m_pSystem->playSound(iter->second, 0, false, &m_pChannelArr[eID]);

	m_pChannelArr[eID]->setVolume(fVolume);

	m_pSystem->update();

	//if (FMOD_Channel_IsPlaying(m_pChannelArr[eID], &bPlay))
	//{
	//	FMOD_System_PlaySound(m_pSystem, 0, iter->second, FALSE, &m_pChannelArr[eID]);
	//}

	//FMOD_Channel_SetVolume(m_pChannelArr[eID], fVolume);

	//FMOD_System_Update(m_pSystem);
}

void CSound_Manager::PlaySound(const TCHAR* pSoundKey, UINT eID, float* pVolume)
{
	map<TCHAR*, FMOD::Sound*>::iterator iter;

	// iter = find_if(m_mapSound.begin(), m_mapSound.end(), CTag_Finder(pSoundKey));
	iter = find_if(m_Sounds.begin(), m_Sounds.end(),
		[&](auto& iter)->bool
		{
			return !lstrcmp(pSoundKey, iter.first);
		});

	if (iter == m_Sounds.end())
		return;


	m_pSystem->playSound(iter->second, 0, false, &m_pChannelArr[eID]);

	m_pChannelArr[eID]->setVolume(*pVolume);

	m_pSystem->update();
}

void CSound_Manager::Play_SoundRepeat(const TCHAR* pSoundKey, UINT eID, float fVolume)
{
	if (!Check_IsPlaying(eID))
	{
		PlaySound(pSoundKey, eID, fVolume);
	}
}

void CSound_Manager::PlayBGM(const TCHAR* pSoundKey, float fVolume)
{
	map<TCHAR*, FMOD::Sound*>::iterator iter;

	// iter = find_if(m_mapSound.begin(), m_mapSound.end(), CTag_Finder(pSoundKey));
	iter = find_if(m_Sounds.begin(), m_Sounds.end(), [&](auto& iter)->bool
		{
			return !lstrcmp(pSoundKey, iter.first);
		});

	if (iter == m_Sounds.end())
		return;

	//FMOD_System_PlaySound(m_pSystem, FMOD_CHANNEL_FREE, iter->second, FALSE, &m_pChannelArr[SOUND_BGM]);
	m_pSystem->playSound(iter->second, 0, false, &m_pChannelArr[0]);

	//FMOD_Channel_SetMode(m_pChannelArr[SOUND_BGM], FMOD_LOOP_NORMAL);
	m_pChannelArr[0]->setMode(FMOD_LOOP_NORMAL);

	//FMOD_Channel_SetVolume(m_pChannelArr[SOUND_BGM], fVolume);
	m_pChannelArr[0]->setVolume(fVolume);

	//FMOD_System_Update(m_pSystem);
	m_pSystem->update();
}

void CSound_Manager::StopSound(UINT eID)
{
	//FMOD_Channel_Stop(m_pChannelArr[eID]);
	m_pChannelArr[eID]->stop();
}

void CSound_Manager::StopAll()
{
	for (int i = 0; i < MAXCHANNEL; ++i)
		m_pChannelArr[i]->stop();
}

void CSound_Manager::SetChannelVolume(UINT eID, float fVolume)
{
	//FMOD_Channel_SetVolume(m_pChannelArr[eID], fVolume);
	m_pChannelArr[eID]->setVolume(fVolume);

	//FMOD_System_Update(m_pSystem);
	m_pSystem->update();
}

void CSound_Manager::SetPlayeSpeed(UINT eID, float fSpeedRatio)
{
	float fCurrentFrequency = {};
	m_pChannelArr[eID]->getFrequency(&fCurrentFrequency);

	//44100.f
	m_pChannelArr[eID]->setFrequency(fCurrentFrequency * fSpeedRatio);

	m_pSystem->update();
}

void CSound_Manager::Pause(UINT eID)
{
	bool bIsPause;
	m_pChannelArr[eID]->getPaused(&bIsPause);

	m_pChannelArr[eID]->setPaused(bIsPause);
}

bool CSound_Manager::Check_IsPlaying(UINT eID)
{
	bool isPlay;

	m_pChannelArr[eID]->isPlaying(&isPlay);

	return isPlay;
}

void CSound_Manager::LoadSoundFile()
{
	// _finddata_t : <io.h>���� �����ϸ� ���� ������ �����ϴ� ����ü
	_finddata_t fd;

	// _findfirst : <io.h>���� �����ϸ� ����ڰ� ������ ��� ������ ���� ù ��° ������ ã�� �Լ�
	intptr_t handle = _findfirst("../x64/Sound/*", &fd);

	if (handle == -1)
		return;

	int iResult = 0;

	char szCurPath[128] = "../x64/Sound/";	 // ��� ���
	char szFullPath[128] = "";

	while (iResult != -1)
	{
		strcpy_s(szFullPath, szCurPath);
		strcat_s(szFullPath, fd.name);

		//FMOD_SOUND* pSound = nullptr;
		FMOD::Sound* pSound = nullptr;

		//FMOD_RESULT eRes = FMOD_System_CreateSound(m_pSystem, szFullPath, FMOD_HARDWARE, 0, &pSound);
		FMOD_RESULT eRes = m_pSystem->createSound(szFullPath, FMOD_LOOP_OFF, 0, &pSound);

		if (eRes == FMOD_OK)
		{
			int iLength = int(strlen(fd.name) + 1);

			TCHAR* pSoundKey = new TCHAR[iLength];
			ZeroMemory(pSoundKey, sizeof(TCHAR) * iLength);

			// �ƽ�Ű �ڵ� ���ڿ��� �����ڵ� ���ڿ��� ��ȯ�����ִ� �Լ�
			MultiByteToWideChar(CP_ACP, 0, fd.name, iLength, pSoundKey, iLength);

			m_Sounds.emplace(pSoundKey, pSound);
		}
		//_findnext : <io.h>���� �����ϸ� ���� ��ġ�� ������ ã�� �Լ�, ���̻� ���ٸ� -1�� ����
		iResult = _findnext(handle, &fd);
	}

	m_pSystem->update();

	_findclose(handle);
}



void CSound_Manager::Free()
{
	for (auto& Pair : m_Sounds)
	{
		delete[] Pair.first;

		//FMOD_Sound_Release(Mypair.second);
		Pair.second->release();
	}
	m_Sounds.clear();

	m_pSystem->release();
	m_pSystem->close();


	delete this;
}
