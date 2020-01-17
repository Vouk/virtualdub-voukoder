#include "stdafx.h"
#include <ctype.h>

#include "oshelper.h"
#include <vd2/system/file.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Dita/resources.h>
#include <vd2/Meia/decode_png.h>
#include <vd2/Kasumi/pixmapops.h>
#include "ProgressDialog.h"
#include "InputFileImages.h"
#include "VideoSourceImages.h"
#include "image.h"
#include "imagejpegdec.h"
#include "imageiff.h"
#include "VBitmap.h"

///////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_PNGDecodeErrors = 100 };
}

///////////////////////////////////////////////////////////////////////////

class VideoSourceImages : public VideoSource {
public:
	VideoSourceImages(VDInputFileImages *parent);
	~VideoSourceImages();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp)					{ return true; }
	VDPosition nearestKey(VDPosition lSample)			{ return lSample; }
	VDPosition prevKey(VDPosition lSample)				{ return lSample>0 ? lSample-1 : -1; }
	VDPosition nextKey(VDPosition lSample)				{ return lSample<mSampleLast ? lSample+1 : -1; }

	bool setTargetFormat(int depth);

	void invalidateFrameBuffer()			{ mCachedFrame = -1; }
	bool isFrameBufferValid()				{ return mCachedFrame >= 0; }
	bool isStreaming()						{ return false; }

	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);

	const void *getFrame(VDPosition frameNum);

	char getFrameTypeChar(VDPosition lFrameNum)	{ return 'K'; }
	eDropType getDropType(VDPosition lFrameNum)	{ return kIndependent; }
	bool isKeyframeOnly()					{ return true; }
	bool isType1()							{ return false; }
	bool isDecodable(VDPosition sample_num)		{ return true; }

private:
	vdrefptr<VDInputFileImages> mpParent;
	vdfastvector<wchar_t> mPathBuf;

	VDPosition	mCachedFrame;
	VBitmap	mvbFrameBuffer;

	VDPosition	mCachedHandleFrame;
	VDFile	mCachedFile;

	vdautoptr<IVDJPEGDecoder> mpJPEGDecoder;
	vdautoptr<IVDImageDecoderIFF> mpIFFDecoder;
	vdautoptr<IVDImageDecoderPNG> mpPNGDecoder;
};

IVDVideoSource *VDCreateVideoSourceImages(VDInputFileImages *parent) {
	return new VideoSourceImages(parent);
}

///////////////////////////////////////////////////////////////////////////

VideoSourceImages::VideoSourceImages(VDInputFileImages *parent)
	: mpParent(parent)
	, mCachedHandleFrame(-1)
{
	mSampleFirst = 0;

	mpTargetFormatHeader.resize(sizeof(BITMAPINFOHEADER));
	allocFormat(sizeof(BITMAPINFOHEADER));

	invalidateFrameBuffer();

	// This has to be 1 so that read() doesn't kick away the request.

	mSampleLast = mpParent->GetFrameCount();
	getFrame(0);

	// Fill out streamInfo
	extern const VDFraction& VDPreferencesGetImageSequenceFrameRate();
	const VDFraction& fr = VDPreferencesGetImageSequenceFrameRate();

	streamInfo.fccType					= VDAVIStreamInfo::kTypeVideo;
	streamInfo.fccHandler				= NULL;
	streamInfo.dwFlags					= 0;
	streamInfo.dwCaps					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwScale					= fr.getLo();
	streamInfo.dwRate					= fr.getHi();
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= VDClampToUint32(mSampleLast);
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= (DWORD)-1;
	streamInfo.dwSampleSize				= 0;
	streamInfo.rcFrameLeft				= 0;
	streamInfo.rcFrameTop				= 0;
	streamInfo.rcFrameRight				= (uint16)getImageFormat()->biWidth;
	streamInfo.rcFrameBottom			= (uint16)getImageFormat()->biHeight;
}

VideoSourceImages::~VideoSourceImages() {
}

