# Microsoft Developer Studio Generated NMAKE File, Based on emupi.dsp
!IF "$(CFG)" == ""
CFG=emupi - Win32 Debug
!MESSAGE No configuration specified. Defaulting to emupi - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "emupi - Win32 Release" && "$(CFG)" != "emupi - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "emupi.mak" CFG="emupi - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "emupi - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "emupi - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "emupi - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\emupi.dll"


CLEAN :
	-@erase "$(INTDIR)\emu.obj"
	-@erase "$(INTDIR)\factory.obj"
	-@erase "$(INTDIR)\inferno.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\resource.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\emupi.dll"
	-@erase "$(OUTDIR)\emupi.exp"
	-@erase "$(OUTDIR)\emupi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "C:\users\inferno\emu" /I "C:\users\inferno\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "EMUPI_EXPORTS" /D "_WIN32_DCOM" /Fp"$(INTDIR)\emupi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\resource.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\emupi.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib urlmon.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\emupi.pdb" /machine:I386 /def:".\emupi.def" /out:"$(OUTDIR)\emupi.dll" /implib:"$(OUTDIR)\emupi.lib" 
DEF_FILE= \
	".\emupi.def"
LINK32_OBJS= \
	"$(INTDIR)\emu.obj" \
	"$(INTDIR)\factory.obj" \
	"$(INTDIR)\inferno.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\resource.res"

"$(OUTDIR)\emupi.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "emupi - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\emupi.dll" "$(OUTDIR)\emupi.bsc"


CLEAN :
	-@erase "$(INTDIR)\emu.obj"
	-@erase "$(INTDIR)\emu.sbr"
	-@erase "$(INTDIR)\factory.obj"
	-@erase "$(INTDIR)\factory.sbr"
	-@erase "$(INTDIR)\inferno.obj"
	-@erase "$(INTDIR)\inferno.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\resource.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\emupi.bsc"
	-@erase "$(OUTDIR)\emupi.dll"
	-@erase "$(OUTDIR)\emupi.exp"
	-@erase "$(OUTDIR)\emupi.ilk"
	-@erase "$(OUTDIR)\emupi.lib"
	-@erase "$(OUTDIR)\emupi.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "C:\users\inferno\emu" /I "C:\users\inferno\include" /D "_DEBUG" /D "_WIN32_DCOM" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "EMUPI_EXPORTS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\emupi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\resource.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\emupi.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\emu.sbr" \
	"$(INTDIR)\factory.sbr" \
	"$(INTDIR)\inferno.sbr" \
	"$(INTDIR)\main.sbr"

"$(OUTDIR)\emupi.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib urlmon.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\emupi.pdb" /debug /machine:I386 /def:".\emupi.def" /out:"$(OUTDIR)\emupi.dll" /implib:"$(OUTDIR)\emupi.lib" /pdbtype:sept 
DEF_FILE= \
	".\emupi.def"
LINK32_OBJS= \
	"$(INTDIR)\emu.obj" \
	"$(INTDIR)\factory.obj" \
	"$(INTDIR)\inferno.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\resource.res"

"$(OUTDIR)\emupi.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("emupi.dep")
!INCLUDE "emupi.dep"
!ELSE 
!MESSAGE Warning: cannot find "emupi.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "emupi - Win32 Release" || "$(CFG)" == "emupi - Win32 Debug"
SOURCE=.\emu.cpp

!IF  "$(CFG)" == "emupi - Win32 Release"


"$(INTDIR)\emu.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "emupi - Win32 Debug"


"$(INTDIR)\emu.obj"	"$(INTDIR)\emu.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\factory.cpp

!IF  "$(CFG)" == "emupi - Win32 Release"


"$(INTDIR)\factory.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "emupi - Win32 Debug"


"$(INTDIR)\factory.obj"	"$(INTDIR)\factory.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\inferno.cpp

!IF  "$(CFG)" == "emupi - Win32 Release"


"$(INTDIR)\inferno.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "emupi - Win32 Debug"


"$(INTDIR)\inferno.obj"	"$(INTDIR)\inferno.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\main.cpp

!IF  "$(CFG)" == "emupi - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "emupi - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\resource.rc

"$(INTDIR)\resource.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

