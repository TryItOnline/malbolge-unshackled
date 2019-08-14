/**
 * Interpreter for Malbolge Unshackled.
 *
 * Maximum rotation width is UINTMAX_MAX, which should be at least 2^64-1.
 * Malbolge Unshackled uses Unicode for I/O. This interpreter uses UTF-8
 * encoding when the program reads from stdin or writes to stdout.
 *
 * Please compile with -O3 flag.
 * 
 * 2017 Matthias Lutter.
 * Please visit <https://lutter.cc/unshackled/>
 *
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain 
 * worldwide. This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define T0 0
#define T1 1
#define T2 2

typedef struct Trits {
	int_fast8_t trit;
	struct Trits* left;
	struct Trits* right;
} Trits;

struct MemCell;

typedef struct Number {
	int_fast8_t head;
	uintmax_t width;
	Trits* tail;
	struct MemCell* memptr; // pointer to memory[Number]; 0: to be computed
	int32_t unicode; // unicode codepoint of number; -1: not an unicode codepoint; -2: to be computed
} Number;

typedef struct MemCell {
	Number* val;
	struct MemCell* next; // pointer to next memory cell (to save computation time)
} MemCell;

typedef struct MemoryTree {
	MemCell* cell;
	struct MemoryTree* child[3];
} MemoryTree;

static inline void* malloc_or_die(size_t size) {
	void* mem = malloc(size);
	if (!mem) {
		fprintf(stderr,"out of memory");
		exit(1);
	}
	return mem;
}

// step: fixed value between 4 and 12; slack: fixed value between 0 and 5
static inline uintmax_t det_growth_policy(uintmax_t new_wordwidth, uintmax_t old_rotwidth, uintmax_t step, uintmax_t slack) {
	uintmax_t ret = old_rotwidth;
	if (new_wordwidth > (old_rotwidth - slack)/2) {
		if (old_rotwidth > UINTMAX_MAX-step) {
			fprintf(stderr,"maximal supported rotation width exceeded\n");
			exit(1);
		}
		ret = old_rotwidth + step;
		if (new_wordwidth > UINTMAX_MAX/2) {
			fprintf(stderr,"maximal supported rotation width exceeded\n");
			exit(1);
		}
		uintmax_t alt = 2*new_wordwidth;
		if (alt > ret) {
			ret = alt;
		}
	}
	return ret;
}

// prob: fixed value between 0.2*RAND_MAX and 0.8*RAND_MAX; slack: fixed value between 0 and 5
static inline uintmax_t nondet_growth_policy(uintmax_t new_wordwidth, uintmax_t old_rotwidth, uintmax_t prob, uintmax_t slack) {
	uintmax_t ret = old_rotwidth;
	int change = 0;
	if (new_wordwidth > old_rotwidth/2) {
		change = 1;
	}
	if (rand() <= prob) {
		change = 1;
	}
	if (change) {
		if (new_wordwidth > UINTMAX_MAX/2) {
			fprintf(stderr,"maximal supported rotation width exceeded\n");
			exit(1);
		}
		if (2*new_wordwidth > ret) {
			ret = 2*new_wordwidth;
		}
		uintmax_t rnd = rand() % (slack+1);
		if (ret > UINTMAX_MAX-rnd) {
			fprintf(stderr,"maximal supported rotation width exceeded\n");
			exit(1);
		}
		ret += rnd;
	}
	return ret;
}

static inline void update_memptr(Number* n, MemoryTree m[]) {
	if (n->memptr) return;
	MemoryTree* cur_node = &m[n->head];
	MemCell* last_match = cur_node->cell;
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		if (cur_node->child[it->trit]) {
			cur_node = cur_node->child[it->trit];
			last_match = cur_node->cell;
		}else {
			cur_node->child[it->trit] = (MemoryTree*)malloc_or_die(sizeof(MemoryTree));
			cur_node = cur_node->child[it->trit];
			if (it->trit == n->head) {
				cur_node->cell = last_match;
			}else{
				cur_node->cell = (MemCell*)malloc_or_die(sizeof(MemCell));
				cur_node->cell->val = 0;
				cur_node->cell->next = 0;
			}
			cur_node->child[0] = 0;
			cur_node->child[1] = 0;
			cur_node->child[2] = 0;
			last_match = cur_node->cell;
		}
		it = it->left;
	}
	n->memptr = last_match;
}

/*
void print_number(FILE* f, Number* n) {
	fprintf(f,"...%c%c",'0'+(char)n->head,'0'+(char)n->head);
	int printed = 0;
	Trits* it = n->tail->right;
	for (uintmax_t i=0; i<n->width; i++) {
		if (printed || it->trit != n->head) {
			fprintf(f,"%c",'0'+(char)it->trit);
			printed = 1;
		}
		it = it->right;
	}
}
*/

