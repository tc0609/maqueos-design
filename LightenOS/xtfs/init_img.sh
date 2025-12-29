#!/bin/sh

cd bin
./compile.sh xtsh 
./compile.sh print
./compile.sh share
./compile.sh shmem
./compile.sh hello
./compile.sh read
./compile.sh write
./compile.sh create
./compile.sh destroy
./compile.sh sync
# new
./compile.sh kill
./compile.sh read1
./compile.sh write1
./compile.sh ls
./compile.sh filesize
./compile.sh ftype1
./compile.sh ftype2
./compile.sh ftype3
./compile.sh ftype4

dd if=/dev/zero of=xtfs.img bs=512 count=4096 2> /dev/null
../format 
../copy xtsh 1
../copy print 1
../copy share 1
../copy shmem 1
../copy hello 1
../copy read 1
../copy write 1
../copy create 1
../copy destroy 1
../copy sync 1
# new
../copy kill 1
../copy cqut.txt 2
../copy test.txt 2
../copy tzh.txt 2
../copy read1 1
../copy write1 1
../copy ls 1
../copy filesize 1
../copy ftype1 1
../copy ftype2 1
../copy ftype3 1
../copy ftype4 1
../copy frw.txt 2
../copy fr.txt 3
../copy fw.txt 4

rm -f xtsh print share shmem hello read write create destroy sync read1 write1 kill ls filesize ftype1 ftype2 ftype3 ftype4
mv xtfs.img ../../run
cd ../
