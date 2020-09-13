/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "Display.h"
#include "..\common\adl_helper.h"
#include "..\common\stopwatch.h"

static ATOM    g_userDataUniqueID_atom = 0;
const char *   g_userDataUniqueID      = "ActiveSiliconDisplayAtom";

CDisplay::CDisplay(HWND hwnd) : 
  m_hwnd(hwnd)
, m_superWndProc(0)
, m_adlDisplay(0)
, m_adlTarget(0)
, m_fpsCount(0)
, m_fps(0.)
, m_bFitToDisplay(false)
{
   m_fpsLastPerfCounter.QuadPart = 0;

   if (!hwnd) THROW_ADL("Null window pointer.");

   InitializeCriticalSection(&m_cs);


   // Create and initialise ADL objects
   _ADL_THROW_ON_ERROR(ADL_DisplayCreate(&m_adlDisplay, ADL_IMPLEMENTATION_OPENGL));

   _ADL_THROW_ON_ERROR(m_adlDisplay->SetAttribute(ADL_DISPLAY_ATTRIBUTE_WINDOW, hwnd));
   _ADL_THROW_ON_ERROR(m_adlDisplay->Init());   
  
   _ADL_THROW_ON_ERROR(m_adlDisplay->TargetCreate(&m_adlTarget));
   _ADL_THROW_ON_ERROR(m_adlTarget->Init());

   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_STRETCH_TO_FIT_DISPLAY,   (void *)false));
   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_KEEP_ASPECT_RATIO,        (void *)false));

   RECT rect;
   GetClientRect(m_hwnd, &rect);

   ADL_RECT geometry;
   geometry.width  = rect.right  - rect.left;
   geometry.height = rect.bottom - rect.top;
   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_GEOMETRY, &geometry));

   // initialise member ROI to map 1/1 with the window
   m_roi.x         = 0;
   m_roi.y         = 0;
   m_roi.width     = rect.right  - rect.left;
   m_roi.height    = rect.bottom - rect.top;
   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, &m_roi));

   // initialise scroll bars
   SCROLLINFO si;
   si.cbSize   = sizeof(si);
   si.fMask    = SIF_ALL;
   si.nPos     = 0;
   si.nMin     = 0;
   si.nPage    = rect.right - rect.left;
   si.nMax     = si.nPage;
   //Don't display the horizontal scoll bar
   SetScrollInfo(m_hwnd, SB_HORZ, &si, true);

   si.nPage    = rect.bottom - rect.top;
   si.nMax     = si.nPage;
   //Don't display the vertical scoll bar
   SetScrollInfo(m_hwnd, SB_VERT, &si, true);

   // Subclass window - last as our window procedure will then be called AT ANY TIME (including now!)
   SetLastError(0);
   m_superWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OurWndProc));
   if (DWORD e = GetLastError()) throw std::runtime_error(decodeError(e));

   g_userDataUniqueID_atom = GlobalAddAtom(g_userDataUniqueID);
   SetProp(m_hwnd, MAKEINTRESOURCE(g_userDataUniqueID_atom), this);
}

CDisplay::~CDisplay(void)
{
   try {
      DeleteCriticalSection(&m_cs);

      RemoveProp(m_hwnd, MAKEINTRESOURCE(g_userDataUniqueID_atom));

      ALERT_AND_SWALLOW(ADL_DisplayDestroy(&m_adlDisplay, ADL_IMPLEMENTATION_OPENGL));

      SetLastError(0);
      SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_superWndProc));
      if (DWORD e = GetLastError()) throw std::runtime_error(decodeError(e));

   } catch (std::exception &e) {
      MessageBox(0, e.what(), "Error", MB_OK);
   } catch (...) {
      MessageBox(0, "Unknown exception", "Error", MB_OK);
   }
}

void CDisplay::calculatefps()
{
   if (!(++m_fpsCount%60)) {
      LARGE_INTEGER frequency;
      QueryPerformanceFrequency(&frequency);

      LARGE_INTEGER counter;
      QueryPerformanceCounter(&counter);
      double seconds       = (counter.QuadPart - m_fpsLastPerfCounter.QuadPart) * 1.0 / (frequency.QuadPart);
      m_fpsLastPerfCounter = counter;
      m_fps                = seconds ? m_fpsCount / seconds : 0.;
      m_fpsCount           = 0;
   }
}

