all: prog2 prog2_arq prog2_gbn prog2_sr 
	@echo 'done'

prog2: prog2.c prog2.h
	gcc -Wall -Wextra -g prog2.c -o prog2
	@echo 'prog2'

prog2_arq: prog2_arq.c prog2.h
	gcc -Wall -Wextra -g prog2_arq.c -o prog2_arq
	@echo 'arq'

prog2_gbn: prog2_gbn.c prog2.h
	gcc -Wall -Wextra -g prog2_gbn.c -o prog2_gbn
	@echo 'gbn'

prog2_sr: prog2_sr.c prog2.h
	gcc -Wall -Wextra -g prog2_sr.c -o prog2_sr
	@echo 'sr'

clean:
	rm -f prog2 *~ *.swp
	@echo 'clean'
