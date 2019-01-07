CC=gcc
GXX=g++
DEBUG_FLAG = -D_SQL_DEBUG_ZYX -D_ZYX_DEBUG
MYINCLUDE = -I../../lib/libXBase/include 
MYLIB =-L../../lib/libXBase/ -lXBase
DEBUG_CXXFLAGS = ${MYINCLUDE} -I./ -I/usr/local/include/libxml2 -I/usr/include/mysql/ -D_DEBUG -D_ALL_SUPER_GM $(DEBUG_FLAG) -g -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated --std=c++0x
DEBUG_CXXLFLAGS =${MYLIB} -L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex -lrt

DIST_CXXFLAGS =-I./ -I../ -I../base -I../util -I../base/recastnavigation/Recast/Include -I../base/recastnavigation/Detour/Include -I../Command -I/usr/local/include/libxml2 -I/usr/include/mysql/ -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated -g3 #-O2 优化暂不打开
DIST_CXXLFLAGS =-L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex -lrt
BASE_OBJECTS =xPos.o xPos3.o
BINPATH=../../lib/XGameData/
TARGET = $(BINPATH)libXGameData.a
CXXLFLAGS=$(DEBUG_CXXLFLAGS) 
CXXFLAGS=$(DEBUG_CXXFLAGS)
.PHONY : all

all: debug

debug:$(TARGET) $(ZLIB_OBJECTS) $(DES_OBJECTS) $(SHA1_OBJECTS)
$(TARGET):$(BASE_OBJECTS)
	mkdir -p $(BINPATH)include/
	cp $(wildcard ./*.h) $(BINPATH)include/
	
	ar rcu $(TARGET) $(BASE_OBJECTS)	
	ranlib $(TARGET)	
$(BASE_OBJECTS): %.o: %.cpp 
	$(CXX) $(CXXFLAGS) -c $< -o $@ 	


.PHONY: clean
clean:
	
	rm  -rf $(BASE_OBJECTS) $(TARGET)/*.o
	rm -rf .*.dep 
	rm -rf ${BINPATH}
	