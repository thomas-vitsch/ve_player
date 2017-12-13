/*
 *
 * Copyright (c) 2017,
 *      Daan Vreeken <Daan @ Vitsch . nl> - http://Vitsch.nl/
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE	(40 * 1024 * 1024)
#define BLOCK_SIZE	(128 * 1024)

uint8_t		*buffer;
uint8_t		*buffer_end;
uint8_t		*head, *tail;
long		buf_size = BUFFER_SIZE;
long		block_size = BLOCK_SIZE;
long		buf_max_full = BUFFER_SIZE - BLOCK_SIZE;
long		buf_used = 0;
int		buf_is_full = 0;
int		buf_is_empty = 0;

int		in_fd;
int		out_fd;


//#define DEBUG

#ifdef DEBUG
#define dprintf			printf
#else
#define dprintf(x, ...)		do { } while (0)
#endif


// Mutual exclusion lock.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// Consumer waits on this when the queue is empty.
pthread_mutex_t empty_lock = PTHREAD_MUTEX_INITIALIZER;
// Producer waits on this when the queue is full.
pthread_mutex_t full_lock = PTHREAD_MUTEX_INITIALIZER;



// Producer.
void *provider_loop(void *arg) {
	int ret, buf_free, bytes_before_wrapping;
	int try_to_read;

	while (1) {
		while (buf_is_full)
			pthread_mutex_lock(&full_lock);
		
		// Lock access to the queue only to quickly see how much room
		// we have left to fill.
		pthread_mutex_lock(&mutex);
		buf_free = buf_size - buf_used;
		bytes_before_wrapping = buffer_end - head;
		// Let the consumer have the queue back so we can go into a
		// blocking read() without stalling the consumer.
		pthread_mutex_unlock(&mutex);
		
		dprintf("buf_free=%d, before_wrap=%d\n", buf_free,
		    bytes_before_wrapping);
		
		// Never read past the end of the buffer.
		if (bytes_before_wrapping > buf_free)
			try_to_read = buf_free;
		else
			try_to_read = bytes_before_wrapping;
		if (try_to_read > BLOCK_SIZE)
			try_to_read = BLOCK_SIZE;
		
#ifdef TEST
		if (1) {
			int t;
			t = time(NULL);
			if ((t % 5) == 0)
				sleep(1);
		}
#endif
		
		// Read the bytes.
		ret = read(in_fd, head, try_to_read);
		dprintf("tried:%d, got:%d\n", try_to_read, ret);
		if (ret == 0) {
			// We're at the end of the file. Try to seek it back
			// to the start.
			ret = lseek(in_fd, 0, SEEK_SET);
			if (ret == -1)
				err(1, "Failed to seek back to start :(");
		}
		if (ret < 0) {
			// Read failed.
			err(1, "EEK! read() failed!");
		}
		
		// We have fresh bytes. Inform consumer.
		pthread_mutex_lock(&mutex);
		head += ret;
		if (head >= buffer_end)
			head = buffer;
		buf_used += ret;
		if (buf_used >= buf_max_full)
			buf_is_full = 1;
		if (buf_is_empty) {
			// Tell the consumer that he's got fresh data.
			buf_is_empty = 0;
			pthread_mutex_unlock(&empty_lock);
		}
		pthread_mutex_unlock(&mutex);
		
		dprintf("p: buf_used=%ld, buf_is_full=%d\n", buf_used,
		    buf_is_full);
	}

	return 0;
} 


// Consumer thread.
void *consumer_loop(void *arg) { 
	int ret, try_to_get, bytes_before_wrapping, in_buf;
	int t_now, t_report;

	t_report = 0;

	while (1) {
		while (buf_is_empty)
			pthread_mutex_lock(&empty_lock);

		// Lock access to the queue only to quickly see how much room
		// we have left to fill.
		pthread_mutex_lock(&mutex);
		if (buf_used == 0) {
			// Buffer empty. Start waiting.
			buf_is_empty = 1;
			pthread_mutex_unlock(&mutex);
			continue;
		}
		in_buf = buf_used;
		bytes_before_wrapping = buffer_end - tail;
		// Let the consumer have the queue back so we can go into a
		// blocking read() without stalling the consumer.
		pthread_mutex_unlock(&mutex);
		
		// If we make it to this point, there's data to be read.
		if (bytes_before_wrapping > in_buf)
			try_to_get = in_buf;
		else
			try_to_get = bytes_before_wrapping;
		if (try_to_get > BLOCK_SIZE)
			try_to_get = BLOCK_SIZE;
		
		// Output the bytes.
		ret = write(out_fd, tail, try_to_get);
		if (ret < 0)
			err(1, "write() failed");
		
		// Tell provider that we've emptied part of the buffer.
		pthread_mutex_lock(&mutex);
		buf_used -= ret;
		tail += ret;
		if (tail >= buffer_end)
			tail = buffer;
		if (buf_is_full) {
			// See if we've emptied enough to restart the provider.
			if (buf_used < buf_max_full) {
				// Yep. Release the beast.
				buf_is_full = 0;
				pthread_mutex_unlock(&full_lock);
			}
		}
		if (buf_used == 0) {
			buf_is_empty = 1;
		}
		in_buf = buf_used;
		pthread_mutex_unlock(&mutex);

		// Report buffer size now and then.
		t_now = time(NULL);
		if (t_now != t_report) {
			t_report = t_now;
			fprintf(stderr, "\tBuffer: %dKB full\n", in_buf /
			    1024);
		}
		
		dprintf("c: used:%d\n", buf_used);
	}

	return 0;
} 



int main(int argc, char **argv)
{
	pthread_t producer, consumer;
	char *in_filename;

	if (argc != 1 + 1)
		errx(1, "usage: %s [input filename]", argv[0]);

	in_filename = argv[1];

	in_fd = open(in_filename, O_RDONLY);
	if (in_fd == -1)
		err(1, "Failed to open '%s'", in_filename);
	// stdout
	out_fd = 1;
	
	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		err(1, "Failed to allocate memory");
	
	buffer_end = buffer + BUFFER_SIZE;
	head = tail = buffer;

	// Start producer thread.
	pthread_create(&producer, NULL, provider_loop, NULL);
	// Start consumer.
	pthread_create(&consumer, NULL, consumer_loop, NULL);

	// Main thread quits.
	pthread_exit(NULL);
	exit(0);
} 



