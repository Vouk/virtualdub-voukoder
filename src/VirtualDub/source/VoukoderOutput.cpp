#include "stdafx.h"
#include "VoukoderOutput.h"
#include "VoukoderTypeLib_i.c"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class VoukoderOutputStream : public AVIOutputStream {
public:
	VoukoderOutputStream(VoukoderOutput* pParent, bool isVideo);
	~VoukoderOutputStream();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}

private:
	VoukoderOutput *const mpParent;
	bool isVideo;
};

VoukoderOutputStream::VoukoderOutputStream(VoukoderOutput* pParent, bool isVideo):
	mpParent(pParent) {
	this->isVideo = isVideo;
}

VoukoderOutputStream::~VoukoderOutputStream() {}

void VoukoderOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	if (isVideo) {
		if (mpParent->writeVideoFrame(pBuffer, cbBuffer)) 
			throw MyError("Unable to write video frame!");
	}
	else {
		if (mpParent->writeAudioChunk(pBuffer, cbBuffer, samples))
			throw MyError("Unable to write audio chunk!");
	}
}

void VoukoderOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial writes are not supported.");
}

//////

VoukoderOutput::VoukoderOutput() {}

VoukoderOutput::~VoukoderOutput() {}

void VoukoderOutput::SetConfiguration(const VKENCODERCONFIG& config) {
	mConfig = config;
}

void VoukoderOutput::SetInputLayout(const VDPixmapLayout& layout) {
	mInputLayout = layout;
}

IVDMediaOutputStream *VoukoderOutput::createVideoStream() {
	VDASSERT(!videoOut);

	if (!(videoOut = new_nothrow VoukoderOutputStream(this, true)))
		throw MyMemoryError();

	return videoOut;
}

IVDMediaOutputStream *VoukoderOutput::createAudioStream() {
	VDASSERT(!audioOut);

	if (!(audioOut = new_nothrow VoukoderOutputStream(this, false)))
		throw MyMemoryError();

	return audioOut;
}

bool VoukoderOutput::init(const wchar_t *pwszFile) {
	// Create the Voukoder COM instance
	HRESULT hr = CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder, (void**)&pVoukoder);
	if (FAILED(hr))
	{
		MessageBox(NULL, "The Voukoder core installation could not be found! Please go to www.voukoder.org and install the core component.", "Voukoder", MB_OK | MB_ICONERROR);
		return false;
	}

	// Apply the encoder settings
	pVoukoder->SetConfig(mConfig);

	// Video & audio settings
	VKENCODERINFO info = { 0 };
	wcscpy_s(info.filename, pwszFile);
	wcscpy_s(info.application, L"VirtualDub");

	// Video
	AVIStreamHeader_fixed si = videoOut->getStreamInfo();
	info.video.enabled = videoOut && mInputLayout.w > 0 && mInputLayout.h > 0;
	info.video.width = mInputLayout.w;
	info.video.height = mInputLayout.h;
	info.video.fieldorder = FieldOrder::Progressive; //TODO
	info.video.timebase = { (int)si.dwScale, (int)si.dwRate };
	info.video.aspectratio = { 1, 1 }; //TODO

	// Audio
	info.audio.enabled = audioOut != NULL;
	if (info.audio.enabled) {
		const WAVEFORMATEX& wfex = *(const WAVEFORMATEX *)audioOut->getFormat();

		if (wfex.wFormatTag != WAVE_FORMAT_PCM)
			throw MyError("Voukoder doesn't support compressed audio.");

		if (wfex.nChannels == 1) {
			info.audio.channellayout = ChannelLayout::Mono;
			info.audio.numberChannels = 1;
		}
		else if (wfex.nChannels == 2) {
			info.audio.channellayout = ChannelLayout::Stereo;
			info.audio.numberChannels = 2;
		}
		else if (wfex.nChannels == 6) {
			info.audio.channellayout = ChannelLayout::FivePointOne;
			info.audio.numberChannels = 6;
		}
		else
			throw MyError("Unsupported audio channel layout.");

		info.audio.samplerate = wfex.nSamplesPerSec;
	}

	// Open Voukoder
	if (FAILED(pVoukoder->Open(info)))
	{
		MessageBox(NULL, "Can not open voukoder", "Voukoder", MB_OK | MB_ICONERROR);
		return false;
	}

	initVideoFrame();

	return true;
}

