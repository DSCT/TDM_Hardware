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

#include <windows.h>
#include <winuser.h>
#include <string>

#include <adl.h>
#include <tmg2_api.h>

class CDisplay
{
public:
   CDisplay                   (HWND);
   ~CDisplay                  (void);

   void setImage              (IADL_Image *);
   void fitToDisplay          (bool bFit);
   bool isStretched           () { return m_bFitToDisplay; }
   IADL_Display * adlDisplay  () { return m_adlDisplay; }
   double fps                 () { return m_fps; }

   static std::string msgComposer(const char* pFormat, ...);

private:
   HWND m_hwnd;
   WNDPROC m_superWndProc;

   IADL_Display *    m_adlDisplay;
   IADL_Target  *    m_adlTarget;

   bool              m_bFitToDisplay;

   ADL_RECT          m_roi;
   CRITICAL_SECTION  m_cs;

   uint32_t          m_fpsCount;
   double            m_fps;
   LARGE_INTEGER     m_fpsLastPerfCounter;

   static LRESULT CALLBACK OurWndProc     (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
   static std::string      decodeError    (DWORD e);
   void                    calculatefps   ();
   

   class CLock
   {
   public:
      explicit CLock (CRITICAL_SECTION &cs)  : m_cs(cs)  { EnterCriticalSection(&m_cs); }
      ~CLock         (void)                              { LeaveCriticalSection(&m_cs); }
   private:
      CRITICAL_SECTION &m_cs;
   };

};

#include <sstream>

#define THROW_ADL(msg) \
{\
   std::stringstream ss;\
   ss << "ADL Error: " << msg << "file: "__FILE__ " function: "__FUNCTION__ " line: " << std::dec << __LINE__;\
   throw std::runtime_error(ss.str());\
}

#define _ADL_THROW_ON_ERROR(Function) \
{\
   err_t _dwStatus;\
   _dwStatus = Function;\
   if (ASL_FAILED(_dwStatus)) {\
      size_t size;\
      ADL_GetLastError(&_dwStatus, &size, 0);\
      char *pString = new char [size];\
      ADL_GetLastError(&_dwStatus, &size, pString);\
      std::string error_msg (pString);\
      delete [] pString;\
      THROW_ADL(error_msg);\
   }\
}

#define ALERT_AND_SWALLOW(x) \
   try {\
      x;\
   } catch (std::exception &e) {\
      MessageBox(0, e.what(), "Error", MB_OK);\
   } catch (...) {\
      MessageBox(0, "Unknown exception", "Error", MB_OK);\
   }