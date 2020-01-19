#pragma once

#include "AVIOutput.h"
#include "DubOutput.h"

#include "VoukoderTypeLib_h.h"

class VoukoderOutput: public AVIOutput
{
public:
	VoukoderOutput();
	~VoukoderOutput();

	void SetInputLayout(const VDPixmapLayout& layout);

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();
	bool writeVideoFrame(const void *pBuffer, uint32 cbBuffer);
	bool writeAudioChunk(const void *pBuffer, uint32 cbBuffer, uint32 samples);

private:
	IVoukoder* pVoukoder;
	VDPixmapLayout mInputLayout;
	VKVIDEOFRAME videoFrame = { 0 };
	void initVideoFrame();
};