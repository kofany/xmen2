/* Copyright (c) 2002 Maciej Freudenheim <fahren@bochnia.pl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE..
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

char *info[] = {
	"X-Men",
	"0.9.4",
	"Maciek Freudenheim",
	"fahren@bochnia.pl",
	"Maurycy Paw³owski",
	"maurycy@bofh.szczecin.pl",
	"Micha³ Lebiecki",
	"mike@from.poznan.pl",
	"\0",
};

char *reasons[] = {
	".:X-Men:. We are the children of the atom.",
	".:X-Men:. Protecting the world that fears and hates us.",
	".:X-Men:. Born with strange and amazing powers.",
	".:X-Men:. Born again .. but still hated and feared by humanity",
	".:X-Men:. Green monsters who toss tanks like snowballs.",
	".:X-Men:. Gods who command the skies.",
	".:X-Men:. How well do you know your heroes?",
	".:X-Men:. Mutation... it is the key to our evolution.",
	".:X-Men:. Believing that humanity would never accept us.",
	".:X-Men:. The world's greates heroes striking for justice.",
	"\0",
};

char *realnames[] = {
	"Despite all my rage...",
	"Love is suicide!",
	"It's the little things that kill",
	"Just ask me!",
	"What did you expect to read here?",
	"Born 2 b wild",
	"This confusion is my illusion",
	"Tell me, tell me, what's wrong with me",
	"I can't wait 'til I'm stronger",
	"Driven by hate, consumed by fear",
	"Army of Me.",
	"\0",
};

void xoruj(FILE *, FILE *, char *, char **);

int main(int argc, char **argv)
{
	FILE *fp, *fp2;

	srand((unsigned int) time(0) ^ getpid());
		
	fp = fopen("xmen.info", "w");
	fp2 = fopen("hide.info", "w");
	xoruj(fp, fp2, "info", info);
	fclose(fp); fclose(fp2);
	return;
	fp = fopen("xmen.reasons", "w");
	fp2 = fopen("hide.reasons", "w");
	xoruj(fp, fp2, "reasons", reasons);
	fclose(fp); fclose(fp2);
	fp = fopen("xmen.realnames", "w");
	fp2 = fopen("hide.realnames", "w");
	xoruj(fp, fp2, "realnames", realnames);
	fclose(fp); fclose(fp2);
	
	return 0;
}

int xrand(float c)
{
        return ((int) (c*rand()/(RAND_MAX+1.0)));
}

void xoruj(FILE *fp, FILE *fp2, char *name, char **arr)
{
	int l;
	unsigned char *p, sum, xor;
	unsigned char r_start[3];
	unsigned char r_diff[3];
	unsigned char start, diff;
	r_start[0] = 0x69; r_diff[0] = 7;
	r_start[1] = 0x66; r_diff[1] = 5;
	r_start[2] = 0x59; r_diff[2] = 11;
	r_start[3] = 0x77; r_diff[3] = 13;
	r_start[4] = 0x71; r_diff[4] = 17;
	
	l = sum = 0;
	fprintf(fp, "// xmen.%s\nstring x%s[] = {\n", name, name);
	fprintf(fp2, "// hide.%s\nchar *h%s[] = {\n", name, name);
	for (; **arr; arr++, l++, sum = 0)
	{
		start = xor = r_start[xrand(5)];
		diff = r_diff[xrand(5)];
		fprintf(fp, "\t// %s\n", *arr);
		fprintf(fp2, "\t// %s\n\t\"", *arr);
		for (p = *arr; *p; p++) {
			sum ^= *p;
			fprintf(fp2, "\\x%x", *p ^ (xor += diff));
		}
		fprintf(fp2, "\",\n");
		fprintf(fp, "\t{0, %d, %d, %d, %d},\n", strlen(*arr), start, sum, diff);
	}
	fprintf(fp2, "};\n");
	fprintf(fp, "};\n");
	fprintf(fp, "const float lx%s = %d.0;\n", name, l);
}
