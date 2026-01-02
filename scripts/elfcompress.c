/*
 * elfcompress - Compress ELF segments using external commands
 *
 * This tool compresses segments in ELF files (both 32-bit and 64-bit)
 * using a user-specified compression command. Compressed segments are
 * marked with a special flag for identification by custom loaders.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <getopt.h>
#include <elf.h>

#include "common.h"
#include "common.c"

// TODO can't use here
#ifdef CONFIG_32BIT
#define Elf_Ehdr	Elf32_Ehdr
#define Elf_Phdr	Elf32_Phdr
#define is_64bit()	false
#else
#define Elf_Ehdr	Elf64_Ehdr
#define Elf_Phdr	Elf64_Phdr
#define is_64bit()	true
#endif

/* Custom flag to mark compressed segments */
#define PF_COMPRESSED	0x01000000

struct elf_file {
	int is_64bit;
	int is_bigendian;
	uint8_t *data;
	size_t size;
	Elf_Ehdr *ehdr;
};

static const char *argv0;
static char *sh = "/bin/sh";
static int verbose;

#define log(fmt, ...) printf("%s: " fmt, argv0, ##__VA_ARGS__);

#define log_verbose(args...) do {	\
	if (verbose)			\
		log(args);		\
} while (0)

#define panic(fmt, ...) do {                                    \
	fprintf(stderr, "%s: " fmt, argv0, ##__VA_ARGS__);      \
	exit(6);                                                \
} while (0)

#define xasprintf(args...) ({           \
	char *_buf;                     \
	if (asprintf(&_buf, args) < 0)  \
	panic("asprintf: %m\n");        \
	_buf;                           \
})

/* Simplified libelf-like API */

static int elf_identify(const uint8_t *data, size_t size)
{
	if (size < EI_NIDENT)
		return -1;
	if (memcmp(data, ELFMAG, SELFMAG) != 0)
		return -1;
	return 0;
}

static int elf_begin(const char *filename, struct elf_file *elf)
{
	const char *bitstr[] = { "32", "64" };
	memset(elf, 0, sizeof(*elf));

	elf->data = read_file(filename, &elf->size);
	if (!elf->data)
		panic("Failed to read file: %m\n");

	log_verbose("%s: read %zu bytess\n", filename, elf->size);

	if (elf_identify(elf->data, elf->size) < 0)
		panic("Not a valid ELF file\n");

	elf->is_64bit = (elf->data[EI_CLASS] == ELFCLASS64);
	elf->is_bigendian = (elf->data[EI_DATA] == ELFDATA2MSB);
	elf->ehdr = (Elf_Ehdr *)elf->data;

	if (is_64bit() != elf->is_64bit) {
		panic("Can't process %s-bit ELF when compiled for %s\n",
		      bitstr[elf->is_64bit], bitstr[is_64bit()]);


	}
	log_verbose("%s: %s-bit %s-endian\n", filename,
		    bitstr[elf->is_64bit],
		    elf->is_bigendian ? "big" : "little");

	return 0;
}

static void elf_end(struct elf_file *elf)
{
	free(elf->data);
	memset(elf, 0, sizeof(*elf));
}

static int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		panic("fcntl failed: %m\n");
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Compression helpers */

