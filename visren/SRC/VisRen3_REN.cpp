/****************************************************************************
 * VisRen3_REN.cpp
 *
 * Plugin module for FAR Manager 1.71
 *
 * Copyright (c) 2007 Alexey Samlyukov
 ****************************************************************************/

enum {
  CASE_NAME_NONE   = 0x00000001,
  CASE_NAME_LOWER  = 0x00000002,
  CASE_NAME_UPPER  = 0x00000004,
  CASE_NAME_FIRST  = 0x00000008,
  CASE_NAME_TITLE  = 0x00000010,
  CASE_EXT_NONE    = 0x00000020,
  CASE_EXT_LOWER   = 0x00000040,
  CASE_EXT_UPPER   = 0x00000080,
  CASE_EXT_FIRST   = 0x00000100,
  CASE_EXT_TITLE   = 0x00000200
};


/****************************************************************************
 * ���⪠ ����⮢ ��� �⪠� ��२���������
 ****************************************************************************/
static void FreeUndo()
{
  for (int i=sUndoFI.iCount-1; i>=0; i--)
  {
    my_free(sUndoFI.CurFileName[i]);
    my_free(sUndoFI.OldFileName[i]);
  }
  my_free(sUndoFI.CurFileName); sUndoFI.CurFileName=0;
  my_free(sUndoFI.OldFileName); sUndoFI.OldFileName=0;
  my_free(sUndoFI.Dir); sUndoFI.Dir=0;
  sUndoFI.iCount=0;
}

/****************************************************************************
 * ���������� �������� ����⠬� ��� �⪠�
 ****************************************************************************/
static bool BuildUndoItem(const TCHAR *CurFileName, const TCHAR *OldFileName, int Count)
{
  if (Count>=sUndoFI.iCount)
  {
    sUndoFI.CurFileName=(TCHAR**)my_realloc(sUndoFI.CurFileName, (Count+1)*sizeof(TCHAR*));
    sUndoFI.OldFileName=(TCHAR**)my_realloc(sUndoFI.OldFileName, (Count+1)*sizeof(TCHAR*));
    if (!sUndoFI.CurFileName || !sUndoFI.OldFileName) return false;
  }
  sUndoFI.CurFileName[Count]=(TCHAR*)my_realloc(sUndoFI.CurFileName[Count], (lstrlen(CurFileName)+1)*sizeof(TCHAR));
  sUndoFI.OldFileName[Count]=(TCHAR*)my_realloc(sUndoFI.OldFileName[Count], (lstrlen(OldFileName)+1)*sizeof(TCHAR));
  if (!sUndoFI.CurFileName[Count] || !sUndoFI.OldFileName[Count]) return false;
  lstrcpy(sUndoFI.CurFileName[Count], CurFileName);
  lstrcpy(sUndoFI.OldFileName[Count], OldFileName);
  return true;
}

/****************************************************************************
 * �஢�ઠ ����� 䠩�� �� �����⨬� ᨬ����
 ****************************************************************************/
static bool CheckFileName(const TCHAR *str)
{
  static TCHAR Denied[]=_T("<>:\"*?/\\|");

  for (TCHAR *s=(TCHAR *)str; *s; s++)
    for (TCHAR *p=Denied; *p; p++)
      if (*s==*p) return false;
  return true;
}

/****************************************************************************
 * �८�ࠧ������ �� ��᪥ ����� (bName==1), ���� ���७�� 䠩��� (bName==0)
 ****************************************************************************/
