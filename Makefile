ifndef config
  config=release
endif
export config

QMAKE_DIR=$(realpath ./Ports/Quake2/Premake/Build-Linux/gmake)
EXTLIBS_DIR=$(realpath ./Engine/External/Libs)
S3INPUTLIB_DIR=$(realpath ./Engine/External/Sources/s3sdlinput)

PROJECTS := quake2-game quake2-gles2

.PHONY: all clean $(PROJECTS)

all: $(PROJECTS)

$(EXTLIBS_DIR)/libSDL3.a:
	${MAKE} -C $(S3INPUTLIB_DIR)
	cp $(S3INPUTLIB_DIR)/build/libSDL3.a $(EXTLIBS_DIR)/

lib-deps: $(EXTLIBS_DIR)/libSDL3.a $(EXTLIBS_DIR)/libscreen.so

quake2-gles2: lib-deps

quake2-game quake2-gles2:
	INCLUDES="-I$(S3INPUTLIB_DIR)/include" \
	LDFLAGS="-L$(EXTLIBS_DIR)/" \
	${MAKE} -C $(QMAKE_DIR) $@

clean:
	${MAKE} -C $(QMAKE_DIR) clean
	${MAKE} -C $(S3INPUTLIB_DIR) clean
	rm -f $(EXTLIBS_DIR)/libSDL3.a

