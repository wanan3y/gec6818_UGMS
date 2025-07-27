all:
	arm-linux-gcc ./src/*.c -o ./bin/Car  -I ./include/ -L ./lib/ -lpthread -lsqlite3 -ldl -ljpeg -lm