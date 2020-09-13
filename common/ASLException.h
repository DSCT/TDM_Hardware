/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#ifndef CASLEXCEPTION_H
#define CASLEXCEPTION_H

#include <iostream>
#include <sstream>
#include <string> 

namespace ActiveSilicon {

   class CASLException : public std::exception
   {
   public:
      CASLException(std::string & description, const  char *functionName, const char *sourceFileName, unsigned int sourceLine) :
      std::exception()
      , m_FunctionName(functionName)
      , m_SourceLine(sourceLine)
      , m_SourceFileName(sourceFileName)
      , m_Description(description.c_str())
      {
         makeMessage();
      }

      CASLException(const char* description, const  char *functionName, const char *sourceFileName, unsigned int sourceLine) :
      std::exception()
      , m_FunctionName(functionName)
      , m_SourceLine(sourceLine)
      , m_SourceFileName(sourceFileName)
      , m_Description(description)
      {
         makeMessage();
      }

      CASLException(const char* description, const  char *functionName, const char *sourceFileName, unsigned int sourceLine, const char *exceptionType) :
      std::exception()
      , m_FunctionName(functionName)
      , m_SourceLine(sourceLine)
      , m_SourceFileName(sourceFileName)
      , m_Description(description)
      , m_ExceptionType(exceptionType)
      {
         makeMessage();
      }

      virtual ~CASLException() throw()
      {
      }

      virtual const char* GetDescription() const throw()
      {
         return m_Description.c_str();
      }

      virtual const char* GetFunctionName() const throw()
      {
         return m_FunctionName.c_str();
      }

      virtual const char* GetSourceFileName() const throw()
      {
         return m_SourceFileName.c_str();
      }

      virtual unsigned int GetSourceLine() const throw()
      {
         return m_SourceLine;
      }

      virtual const char *what() const throw()
      {
         return m_What.c_str();
      }

      static std::string msgComposer(const char* pFormat, ...)
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

   private:
      std::string  m_FunctionName;
      unsigned int m_SourceLine;
      std::string  m_SourceFileName;
      std::string  m_Description;
      std::string  m_ExceptionType;
      std::string  m_What; //Complete error message, including file name and line number

      void makeMessage()
      {
         // extract the file name from the path
         std::string Buffer( m_SourceFileName );
         size_t found = Buffer.find_last_of("/\\");
         std::string ShortFileName = Buffer.substr(found+1);

         // assemble the error message
         std::stringstream Message;

         Message << m_Description;

         if(!m_ExceptionType.empty())
         Message << " : " << m_ExceptionType << " thrown";

         Message << " (function '" << m_FunctionName << "()', file '" << ShortFileName << "', line " << m_SourceLine << ")";

         m_What = Message.str().c_str();
      }
   };

   #define DECLARE_AND_DEFINE_EXCEPTION( name ) \
   class name : public ActiveSilicon::CASLException \
   { \
   public: \
      name(std::string & description, const char *functionName, const char *sourceFileName, unsigned int sourceLine) \
          : ActiveSilicon::CASLException(description, functionName, sourceFileName, sourceLine) \
      { \
      } \
      name(const char* description, const char *functionName, const char *sourceFileName, unsigned int sourceLine) \
          : ActiveSilicon::CASLException(description, functionName, sourceFileName, sourceLine) \
      { \
      } \
      name(const char* description, const char *functionName, const char *sourceFileName, unsigned int sourceLine, const char *exceptionType) \
          : ActiveSilicon::CASLException(description, functionName, sourceFileName, sourceLine, exceptionType) \
      { \
      } \
   };

   DECLARE_AND_DEFINE_EXCEPTION(InvalidEnumException);
   DECLARE_AND_DEFINE_EXCEPTION(OutOfRangeException);
   DECLARE_AND_DEFINE_EXCEPTION(InvalidOperationException);
   DECLARE_AND_DEFINE_EXCEPTION(OutOfMemoryException);
   DECLARE_AND_DEFINE_EXCEPTION(NotSupportedException);
   DECLARE_AND_DEFINE_EXCEPTION(DynamicCastException);
   DECLARE_AND_DEFINE_EXCEPTION(RuntimeException);
}

   #define THROW_INVALID_ENUM(x)       throw ActiveSilicon::InvalidEnumException       ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_OUT_OF_RANGE_VALUE(x) throw ActiveSilicon::OutOfRangeException        ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_INVALID_OPERATION(x)  throw ActiveSilicon::InvalidOperationException  ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_OUT_OF_MEMORY(x)      throw ActiveSilicon::OutOfMemoryException       ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_NOT_SUPPORTED(x)      throw ActiveSilicon::NotSupportedException      ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_DYNAMIC_CAST(x)       throw ActiveSilicon::DynamicCastException       ((x), __FUNCTION__, __FILE__, __LINE__)
   #define THROW_RUNTIME(x)            throw ActiveSilicon::RuntimeException           ((x), __FUNCTION__, __FILE__, __LINE__)

#endif // CASLEXCEPTION_H
