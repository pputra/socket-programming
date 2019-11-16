all:
	g++ -ggdb -o serverA serverA.cpp
	g++ -ggdb -o serverB serverB.cpp
	g++ -ggdb -o aws aws.cpp
	g++ -ggdb -o client client.cpp