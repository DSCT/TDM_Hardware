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

#include "camera.h"

class CCommandLine
{
public:
   CCommandLine(CCamera & camera);
   ~CCommandLine(void);

private:
   CCamera &         m_camera;
   DWORD             m_ThreadID;
   HANDLE            m_ThreadHandle;
   volatile uint8_t  m_ThreadStop;

   static DWORD WINAPI ThreadLoop(void * x);

   void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ");
   void stoupper(std::string& s) ;
   int32_t stoint(const std::string &s);

   enum COMMSACCESS_CMD {
      COMMSACCESS_CMD_NONE = 1,
      COMMSACCESS_CMD_CAMERA_READ,
      COMMSACCESS_CMD_CAMERA_WRITE,
      COMMSACCESS_CMD_FG_GET,
      COMMSACCESS_CMD_FG_SET
   };

};