int VideoSourceImages::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *plBytesRead, uint32 *plSamplesRead) {
	if (plBytesRead)
		*plBytesRead = 0;

	if (plSamplesRead)
		*plSamplesRead = 0;

	const wchar_t *buf = mpParent->ComputeFilename(mPathBuf, lStart);

	// Check if we already have the file handle cached.  If not, open the file.
	
	if (lStart == mCachedHandleFrame) {
		mCachedFile.seek(0);
	} else{
		mCachedHandleFrame = -1;
		mCachedFile.closeNT();
		mCachedFile.open(buf, nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
		mCachedHandleFrame = lStart;
	}

	// Replace

	uint32 size = (uint32)mCachedFile.size();

	if (size > 0x3fffffff)
		throw MyError("VideoSourceImages: File \"%s\" is too large (>1GB).", VDTextWToA(buf).c_str());

	if (!lpBuffer) {
		if (plBytesRead)
			*plBytesRead = size;

		if (plSamplesRead)
			*plSamplesRead = 1;

		return 0;
	}

	if (size > cbBuffer) {
		if (plBytesRead)
			*plBytesRead = size;

		return IVDStreamSource::kBufferTooSmall;
	}

	mCachedFile.read(lpBuffer, size);
	
	if (plBytesRead)
		*plBytesRead = size;

	if (plSamplesRead)
		*plSamplesRead = 1;

	return 0;
}

bool VideoSourceImages::setTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	switch(format) {
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB888:
	case nsVDPixmap::kPixFormat_XRGB8888:
		if (!VideoSource::setTargetFormat(format))
			return false;

		invalidateFrameBuffer();

		mvbFrameBuffer.init((void *)getFrameBuffer(), mpTargetFormatHeader->biWidth, mpTargetFormatHeader->biHeight, mpTargetFormatHeader->biBitCount);
		mvbFrameBuffer.AlignTo4();

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

const void *VideoSourceImages::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num, VDPosition target_sample) {
	// We may get a zero-byte frame if we already have the image.

	if (!data_len)
		return getFrameBuffer();

	int w, h;
	bool bHasAlpha;

	bool bIsPNG = false;
	bool bIsJPG = false;
	bool bIsBMP = false;
	bool bIsIFF = false;
	bool bIsTGA = false;

	bIsPNG = VDDecodePNGHeader(inputBuffer, data_len, w, h, bHasAlpha);
	if (!bIsPNG) {
		bIsJPG = VDIsJPEGHeader(inputBuffer, data_len);
		if (!bIsJPG) {
			bIsBMP = DecodeBMPHeader(inputBuffer, data_len, w, h, bHasAlpha);
			if (!bIsBMP) {
				bIsIFF = VDIsMayaIFFHeader(inputBuffer, data_len);
				if (!bIsIFF)
					bIsTGA = DecodeTGAHeader(inputBuffer, data_len, w, h, bHasAlpha);
			}
		}
	}

	if (!bIsBMP && !bIsTGA && !bIsJPG && !bIsPNG && !bIsIFF)
		throw MyError("Image file must be in PNG, Windows BMP, truecolor TARGA format, MayaIFF, or sequential JPEG format.");

	if (bIsJPG) {
		if (!mpJPEGDecoder)
			mpJPEGDecoder = VDCreateJPEGDecoder();
		mpJPEGDecoder->Begin(inputBuffer, data_len);
		mpJPEGDecoder->DecodeHeader(w, h);
	}

	VDPixmap pxIFF;
	if (bIsIFF) {
		if (!mpIFFDecoder)
			mpIFFDecoder = VDCreateImageDecoderIFF();
		pxIFF = mpIFFDecoder->Decode(inputBuffer, data_len);
		w = pxIFF.w;
		h = pxIFF.h;
	}

	// Check image header.

	VDAVIBitmapInfoHeader *pFormat = getImageFormat();

	if (getFrameBuffer()) {
		if (w != pFormat->biWidth || h != pFormat->biHeight) {
			vdfastvector<wchar_t> errBuf;

			throw MyError("Image \"%ls\" (%dx%d) doesn't match the image dimensions of the first image (%dx%d)."
					, mpParent->ComputeFilename(errBuf, frame_num), w, h, pFormat->biWidth, pFormat->biHeight);
		}

	} else {
		if (!AllocFrameBuffer(w * h * 4))
			throw MyMemoryError();

		pFormat->biSize				= sizeof(BITMAPINFOHEADER);
		pFormat->biWidth			= w;
		pFormat->biHeight			= h;
		pFormat->biPlanes			= 1;
		pFormat->biCompression		= 0xFFFFFFFFUL;
		pFormat->biBitCount			= 0;
		pFormat->biSizeImage		= 0;
		pFormat->biXPelsPerMeter	= 0;
		pFormat->biYPelsPerMeter	= 0;
		pFormat->biClrUsed			= 0;
		pFormat->biClrImportant		= 0;

		// special case for initial read in constructor

		return NULL;
	}

	if (bIsJPG) {
		int format;

		switch(mvbFrameBuffer.depth) {
		case 16:	format = IVDJPEGDecoder::kFormatXRGB1555;	break;
		case 24:	format = IVDJPEGDecoder::kFormatRGB888;		break;
		case 32:	format = IVDJPEGDecoder::kFormatXRGB8888;	break;
		}

		mpJPEGDecoder->DecodeImage((char *)mvbFrameBuffer.data + mvbFrameBuffer.pitch * (mvbFrameBuffer.h - 1), -mvbFrameBuffer.pitch, format);
		mpJPEGDecoder->End();
	}

	if (bIsIFF)
		VDPixmapBlt(getTargetFormat(), pxIFF);

	if (bIsBMP)
		DecodeBMP(inputBuffer, data_len, VDAsPixmap(mvbFrameBuffer));
	if (bIsTGA)
		DecodeTGA(inputBuffer, data_len, VDAsPixmap(mvbFrameBuffer));
	if (bIsPNG) {
		if (!mpPNGDecoder)
			mpPNGDecoder = VDCreateImageDecoderPNG();

		PNGDecodeError err = mpPNGDecoder->Decode(inputBuffer, data_len);

		if (err) {
			if (err == kPNGDecodeOutOfMemory)
				throw MyMemoryError();

			vdfastvector<wchar_t> errBuf;

			throw MyError("Error decoding \"%ls\": %ls\n", mpParent->ComputeFilename(errBuf, frame_num), VDLoadString(0, kVDST_PNGDecodeErrors, err));
		}

		VDPixmapBlt(VDAsPixmap(mvbFrameBuffer), mpPNGDecoder->GetFrameBuffer());
	}

	mCachedFrame = frame_num;

	return mpFrameBuffer;
}

const void *VideoSourceImages::getFrame(VDPosition frameNum) {
	uint32 lBytes;
	const void *pFrame = NULL;

	if (mCachedFrame == frameNum)
		return mpFrameBuffer;

	if (!read(frameNum, 1, NULL, 0x7FFFFFFF, &lBytes, NULL) && lBytes) {
		char *pBuffer = new char[lBytes];

		try {
			uint32 lReadBytes;

			read(frameNum, 1, pBuffer, lBytes, &lReadBytes, NULL);
			pFrame = streamGetFrame(pBuffer, lReadBytes, FALSE, frameNum, frameNum);
		} catch(MyError e) {
			delete[] pBuffer;
			throw;
		}

		delete[] pBuffer;
	}

	return pFrame;
}
