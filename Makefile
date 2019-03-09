JobExecutor.o : JobExecutor.c
	@echo "Compile Project2...\n";
	gcc -c JobExecutor.c -o JobExecutor.o

TrieImplementation.o : TrieImplementation.c
	gcc -c TrieImplementation.c -o TrieImplementation.o

JobExecutor : JobExecutor.o TrieImplementation.o
	gcc JobExecutor.o TrieImplementation.o -o JobExecutor -lm

clean: 
	-rm -f *.o
	-rm -f JobExecutor