void VoukoderOutput::finalize() {
	// Free buffers
	for (int p = 0; p < videoFrame.planes; p++)
		delete(videoFrame.buffer[p]);

	delete(videoFrame.rowsize);
	delete(videoFrame.buffer);

	// Close Voukoder and release COM instance
	if (pVoukoder)
	{
		pVoukoder->Close(TRUE);
		pVoukoder->Release();
	}
}

void VoukoderOutput::initVideoFrame() {
	//
	videoFrame.width = mInputLayout.w;
	videoFrame.height = mInputLayout.h;
	videoFrame.planes = 3;
	videoFrame.pass = 1;
	strcpy_s(videoFrame.format, "yuv420p");

	// Rowsizes for each plane
	videoFrame.rowsize = new int[videoFrame.planes]{
		(int)mInputLayout.pitch,
		(int)mInputLayout.pitch2,
		(int)mInputLayout.pitch3
	};

	// Fill frame buffer
	videoFrame.buffer = new BYTE *[videoFrame.planes];
	for (int p = 0; p < videoFrame.planes; p++)
		videoFrame.buffer[p] = new BYTE[(size_t)videoFrame.rowsize[p] * videoFrame.height];
}

bool VoukoderOutput::writeVideoFrame(const void *pBuffer, uint32 cbBuffer) {
	// Fill the frame (I420 > planar)
	memcpy(videoFrame.buffer[0], (BYTE*)pBuffer, (size_t)videoFrame.rowsize[0] * videoFrame.height);
	memcpy(videoFrame.buffer[1], (BYTE*)pBuffer + (int)mInputLayout.data2, ((size_t)videoFrame.rowsize[1] / 2) * videoFrame.height);
	memcpy(videoFrame.buffer[2], (BYTE*)pBuffer + (int)mInputLayout.data3, ((size_t)videoFrame.rowsize[2] / 2) * videoFrame.height);

	// Send frame to Voukoder
	return FAILED(pVoukoder->SendVideoFrame(videoFrame));
}

bool VoukoderOutput::writeAudioChunk(const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	const WAVEFORMATEX& wfex = *(const WAVEFORMATEX *)audioOut->getFormat();
	
	if (wfex.wFormatTag != WAVE_FORMAT_PCM)
		throw MyError("Voukoder doesn't support compressed audio.");

	// Fill audio chunk
	VKAUDIOCHUNK chunk = { 0 };
	chunk.buffer = new BYTE *[1];
	chunk.samples = samples;
	chunk.buffer[0] = new uint8_t[cbBuffer];
	chunk.blockSize = wfex.nBlockAlign;
	chunk.planes = 1;
	chunk.sampleRate = wfex.nSamplesPerSec;

	if (wfex.wBitsPerSample == 16)
		strcpy_s(chunk.format, "s16");
	else if (wfex.wBitsPerSample == 32)
		strcpy_s(chunk.format, "s32");
	else
		throw MyError("Unsupported audio bit depth.");

	if (wfex.nChannels == 1)
		chunk.layout = ChannelLayout::Mono;
	else if (wfex.nChannels == 2)
		chunk.layout = ChannelLayout::Stereo;
	else if (wfex.nChannels == 6)
		chunk.layout = ChannelLayout::FivePointOne;
	else
		throw MyError("Unsupported audio channel layout.");

	// Copy buffer
	memcpy(chunk.buffer[0], pBuffer, cbBuffer);

	bool ret = FAILED(pVoukoder->SendAudioSampleChunk(chunk));

	delete(chunk.buffer[0]);
	delete(chunk.buffer);

	// Send chunk to Voukoder
	return ret;
}