static bool TransformNameOrExt(const TCHAR *src, TCHAR *dest, bool bName,
                               unsigned Index, DWORD *dwCase, FILETIME ftLastWriteTime)
{
  TCHAR *pMask=(bName?Opt.MaskName:Opt.MaskExt),
         buf[512];
  TCHAR *p=buf;
  SYSTEMTIME mod; FILETIME local;
  FileTimeToLocalFileTime(&ftLastWriteTime, &local);
  FileTimeToSystemTime(&local, &mod);

  while (*pMask)
  {
#if 0
    if (*pMask==_T('[') && !strrchr(pMask, _T(']')))
      return false;
    else if (*pMask==_T(']'))
    {
      TCHAR *pStart=(bName?Opt.MaskName:Opt.MaskExt);
      while (pStart!=pMask && *pStart!=_T('[')) pStart++;
      if (*pStart!=_T('[')) return false;
      else *p++=*pMask++;
    }
    else
#endif
    if (!strncmp(pMask, _T("[[]"), 3))
    {
      *p++=*pMask++; pMask+=2;
    }
    else if (!strncmp(pMask, _T("[]]"), 3))
    {
      pMask+=2; *p++=*pMask++;
    }
    else if (!strncmp(pMask, (bName ? _T("[N") : _T("[E")), 2))
    {
      int lenSrc=lstrlen(src);

      if (*(pMask+2)==_T(']'))  // [N] ��� [E]
      {
        lstrcpy(p, src); pMask+=3; p+=lenSrc;
      }
      else // [N#-#] ��� [E#-#]
      {
        TCHAR buf2[512], Start[512], End[512];
        int i, iStart, iEnd;
        pMask+=2;
        for (i=0; *pMask>=_T('0') && *pMask<=_T('9'); i++, pMask++) Start[i]=*pMask;
        Start[i]=_T('\0'); iStart=FSF.atoi(Start);
        if (!iStart) return false;
        iStart--;
        if (iStart>lenSrc) iStart=lenSrc;

        lenSrc=lstrlen(src+iStart);

        if (*pMask==_T(']'))
        {
          *p++=*(src+iStart); pMask++;
        }
        else if (*pMask==_T('-'))
        {
          pMask++;
          if (*pMask==_T(']'))
          {
            lstrcpy(p, src+iStart);
            p+=lenSrc; pMask++;
          }
          else
          {
            for (i=0; *pMask>=_T('0') && *pMask<=_T('9'); i++, pMask++) End[i]=*pMask;
            End[i]=_T('\0'); iEnd=FSF.atoi(End);
            if (!iEnd || iEnd<=iStart || *pMask!=_T(']')) return false;
            lstrcpyn(buf2, src, iEnd+1); lstrcpy(p, buf2+iStart);
            p+=lstrlen(buf2+iStart); pMask++;
          }
        }
        else if (*pMask==_T(','))
        {
          pMask++;
          for (i=0; *pMask>=_T('0') && *pMask<=_T('9'); i++, pMask++) End[i]=*pMask;
          End[i]=_T('\0'); iEnd=FSF.atoi(End);
          if (!iEnd || *pMask!=_T(']')) return false;
          lstrcpyn(p, src+iStart, iEnd+1);
          p+=lenSrc; pMask++;
        }
        else return false;
      }
    }
    else if (!strncmp(pMask, _T("[C"), 2))
    {
      static TCHAR Start[512], Width[512], Step[512];
      unsigned i, iStart, iWidth, iStep;
      pMask+=2;
      for (i=0; *pMask>=_T('0') && *pMask<=_T('9'); i++, pMask++) Start[i]=*pMask;
      if (!i || i>9 || *pMask!=_T('+')) return false;
      Start[i]=_T('\0'); iStart=FSF.atoi(Start);
      iWidth=i; pMask++;
      for (i=0; *pMask>=_T('0') && *pMask<=_T('9'); i++, pMask++) Step[i]=*pMask;
      if (!i || *pMask!=_T(']')) return false;
      Step[i]=_T('\0'); iStep=FSF.atoi(Step);
      FSF.sprintf(p, _T("%0*d"), iWidth, iStart+Index*iStep);
      p+=lstrlen(p); pMask++;
    }
    else if (!strncmp(pMask, _T("[L]"), 3))
    {
      *dwCase|=(bName ? CASE_NAME_LOWER : CASE_EXT_LOWER);
      pMask+=3;
    }
    else if (!strncmp(pMask, _T("[U]"), 3))
    {
      *dwCase|=(bName ? CASE_NAME_UPPER : CASE_EXT_UPPER);
      pMask+=3;
    }
    else if (!strncmp(pMask, _T("[F]"), 3))
    {
      *dwCase|=(bName ? CASE_NAME_FIRST : CASE_EXT_FIRST);
      pMask+=3;
    }
    else if (!strncmp(pMask, _T("[T]"), 3))
    {
      *dwCase|=(bName ? CASE_NAME_TITLE : CASE_EXT_TITLE);
      pMask+=3;
    }
    else if (!strncmp(pMask, _T("[d]"), 3))
    {
      FSF.sprintf(p, "%02d.%02d.%04d", mod.wDay, mod.wMonth, mod.wYear);
      pMask+=3; p+=10;
    }
    else if (!strncmp(pMask, _T("[t]"), 3))
    {
      FSF.sprintf(p, "%02d-%02d", mod.wHour, mod.wMinute);
      pMask+=3; p+=5;
    }
    else if (*pMask==_T('[') || *pMask==_T(']')) return false;
    else *p++=*pMask++;
  }
  *p=_T('\0');
  if (lstrlen(buf)>=MAX_PATH) return false;
  lstrcpy(dest, buf);
  return true;
}


