//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"

#include <vd2/VDXFrame/VideoFilter.h>

class VDVFNull : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();
};

uint32 VDVFNull::GetParams() {
	switch(fa->src.mpPixmapLayout->format) {
	case nsVDXPixmap::kPixFormat_XRGB1555:
	case nsVDXPixmap::kPixFormat_RGB565:
	case nsVDXPixmap::kPixFormat_RGB888:
	case nsVDXPixmap::kPixFormat_XRGB8888:
	case nsVDXPixmap::kPixFormat_Y8:
	case nsVDXPixmap::kPixFormat_YUV422_UYVY:
	case nsVDXPixmap::kPixFormat_YUV422_YUYV:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV411_Planar:
	case nsVDXPixmap::kPixFormat_YUV410_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
	case nsVDXPixmap::kPixFormat_Y8_FR:
	case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
	case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
	case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
	case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
	case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
	case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV420i_Planar:
	case nsVDXPixmap::kPixFormat_YUV420i_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV420i_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV420i_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV420it_Planar:
	case nsVDXPixmap::kPixFormat_YUV420it_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV420it_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV420it_Planar_709_FR:
	case nsVDXPixmap::kPixFormat_YUV420ib_Planar:
	case nsVDXPixmap::kPixFormat_YUV420ib_Planar_FR:
	case nsVDXPixmap::kPixFormat_YUV420ib_Planar_709:
	case nsVDXPixmap::kPixFormat_YUV420ib_Planar_709_FR:
		break;

	default:
		return FILTERPARAM_NOT_SUPPORTED;
	}

	fa->dst.mpPixmapLayout->pitch = fa->src.mpPixmapLayout->pitch;
	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVFNull::Run() {
}

extern const VDXFilterDefinition g_VDVFNull = VDXVideoFilterDefinition<VDVFNull>(
	NULL,
	"null transform",
	"Does nothing. Typically used as a placeholder for cropping.");
