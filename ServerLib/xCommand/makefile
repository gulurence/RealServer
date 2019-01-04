CC=gcc
GXX=g++
BINPATH=../../lib/xCommand
.PHONY : all

all: debug

debug:
	mkdir -p $(BINPATH)
	cp $(wildcard ./*.h) $(BINPATH)
	@echo 生成路径$(BINPATH)
	@echo ---------------生成xCommand完成---------------
.PHONY: clean
clean:
	rm  -rf ${BINPATH}
	@echo ---------------清除xCommand完成---------------