/****************************************************************************
 * ���� � ������ � ������ 䠩��� (ॣ���஧���ᨬ��, ��樮���쭮).
 ****************************************************************************/
static bool Replase(const TCHAR *src, TCHAR *dest)
{
  int lenSearch=lstrlen(Opt.Search);
  if (!lenSearch) return true;

  int lenSrc=lstrlen(src);
  int lenReplace=lstrlen(Opt.Replace);
  int j=0;
  TCHAR buf[512];
  // ������ ������
  if (!Opt.RegEx)
  {
    for (int i=0; i<lenSrc; )
    {
      if (!(Opt.CaseSensitive?strncmp(src+i, Opt.Search, lenSearch)
                             :FSF.LStrnicmp(src+i, Opt.Search, lenSearch) ))
      {
        for (int n=0; n<lenReplace; n++)
          buf[j++]=Opt.Replace[n];
        i+=lenSearch;
      }
      else
        buf[j++]=src[i++];
    }
    buf[j]=_T('\0');
    if (j>=MAX_PATH) return false;
    lstrcpy(dest, buf);
  }
  else
  {
    pcre *re;
   // pcre_extra *re_ext;
    const char *Error;
    int ErrPtr;
    int matchCount;
    int start_offset=0;
    const int OvectorSize=99;
    int Ovector[OvectorSize];
    TCHAR AnsiSrc[MAX_PATH], AnsiSearch[512];
    OemToChar(src, AnsiSrc);  OemToChar(Opt.Search, AnsiSearch);

    if (!(re=pcre_compile(AnsiSearch, Opt.CaseSensitive?0:PCRE_CASELESS,
                           &Error, &ErrPtr, 0))) return false;
    //if (!(re_ext=pcre_study(re, 0, &Error))) { pcre_free(re); return false; }

    while (matchCount=pcre_exec(re, /*re_ext*/0, AnsiSrc, lenSrc, start_offset,
             PCRE_NOTEMPTY, Ovector, OvectorSize)>=0)
    {
      if (matchCount==PCRE_ERROR_NOMATCH) break;
      for (int i=start_offset; i<Ovector[0]; i++)
        buf[j++]=src[i];
      for (int i=0; i<lenReplace; i++)
        buf[j++]=Opt.Replace[i];
      buf[j]=_T('\0');
      start_offset=Ovector[1];
    }
    pcre_free(re);
    //pcre_free(re_ext);
    for (int i=start_offset; i<lenSrc; i++)
      buf[j++]=src[i];
    buf[j]=_T('\0');
    if (j>=MAX_PATH) return false;
    lstrcpy(dest, buf);
  }
  return true;
}


/****************************************************************************
 * ��������� ॣ���� ���� 䠩���.
 ****************************************************************************/