static inline int is_nl(Number* n) {
	if (n->head != T2) {
		return 0;
	}
	Trits* it = n->tail->right;
	for (uintmax_t i=0; i<n->width-1; i++) {
		if (it->trit != T2) {
			return 0;
		}
		it = it->right;
	}
	if (it->trit != T1) {
		return 0;
	}
	return 1;
}

static inline Number* clone_number(Number* in){
	Number* n = (Number*)malloc_or_die(sizeof(Number));
	n->head = in->head;
	n->width = in->width;
	n->memptr = in->memptr;
	n->unicode = in->unicode;
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	Trits* it = n->tail;
	Trits* in_it = in->tail;
	for (uintmax_t i=0;i<in->width;i++) {
		it->trit = in_it->trit;
		if (i==in->width-1) {
			it->left = n->tail;
		}else{
			it->left = (Trits*)malloc_or_die(sizeof(Trits));
		}
		it->left->right = it;
		it = it->left;
		in_it = in_it->left;
	}
	return n;
}

static inline void copy_number(Number* n, Number* in) {
	// clear old trit sequence
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		Trits* tmp = it;
		it = it->left;
		free(tmp);
	}
	n->head = in->head;
	n->width = in->width;
	n->memptr = in->memptr;
	n->unicode = in->unicode;
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	it = n->tail;
	Trits* in_it = in->tail;
	for (uintmax_t i=0;i<in->width;i++) {
		it->trit = in_it->trit;
		if (i==in->width-1) {
			it->left = n->tail;
		}else{
			it->left = (Trits*)malloc_or_die(sizeof(Trits));
		}
		it->left->right = it;
		it = it->left;
		in_it = in_it->left;
	}
}

static inline void free_number(Number** ptr) {
	if (!ptr) return;
	Number* n = *ptr;
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		Trits* tmp = it;
		it = it->left;
		free(tmp);
	}
	free(n);
	(*ptr) = 0;
}

static inline void update_unicode(Number* n) {
	if (n->unicode != -2) {
		return; // no update needed
	}
	if (n->head != T0) {
		n->unicode = -1;
		return;
	}
	int32_t unicode = 0;
	int32_t factor = 1;
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		unicode += factor * it->trit;
		if (factor < 0x110000) {
			factor *= 3;
		}
		if (unicode >= 0x110000) {
			n->unicode = -1;
			return;
		}
		it = it->left;
	}
	n->unicode = unicode;
}

// unicode-character to Number*
static inline Number* to_number(int32_t symbol) {
	if (symbol < 0) {
		fprintf(stderr,"internal error: unexpected negative value\n");
		exit(1);
	}
	Number* n = (Number*)malloc_or_die(sizeof(Number));
	n->head = T0;
	n->width = 1;
	n->memptr = 0; // to be computed
	n->unicode = (symbol<0x110000?symbol:-1);
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	n->tail->trit = symbol % 3;
	Trits* it = n->tail;
	while (symbol /= 3) {
		it->left = (Trits*)malloc_or_die(sizeof(Trits));
		it->left->right = it;
		it = it->left;
		it->trit = symbol % 3;
		n->width++;
	}
	it->left = n->tail;
	n->tail->right = it;
	return n;
}

static inline Number* nl() {
	Number* n = (Number*)malloc_or_die(sizeof(Number));
	n->head = T2;
	n->width = 1;
	n->memptr = 0; // to be computed
	n->unicode = -1; // no unicode character
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	n->tail->trit = T1;
	return n;
}

static inline Number* eof() {
	Number* n = (Number*)malloc_or_die(sizeof(Number));
	n->head = T2;
	n->width = 1;
	n->memptr = 0; // to be computed
	n->unicode = -1; // no unicode character
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	n->tail->trit = T2;
	return n;
}

// correct values only for modul >= 2 and modul <= 29524
static inline int mod(Number* n, int modul) {
	int result = (29524 % modul) * (int)n->head;
	Trits* it = n->tail;
	int position = 1;
	for (uintmax_t i = 0; i<n->width; i++) {
		result += position * (((int)it->trit)+(modul-(int)n->head));
		result %= modul;
		position *= 3;
		position %= modul;
		it = it->left;
	}
	return result;
}

