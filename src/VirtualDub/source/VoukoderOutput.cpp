#include "stdafx.h"
#include "VoukoderOutput.h"

#include "VoukoderTypeLib_i.c"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class VoukoderOutputStream : public AVIOutputStream {
public:
	VoukoderOutputStream(VoukoderOutput* pParent);
	~VoukoderOutputStream();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}

private:
	VoukoderOutput *const mpParent;
};

VoukoderOutputStream::VoukoderOutputStream(VoukoderOutput* pParent):
	mpParent(pParent)
{}

VoukoderOutputStream::~VoukoderOutputStream() {}

void VoukoderOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	mpParent->write(pBuffer, cbBuffer);
}

void VoukoderOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial writes are not supported.");
}

//////

VoukoderOutput::VoukoderOutput() {}

VoukoderOutput::~VoukoderOutput() {}

void VoukoderOutput::SetInputLayout(const VDPixmapLayout& layout) {
	mInputLayout = layout;
}

IVDMediaOutputStream *VoukoderOutput::createVideoStream() {
	VDASSERT(!videoOut);

	if (!(videoOut = new_nothrow VoukoderOutputStream(this)))
		throw MyMemoryError();

	return videoOut;
}

IVDMediaOutputStream *VoukoderOutput::createAudioStream() {
	VDASSERT(!audioOut);

	if (!(audioOut = new_nothrow VoukoderOutputStream(this)))
		throw MyMemoryError();

	return audioOut;
}

bool VoukoderOutput::init(const wchar_t *pwszFile) {

	//static_cast<VoukoderOutputStream *>(videoOut)->init();

	// Create the Voukoder COM instance
	HRESULT hr = CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder, (void**)&pVoukoder);
	if (FAILED(hr))
	{
		MessageBox(NULL, "The Voukoder core installation could not be found! Please go to www.voukoder.org and install the core component.", "Voukoder", MB_OK | MB_ICONERROR);
		return false;
	}

	// Define the encoder settings
	VKENCODERCONFIG config = { 0 };
	strcpy_s(config.video.encoder, "libx264");
	strcpy_s(config.video.options, "_pixelFormat=yuv420p");
	strcpy_s(config.audio.encoder, "aac");
	strcpy_s(config.audio.options, "_sampleFormat=fltp");
	strcpy_s(config.format.container, "mp4");

	pVoukoder->SetConfig(config);

	AVIStreamHeader_fixed si = videoOut->getStreamInfo();
	const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)videoOut->getFormat();

	// Video & audio settings
	VKENCODERINFO info = { 0 };
	wcscpy_s(info.filename, pwszFile);
	wcscpy_s(info.application, L"VirtualDub");
	info.video.enabled = mInputLayout.w > 0 && mInputLayout.h > 0;
	info.video.width = mInputLayout.w;
	info.video.height = mInputLayout.h;
	info.video.fieldorder = FieldOrder::Progressive; //TODO
	info.video.timebase = { (int)si.dwScale, (int)si.dwRate };
	info.video.aspectratio = { 1, 1 }; //TODO
	info.audio.enabled = false; //TODO
	//info.audio.channellayout = ChannelLayout::Stereo;//TODO
	//info.audio.numberChannels = 2;//TODO
	//info.audio.samplerate = 48000;//TODO

	if (FAILED(pVoukoder->Open(info)))
	{
		MessageBox(NULL, "Can not open voukoder", "Voukoder", MB_OK | MB_ICONERROR);
		return false;
	}

	frame.width = mInputLayout.w;
	frame.height = mInputLayout.h;
	frame.planes = 3;
	frame.pass = 1;
	strcpy_s(frame.format, "yuv420p");

	// Rowsizes for each plane
	frame.rowsize = new int[frame.planes]{
		(int)mInputLayout.pitch,
		(int)mInputLayout.pitch2,
		(int)mInputLayout.pitch3
	};

	// Fill frame buffer
	frame.buffer = new BYTE *[frame.planes];
	for (int p = 0; p < frame.planes; p++)
		frame.buffer[p] = new uint8_t[(size_t)frame.rowsize[p] * frame.height];

	return true;
}

void VoukoderOutput::finalize() {
	//
	for (int p = 0; p < frame.planes; p++)
		free(frame.buffer[p]);

	free(frame.rowsize);
	free(frame.buffer);

	//
	if (pVoukoder)
	{
		pVoukoder->Close(TRUE);
		pVoukoder->Release();
	}
}

bool VoukoderOutput::write(const void *pBuffer, uint32 cbBuffer) {
	// Fill the frame (I420 > planar)
	memcpy(frame.buffer[0], (BYTE*)pBuffer, (size_t)frame.rowsize[0] * frame.height);
	memcpy(frame.buffer[1], (BYTE*)pBuffer + (int)mInputLayout.data2, ((size_t)frame.rowsize[1] / 2) * frame.height);
	memcpy(frame.buffer[2], (BYTE*)pBuffer + (int)mInputLayout.data3, ((size_t)frame.rowsize[2] / 2) * frame.height);
	   	 
	// Send frame to Voukoder
	return FAILED(pVoukoder->SendVideoFrame(frame));
}