static void Case(TCHAR *Name, TCHAR *Ext, DWORD dwCase)
{
  if (!dwCase) return;
  // ॣ���� �����
  if (dwCase&CASE_NAME_LOWER)
    FSF.LStrlwr(Name);
  else if (dwCase&CASE_NAME_UPPER)
    FSF.LStrupr(Name);
  else if (dwCase&CASE_NAME_FIRST)
  {
    *Name = (char)FSF.LUpper((BYTE)*Name);
     FSF.LStrlwr(Name+1);
  }
  else if (dwCase&CASE_NAME_TITLE)
    for (int i=0; Name[i]; i++)
    {
      if (!i || memchr(Opt.WordDiv, Name[i-1], lstrlen(Opt.WordDiv)))
        Name[i]=(char)FSF.LUpper((BYTE)Name[i]);
      else
        Name[i]=(char)FSF.LLower((BYTE)Name[i]);
    }
  // ॣ���� ���७��
  if (dwCase&CASE_EXT_LOWER)
    FSF.LStrlwr(Ext);
  else if (dwCase&CASE_EXT_UPPER)
    FSF.LStrupr(Ext);
  else if (dwCase&CASE_EXT_FIRST)
  {
    *Ext = (char)FSF.LUpper((BYTE)*Ext);
     FSF.LStrlwr(Ext+1);
  }
  else if (dwCase&CASE_EXT_TITLE)
    for (int i=0; Ext[i]; i++)
    {
      if (!i || memchr(Opt.WordDiv, Ext[i-1], lstrlen(Opt.WordDiv)))
        Ext[i]=(char)FSF.LUpper((BYTE)Ext[i]);
      else
        Ext[i]=(char)FSF.LLower((BYTE)Ext[i]);
    }
}


/****************************************************************************
 * �᭮���� �㭪�� �� ��ࠡ�⪥ � ᮧ����� ����� ���� 䠩���.
 ****************************************************************************/
static bool ProcessFileName()
{
  if (!CheckFileName(Opt.MaskName) || !CheckFileName(Opt.MaskExt) || !CheckFileName(Opt.Replace))
    return false;

  TCHAR Name[NM], Ext[NM], NewName[NM], NewExt[NM];
  DWORD dwCase=0;

  for (unsigned Index=0; Index<sFI.iCount; Index++)
  {
    TCHAR *src  = sFI.ppi[Index].FindData.cFileName,
          *dest = sFI.DestFileName[Index];
    lstrcpy(Name, src);
    TCHAR *ptr=strrchr(Name, _T('.'));

    if (ptr) { *ptr=_T('\0'); lstrcpy(Ext, ++ptr); }
    else Ext[0]=_T('\0');

    if ( !TransformNameOrExt(Name, NewName, 1, Index, &dwCase, sFI.ppi[Index].FindData.ftLastWriteTime) ||
         !TransformNameOrExt(Ext, NewExt, 0, Index, &dwCase, sFI.ppi[Index].FindData.ftLastWriteTime) )
      return false;

    lstrcpy(dest, NewName);
    if (NewExt[0])
    {
      if (lstrlen(NewExt)+1>=MAX_PATH) return false;
      lstrcat(lstrcat(dest, _T(".")), NewExt);
    }

    if (!Replase(dest, dest)) return false;

    lstrcpy(NewName, dest);
    ptr=strrchr(NewName, _T('.'));

    if (ptr) { *ptr=_T('\0'); lstrcpy(NewExt, ++ptr); }
    else NewExt[0]=_T('\0');

    Case(NewName, NewExt, dwCase);
    lstrcpy(dest, NewName);
    if (NewExt[0]) lstrcat(lstrcat(dest, _T(".")), NewExt);
  }
  return true;
}


/****************************************************************************
 * ��ந� ������ ��� 䠩�� �� ��� � �����
 ****************************************************************************/
static TCHAR *BuildFullFilename(TCHAR *FullFileName, const TCHAR *Dir, const TCHAR *FileName)
{
  FSF.AddEndSlash(lstrcpy(FullFileName, Dir));
  return lstrcat(FullFileName, FileName);
}


/****************************************************************************
 * �㭪�� �� ��२��������� 䠩���. ������ ��⮪�� ��२���������.
 * �����뢠�� �।�०�����, �᫨ �ந��諠 �訡�� �� ��२��������� 䠩��.
 * �����뢠�� ���-�� ��ࠡ�⠭��� 䠩��� �� ��饣� �᫠ ��।�����.
 ****************************************************************************/
