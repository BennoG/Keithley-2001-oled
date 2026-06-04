#include <string.h>

#ifndef uint8_t
  typedef unsigned char uint8_t;
#endif

namespace ansStl
{
    class cST
    {
    public:
        cST() { initVars();  };
        ~cST() {};
        int length();
        void set(const cST *val);
        void set(const cST& val);
        void set(int len,const char *p);
        void set(const char *p){ set(-1, p); }
        void setf(const char *fmt,...);
        void append(char ch);
        //void append(const char *p);
        void append(const char *fmt,...);
        char& operator[](int iIdx);
		char& get(int iIdx);
        const char *buf(){ return sBuf; }
        bool compare(const cST& val);
        bool compare(const char *val);
        bool comparei(const char *val);
        void insDel(int pos,int len,char chFill = 0);
        cST substr(int iStart, int iLength);
        void clear();

    private:
    #define _MaxCLen_ 200
        void initVars(){ iLen = 0; memset(sBuf,0,_MaxCLen_); cDummy = 0; }
        int iLen;
        char sBuf[_MaxCLen_];
        char cDummy;
    };
     
}