LRESULT CALLBACK CDisplay::OurWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   LRESULT lRet = false;

   try {
      CDisplay * This = reinterpret_cast<CDisplay *>(GetProp(hwnd, MAKEINTRESOURCE(g_userDataUniqueID_atom)));

      if (!This) {
         if (DWORD e = GetLastError()) throw std::runtime_error(decodeError(e));
      } else {         
	      switch (msg) {
	         case WM_PAINT:
            {
               if (IsIconic(hwnd)) break;
               CDisplay::CLock lock(This->m_cs);
               if (This->m_adlDisplay) _ADL_THROW_ON_ERROR(This->m_adlDisplay->Show());
               This->calculatefps();
            } break;

		         case WM_SIZE:
		         {
                  if (IsIconic(hwnd)) break;

                  if (!This->m_adlTarget) break;

                  ADL_RECT geometry;
                  IADL_Image * image = 0;
                  uint32_t width, height;
                  
                  _ADL_THROW_ON_ERROR(This->m_adlTarget->GetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, &image));            
                  if (!image) break;

                  _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH,   &width));
                  _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT,  &height));

                  geometry.width  = This->m_bFitToDisplay ? width  : min(LOWORD (lParam), width);
                  geometry.height = This->m_bFitToDisplay ? height : min(HIWORD (lParam), height);

                  _ADL_THROW_ON_ERROR(This->m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_GEOMETRY, &geometry));

                  {
                     CDisplay::CLock lock(This->m_cs);

                     if (!This->m_bFitToDisplay) {
                        This->m_roi.width  = min(LOWORD (lParam), width);
                        This->m_roi.height = min(HIWORD (lParam), height);
                     } else {
                        This->m_roi.x = 0;
                        This->m_roi.y = 0;
                     }

                     // keep roi within the image
                     int x = width  - (This->m_roi.x + This->m_roi.width);
                     int y = height - (This->m_roi.y + This->m_roi.height);
                     if (x < 0) This->m_roi.x += x;
                     if (y < 0) This->m_roi.y += y;

                     _ADL_THROW_ON_ERROR(This->m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, &This->m_roi));
                  }

                  if (!This->m_bFitToDisplay) {
                     // we need to set scroll info after changing ROI as SetScrollInfo might emit a WM_SIZE message
                     SCROLLINFO si;
                     si.cbSize = sizeof(si);

                     si.fMask = SIF_ALL;
                     GetScrollInfo(hwnd, SB_HORZ, &si);
                     si.fMask = SIF_PAGE;
                     si.nPage = LOWORD (lParam);
                     SetScrollInfo(hwnd, SB_HORZ, &si, true);

                     si.fMask = SIF_ALL;
                     GetScrollInfo(hwnd, SB_VERT, &si);
                     si.fMask = SIF_PAGE;
                     si.nPage = HIWORD (lParam);
                     SetScrollInfo(hwnd, SB_VERT, &si, true);
                  }
		         } break;

		      case WM_HSCROLL:
		      {
               if (!This->m_adlTarget) break;

               SCROLLINFO si;
               si.cbSize = sizeof(si); 
               si.fMask  = SIF_ALL;
               GetScrollInfo(hwnd, SB_HORZ, &si);

               int pos = si.nPos;

               switch (LOWORD(wParam)) {
                  case SB_LINERIGHT:
                     pos += 1;
                     break;
                  case SB_PAGERIGHT:
                     pos += si.nPage;
                     break; 
                  case SB_LINELEFT:
                     pos -= 1;
                     break;
                  case SB_PAGELEFT:
                     pos -= si.nPage;
                     break;
                  case SB_THUMBTRACK:
                      pos = HIWORD(wParam);
                      break;
                  default:
                     break;
               }

               // Range check the new position
               pos = max(si.nMin, pos);
               pos = min(si.nMax - static_cast<int>(si.nPage), pos);

               si.fMask = SIF_POS;
               si.nPos  = pos;
               SetScrollInfo(hwnd, SB_HORZ, &si, true);               

               IADL_Image * image = 0;
               int32_t width, height;
               
               _ADL_THROW_ON_ERROR(This->m_adlTarget->GetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, &image));            
               if (!image) break;

               _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH, &width));
               _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT, &height));

               {
                  CDisplay::CLock lock(This->m_cs);

                  This->m_roi.x = pos;
                  if (This->m_roi.x + This->m_roi.width > width) {
                     This->m_roi.x = 0;
                  }

                  _ADL_THROW_ON_ERROR(This->m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, &This->m_roi));
               }

               InvalidateRect(hwnd, 0, false);
		      }
		      break; 

		      case WM_VSCROLL:
		      {
               if (!This->m_adlTarget) break;

               SCROLLINFO si;
               si.cbSize = sizeof(si); 
               si.fMask  = SIF_ALL;
               GetScrollInfo(hwnd, SB_VERT, &si);

               int pos = si.nPos;

               switch (LOWORD(wParam)) {
                  case SB_LINEUP:
                     pos -= 1;
                     break;
                  case SB_PAGEUP:
                     pos -= si.nPage;
                     break; 
                  case SB_LINEDOWN:
                     pos += 1;
                     break;
                  case SB_PAGEDOWN:
                     pos += si.nPage;
                     break; 
                  case SB_THUMBTRACK:
                     pos = HIWORD(wParam);
                     break;
                  default:
                     break;
               }

               // Range check the new position
               pos = max(si.nMin, pos); 
               pos = min(si.nMax - static_cast<int>(si.nPage), pos); 

               si.fMask = SIF_POS;
               si.nPos  = pos;
               SetScrollInfo(hwnd, SB_VERT, &si, true);

               IADL_Image * image = 0;
               int32_t width, height;
               
               _ADL_THROW_ON_ERROR(This->m_adlTarget->GetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, &image));            
               if (!image) break;

               _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH,  &width));
               _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT, &height));

               {
                  CDisplay::CLock lock(This->m_cs);
                  This->m_roi.y = pos;
                  if (This->m_roi.y + This->m_roi.height > height) {
                     This->m_roi.y = 0;
                  }
                  _ADL_THROW_ON_ERROR(This->m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, &This->m_roi));
               }

               InvalidateRect(hwnd, 0, false);
		      }
		      break; 
            default: break;
	      }

         lRet = CallWindowProc(This->m_superWndProc, hwnd, msg, wParam, lParam);
      }
   } catch (std::exception & e) {
      printf("Error in %s: %s", __FUNCTION__, e.what());
   } catch (...) {
      printf("Error in %s: Unknown exception", __FUNCTION__);
   }

   return lRet;
}

