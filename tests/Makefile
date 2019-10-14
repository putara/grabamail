TARGET_NAME = unittest
INTDIR = ..\obj\$(TARGET_NAME)
OUTDIR = ..\bin\$(TARGET_NAME)

TARGET_EXE = $(OUTDIR)\$(TARGET_NAME).exe
TARGET_MAP = $(OUTDIR)\$(TARGET_NAME).map
TARGET_PDB = $(OUTDIR)\$(TARGET_NAME).pdb
TARGET_OBJS = "$(INTDIR)\unittest.obj"

CC = cl.exe
LD = link.exe

!if "$(AMD64)" == "1"
TARGET_ARCH = x64
!else
TARGET_ARCH = x86
!endif

CFLAGS = /nologo /c /GF /GR- /GS /Gy /MD /O1ib1 /W3 /Zi /fp:fast /FAcs /Fa"$(INTDIR)/" /Fo"$(INTDIR)/" /Fd"$(INTDIR)/"
LDFLAGS = /nologo /dynamicbase:no /machine:$(TARGET_ARCH) /map:"$(TARGET_MAP)" /nxcompat /opt:noicf /opt:ref /release /debug /PDBALTPATH:"%_PDB%" /subsystem:windows

CDEFS = /D "NDEBUG" /D "_STATIC_CPPLIB" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE"
LDLIBS = kernel32.lib user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib advapi32.lib

all: "$(INTDIR)" "$(OUTDIR)" "$(TARGET_EXE)"

clean: cleanobj
 -@erase "$(TARGET_EXE)" 2>NUL
 -@erase "$(TARGET_MAP)" 2>NUL
 -@rmdir "$(INTDIR)" 2>NUL

cleanobj: cleanpdb cleanobjonly

cleanpdb:
 -@erase "$(TARGET_PDB)" 2>NUL

cleanobjonly:
 -@erase "$(INTDIR)\*.cod" 2>NUL
 -@erase $(TARGET_OBJS) 2>NUL
 -@erase "$(INTDIR)\vc??.pdb" 2>NUL
 -@erase "$(INTDIR)\vc??.idb" 2>NUL

"$(INTDIR)":
 @if not exist "$(INTDIR)" mkdir "$(INTDIR)"

"$(OUTDIR)":
 @if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

"$(TARGET_EXE)" : $(TARGET_OBJS)
 $(LD) @<<
  /out:$@ $(LDFLAGS) $(LDLIBS) $(TARGET_OBJS)
<<
 $@

.SUFFIXES: .c .cpp .obj .rc

.c{$(INTDIR)}.obj::
 $(CC) @<<
  $(CFLAGS) $(CDEFS) $<
<<

.cpp{$(INTDIR)}.obj::
 $(CC) @<<
  $(CFLAGS) $(CDEFS) $<
<<

"$(INTDIR)\unittest.obj": unittest.h datetest.h fldtest.h
