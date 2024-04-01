OUTPUT=./samples
# CC=icc
CC=mpicc #gcc
build:
	$(CC) -O3 -march=native samples.c model.c ./squiggle_c/squiggle.c  ./squiggle_c/squiggle_more.c -lm -fopenmp -o $(OUTPUT)

run:
	$(OUTPUT)

save:
	$(OUTPUT) > output.txt

save-time:
	/bin/time -f "\nTime taken: %es" ./samples > output.txt 2>&1 && cat output.txt

launch:
	sbatch launch.sh
	squeue

install: 
	rm -r squiggle_c
	wget https://git.nunosempere.com/personal/squiggle.c/raw/branch/master/squiggle.c
	wget https://git.nunosempere.com/personal/squiggle.c/raw/branch/master/squiggle.h
	wget https://git.nunosempere.com/personal/squiggle.c/raw/branch/master/squiggle_more.c
	wget https://git.nunosempere.com/personal/squiggle.c/raw/branch/master/squiggle_more.h
	mkdir temp
	mv squiggle* temp
	mv temp squiggle_c

install-git:
	rm -r squiggle_c
	git clone https://git.nunosempere.com/personal/squiggle.c
	mv squiggle.c squiggle_c
	sudo rm -r squiggle_c/.git
