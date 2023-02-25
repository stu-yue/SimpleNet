all: son/son sip/sip 
common/pkt.o: common/pkt.c common/pkt.h common/constants.h
	gcc -Wall -pedantic -g -c common/pkt.c -o common/pkt.o
common/tcp.o: common/tcp.c
	gcc -Wall -pedantic -g -c common/tcp.c -o common/tcp.o
topology/topology.o: topology/topology.c 
	gcc -Wall -pedantic -g -c topology/topology.c -o topology/topology.o
son/neighbortable.o: son/neighbortable.c
	gcc -Wall -pedantic -g -c son/neighbortable.c -o son/neighbortable.o
son/son: topology/topology.o common/pkt.o common/tcp.o son/neighbortable.o son/son.c 
	gcc -Wall -pedantic -g -pthread son/son.c topology/topology.o common/pkt.o common/tcp.o son/neighbortable.o -o son/son
sip/sip: common/pkt.o common/tcp.o topology/topology.o sip/sip.c 
	gcc -Wall -pedantic -g -pthread common/pkt.o common/tcp.o topology/topology.o sip/sip.c -o sip/sip 

clean:
	rm -rf common/*.o
	rm -rf topology/*.o
	rm -rf son/*.o
	rm -rf son/son
	rm -rf sip/*.o
	rm -rf sip/sip