
#include "cST.h"
#include <cstdlib>
#include <stdarg.h>
#include <stdio.h>

using namespace ansStl;

int cST::length() 
{
    return iLen;
}

void cST::set(const cST *val)
{
    if (val == NULL) return;
    set(val->iLen,val->sBuf);
}

void cST::set(const cST &val)
{
    set(&val);
}

void cST::set(int len,const char *p)
{
    if ((len <= 0) && (p != NULL)) len=(int)strlen(p);
    if (len + 5 > _MaxCLen_) return;        // silent fail if string is too long
    //if (len + 5 > _MaxCLen_) throw "maximum alloc size reached";
    iLen = len;
    if (iLen < 0) iLen = 0;
    if (p != sBuf)		// so we don't copy to ourselve
    {
        if ((p) && (iLen > 0)) memcpy(sBuf, p, iLen * sizeof(char));
        else   memset(sBuf, 0, _MaxCLen_ * sizeof(char));
    }
    sBuf[iLen] = 0;
}

void cST::setf(const char *fmt,...)
{
	if (fmt == NULL) return;
	va_list args;
	va_start(args, fmt);
	int iRes = vsnprintf(sBuf, _MaxCLen_ - 5, fmt, args);
	va_end(args);
	if (iRes < 0) return;   // encoding error
	iLen = iRes;
	sBuf[iLen] = 0;
}

void cST::append(char ch)
{
    if (iLen + 5 + 1 > _MaxCLen_) return;      // silent fail if data is too long
    //if (iLen + 5 + 1 > _MaxCLen_) throw "maximum alloc size reached";
    sBuf[iLen] = ch;
    iLen++;
    sBuf[iLen] = 0;
}

void cST::insDel(int pos,int len,char chFill /* = 0 */)
{
 	if (pos < 0) pos = iLen + pos;	// zodat -1 laatste teken word -2 op 1 na laatste
	if ((pos < 0) || (pos > iLen)) return;
	if (len == 0) return;
	if (len < 0)	// delete
	{
		len = -len;			// number of bytes to remove
		if (pos + len > iLen) len = iLen - pos;
		int iStartSrc = pos + len;
		int iMvLen = iLen - iStartSrc;	// number of bytes to move (if any)
		if (iMvLen > 0)	memmove(sBuf + pos , sBuf + iStartSrc, iMvLen );
		iLen -= len;
		if (iLen < 0) iLen = 0;
	}
	else			// insert
	{
		if (iLen + len > _MaxCLen_) return;      // silent fail if data is too long
		//if (iLen + len > _MaxCLen_) throw "maximum alloc size reached";
		int iMvLen = iLen - pos;
		if (iMvLen > 0) memmove(sBuf + pos + len, sBuf + pos, iMvLen);
		memset(sBuf + pos, chFill, len);
		iLen += len;
	}
	sBuf[iLen] = 0;
}

cST cST::substr(int iStart, int iLength)
{
 	if (iStart < 0) iStart = iLen + iStart;
	if (iStart < 0) iStart = 0;
	if (iLength < 0) iLength = iLen - iStart;		// all remaining data
	if (iLength + iStart > iLen) iLength = iLen - iStart;
	if (iLength < 0) iLength = 0;
	cST res;
	if (iLength > 0)
		res.set(iLength, sBuf + iStart);
	else
        res.clear();
	return res;
}

