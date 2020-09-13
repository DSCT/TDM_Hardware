/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "adl_helper.h"

err_t ImageFromTMG (IADL_Image ** ppImage, ui32 TMG_hImage)
{
   if (!*ppImage) {
      _Roe(ADL_ImageCreate(ppImage));
   }

   ui32 dwPixelFormat;
   ui32 dwSourceWidth;
   ui32 dwSourceHeight;
   ui32 dwBytesPerLine;
   ui8 *pbImageBuffer;

   dwBytesPerLine = TMG_image_get_parameter  (TMG_hImage, TMG_BYTES_PER_LINE);
   dwPixelFormat  = TMG_image_get_parameter  (TMG_hImage, TMG_PIXEL_FORMAT);
   dwSourceWidth  = TMG_image_get_parameter  (TMG_hImage, TMG_WIDTH);
   dwSourceHeight = TMG_image_get_parameter  (TMG_hImage, TMG_HEIGHT);
   pbImageBuffer  = (ui8 *)TMG_image_get_ptr (TMG_hImage, TMG_IMAGE_DATA);

   _Roe((*ppImage)->SetAttribute(ADL_IMAGE_ATTRIBUTE_BITS,          (void *)pbImageBuffer));
   _Roe((*ppImage)->SetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH,         (void *)dwSourceWidth));
   _Roe((*ppImage)->SetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT,        (void *)dwSourceHeight));

   ADL_PIXEL_FORMAT format;

   _ADL_SHOW_AND_ROE2(TMG_to_ADL_PixelFormat(dwPixelFormat, format), "Unknown pixel format.");

   _Roe((*ppImage)->SetAttribute(ADL_IMAGE_ATTRIBUTE_PIXELFORMAT,   (void *)format));

   if (!BytesPerPixel(format)) _ADL_SHOW_AND_ROE2(ASL_OUT_OF_RANGE_VALUE, "Unknown pixel format.");
   ui32 stride = dwBytesPerLine - (dwSourceWidth * BytesPerPixel(format));

   _Roe((*ppImage)->SetAttribute(ADL_IMAGE_ATTRIBUTE_STRIDE,        (void *)stride));

   _Roe((*ppImage)->Init());

   return ASL_NO_ERROR;
}

