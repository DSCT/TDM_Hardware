/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CommandLine.h"

#define PRINTF_AND_SWALLOW(x) \
   try {\
      x;\
   } catch (std::exception &e) {\
      printf("Error: %s\n", e.what());\
   } catch (...) {\
      printf("Unknwon Error\n");\
   }

CCommandLine::CCommandLine(CCamera & camera) :
  m_camera(camera)
, m_ThreadID(0)
, m_ThreadHandle(0)
, m_ThreadStop(0)
{
   m_ThreadHandle = CreateThread(NULL, 0, ThreadLoop,  this, 0, &m_ThreadID);
}

CCommandLine::~CCommandLine(void)
{
   m_ThreadStop = 1;
   WaitForSingleObject(m_ThreadHandle, 2500);
}

DWORD WINAPI CCommandLine::ThreadLoop(void * x)
{
   DWORD ret = 0;
   char cmdline[4096];
   std::vector<std::string> tokens;

   try {
      CCommandLine * This = reinterpret_cast<CCommandLine *>(x);

      if (!This->m_camera.isCameraLink()) {
         printf("\n\n");
         printf("Use this command line to access the hardware. Values read and written are 4\n"
                "bytes integers.\n");
         printf("Read camera register      'r <address>'\n");
         printf("Write camera register     'w <address> <data>'\n");
         printf("\nThe commands are not case sensitive.\n"
               "Data values prepended with '0x' are interpreted as hexadecimal numbers\n"
               "otherwise they are interpreted as decimal numbers.\n"
               "Addresses are always interpreted as hexadecimal numbers.\n");
         printf("\n");
      }

      while (!This->m_ThreadStop) {
         while (!_kbhit()) {
            Sleep(100);
            if (This->m_ThreadStop) break;
         }

         if (This->m_ThreadStop) continue;

         fgets(cmdline, sizeof(cmdline), stdin);
         cmdline[strlen(cmdline) - 1] = 0; // fgets() appends a 'newline character' before the terminating '\0' character. We need to get rid of it.

         tokens.clear();
         std::string s(cmdline);
         This->stoupper(s);
         This->Tokenize(s, tokens);

         if (!tokens.size()) continue;

         if (tokens.size() >= 2) {

            enum COMMSACCESS_CMD cmd;
            int32_t address = 0xBADB00B5, data = 0;

                 if (tokens[0] == std::string("W")) cmd = COMMSACCESS_CMD_CAMERA_WRITE;
            else if (tokens[0] == std::string("R")) cmd = COMMSACCESS_CMD_CAMERA_READ;
            else if (tokens[0] == std::string("S")) cmd = COMMSACCESS_CMD_FG_SET;
            else if (tokens[0] == std::string("G")) cmd = COMMSACCESS_CMD_FG_GET;
            else                                    cmd = COMMSACCESS_CMD_NONE;

            if (  ((cmd == COMMSACCESS_CMD_CAMERA_WRITE) || (cmd == COMMSACCESS_CMD_FG_SET)) &&
                  (tokens.size() < 3)) {
               printf("There is not enough parameters to make a valid 'write' command.\n");
               cmd = COMMSACCESS_CMD_NONE;            
            }

            if ((cmd == COMMSACCESS_CMD_CAMERA_WRITE || cmd == COMMSACCESS_CMD_CAMERA_READ) && This->m_camera.isCameraLink()) {
               cmd = COMMSACCESS_CMD_NONE;
            }

            if (cmd != COMMSACCESS_CMD_NONE) {               

               std::string hex = tokens[1];
               size_t pos = hex.find("0X");
               if ((pos == std::string::npos) && (hex.size())) hex = std::string("0X") + hex;

               address = This->stoint(hex);
               if ((cmd == COMMSACCESS_CMD_CAMERA_WRITE) || (cmd == COMMSACCESS_CMD_FG_SET)) {
                  data = This->stoint(tokens[2]);
               }

               switch (cmd) {
                  case COMMSACCESS_CMD_CAMERA_READ:
                     PRINTF_AND_SWALLOW(This->m_camera.deviceRead(address, data));
                     printf("Successfully read 0x%08X (%d) from camera address 0x%08X\n", data, data, address);
                     break;
                  case COMMSACCESS_CMD_CAMERA_WRITE:
                     PRINTF_AND_SWALLOW(This->m_camera.deviceWrite(address, data));
                     break;
                  case COMMSACCESS_CMD_FG_GET:
                     PRINTF_AND_SWALLOW(This->m_camera.hostReadWord(address, data));
                     printf("Successfully read 0x%08X (%d) from frame grabber address 0x%08X\n", data, data, address);
                     break;
                  case COMMSACCESS_CMD_FG_SET:
                     PRINTF_AND_SWALLOW(This->m_camera.hostWriteWord(address, data));
                     break;
                  default:
                     break;
               }
            }
         }
      }
   } catch (std::exception &e) {
      MessageBox(0, e.what(), "Error in"__FUNCTION__, MB_OK);
   }  catch (...) {
      MessageBox(0, "Unknown Exception", "Error in"__FUNCTION__, MB_OK);
   }
   
   return ret;
}

void CCommandLine::Tokenize(const std::string& str,
              std::vector<std::string>& tokens,
              const std::string& delimiters)
{
   // Skip delimiters at beginning.
   std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
   // Find first "non-delimiter".
   std::string::size_type pos = str.find_first_of(delimiters, lastPos);

   while (std::string::npos != pos || std::string::npos != lastPos) {
      // Found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      // Skip delimiters.  Note the "not_of"
      lastPos = str.find_first_not_of(delimiters, pos);
      // Find next "non-delimiter"
      pos = str.find_first_of(delimiters, lastPos);
   }
}

void CCommandLine::stoupper(std::string& s)  
{  
   std::string::iterator i   = s.begin();  
   std::string::iterator end = s.end();  
   while (i != end) {  
      *i = toupper((unsigned char)*i);  
      ++i;  
   }
}

int32_t CCommandLine::stoint(const std::string &s)
{
   int base = 10;
   if (s.find("0X") != std::string::npos) base = 16;
   return strtoul(s.c_str(), 0, base);
}