/* INTERNAL */
// Sets start and end pos of search character (from begin or end)
int cST::_strTlGetOfs(char sSrc,int iCnt,int *pStart,int *pEind)
{
	char *pBuf = sBuf;
	int iStart=*pStart,iEind=*pEind;
	if (iCnt==0) return 0;		// We want it all return immediately
	if (iCnt>0){
		pBuf+=iStart;			// Set buffer pointer to start from where to search
		while (iStart<iEind){
			iStart++;
			if (*pBuf++==sSrc){
				iCnt--;
				if (iCnt==1) *pStart=iStart;
				else if (iCnt==0){
					if (pEind) *pEind=iStart-1;
					return 0;
				}
			}
		}
		if (iCnt>1) *pStart=*pEind;	// not enough delimitrs fouund set start to end pos and return missing delimiter cou
		return (iCnt-1);
	}
	// Search backwards (from the end of the data)
	pBuf+=iEind-1;					// Don't bother if pointer becomes before actual data
	if (iCnt==-1){					// We want only the data beyond the end
		*pStart=*pEind;
		if (iEind>iStart) return 1;	// Data is avail (so store needs extra delimiter)
		return 0;
	}
	while (iEind>iStart){
		iEind--;
		if (*pBuf--==sSrc){
			iCnt++;
			if (iCnt==-2) *pEind=iEind;
			else if (iCnt==-1){
				if (pStart) *pStart=iEind+1;
				return 0;
			}
		}
	}
	if (iCnt<-2) *pEind=*pStart;	// We wanted further back than te beginning.
	return (iCnt+2);				// Signal it back to caller
}


/* Get value with user delimiter
 *  iD1  index to get (neg count from the back)
 *  cDlm character to use as delimiter
 * Return SubPart of STP
*/
cST cST::getDlm(int iD1,char cDlm)
{
	cST res;
	int iEind  = iLen;
	int iStart = 0;
	_strTlGetOfs(cDlm,iD1,&iStart,&iEind);
	int len = iEind - iStart;
	if (len > 0) res.set(len, sBuf + iStart);
	return res;
}

int ansStl::cST::toInt(int iRadiX /* = 0 */)
{
	return strtol(sBuf,NULL,iRadiX);
}

double ansStl::cST::toDouble()
{
	return strtod(sBuf,NULL);
}

void cST::append(const char *p)
{
   if (p == NULL) return;
  while (*p){ append(*p); p++; }
}

void cST::appendf(const char *fmt,...)
{
	if (fmt == NULL) return;
	va_list args;
	va_start(args, fmt);
	int iRes = vsnprintf(sBuf + iLen, _MaxCLen_ - 5 - iLen, fmt, args);
	va_end(args);
	if (iRes < 0) return;   // encoding error
	iLen += iRes;
	sBuf[iLen] = 0;
}

char& cST::operator [](int iIdx)
{
	if (iIdx < 0) iIdx = iLen + iIdx;	// zodat -1 laatste teken word -2 op 1 na laatste
	if ((iIdx < 0) || (iIdx >= iLen)) return cDummy;
	return sBuf[iIdx];
}

char& cST::get(int iIdx)
{
	if (iIdx < 0) iIdx = iLen + iIdx;	// zodat -1 laatste teken word -2 op 1 na laatste
	if ((iIdx < 0) || (iIdx >= iLen)) return cDummy;
	return sBuf[iIdx];
}

bool cST::compare(const char *val)
{
    if (val == NULL) return false;
    return (strcmp(sBuf,val) == 0);
}
bool cST::comparei(const char *val)
{
    if (val == NULL) return false;
    return (strcasecmp(sBuf,val) == 0);
}

bool cST::compare(const cST &val)
{
    if (iLen != val.iLen) return false;
    return (memcmp(sBuf,val.sBuf,iLen) == 0);
}
bool cST::startsWith(const char *scmp,bool caseSensitive /* = true */)
{
	if ((scmp == NULL) && (iLen == 0)) return true;
	if ((scmp) && (sBuf))
	{
		int cmpLen = (int)strlen(scmp);
		if (cmpLen > iLen) return false;
		if (caseSensitive)
			return (strncmp(scmp,sBuf,cmpLen) == 0);
		return (strncasecmp(scmp,sBuf,cmpLen) == 0);
	}
	return false;
}

void cST::clear()
{
    iLen = 0;
    sBuf[iLen] = 0;
}