static inline void increment(Number* n) {
	Trits* it = n->tail;
	if (n->unicode >= 0 && n->unicode < 0x110000-1) {
		n->unicode++;
	}else{
		n->unicode = -2;
	}
	if (n->memptr) {
		n->memptr = n->memptr->next;
	}
	for (uintmax_t i=0; i<n->width; i++) {
		it->trit++;
		it->trit %= 3;
		if (it->trit != 0) return;
		it = it->left;
	}
	if (n->head == T2) {
		n->head = T0;
		return;
	}
	it = it->right;
	it->left = (Trits*)malloc_or_die(sizeof(Trits));
	it->left->right = it;
	it = it->left;
	it->trit = n->head + 1;
	it->left = n->tail;
	n->tail->right = it;
	n->width++;
}

static inline void xlat2(Number* n) {
	update_unicode(n);
	if (n->unicode < 33 || n->unicode > 126) {
		fprintf(stderr,"cannot apply xlat2\n");
		exit(1);
	}
	const char* xlat2 = "5z]&gqtyfr$(we4{WP)H-Zn,[%\\3dL+Q;>U!pJS72FhOA1C" \
			"B6v^=I_0/8|jsb9m<.TVac`uY*MK'X~xDl}REokN:#?G\"i@";
	n->unicode = (int32_t)((unsigned char)xlat2[(n->unicode-33)%94]);
	// clear old trit sequence
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		Trits* tmp = it;
		it = it->left;
		free(tmp);
	}
	n->tail = 0;
	n->width = 0;
}

// this is not done automatically to increase speed
static inline void repair_number_after_xlat2(Number* n) {
	if (n->tail != 0 && n->width != 0) {
		return;
	}
	if (n->unicode < 0) {
		return;
	}
	n->width = 1;
	// create new trit sequence
	int32_t symbol = n->unicode;
	n->tail = (Trits*)malloc_or_die(sizeof(Trits));
	n->tail->left = n->tail;
	n->tail->right = n->tail;
	n->tail->trit = symbol % 3;
	Trits* it = n->tail;
	while (symbol /= 3) {
		it->left = (Trits*)malloc_or_die(sizeof(Trits));
		it->left->right = it;
		it = it->left;
		it->trit = symbol % 3;
		n->width++;
	}
	it->left = n->tail;
	n->tail->right = it;
	n->memptr = 0;
}

static inline void rotate_r(Number* n, uintmax_t rotwidth) {
	while (n->width < rotwidth) {
		Trits* tmp = n->tail->right;
		n->tail->right = (Trits*)malloc_or_die(sizeof(Trits));
		n->tail->right->right = tmp;
		tmp->left = n->tail->right;
		n->tail->right->left = n->tail;
		n->tail->right->trit = n->head;
		n->width++;
	}
	n->tail = n->tail->left;
	n->memptr = 0; // to be computed
	n->unicode = -2; // to be computed
}

static inline uintmax_t get_real_width(Number* n) {
	uintmax_t real_width = 0;
	Trits* it = n->tail;
	for (uintmax_t i=0; i<n->width; i++) {
		if (it->trit != n->head) {
			real_width = i+1;
		}
		it = it->left;
	}
	return real_width;
}

static inline void opr(Number* a, Number* d) {
	Trits* it_a = a->tail;
	Trits* it_d = d->tail;
	const int_fast8_t OPR[] = {
			1,0,0,
			1,0,2,
			2,2,1};
	uintmax_t pos = 0;
	while (pos < a->width || pos < d->width) {
		it_a->trit = (it_d->trit = OPR[(it_a->trit%3) + 3*(it_d->trit%3)]);
		pos++;
		if (pos >= a->width && pos < d->width) {
			// insert into a
			it_a->left = (Trits*)malloc_or_die(sizeof(Trits));
			it_a->left->right = it_a;
			it_a = it_a->left;
			it_a->trit = a->head;
			it_a->left = a->tail;
			a->tail->right = it_a;
			a->width++;
		}else{
			it_a = it_a->left;
		}
		if (pos >= d->width && pos < a->width) {
			// insert into d
			it_d->left = (Trits*)malloc_or_die(sizeof(Trits));
			it_d->left->right = it_d;
			it_d = it_d->left;
			it_d->trit = d->head;
			it_d->left = d->tail;
			d->tail->right = it_d;
			d->width++;
		}else{
			it_d = it_d->left;
		}
	}
	a->head = (d->head = OPR[(a->head%3) + 3*(d->head%3)]);
	a->memptr = 0; // to be computed
	a->unicode = -2; // to be computed
	d->memptr = 0; // to be computed
	d->unicode = -2; // to be computed
}