err_t ADLToTMG(IADL_Image * pImage, int TMG_hImage)
{
   if (!pImage) return ASL_NO_ERROR;

   enum ADL_PIXEL_FORMAT PixelFormat;
   ui8 * pbBits;
   ui32 dwWidth;
   ui32 dwHeight;
   ui32 dwBytesPerLine;
   ui32 dwStride;
   ui32 dwNumBytesData;

   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_PIXELFORMAT,     &PixelFormat));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_BITS,            &pbBits));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH,           &dwWidth));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT,          &dwHeight));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_STRIDE,          &dwStride));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_BYTES_PER_LINE,  &dwBytesPerLine));
   _Roe(pImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_NUM_BYTES_DATA,  &dwNumBytesData));

   if (!TMG_hImage) {
      if (TMG_Image_Create(&TMG_hImage, 0, NULL) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_Create().");
   }

   if (TMG_Image_ParameterSet(TMG_hImage, TMG_IMAGE_WIDTH,  dwWidth)  != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_ParameterSet().");
   if (TMG_Image_ParameterSet(TMG_hImage, TMG_IMAGE_HEIGHT, dwHeight) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_ParameterSet().");

   ui32 dwFormat;
   _Roe(ADL_to_TMG_PixelFormat(PixelFormat, dwFormat));
   if (TMG_Image_ParameterSet(TMG_hImage, TMG_PIXEL_FORMAT, dwFormat) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_ParameterSet().");

   if (TMG_Image_ParameterPtrSet(TMG_hImage, TMG_IMAGE_BUFFER, (void *)pbBits) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_ParameterSet().");

   if (TMG_Image_ParameterSet(TMG_hImage, TMG_ALIGNMENT_LINE, dwBytesPerLine)  != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_Image_ParameterSet().");

   if (TMG_image_set_flags(TMG_hImage, TMG_USER_ALLOCATED_MEM, TRUE) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_image_set_flags().");
   if (TMG_image_set_flags(TMG_hImage, TMG_LOCKED, TRUE) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_image_set_flags().");

   if (TMG_image_check(TMG_hImage) != TMG_OK) _ADL_SHOW_AND_ROE2(ASL_RUNTIME_ERROR, "Error calling TMG_image_check().");

   return ASL_NO_ERROR;
}

ui32 BytesPerPixel (ADL_PIXEL_FORMAT ADL_Format)
{
   switch(ADL_Format) {
   case ADL_PIXEL_FORMAT_YUV422: return 2;
   case ADL_PIXEL_FORMAT_Y8:     return 1;
   case ADL_PIXEL_FORMAT_Y16:    return 2;
   case ADL_PIXEL_FORMAT_RGBX32:
   case ADL_PIXEL_FORMAT_BGRX32:
   case ADL_PIXEL_FORMAT_XBGR32:
   case ADL_PIXEL_FORMAT_XRGB32: return 4;
   case ADL_PIXEL_FORMAT_RGB24:
   case ADL_PIXEL_FORMAT_BGR24:  return 3;
   default:                      return 0;
   }
}

err_t TMG_to_ADL_PixelFormat (ui32 TMG_Format, ADL_PIXEL_FORMAT &ADL_Format)
{
   static const char *szFnName = __FUNCTION__;

   switch (TMG_Format) {
      case TMG_RGB24:  ADL_Format = ADL_PIXEL_FORMAT_RGB24;    break;
      case TMG_BGR24:  ADL_Format = ADL_PIXEL_FORMAT_BGR24;    break;
      case TMG_RGBX32: ADL_Format = ADL_PIXEL_FORMAT_RGBX32;   break;
      case TMG_BGRX32: ADL_Format = ADL_PIXEL_FORMAT_BGRX32;   break;
      case TMG_XBGR32: ADL_Format = ADL_PIXEL_FORMAT_XBGR32;   break;
      case TMG_XRGB32: ADL_Format = ADL_PIXEL_FORMAT_XRGB32;   break;
      case TMG_YUV422: ADL_Format = ADL_PIXEL_FORMAT_YUV422;   break;
      case TMG_Y8:     ADL_Format = ADL_PIXEL_FORMAT_Y8;       break;
      case TMG_Y16:    ADL_Format = ADL_PIXEL_FORMAT_Y16;      break;
      default: return ASL_OUT_OF_RANGE_VALUE;
   }
   return ASL_NO_ERROR;
}

err_t ADL_to_TMG_PixelFormat (ADL_PIXEL_FORMAT ADL_Format, ui32 &TMG_Format)
{
   static const char *szFnName = __FUNCTION__;

   switch (ADL_Format) {
      case ADL_PIXEL_FORMAT_RGB24:  TMG_Format = TMG_RGB24;    break;
      case ADL_PIXEL_FORMAT_BGR24:  TMG_Format = TMG_BGR24;    break;
      case ADL_PIXEL_FORMAT_RGBX32: TMG_Format = TMG_RGBX32;   break;
      case ADL_PIXEL_FORMAT_BGRX32: TMG_Format = TMG_BGRX32;   break;
      case ADL_PIXEL_FORMAT_XBGR32: TMG_Format = TMG_XBGR32;   break;
      case ADL_PIXEL_FORMAT_XRGB32: TMG_Format = TMG_XRGB32;   break;
      case ADL_PIXEL_FORMAT_YUV422: TMG_Format = TMG_YUV422;   break;
      case ADL_PIXEL_FORMAT_Y8:     TMG_Format = TMG_Y8;       break;
      case ADL_PIXEL_FORMAT_Y16:    TMG_Format = TMG_Y16;      break;
      default: return ASL_OUT_OF_RANGE_VALUE;
   }
   return ASL_NO_ERROR;
}

err_t translate(IADL_Display *pD, IADL_Target * pT, int x, int y)
{
   ADL_RECT geometry;
   ADL_RECT DisplayRect;
   ADL_POINT origin;
   HWND hWnd;

   if (!pD) return ASL_NO_ERROR;
   if (!pT) return ASL_NO_ERROR;   

   _Roe(pT->GetAttribute(ADL_TARGET_ATTRIBUTE_LOCATION, (void *)&origin));
   _Roe(pT->GetAttribute(ADL_TARGET_ATTRIBUTE_GEOMETRY, (void *)&geometry));
   _Roe(pD->GetAttribute(ADL_DISPLAY_ATTRIBUTE_WINDOW, (void *)&hWnd));

   RECT r;
   GetClientRect(hWnd, &r);
   DisplayRect.x        = r.left;
   DisplayRect.y        = r.top;
   DisplayRect.width    = abs(r.right - r.left);
   DisplayRect.height   = abs(r.top - r.bottom);

   if (x<0) if (origin.x + x < DisplayRect.x) x = 0;
   if (x>0) if (origin.x + geometry.width + x > DisplayRect.x + DisplayRect.width)  x = 0;

   if (y>0) if (origin.y + y > DisplayRect.y) y = 0;
   if (y<0) if (origin.y + geometry.width     < DisplayRect.y + DisplayRect.height) y = 0;

   origin.x += x;
   origin.y += y;
   _Roe(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_LOCATION, (void *)&origin));
   return ASL_NO_ERROR;
}

err_t translateROI(IADL_Target * pT, int x, int y)
{
   ADL_RECT r;
   if (!pT) return ASL_NO_ERROR;   
   _Roe(pT->GetAttribute(ADL_TARGET_ATTRIBUTE_ROI, (void *)&r));     
   r.x += x;
   r.y += y;
   _Roe(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, (void *)&r));
   return ASL_NO_ERROR;
}

err_t translate(IADL_Target * T, int x, int y)
{
   ADL_POINT origin;
   if (!T) return ASL_NO_ERROR;   
   _Roe(T->GetAttribute(ADL_TARGET_ATTRIBUTE_LOCATION, (void *)&origin));     
   origin.x += x;
   origin.y += y;
   _Roe(T->SetAttribute(ADL_TARGET_ATTRIBUTE_LOCATION, (void *)&origin));
   return ASL_NO_ERROR;
}

err_t translate(IADL_Target ** T, int x, int y)
{
   for (int i=0; i<(sizeof(T)/sizeof(IADL_Target *)); i++) {
      _Roe(translate(T[i], x, y));
   }
   return ASL_NO_ERROR;
}
