CC=gcc
GXX=g++
DEBUG_FLAG = -D_SQL_DEBUG_ZYX -D_ZYX_DEBUG
MYINCLUDE =-I../../lib/libXNet/include/ -I../../lib/libXBase/include/ -I../../lib/xCommand
MYLIB =-L../../lib/libXBase/ -lXBase -L./Bin/libXNet/ -lXNet
DEBUG_CXXFLAGS = ${MYINCLUDE} -I./ -I/usr/local/include/libxml2 -I/usr/local/mysql5.6/include/ -D_DEBUG -D_ALL_SUPER_GM $(DEBUG_FLAG) -g -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated --std=c++0x
DEBUG_CXXLFLAGS =${MYLIB} -L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex -lrt

DIST_CXXFLAGS =-I./ -I../ -I../base -I../util -I../base/recastnavigation/Recast/Include -I../base/recastnavigation/Detour/Include -I../Command -I/usr/local/include/libxml2 -I/usr/local/mysql5.6/include/ -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated -g3 #-O2 优化暂不打开
DIST_CXXLFLAGS =-L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex -lrt
BASE_OBJECTS =xTask.o xClient.o xServer.o ZoneServer.o xBalanceMgr.o
BINPATH=../../lib/libXServer/
TARGET = $(BINPATH)libXServer.a
.PHONY : all

all: debug

debug:$(TARGET) $(ZLIB_OBJECTS) $(DES_OBJECTS) $(SHA1_OBJECTS)
$(TARGET):$(BASE_OBJECTS)
	mkdir -p $(BINPATH)include/
	cp $(wildcard ./*.h) $(BINPATH)include/
	
	ar rcu $(TARGET) $(BASE_OBJECTS)
	ranlib $(TARGET)	
$(BASE_OBJECTS): %.o: %.cpp 
	$(CXX) $(DEBUG_CXXFLAGS)  $(DEBUG_CXXLFLAGS) -c -fPIC $< -o $@ 	


.PHONY: clean
clean:
	rm  -rf $(BASE_OBJECTS) $(TARGET)/*.o
	rm -rf .*.dep 
	rm -rf ${BINPATH}