static bool RenameFile()
{
  PanelInfo PInfo;
  Info.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELSHORTINFO, &PInfo);
  HANDLE hScreen=Info.SaveScreen(0,0,-1,-1);
  TCHAR srcFull[MAX_PATH], destFull[MAX_PATH];
  bool bSkipAll=false;
  bool PathIdent= (Opt.Undo ? !FSF.LStricmp(PInfo.CurDir, sUndoFI.Dir) : 0);
  int Count = (Opt.Undo ? sUndoFI.iCount : sFI.iCount);
  int i, iSkip, index;
  SetLastError(ERROR_SUCCESS);
  for (i=0, iSkip=0, index=0; i<Count; i++)
  {
    TCHAR *src =( Opt.Undo ?
                  BuildFullFilename(srcFull, sUndoFI.Dir, sUndoFI.CurFileName[i]) :
                  sFI.ppi[i].FindData.cFileName );
    TCHAR *dest=( Opt.Undo ?
                  BuildFullFilename(destFull, sUndoFI.Dir, sUndoFI.OldFileName[i]) :
                  sFI.DestFileName[i] );
    // ����� ᮢ������ - �ய��⨬ ��२���������:
    if (!lstrcmp(src, dest))
    {
      iSkip++; continue;
    }

 RETRY:
    if (MoveFile(src, dest))
    {
      if (Opt.LogRen && !Opt.Undo && !BuildUndoItem(dest, src, index++))
      {
        Opt.LogRen=0;
        ErrorMsg(MErrorCreateLog, MErrorNoMem);
      }
      continue;
    }

    if (GetLastError()==ERROR_FILE_NOT_FOUND || bSkipAll)
      continue;

    // ����� � ᮮ�饭���-�訡��� ��२���������
    const TCHAR *MsgItems[]={
      GetMsg(MVRenTitle),
      GetMsg(MRenameFail),
      src,
      GetMsg(MTo),
      dest,
      GetMsg(MSkip), GetMsg(MSkipAll), GetMsg(MRetry), GetMsg(MCancel)
    };
    int Ret=Info.Message( Info.ModuleNumber, FMSG_WARNING|FMSG_ERRORTYPE, 0,
                          MsgItems, sizeof(MsgItems) / sizeof(MsgItems[0]), 4 );
    switch (Ret)
    {
      case 0:     // �ய�����
        break;
      case 1:     // �ய����� ��
        bSkipAll=true;
        break;
      case 2:     // �������
        goto RETRY;
      default:    // �⬥����
        return false;
    }
  }

  for (int j=sUndoFI.iCount-1; j>index-1; j--)
  {
    my_free(sUndoFI.CurFileName[j]); sUndoFI.CurFileName[j]=0;
    my_free(sUndoFI.OldFileName[j]); sUndoFI.OldFileName[j]=0;
  }

  if (sUndoFI.iCount=index)
  {
    if (!(sUndoFI.Dir=(TCHAR*)my_realloc(sUndoFI.Dir, (lstrlen(PInfo.CurDir)+1)*sizeof(TCHAR))) )
    {
      FreeUndo();
      ErrorMsg(MVRenTitle, MErrorCreateLog);
    }
    else
      lstrcpy(sUndoFI.Dir, PInfo.CurDir);
  }


  Info.RestoreScreen(hScreen);
  Info.Control(INVALID_HANDLE_VALUE, FCTL_UPDATEPANEL, 0);
  Info.Control(INVALID_HANDLE_VALUE, FCTL_REDRAWPANEL, 0);
  if (Count-iSkip)
  {
    TCHAR buf[80];
    FSF.sprintf(buf, GetMsg(MProcessedFmt), i-iSkip, Count-iSkip);
    const TCHAR *MsgItems[]={GetMsg(MVRenTitle), buf, GetMsg(MOK)};
    Info.Message( Info.ModuleNumber, 0, 0, MsgItems,
                  sizeof(MsgItems) / sizeof(MsgItems[0]), 1 );

    if (Opt.Undo && !PathIdent)
      Info.Control(INVALID_HANDLE_VALUE, FCTL_SETPANELDIR, sUndoFI.Dir);
  }

  return true;
}
