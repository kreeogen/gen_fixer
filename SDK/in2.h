#ifndef NULLSOFT_WINAMP_IN2H
#define NULLSOFT_WINAMP_IN2H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef CDECL
#define CDECL __cdecl
#endif

/* упрощённый Out_Module только чтобы структура совпадала по смещениям;
   нам он не нужен, но поле outMod в In_Module должно существовать */
typedef struct Out_Module Out_Module;
struct Out_Module { int _dummy; };

/* Не включаем UNICODE-вариант, оставляем ANSI как в старом in_mp3 */
typedef char in_char;

#define GETFILEINFO_TITLE_LENGTH 2048
#define INFOBOX_EDITED   0
#define INFOBOX_UNCHANGED 1

/* Флаги модуля (совпадают по смыслу, но нам не критично) */
#define IN_MODULE_FLAG_USES_OUTPUT_PLUGIN 1
#define IN_MODULE_FLAG_EQ 2
#define IN_MODULE_FLAG_REPLAYGAIN 8
#define IN_MODULE_FLAG_REPLAYGAIN_PREAMP 16

typedef struct
{
  int version;
  char *description;

  HWND hMainWindow;
  HINSTANCE hDllInstance;

  char *FileExtensions;

  int is_seekable;
  int UsesOutputPlug;

  void (CDECL *Config)(HWND hwndParent);
  void (CDECL *About)(HWND hwndParent);

  void (CDECL *Init)(void);
  void (CDECL *Quit)(void);

  /* ВНИМАНИЕ: в каноническом in2.h GetFileInfo — void, а не int */
  void (*GetFileInfo)(const in_char *file, in_char *title, int *length_in_ms);

  int (CDECL *InfoBox)(const in_char *file, HWND hwndParent);

  int (CDECL *IsOurFile)(const in_char *fn);

  int (CDECL *Play)(const in_char *fn);
  void (CDECL *Pause)(void);
  void (CDECL *UnPause)(void);
  int  (CDECL *IsPaused)(void);
  void (CDECL *Stop)(void);

  int  (CDECL *GetLength)(void);
  int  (CDECL *GetOutputTime)(void);
  void (CDECL *SetOutputTime)(int time_in_ms);

  void (CDECL *SetVolume)(int volume);
  void (CDECL *SetPan)(int pan);

  /* SAVSA/VSA/dsp/eq/SetInfo/… — оставляем поля, чтобы смещения совпали */
  void (CDECL *SAVSAInit)(int maxlatency_in_ms, int srate);
  void (CDECL *SAVSADeInit)(void);
  void (CDECL *SAAddPCMData)(void *PCMData, int nch, int bps, int timestamp);
  int  (CDECL *SAGetMode)(void);
  int  (CDECL *SAAdd)(void *data, int timestamp, int csa);

  void (CDECL *VSAAddPCMData)(void *PCMData, int nch, int bps, int timestamp);
  int  (CDECL *VSAGetMode)(int *specNch, int *waveNch);
  int  (CDECL *VSAAdd)(void *data, int timestamp);
  void (CDECL *VSASetInfo)(int srate, int nch);

  int  (CDECL *dsp_isactive)(void);
  int  (CDECL *dsp_dosamples)(short int *samples, int numsamples, int bps, int nch, int srate);

  void (CDECL *EQSet)(int on, char data[10], int preamp);

  void (CDECL *SetInfo)(int bitrate, int srate, int stereo, int synched);

  Out_Module *outMod;

  /* Доп. поле, которого нет в очень старых версиях: FileExtendedInfo.
     Если у оригинала оно отсутствует, тут всё равно должен быть слот,
     т.к. Winamp 5.x и in_mp3 >= 2.9x его имеют. */
  int (CDECL *FileExtendedInfo)(const in_char *fn, const in_char *data, in_char *ret, int retlen);

} In_Module;

#endif /* NULLSOFT_WINAMP_IN2H */