std::string CDisplay::decodeError(DWORD dw)
{
    LPVOID lpMsgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    std::string s(static_cast<char *>(lpMsgBuf));

    LocalFree(lpMsgBuf);
    return s;
 }

void CDisplay::setImage(IADL_Image * image)
{  
   if (!image) {
	   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, image));
	   return;
   }
	
   if (!m_adlDisplay || !m_adlTarget || !image) return;

   int32_t width, height;
   _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH, &width));
   _ADL_THROW_ON_ERROR(image->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT, &height));

   // Deal with a new image size: reset ROI to fit window and start at (0, 0)
   IADL_Image * currentImage = 0;
   int32_t currentWidth = 0, currentHeight = 0;
   _ADL_THROW_ON_ERROR(m_adlTarget->GetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, &currentImage));

   {
      CDisplay::CLock lock(m_cs);
      if (currentImage) {   
         _ADL_THROW_ON_ERROR(currentImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH, &currentWidth));
         _ADL_THROW_ON_ERROR(currentImage->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT, &currentHeight));
         if (currentWidth != width || currentHeight != height) {
            RECT rect;
            GetClientRect(m_hwnd, &rect);
            CDisplay::CLock lock(m_cs);
            m_roi.x      = 0;
            m_roi.y      = 0;
            m_roi.width  = rect.right  - rect.left;
            m_roi.height = rect.bottom - rect.top;
         }
      }

      if (m_bFitToDisplay) {
         m_roi.width  = width;
         m_roi.height = height;
      }

      _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_ROI, &m_roi));
   }

   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, image));

   if (currentWidth != width || currentHeight != height) {
      SCROLLINFO psi;
      psi.cbSize = sizeof(psi);
      psi.fMask  = SIF_RANGE;
      psi.nMin   = 0;

      psi.nMax = width;

      SetScrollInfo(m_hwnd, SB_HORZ, &psi, true);

      psi.nMax = height;
      SetScrollInfo(m_hwnd, SB_VERT, &psi, true);
   }
}

void CDisplay::fitToDisplay(bool bFit)
{
   m_bFitToDisplay = bFit;
   SCROLLINFO psi;
   psi.cbSize = sizeof(psi);
   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_STRETCH_TO_FIT_DISPLAY, (void *)bFit));
   _ADL_THROW_ON_ERROR(m_adlTarget->SetAttribute(ADL_TARGET_ATTRIBUTE_KEEP_ASPECT_RATIO, (void *)bFit));

   // Set scrollbars
   int32_t width = m_roi.width;
   int32_t height = m_roi.height;
   psi.fMask  = SIF_ALL;
   psi.nPos   = 0;
   psi.nMin   = 0;

   psi.nPage  = bFit ? 0:width;
   psi.nMax   = psi.nPage;
   SetScrollInfo(m_hwnd, SB_HORZ, &psi, true);

   psi.nPage  = bFit ? 0:height;
   psi.nMax   = psi.nPage;
   SetScrollInfo(m_hwnd, SB_VERT, &psi, true);
}

std::string CDisplay::msgComposer(const char* pFormat, ...)
{
   char pBuffer[512];
   va_list vap;
   va_start(vap, pFormat);
#ifdef _WIN32
   _vsnprintf_s(pBuffer, sizeof(pBuffer), pFormat, vap);
#else
   vsnprintf(pBuffer, sizeof(pBuffer), pFormat, vap);
#endif
   return std::string(pBuffer);
};
