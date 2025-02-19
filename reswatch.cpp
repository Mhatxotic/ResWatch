/* ========================================================================= */
/* RESOLUTION WATCHER                                © MHATXOTIC DESIGN 2009 */
/* ========================================================================= */
#define VS_MAJOR                     1 // Major version of application
#define VS_MINOR                     0 // Minor version of application
#define VS_BUILD                     0 // Build of application
#define VS_REVISION                  0 // Revision of application
/* ========================================================================= */
#ifndef RC_INVOKED                     // If not using resource compiler
/* ========================================================================= */
#define WIN32_LEAN_AND_MEAN            // Faster compilation of headers
#define _WIN32_WINNT            0x0500 // Windows XP or Later required
/* ========================================================================= */
#include <Windows.H>                   // Windows API
#include <CommCtrl.H>                  // Common controls
#include <Assert.H>                    // Assertation functions
#include <StdIO.H>                     // Standard IO functions
#include <StdLib.H>                    // Standard library functions
#include <Direct.H>                    // Directory functions
#include <Process.H>                   // Processes functions
/* ========================================================================= */
#pragma comment(lib, "Gdi32.Lib")      // Want functions from GDI32.DLL
#pragma comment(lib, "User32.Lib")     // Want functions from USER32.DLL
#pragma comment(lib, "ComCtl32.Lib")   // Want functions from COMCTL32.DLL
/* ========================================================================= */
#define WAIT_TIME                  100 // Countdown timer in milliseconds
#define WAIT_DELAY                 100 // Delay between countdown ticks
/* ========================================================================= */
INT    iVerMajor = VS_MAJOR;           // Major version of application
INT    iVerMinor = VS_MINOR;           // Minor version of application
INT    iVerBits  = sizeof(PVOID) << 3; // Bits version of program (32/64)
INT    iWinVer   = GetVersion();       // Windows version
INT    iCompVer  = _MSC_VER;           // Visual C++ version
PCHAR  cpVerDate = __DATE__;           // Date program was compiled
PCHAR  cpName    = "ResWatch";         // Label
PCHAR  cpMsgName = "ResWatch Message"; // Message box label
PCHAR  cpConfig  = "ResWatch.Cfg";     // Configuration file
/* ------------------------------------------------------------------------- */
HANDLE hThreadHandle;                  // Timer thread handle
HWND   hwndWindow;                     // Dialog window handle
/* ------------------------------------------------------------------------- */
DEVMODE dmCurrent;                     // Current desktop resolution
DEVMODE dmNew;                         // New desktop resolution
DEVMODE dmMinimum;                     // Minimum supported desktop resolution
DEVMODE dmMaximum;                     // Maximum supported desktop resolution
/* ========================================================================= */
enum CVAR                              // Configuration option id's
{ /* ----------------------------------------------------------------------- */
  CFG_DESKTOP_WIDTH,                   // Requested desktop width
  CFG_DESKTOP_HEIGHT,                  // Requested desktop height
  CFG_DESKTOP_DEPTH,                   // Requested desktop bit-depth
  CFG_DESKTOP_REFRESH,                 // Requested desktop refresh rate
  /* ----------------------------------------------------------------------- */
  CFG_MAX                              // Maximum number of options supported
  /* ----------------------------------------------------------------------- */
};
/* ========================================================================= */
struct _CONFIG                         // Configuration data lookup table
{
  /* ----------------------------------------------------------------------- */
  char *cpValue;                       // Var will be alloc'd if set
  int   iValue;                        // Integer value (if not string)
  int   iValueMin;                     // Minimum integer value
  int   iValueMax;                     // Maximum integer value
  char *cpName;                        // Name of variable
  int   iFlags;                        // Variable flags (FLAGS_*)
  /* ----------------------------------------------------------------------- */
}
/* ========================================================================= */
CONFIG[]=                              // Beginning of static lookup table
{
  /* ----------------------------------------------------------------------- */
  #define FLAGS_NONE       0x00000000L // No flags
  #define FLAGS_INT        0x00000001L // Option is a integer
  #define FLAGS_INTRANGE   0x00000002L // Integer is limited by range
  #define FLAGS_INTPOWER2  0x00000004L // Integer must be a power of two
  #define FLAGS_STRING     0x00000008L // Option is a string
  #define FLAGS_BOOLEAN    0x00000010L // String is a boolean operator
  #define FLAGS_PATH       0x00000020L // String is a pathname
  #define FLAGS_FILE       0x00000040L // String is a filename
  #define FLAGS_PATHEXIST  0x00000080L // String path must exist on system
  #define FLAGS_NULLERROR  0x00000100L // String must NOT be null
  #define FLAGS_ALLOCATED  0x80000000L // String has been allocated
  /* ----------------------------------------------------------------------- */
  NULL,     800, 320, 2048, "desktop_width",         FLAGS_INT|FLAGS_INTRANGE,
  NULL,     600, 240, 1536, "desktop_height",        FLAGS_INT|FLAGS_INTRANGE,
  NULL,      32,   4,   32, "desktop_depth",         FLAGS_INT|FLAGS_INTRANGE|FLAGS_INTPOWER2,
  NULL,      60,  50,  100, "desktop_refresh",       FLAGS_INT|FLAGS_INTRANGE,
  /* ----------------------------------------------------------------------- */
  NULL,         0, 0,     0, NULL,                    FLAGS_NONE
  /* ----------------------------------------------------------------------- */
};
/* ========================================================================= */
DWORD Exception(INT iLine, INT iCode, PCHAR pcFormat, ...)
{
  CHAR cFormatBuffer[1024];
  _vsnprintf_s(cFormatBuffer, sizeof(cFormatBuffer), pcFormat, (PCHAR)(&pcFormat + 1));
  CHAR cFinalBuffer[1024];
  if(iLine)
    _snprintf_s(cFinalBuffer, sizeof(cFinalBuffer), "Error %u-%x-%u: %s.", iCode, iCode, iLine, cFormatBuffer);
  else
    _snprintf_s(cFinalBuffer, sizeof(cFinalBuffer), "%s.", cFormatBuffer);
  return MessageBox(IsWindow(hwndWindow) ? hwndWindow : NULL, cFinalBuffer, cpMsgName, (iLine ? MB_ICONEXCLAMATION : MB_ICONINFORMATION) | MB_APPLMODAL);
}
/* ========================================================================= */
PVOID GetCVar(CVAR cvId)
{
  assert(cvId >= 0 && cvId < CFG_MAX);
  struct _CONFIG *PCONFIG = &CONFIG[cvId];
  assert(PCONFIG != NULL);

  if(PCONFIG->iFlags & FLAGS_INT || PCONFIG->iFlags & FLAGS_BOOLEAN)
    return &PCONFIG->iValue;

  if(PCONFIG->iFlags & FLAGS_STRING)
    return PCONFIG->cpValue;

  return NULL;
}
/* ========================================================================= */
INT InitConfig(VOID)
{
  FILE *fStream;

  if(fopen_s(&fStream, cpConfig, "rt") != 0)
  {
    Exception(__LINE__, errno, "Failed to open configuration file '%s'!", cpConfig);
    return -1;
  }

  int iLine = 0, iErrors = 0, iChanges = 0;

  struct _CONFIG *PCONFIG;

  while(feof(fStream) == 0)
  {
    char cString[1024];

    if(!fgets(cString, sizeof(cString), fStream))
      continue;

    ++iLine;

    if(ferror(fStream))
    {
      Exception(__LINE__, errno, "Error reading configuration file '%s'!", cpConfig);
      break;
    }

    if(*cString < 32 || *cString == ';')
      continue;

    PCHAR cpContext = NULL;
    PCHAR cpName = strtok_s(cString, "=", &cpContext);
    if(cpName == NULL)
    {
      Exception(__LINE__, iLine, "Variable syntax error detected on line '%u' of '%s'!", iLine, cpConfig);
      ++iErrors;
      continue;
    }
    for(; *cpName <= ' '; *++cpName);

    size_t iIndex;
    for(iIndex = strlen(cpName); iIndex >= 0 && cpName[iIndex] <= ' '; --iIndex)
      cpName[iIndex] = '\0';

    PCHAR cpValue = strtok_s(NULL, "\r\n", &cpContext);
    if(cpValue == NULL)
    {
      Exception(__LINE__, iLine, "Value syntax error detected on line '%u' of '%s'!", iLine, cpConfig);
      ++iErrors;
      continue;
    }
    for(; *cpValue <= ' '; *++cpValue);
    for(iIndex = strlen(cpValue); iIndex >= 0 && cpValue[iIndex] <= ' '; --iIndex)
      cpValue[iIndex] = '\0';

    for(PCONFIG = CONFIG; PCONFIG->cpName != NULL; ++PCONFIG)
    {
      if(_strcmpi(PCONFIG->cpName, cpName) != 0)
        continue;

      if(PCONFIG->iFlags & FLAGS_INT)
      {
        int iValue = atoi(cpValue);
        if(PCONFIG->iFlags & FLAGS_INTRANGE && (iValue < PCONFIG->iValueMin || iValue > PCONFIG->iValueMax))
        {
          Exception(__LINE__, iLine, "Value of '%i' for variable '%s' is out-of-range (%i-%i) on line '%u' of '%s'!", iValue, cpName, PCONFIG->iValueMin, PCONFIG->iValueMax, iLine, cpConfig);
          ++iErrors;
          break;
        }
        PCONFIG->iValue = iValue;
        ++iChanges;
      }
      else if(PCONFIG->iFlags & FLAGS_BOOLEAN)
      {
        if(!_strcmpi(cpValue, "true") || !_strcmpi(cpValue, "yes") || !_strcmpi(cpValue, "1"))
          PCONFIG->iValue = 1;
        else if(!_strcmpi(cpValue, "false") || !_strcmpi(cpValue, "no") || !_strcmpi(cpValue, "0"))
          PCONFIG->iValue = 0;
        else
        {
          Exception(__LINE__, iLine, "Value of '%s' for variable '%s' is not valid on line '%u' of '%s'!", cpValue, cpName, iLine, cpConfig);
          ++iErrors;
          break;
        }
        ++iChanges;
      }
      else if(PCONFIG->iFlags & FLAGS_STRING)
      {
        if(PCONFIG->iFlags & FLAGS_PATH)
        {
          CHAR caDrive[_MAX_DRIVE];
          CHAR caPath[_MAX_PATH];
          CHAR caFile[_MAX_FNAME];
          CHAR caExt[_MAX_EXT];

          _splitpath_s(cpValue, caDrive, sizeof(caDrive), caPath, sizeof(caPath), caFile, sizeof(caFile), caExt, sizeof(caExt));

          if(*caDrive == '\0')
          {
            Exception(__LINE__, iLine, "Path for '%s' missing drive letter on line '%u' of '%s'!", cpName, iLine, cpConfig);
            ++iErrors;
            break;
          }
          if(*caPath == '\0')
          {
            Exception(__LINE__, iLine, "Path for '%s' missing directory name on line '%u' of '%s'!", cpName, iLine, cpConfig);
            ++iErrors;
            break;
          }
          if(PCONFIG->iFlags & FLAGS_FILE)
          {
            if(*caFile == '\0')
            {
              Exception(__LINE__, iLine, "Path for '%s' missing file name on line '%u' of '%s'!", cpName, iLine, cpConfig);
              ++iErrors;
              break;
            }
            if(*caExt == '\0')
            {
              Exception(__LINE__, iLine, "Path for '%s' missing extension!", cpName, iLine, cpConfig);
              ++iErrors;
              break;
            }
            if(PCONFIG->iFlags & FLAGS_PATHEXIST)
            {
              FILE *fFile;
              if(fopen_s(&fFile, cpValue, "rb") != 0)
              {
                Exception(__LINE__, errno, "Could not open '%s' for variable '%s' on line '%u' of '%s'!", cpValue, cpName, iLine, cpConfig);
                ++iErrors;
                break;
              }
              fclose(fFile);
            }
          }
          else if(PCONFIG->iFlags & FLAGS_PATHEXIST && _chdir(cpValue) == -1)
          {
            Exception(__LINE__, errno, "Could not locate directory '%s' for variable '%s' on line '%u' of '%s'!", cpValue, cpName, iLine, cpConfig);
            ++iErrors;
            break;
          }
        }
      }
      break;
    }
    if(PCONFIG->cpName == NULL)
    {
      Exception(__LINE__, errno, "Variable '%s' not recognised on line '%u' of '%s'!", cpName, iLine, cpConfig);
      ++iErrors;
    }
  }

  for(PCONFIG = CONFIG; PCONFIG->cpName != NULL; ++PCONFIG)
    if(PCONFIG->iFlags & FLAGS_NULLERROR && PCONFIG->cpValue == NULL)
    {
      Exception(__LINE__, errno, "Variable '%s' has not been set in configuration file '%s'!", PCONFIG->cpName, cpConfig);
      ++iErrors;
    }

  fclose(fStream);

  if(iErrors > 0)
    return -1;

  return 0;
}
/* ========================================================================= */
VOID KillThread(VOID)
{
  if(hThreadHandle == NULL)
    return;
  DWORD dwThreadExitCode;
  if(GetExitCodeThread(hThreadHandle, &dwThreadExitCode) == TRUE && dwThreadExitCode == STILL_ACTIVE)
    assert(TerminateThread(hThreadHandle, 1) != 0);
  hThreadHandle = NULL;
}
/* ========================================================================= */
DWORD CALLBACK ThreadMain(PVOID)
{
  CHAR cMessage[1024];
  _snprintf_s(cMessage, sizeof(cMessage), "%s™ Version %u.%u.%u (%s)\n© MhatXtoic Design, 2011. All Rights Reserved", cpName, iVerMajor, iVerMinor, iVerBits, cpVerDate);
  assert(SetDlgItemText(hwndWindow, 11, cMessage) != 0);
  assert(SendNotifyMessage(GetDlgItem(hwndWindow, 5), PBM_SETRANGE32, 0, WAIT_TIME) != 0);
  INT iTimer;
  for(iTimer = WAIT_TIME; iTimer >= 0; --iTimer)
  {
    _snprintf_s(cMessage, sizeof(cMessage), "The desktop resolution will temporarily change from %ux%ux%u to %ux%ux%u in %.1f seconds...", dmCurrent.dmPelsWidth, dmCurrent.dmPelsHeight, dmCurrent.dmBitsPerPel, dmNew.dmPelsWidth, dmNew.dmPelsHeight, dmNew.dmBitsPerPel, (float)iTimer/10);
    assert(SetDlgItemText(hwndWindow, 10, cMessage) != 0);
    SendNotifyMessage(GetDlgItem(hwndWindow, 5), PBM_SETPOS, WAIT_TIME - iTimer, 0);
    Sleep(WAIT_DELAY);
  }
  assert(SendNotifyMessage(hwndWindow, WM_COMMAND, 2, 0) != 0);
  hThreadHandle = NULL;
  return 0;
}
/* ========================================================================= */
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM)
{
  switch(uMsg)
  {
    case WM_CLOSE:
      assert(ChangeDisplaySettingsEx(NULL, &dmCurrent, NULL, CDS_RESET|CDS_GLOBAL|CDS_UPDATEREGISTRY, NULL) == DISP_CHANGE_SUCCESSFUL);
      assert(SendMessage(hwndDlg, WM_COMMAND, 1, 0) != 0);
      break;
    case WM_DESTROY:
      hwndWindow = NULL;
      PostQuitMessage(0);
      break;
    case WM_COMMAND:
      switch(wParam)
      {
        case 1:
          assert(DestroyWindow(hwndDlg) != NULL);
          break;
        case 2:
          KillThread();
          assert(SetDlgItemText(hwndDlg, 10, "Changing the desktop resolution...") != 0);
          SendMessage(GetDlgItem(hwndDlg, 1), WM_ENABLE, FALSE, 0);
          SendMessage(GetDlgItem(hwndDlg, 2), WM_ENABLE, FALSE, 0);
          SendMessage(GetDlgItem(hwndDlg, 3), WM_ENABLE, FALSE, 0);
          assert(ChangeDisplaySettingsEx(NULL, &dmNew, NULL, CDS_FULLSCREEN|CDS_GLOBAL|CDS_UPDATEREGISTRY, NULL) == DISP_CHANGE_SUCCESSFUL);
          assert(ChangeDisplaySettingsEx(NULL, &dmCurrent, NULL, CDS_NORESET|CDS_GLOBAL|CDS_UPDATEREGISTRY, NULL) == DISP_CHANGE_SUCCESSFUL);
          assert(ShowWindow(hwndDlg, SW_HIDE) != 0);
          break;
        case 3:
          assert(SuspendThread(hThreadHandle) != -1);
          Exception(0, 0, "Resolution Watcher, compiled " __TIMESTAMP__ ".\n"
                          "Copyright © MhatXotic Design, 2011. All Rights Reserved.\n"
                          "Visit http://github.com/mhatxotic for more information and updates.\n"
                          "\n"
                          "Current detected resolution:\t\t%ux%u %ubpp %uhz.\n"
                          "Current detected minimum resolution:\t%ux%u %ubpp %uhz.\n"
                          "Current detected maximum resolution:\t%ux%u %ubpp %uhz.\n"
                          "Programmed new resolution:\t\t%ux%u %ubpp %uhz.\n"
                          "\n"
                          "Detected Windows version:\t\t%u.%u (build %u).\n"
                          "Recommended Windows version:\t%u.%u.\n"
                          "\n"
                          "Application version:\t\t\t%u.%u (%u-Bit).\n"
                          "Application compilation date:\t\t%s.\n"
                          "Visual C++ compiler version:\t\t%u.%02u",
            dmCurrent.dmPelsWidth, dmCurrent.dmPelsHeight, dmCurrent.dmBitsPerPel, dmCurrent.dmDisplayFrequency,
            dmMinimum.dmPelsWidth, dmMinimum.dmPelsHeight, dmMinimum.dmBitsPerPel, dmMinimum.dmDisplayFrequency,
            dmMaximum.dmPelsWidth, dmMaximum.dmPelsHeight, dmMaximum.dmBitsPerPel, dmMaximum.dmDisplayFrequency,
            dmNew.dmPelsWidth, dmNew.dmPelsHeight, dmNew.dmBitsPerPel, dmNew.dmDisplayFrequency,
            LOBYTE(LOWORD(iWinVer)), HIBYTE(LOWORD(iWinVer)), HIWORD(iWinVer),
            HIBYTE(_WIN32_WINNT), LOBYTE(_WIN32_WINNT),
            iVerMajor, iVerMinor, iVerBits, cpVerDate,
            LOWORD(iCompVer) / 100, LOWORD(iCompVer) % 100);
          assert(ResumeThread(hThreadHandle) != -1);
          break;
        default:
          return FALSE;
      }
      break;
    default:
      return FALSE;
  }
  return TRUE;
}
/* ========================================================================= */
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, INT)
{
  HANDLE hProcessMutex = CreateMutex(0, 1, cpName);
  assert(hProcessMutex != NULL);

  if(GetLastError() == ERROR_ALREADY_EXISTS)
  {
    for(hwndWindow = FindWindow(NULL, cpMsgName); hwndWindow != NULL; hwndWindow = FindWindow(NULL, cpMsgName))
    {
      assert(SendNotifyMessage(hwndWindow, WM_CLOSE, 0, 0) != 0);
      Sleep(100);
    }
    for(hwndWindow = FindWindow(NULL, cpName); hwndWindow != NULL; hwndWindow = FindWindow(NULL, cpName))
    {
      assert(SendNotifyMessage(hwndWindow, WM_CLOSE, 0, 0) != 0);
      Sleep(100);
    }
    return 0;
  }

  if(InitConfig() < 0)
    return 1;


  INITCOMMONCONTROLSEX iccData;

  iccData.dwSize = sizeof(iccData);
  iccData.dwICC = ICC_PROGRESS_CLASS;

  assert(InitCommonControlsEx(&iccData) != FALSE);

  memset(&dmCurrent, 0, sizeof(dmCurrent));
  memset(&dmNew, 0, sizeof(dmNew));
  memset(&dmMinimum, 0, sizeof(dmMinimum));
  memset(&dmMaximum, 0, sizeof(dmMaximum));
  dmCurrent.dmSize = dmNew.dmSize = dmMinimum.dmSize = dmMaximum.dmSize = sizeof(DEVMODE);
  dmMinimum.dmPelsWidth = dmMinimum.dmPelsHeight = dmMinimum.dmBitsPerPel = 0xFFFF;
  dmCurrent.dmFields = dmNew.dmFields = dmMinimum.dmFields = dmMaximum.dmFields =
    DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

  assert(EnumDisplaySettingsEx(NULL, ENUM_CURRENT_SETTINGS, &dmCurrent, 0) != 0);

  INT iIndex;
  for(iIndex = 0; EnumDisplaySettingsEx(NULL, iIndex, &dmNew, 0) != 0; ++iIndex)
  {
    if(dmNew.dmPelsWidth > dmMaximum.dmPelsWidth ||
       dmNew.dmPelsHeight > dmMaximum.dmPelsHeight ||
       dmNew.dmBitsPerPel > dmMaximum.dmBitsPerPel)
      memcpy(&dmMaximum, &dmNew, sizeof(dmMaximum));
    if(dmNew.dmPelsWidth < dmMinimum.dmPelsWidth ||
       dmNew.dmPelsHeight < dmMinimum.dmPelsHeight ||
       dmNew.dmBitsPerPel < dmMinimum.dmBitsPerPel)
      memcpy(&dmMinimum, &dmNew, sizeof(dmMinimum));
  }

  assert(iIndex > 0);

  dmNew.dmPelsWidth = *(WORD*)GetCVar(CFG_DESKTOP_WIDTH);
  dmNew.dmPelsHeight = *(WORD*)GetCVar(CFG_DESKTOP_HEIGHT);
  dmNew.dmBitsPerPel = *(WORD*)GetCVar(CFG_DESKTOP_DEPTH);
  dmNew.dmDisplayFrequency = *(WORD*)GetCVar(CFG_DESKTOP_REFRESH);
  dmNew.dmFields = 0xFFFFFFFF;

  hwndWindow = CreateDialog(hInstance, MAKEINTRESOURCE(1), GetDesktopWindow(), DialogProc);
  assert(hwndWindow != NULL);
  assert(SetWindowText(hwndWindow, cpName) != 0);
  HICON hIconL = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
  assert(hIconL != NULL);
  SetClassLongPtr(hwndWindow, GCLP_HICON, (LONG_PTR)hIconL);
  HICON hIconS = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
  assert(hIconS != NULL);
  SetClassLongPtr(hwndWindow, GCLP_HICONSM, (LONG_PTR)hIconS);

  assert(ShowWindow(hwndWindow, SW_SHOW) == 0);

  hThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadMain, NULL, 0, NULL);
  assert(hThreadHandle != NULL);

  MSG mMsg;
  while(GetMessage(&mMsg, 0, 0, 0) > 0)
  {
    if(IsDialogMessage(hwndWindow, &mMsg) == 0)
    {
      TranslateMessage(&mMsg);
      DispatchMessage(&mMsg);
    }
  }

  KillThread();

  DeleteObject(hIconL);
  DeleteObject(hIconS);

  assert(ReleaseMutex(hProcessMutex) != FALSE);

  return 0;
}
/* ========================================================================= */
#endif                                 // !RC_INVOKED
/* ========================================================================= */
/* EOF                                                                   EOF */
/* ========================================================================= */
