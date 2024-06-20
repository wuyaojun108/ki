src=kilo.c kilo.h
target=ki

${target}: ${src}
	$(CC) -g ${src} -o ${target} -Wall -Wextra -Wpedantic -std=c99
