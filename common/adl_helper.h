/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#pragma once

#include "adl.h"
#include "tmg2_api.h"

// Convert a TMG image object to an ADL image object
// The image data is not copied.
err_t ImageFromTMG (IADL_Image ** pImage, ui32 TMG_hImage);

// Converts and ADL image object into a TMG image.
// The image data is not copied.
err_t ADLToTMG(IADL_Image * pImage, int TMG_hImage);

// Translates the target T horizontally and vertically by x and y pixels
err_t translate(IADL_Target * T, int x, int y);

// Translates all targets in T[] horizontally and vertically by x and y
// pixels
err_t translate(IADL_Target ** T, int x, int y);

err_t translate(IADL_Display *pD, IADL_Target * pT, int x, int y);

err_t translateROI(IADL_Target * T, int x, int y);

ui32 BytesPerPixel (ADL_PIXEL_FORMAT ADL_Format);
err_t TMG_to_ADL_PixelFormat (ui32 TMG_Format, ADL_PIXEL_FORMAT &ADL_Format);
err_t ADL_to_TMG_PixelFormat (ADL_PIXEL_FORMAT ADL_Format, ui32 &TMG_Format);

#define _Roe(Function) \
{\
   ui32 _dwStatus;\
   _dwStatus = Function;\
   if (ASL_FAILED(_dwStatus)) return(_dwStatus);\
}

#define _ADL_SHOW_AND_ROE2(Function, szMsg) \
{\
   err_t _dwStatus;\
   _dwStatus = Function;\
   if (ASL_FAILED(_dwStatus)) {\
      size_t size;\
      ADL_GetLastError(&_dwStatus, &size, 0);\
      char *pString = new char [size];\
      ADL_GetLastError(&_dwStatus, &size, pString);\
      size_t s = 256 + strlen(szMsg) + size;\
      char *pc = new char [s];\
      sprintf_s(pc, s, "%s\n%s\nFunction:%s\nFile:%s\nLine:%d", szMsg, pString, __FUNCTION__, __FILE__, __LINE__);\
      MessageBox(0, pc, "Error", MB_OK);\
      delete [] pc;\
      delete [] pString;\
      return(_dwStatus);\
   }\
}

#define _ADL_SHOW_AND_ROE1(Function) \
{\
   _ADL_SHOW_AND_ROE2(Function, "There has been an error.");\
}

#define _TMG_SHOW_AND_ROE(Function, szMsg) \
{\
   ui32 _dwStatus;\
   _dwStatus = Function;\
   if (ASL_FAILED(_dwStatus)) {\
      char *pc = new char [256 + strlen(szMsg)];\
      sprintf(pc, "%s\nFile:%s\nLine:%d"szMsg, __FILE__, __LINE__);\
      TMG_ErrProcess(1, __FUNCTION__, _dwStatus, pc);\
      delete [] pc;\
      return(_dwStatus);\
   }\
}

