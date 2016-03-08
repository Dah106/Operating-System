java vmsim -n 8 -a opt gcc.trace >> FOUR_gcc.txt
java vmsim -n 8 -a clock gcc.trace >> FOUR_gcc.txt
java vmsim -n 8 -a nru -r 21 gcc.trace >> FOUR_gcc.txt
java vmsim -n 8 -a work -r 13 -t 5 gcc.trace >> FOUR_gcc.txt

java vmsim -n 16 -a opt gcc.trace >> FOUR_gcc.txt
java vmsim -n 16 -a clock gcc.trace >> FOUR_gcc.txt
java vmsim -n 16 -a nru -r 21 gcc.trace >> FOUR_gcc.txt
java vmsim -n 16 -a work -r 13 -t 5 gcc.trace >> FOUR_gcc.txt

java vmsim -n 32 -a opt gcc.trace >> FOUR_gcc.txt
java vmsim -n 32 -a clock gcc.trace >> FOUR_gcc.txt
java vmsim -n 32 -a nru -r 21 gcc.trace >> FOUR_gcc.txt
java vmsim -n 32 -a work -r 13 -t 5 gcc.trace >> FOUR_gcc.txt

java vmsim -n 64 -a opt gcc.trace >> FOUR_gcc.txt
java vmsim -n 64 -a clock gcc.trace >> FOUR_gcc.txt
java vmsim -n 64 -a nru -r 21 gcc.trace >> FOUR_gcc.txt
java vmsim -n 64 -a work -r 13 -t 5 gcc.trace >> FOUR_gcc.txt