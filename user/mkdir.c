#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
	int i;

	if(argc < 2) {
		fprintf(stderr, "Usage: mkdir files...\n");
		procexit();
	}

	for(i = 1; i < argc; i++) {
		if(mkdir(argv[i]) < 0) {
			fprintf(stderr, "mkdir: %s failed to create\n", argv[i]);
			break;
		}
	}

	procexit();
}
