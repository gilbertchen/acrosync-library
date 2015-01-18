/*qi:begin


OSTYPE = $(platform system)
if $OSTYPE == Windows
    build_dir = build-win
    ext = .exe

    CXX = cl 
    CXXFLAGS += /c /I. /I install-win/include /DWIN32 /EHsc /MD 
    LDFLAGS += /NODEFAULTLIB:libcmtd.lib /LIBPATH:install-win/lib crypt32.lib libeay32.lib libssh2.lib ws2_32.lib advapi32.lib shell32.lib user32.lib gdi32.lib
else
    if $OSTYPE == Darwin
        build_dir = build-mac
        ext = 

        CXX = g++
        CXXFLAGS += -g -I. -Iinstall-mac/include -c -D_FILE_OFFSET_BITS=64 -std=c++0x
        LDFLAGS += -Linstall-mac/lib -lstdc++ -lssh2 -lcrypto -liconv -lz
    else
        build_dir = build-linux
        ext = 

        CXX = g++
        CXXFLAGS += -g -I. -c -D_FILE_OFFSET_BITS=64 -std=c++0x
        LDFLAGS += -static-libgcc -lstdc++ -lssh2 -lcrypto
    endif
endif


base_name = $(match ([^.]+) $source)
base_dir = $(dir $build_dir/$base_name) 
$(mkdir $base_dir)

compile($(join $build_dir $base_name.o)): $source
if $OSTYPE == Windows
   $CXX $CXXFLAGS /Fo$compile($source) $source
else
   $CXX $CXXFLAGS -o $compile($source) $source
endif

clean:
    rm -f $compile($source)

ifdef TEST_PROGRAM

link_depends := $(link_depends $source)

link($build_dir/$base_name$ext): compile($link_depends)
if $OSTYPE == Windows
    $CXX /Fe$link($source) $compile($link_depends) /link $LDFLAGS
else
    $CXX -o $link($source) $compile($link_depends) $LDFLAGS
endif

test: link
    ./$link($source)

clean:
    rm -f $link($source)

endif


qi:end*/
