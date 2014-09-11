build:
	g++ -std=c++11 -c classes/Params.cpp -pthread
	g++ -std=c++11 -c classes/Amplifier.cpp -pthread
	g++ -std=c++11 -c classes/Recorder.cpp -pthread
	g++ -std=c++11 -o LiveEVM main.cpp Params.o Amplifier.o Recorder.o -pthread `pkg-config --cflags --libs opencv`
	rm -f *.o
