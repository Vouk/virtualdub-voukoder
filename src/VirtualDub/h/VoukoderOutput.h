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
	bool write(const void *pBuffer, uint32 cbBuffer);

private:
	IVoukoder* pVoukoder;
	VDPixmapLayout mInputLayout;
	VKVIDEOFRAME frame = { 0 };
};