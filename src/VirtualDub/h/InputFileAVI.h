//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_INPUTFILEAVI_H
#define f_INPUTFILEAVI_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdstl.h>

#include "InputFile.h"

class InputFileAVI;

class VDAVIStreamSource : public vdlist_node {
	VDAVIStreamSource& operator=(const VDAVIStreamSource&);
public:
	VDAVIStreamSource(InputFileAVI *pParent);
	VDAVIStreamSource(const VDAVIStreamSource&);
	~VDAVIStreamSource();

	virtual void Reinit() {}

protected:
	vdrefptr<InputFileAVI> mpParent;
};

class InputFileAVI : public InputFile {
private:
	IAVIReadHandler *pAVIFile;

	bool fAutomated;

	bool fCompatibilityMode, fRedoKeyFlags, fInternalDecoder, fDisableFastIO, fAcceptPartial, fAutoscanSegments;
	int iMJPEGMode;
	FOURCC fccForceVideo;
	FOURCC fccForceVideoHandler;
	long lForceAudioHz;

	typedef std::vector<vdfastvector<uint32> > NewKeyFlags;
	NewKeyFlags mNewKeyFlags;

	typedef vdlist<VDAVIStreamSource> Streams; 
	Streams	mStreams;

	static char szME[];
public:
	InputFileAVI();
	~InputFileAVI();

	void Init(const wchar_t *szFile);
	bool Append(const wchar_t *szFile);

	void GetTextInfo(tFileTextInfo& info);

	bool isOptimizedForRealtime();
	bool isStreaming();

	bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	bool GetAudioSource(int index, AudioSource **ppSrc);

	void setOptions(InputFileOptions *_ifo);
	InputFileOptions *createOptions(const void *buf, uint32 len);
	InputFileOptions *promptForOptions(VDGUIHandle hwnd);
	void EnableSegmentAutoscan();
	void ForceCompatibility();
	void setAutomated(bool fAuto);

	void InfoDialog(VDGUIHandle hwndParent);

public:
	void Attach(VDAVIStreamSource *p);
	void Detach(VDAVIStreamSource *p);
};

#endif
