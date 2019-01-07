CC=gcc
GXX=g++
DEBUG_FLAG = -D_SQL_DEBUG_ZYX -D_ZYX_DEBUG
MYINCLUDE =-I../../lib/libXBase/include
MYLIB =-L../../lib/libXBase/ -lXBase 
DEBUG_CXXFLAGS = -I/usr/local/include/libxml2 -I/usr/include/mysql/ ${MYINCLUDE} -D_DEBUG -D_ALL_SUPER_GM $(DEBUG_FLAG) -g -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated --std=c++0x
DEBUG_CXXLFLAGS = -L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex ${MYLIB} -lrt

DIST_CXXFLAGS = -I./ -I../ -I../base -I../util -I../base/recastnavigation/Recast/Include -I../base/recastnavigation/Detour/Include -I../Command -I/usr/local/include/libxml2 -I/usr/include/mysql/ -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated -g3 #-O2 优化暂不打开
DIST_CXXLFLAGS =-L/usr/lib64/ -lX -lxml2 -lpthread -lmysqlclient -llog4cxx -lboost_regex -lrt
BASE_OBJECTS =xByteBuffer.o xNetProcessor.o xSocket.o
BINPATH=../../lib/libXNet/
TARGET = $(BINPATH)libXNet.a
CXXLFLAGS=$(DEBUG_CXXLFLAGS) 
CXXFLAGS=$(DEBUG_CXXFLAGS)
.PHONY : all

all: debug

debug:$(TARGET)
$(TARGET):$(BASE_OBJECTS)
	mkdir -p $(BINPATH)include/
	cp $(wildcard ./*.h) $(BINPATH)include/
	
	ar rcu $(TARGET) $(BASE_OBJECTS)	
	ranlib $(TARGET)	
$(BASE_OBJECTS): %.o: %.cpp 
	$(CXX) $(CXXFLAGS) -c -fPIC $< -o $@ 	


.PHONY: clean
clean:
	rm  -rf $(BASE_OBJECTS) $(TARGET)/*.o
	rm -rf .*.dep 
	rm -rf ${BINPATH}