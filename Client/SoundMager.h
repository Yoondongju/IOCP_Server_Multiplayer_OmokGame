#pragma once
#include "fmod.h"
#include "fmod.hpp"
#include <io.h>

#include <windows.h>
#include <map>

#define SOUND_MAX 1.0f
#define SOUND_MIN 0.0f
#define SOUND_DEFAULT 0.5f
#define SOUND_WEIGHT 0.1f

class CSound_Manager
{
private:
	//CSound_Manager() = default;
	~CSound_Manager() = default;

public:
	int Initialize();
	void Free();

public:
	void PlaySound(const TCHAR * pSoundKey, UINT eID, float fVolume);				// 그냥 소리재생인듯   (반복 X )
	void PlaySound(const TCHAR * pSoundKey, UINT eID, float* pVolume);


	void Play_SoundRepeat(const TCHAR * pSoundKey, UINT eID, float fVolume);		// 그냥 소리재생인듯   (반복 O )
	void PlayBGM(const TCHAR * pSoundKey, float fVolume);							// 배경음악
	void StopSound(UINT eID);
	void StopAll();
	void SetChannelVolume(UINT eID, float fVolume);
	void SetPlayeSpeed(UINT eID, float fSpeedRatio);
	void  Pause(UINT eID);
	bool Check_IsPlaying(UINT eID);

private:
	void LoadSoundFile();



private:
	// 사운드 리소스 정보를 갖는 객체 
	std::map<TCHAR*, FMOD::Sound*> m_Sounds;

	// FMOD_CHANNEL : 재생하고 있는 사운드를 관리할 객체 
	enum { MAXCHANNEL = 32 }; //임시로 잡아둠 나중에 고치기
	FMOD::Channel* m_pChannelArr[MAXCHANNEL];

	// 사운드 ,채널 객체 및 장치를 관리하는 객체 
	//FMOD::System* m_pSystem;
	FMOD::System* m_pSystem = { nullptr };
};
