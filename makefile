all:
	g++ -o serverA serverA.cpp
	g++ -o serverB serverB.cpp
	g++ -o aws aws.cpp
	g++ -o client client.cpp