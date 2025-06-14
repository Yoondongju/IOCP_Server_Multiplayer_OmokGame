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
	void PlaySound(const TCHAR * pSoundKey, UINT eID, float fVolume);				// �׳� �Ҹ�����ε�   (�ݺ� X )
	void PlaySound(const TCHAR * pSoundKey, UINT eID, float* pVolume);


	void Play_SoundRepeat(const TCHAR * pSoundKey, UINT eID, float fVolume);		// �׳� �Ҹ�����ε�   (�ݺ� O )
	void PlayBGM(const TCHAR * pSoundKey, float fVolume);							// �������
	void StopSound(UINT eID);
	void StopAll();
	void SetChannelVolume(UINT eID, float fVolume);
	void SetPlayeSpeed(UINT eID, float fSpeedRatio);
	void  Pause(UINT eID);
	bool Check_IsPlaying(UINT eID);

private:
	void LoadSoundFile();



private:
	// ���� ���ҽ� ������ ���� ��ü 
	std::map<TCHAR*, FMOD::Sound*> m_Sounds;

	// FMOD_CHANNEL : ����ϰ� �ִ� ���带 ������ ��ü 
	enum { MAXCHANNEL = 32 }; //�ӽ÷� ��Ƶ� ���߿� ��ġ��
	FMOD::Channel* m_pChannelArr[MAXCHANNEL];

	// ���� ,ä�� ��ü �� ��ġ�� �����ϴ� ��ü 
	//FMOD::System* m_pSystem;
	FMOD::System* m_pSystem = { nullptr };
};
