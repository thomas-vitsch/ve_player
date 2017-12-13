#ifndef VE_PLAYER_READ_FILE_H_
#define VE_PLAYER_READ_FILE_H_

int openFile(const char *file);
int closeFile(int fd);
int getNextBufferFromFile(int fp, int **buf);


#endif