static uint8_t *run_compress_cmd(const char *cmd, const uint8_t *input,
				 size_t input_size, size_t *output_size)
{
	int pipe_in[2], pipe_out[2];
	pid_t pid;
	size_t capacity = input_size, len = 0, in_off = 0;
	int status;
	posix_spawn_file_actions_t actions;
	char *argv[] = { sh, "-c", (char *)cmd, NULL };
	uint8_t *output;

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
		panic("pipe failed: %m\n");
	}

	if (set_nonblock(pipe_in[1]) || set_nonblock(pipe_out[0]))
		panic("set_nonblock: %m\n");

	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, pipe_in[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, pipe_out[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&actions, pipe_in[1]);
	posix_spawn_file_actions_addclose(&actions, pipe_out[0]);

	log_verbose("compressing %zu bytes with %s -c \"%s\"\n",
		    input_size, sh, cmd);

	if (posix_spawnp(&pid, sh, &actions, NULL, argv, environ) != 0)
		panic("posix_spawn: %m\n");

	posix_spawn_file_actions_destroy(&actions);

	/* Close unused ends in parent */
	close(pipe_in[0]);
	close(pipe_out[1]);

	output = malloc(capacity);
	if (!output)
		panic("malloc(%zu): %m\n", capacity);

	for (;;) {
		struct pollfd fds[2];
		nfds_t nfds = 0;

		fds[nfds++] = (struct pollfd) {
			.fd = pipe_in[1],
			.events = POLLOUT
		};

		fds[nfds++] = (struct pollfd) {
			.fd = pipe_out[0],
			.events = POLLIN
		};

		if (poll(fds, nfds, -1) < 0) {
			if (errno == EINTR)
				continue;
			panic("poll: %m\n");
		}

		nfds_t idx = 0;

		if (fds[idx].revents & POLLOUT) {
			ssize_t n = write(pipe_in[1],
					  input + in_off,
					  input_size - in_off);
			if (n > 0) {
				in_off += n;
				if (in_off == input_size) {
					close(pipe_in[1]);
					pipe_in[1] = -1;
					continue;
				}
			} else if (n < 0 && errno != EAGAIN) {
				panic("write: %m\n");
			}
		}
		idx++;

		if (fds[idx].revents & (POLLIN | POLLHUP)) {
			if (len == capacity) {
				capacity *= 2;
				uint8_t *tmp = realloc(output, capacity);
				if (!tmp)
					panic("realloc: %m\n");
				output = tmp;
			}

			ssize_t n = read(pipe_out[0],
					 output + len, capacity - len);
			if (n > 0) {
				len += n;
			} else if (n == 0) {
				if (pipe_in[1] >= 0)
					panic("read from input pipe terminated early\n");
				break;
			} else if (errno != EAGAIN) {
				panic("read: %m\n");
			}
		}
	}

	do {
		if (waitpid(pid, &status, WUNTRACED | WCONTINUED) < 0)
			panic("waitpid: %m\n");

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				panic("compressor exited, status=%d\n",
				      WEXITSTATUS(status));

			log_verbose("compressor exited, status=%d\n",
				    WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			panic("compressor killed by signal %d\n",
			      WTERMSIG(status));
		} else if (WIFSTOPPED(status)) {
			log_verbose("compressor stopped by signal %d\n",
				    WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) {
			log_verbose("compressor continued\n");
		}
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	log_verbose("Got %zu bytes out of compressor\n", len);

	*output_size = len;
	return output;
}


static int compress_elf(struct elf_file *elf, const char *cmd,
			  const char *output_file)
{
	Elf_Ehdr *ehdr = elf->ehdr;
	uint64_t last_start = 0;
	Elf_Phdr *phdrs;
	uint8_t *new_data;
	size_t new_size;
	size_t offset;
	int i;
	int fd;
	ssize_t ret;

	if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf_Phdr) > elf->size)
		panic("Invalid program header table\n");

	phdrs = (Elf_Phdr *)(elf->data + ehdr->e_phoff);

	/* Calculate new size estimate */
	new_size = elf->size * 2;
	new_data = malloc(new_size);
	if (!new_data)
		panic("malloc: %m\n");

	/* Copy ELF header */
	memcpy(new_data, elf->data, ehdr->e_ehsize);
	offset = ehdr->e_ehsize;

	/* Align program header table */
	offset = (offset + 7) & ~7;

	Elf_Ehdr *new_ehdr = (Elf_Ehdr *)new_data;
	new_ehdr->e_phoff = offset;
	/* Strip section headers */
	new_ehdr->e_shoff = 0;
	new_ehdr->e_shnum = 0;
	new_ehdr->e_shstrndx = 0;

	/* Reserve space for program headers */
	Elf_Phdr *new_phdrs = (Elf_Phdr *)(new_data + offset);
	offset += ehdr->e_phnum * sizeof(Elf_Phdr);

	/* Process each segment */
	for (i = 0; i < ehdr->e_phnum; i++) {
		uint8_t *compressed = NULL;
		size_t compressed_size;
		Elf_Phdr *phdr = &phdrs[i], *new_phdr = &new_phdrs[i];

		if (phdr->p_filesz && phdr->p_offset < last_start)
			panic("ELF file segment #%u not sorted: %lu >= %lu\n",
			      i, (ulong)phdr->p_offset, last_start);

		last_start = phdr->p_offset;

		*new_phdr = *phdr;

		if (phdr->p_type == PT_LOAD && phdr->p_filesz != 0) {
			compressed = run_compress_cmd(cmd, elf->data + phdr->p_offset,
						      phdr->p_filesz,
						      &compressed_size);
		}

		if (!compressed || compressed_size > phdr->p_filesz) {
			/* Copy non-LOAD or empty segments as-is */
			if (phdr->p_filesz > 0) {
				/* Expand buffer if needed */
				if (offset + phdr->p_filesz > new_size)
					panic("New size shouldn't exceed old\n");

				memcpy(new_data + offset,
				       elf->data + phdr->p_offset,
				       phdr->p_filesz);
				new_phdr->p_offset = offset;
				offset += phdr->p_filesz;
			}
			continue;
		}

		memcpy(new_data + offset, compressed, compressed_size);
		new_phdr->p_offset = offset;
		new_phdr->p_filesz = compressed_size;
		new_phdr->p_flags |= PF_COMPRESSED;

		offset += compressed_size;
		free(compressed);

		log("Segment %d: %lu -> %zu bytes\n", i,
		    (unsigned long)phdr->p_filesz, compressed_size);
	}

	/* Write output file */
	fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		panic("open output: %m\n");

	ret = write(fd, new_data, offset);
	close(fd);
	free(new_data);

	if (ret != (ssize_t)offset)
		panic("Failed to write output file\n");

	return 0;
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s -C <cmd> -o <output> <input>\n", prog);
	fprintf(stderr, "\n");
	fprintf(stderr, "  -C <cmd>     Compression command (input on stdin, output on stdout)\n");
	fprintf(stderr, "  -o <output>  Output file\n");
	fprintf(stderr, "  <input>      Input ELF file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example: %s -C 'gzip -c' -o barebox.z barebox\n", prog);
}

int main(int argc, char **argv)
{
	const char *compress_cmd = NULL;
	const char *output_file = NULL;
	const char *input_file = NULL;
	struct elf_file elf;
	char *env_sh;
	int opt;
	int ret;

	env_sh = getenv("KBUILD_SHELL");
	if (env_sh)
		sh = env_sh;

	if (getenv("KBUILD_VERBOSE"))
		verbose = 1;

	argv0 = argv[0];

	while ((opt = getopt(argc, argv, "C:o:h")) != -1) {
		switch (opt) {
		case 'C':
			compress_cmd = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Error: No input file specified\n");
		usage(argv[0]);
		return 1;
	}

	input_file = argv[optind];

	if (!compress_cmd) {
		fprintf(stderr, "Error: No compression command specified\n");
		usage(argv[0]);
		return 1;
	}

	if (!output_file) {
		fprintf(stderr, "Error: No output file specified\n");
		usage(argv[0]);
		return 1;
	}

	if (elf_begin(input_file, &elf) < 0)
		return 1;

	ret = compress_elf(&elf, compress_cmd, output_file);

	elf_end(&elf);

	if (ret)
		return ret;

	log("Successfully compressed %s -> %s\n",
	    input_file, output_file);

	return ret;
}
