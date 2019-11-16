all:
	g++ -std=c++0x -ggdb -o serverA serverA.cpp
	g++ -std=c++0x -ggdb -o serverB serverB.cpp
	g++ -std=c++0x -ggdb -o aws aws.cpp
	g++ -std=c++0x -ggdb -o client client.cpp