// returns unicode code point or -1 on EOF; exit(1) on error
static inline int32_t read_utf8_character() {
	int32_t in = (int32_t)getchar();
	if (in == EOF) {
		return -1;
	}
	if ((in & 0x80) == 0) {
		return in;
	}
	if ((in & 0xE0) == 0xC0) {
		int in2 = getchar();
		if (in2 == EOF || (in2 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		return (((in & 0x1F) << 6) | (in2 & 0x3F));
	}
	if ((in & 0xF0) == 0xE0) {
		int32_t in2 = (int32_t)getchar();
		if (in2 == EOF || (in2 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		int32_t in3 = (int32_t)getchar();
		if (in3 == EOF || (in3 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		return (((in & 0x0F) << 12) | ((in2 & 0x3F) << 6) | (in3 & 0x3F));
	}
	if ((in & 0xF8) == 0xF0) {
		int32_t in2 = (int32_t)getchar();
		if (in2 == EOF || (in2 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		int32_t in3 = (int32_t)getchar();
		if (in3 == EOF || (in3 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		int32_t in4 = (int32_t)getchar();
		if (in4 == EOF || (in4 & 0xC0) != 0x80) {
			fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
			exit(1);
		}
		return (((in & 0x07) << 18) | ((in2 & 0x3F) << 12) | ((in3 & 0x3F) << 6) | (in4 & 0x3F));
	}
	fprintf(stderr,"invalid utf-8 encoding while reading from stdin\n");
	exit(1);
}

static inline void print_utf8(int32_t symbol) {
	if (symbol < 0 || symbol >= 0x110000) {
		fprintf(stderr,"invalid unicode codepoint\n");
		exit(1);
	}
	if (symbol < 0x80) {
		printf("%c",(char)symbol);
		return;
	}
	if (symbol < 0x800) {
		char first = (0xC0 | (symbol >> 6));
		char second = (0x80 | (symbol & 0x3F));
		printf("%c%c",first,second);
		return;
	}
	if (symbol < 0x800) {
		char first = (0xC0 | (symbol >> 6));
		char second = (0x80 | (symbol & 0x3F));
		printf("%c%c",first,second);
		return;
	}
	if (symbol < 0x10000) {
		char first = (0xE0 | (symbol >> 12));
		char second = (0x80 | ((symbol >> 6) & 0x3F));
		char third = (0x80 | (symbol & 0x3F));
		printf("%c%c%c",first,second,third);
		return;
	}
	char first = (0xF0 | (symbol >> 18));
	char second = (0x80 | ((symbol >> 12) & 0x3F));
	char third = (0x80 | ((symbol >> 6) & 0x3F));
	char fourth = (0x80 | (symbol & 0x3F));
	printf("%c%c%c%c",first,second,third,fourth);
}

int main(int argc, char* argv[]) {
	Number* initial_values[6];
	MemoryTree memory[3] = {{0,{0,0,0}},{0,{0,0,0}},{0,{0,0,0}}};
	for (int_fast8_t i=0;i<3;i++) {
		memory[i].cell = (MemCell*)malloc_or_die(sizeof(MemCell));
		memory[i].cell->val = 0;
		memory[i].cell->next = 0;
	}
	
	Number* a = to_number(0);
	Number* c = to_number(0);
	Number* d = to_number(0);
	uintmax_t max_wordwidth = 0;
	srand(time(NULL));
	uintmax_t rotwidth = 10 + rand()%6;
	uintmax_t growth_slack = rand() % 6;
	uintmax_t growth_step = 4 + rand() % 9;
	uintmax_t growth_prob;
	do {
		growth_prob = rand();
	} while (growth_prob < RAND_MAX/5 || growth_prob/4 > RAND_MAX/5);
	int det_growth = rand()%2;

	unsigned int result;
	FILE* file;
	if (argc < 2) {
		// read program code from STDIN
		file = stdin;
	}else{
		file = fopen(argv[1],"rb");
	}
	if (file == NULL) {
		fprintf(stderr, "file not found: %s\n",argv[1]);
		return 1;
	}

	result = 0;
	Number* init = to_number(0);
	MemCell* prev = 0;
	MemCell* prevprev = 0;
	update_memptr(init,memory);
	int pos = 0;
	while (!feof(file)){
		int instr;
		char val;
		result = fread(&val,1,1,file);
		if (result > 1)
			return 1;
		if (result == 0) {
			if (feof(file))
				break;
			else {
				fprintf(stderr, "error: input error\n");
				return 1;
			}
		}
		instr = ((int)val+pos)%94;
		if (val == ' ' || val == '\t' || val == '\r'
				|| val == '\n');
		else if (val >= 33 && val < 127 &&
				(instr == 4 || instr == 5 || instr == 23 || instr == 39
					|| instr == 40 || instr == 62 || instr == 68
					|| instr == 81)) {
			init->memptr->val = to_number(val);
			prevprev = prev;
			prev = init->memptr;
			increment(init);
			update_memptr(init,memory);
			if (!prev->next) {
				prev->next = init->memptr;
			}
			pos++;
			pos%=564;
		}else{
			fprintf(stderr, "invalid character\n");
			return 1; //invalid characters are not accepted.
		}
	}
	if (file != stdin) {
		fclose(file);
	}
	if (!prevprev) {
		fprintf(stderr, "error: not a valid Malbolge program\n");
		return 1;
	}
	pos %= 6;
	for (; pos < 18; pos++) {
		Number* m1 = clone_number(prev->val);
		Number* m2 = clone_number(prevprev->val);
		opr(m1, m2);
		if (pos < 12) {
			free_number(&m2);
		}else{
			update_unicode(m2);
			update_memptr(m2,memory);
			initial_values[pos-12] = m2;
		}
		update_unicode(m1);
		init->memptr->val = m1;
		prevprev = prev;
		prev = init->memptr;
		increment(init);
		update_memptr(init,memory);
		if (!prev->next) {
			prev->next = init->memptr;
		}
	}
	free_number(&init);

	pos = 0;
	update_memptr(c,memory);
	update_memptr(d,memory);
	int step = 1;
	while (1) {
		if (!c->memptr->val) {
			c->memptr->val = clone_number(initial_values[pos%6]);
		}
		update_unicode(c->memptr->val);
		if (c->memptr->val->unicode < 33 || c->memptr->val->unicode > 126) {
			fprintf(stderr,"error: invalid instruction in step %d\n",step);
			return 1;
		}
		switch ((c->memptr->val->unicode+pos)%94) {
			case 4: // jmp
				if (!d->memptr->val) {
					copy_number(c, initial_values[mod(d,6)]);
				}else{
					repair_number_after_xlat2(d->memptr->val);
					update_memptr(d->memptr->val,memory);
					copy_number(c, d->memptr->val);
				}
				update_memptr(c,memory);
				pos = mod(c,564);
				if (!c->memptr->val) {
					c->memptr->val = clone_number(initial_values[pos%6]);
				}
				break;
			case 5: // out
				// compare A with ...21
				if (is_nl(a)) {
					printf("\n");
				}else{
					update_unicode(a);
					print_utf8(a->unicode);
				}
				break;
			case 23: // in
			{
				int32_t in = read_utf8_character();
				free_number(&a);
				if (in == -1) {
					a = to_number(2);
					a->head = T2;
					a->unicode = -1;
				}else if (in == '\n') {
					a = to_number(1);
					a->head = T2;
				}else{
					a = to_number(in);
				}
				break;
			}
			case 39: // rot
				if (!d->memptr->val) {
					d->memptr->val = clone_number(initial_values[mod(d,6)]);
				}else{
					repair_number_after_xlat2(d->memptr->val);
				}
				rotate_r(d->memptr->val, rotwidth);
				copy_number(a,d->memptr->val);
				break;
			case 40: // movd
				if (!d->memptr->val) {
					copy_number(d,initial_values[mod(d,6)]);
				}else{
					repair_number_after_xlat2(d->memptr->val);
					update_memptr(d->memptr->val,memory);
					copy_number(d, d->memptr->val);
				}
				update_memptr(d,memory);
				// check rotwidth
				if (d->width > max_wordwidth) {
					uintmax_t w = get_real_width(d);
					if (w > max_wordwidth) {
						max_wordwidth = w;
						if (det_growth) {
							rotwidth = det_growth_policy(max_wordwidth, rotwidth, growth_step, growth_slack);
						}else{
							rotwidth = nondet_growth_policy(max_wordwidth, rotwidth, growth_prob, growth_slack);
						}
					}
				}
				break;
			case 62: // opr
				if (!d->memptr->val) {
					d->memptr->val = clone_number(initial_values[mod(d,6)]);
				}else{
					repair_number_after_xlat2(d->memptr->val);
				}
				opr(a,d->memptr->val);
				break;
			case 81: // hlt
				return 0;
			case 68:
			default: // nop
				break;
		}
		xlat2(c->memptr->val);
		prev = c->memptr;
		increment(c);
		update_memptr(c,memory);
		if (!prev->next) {
			prev->next = c->memptr;
		}
		pos++;
		pos %= 564;
		prev = d->memptr;
		increment(d);
		update_memptr(d,memory);
		if (!prev->next) {
			prev->next = d->memptr;
		}
		step++;
	}
}
