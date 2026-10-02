#ifndef _espconfiguration
#define _espconfiguration

/*
 * Host shim of the node's espconfiguration.h: the node training code reads the dataset
 * through DATA_PATH, here it is the path given on the command line.
 */

extern const char *teacherclient_dataset_path;
#define DATA_PATH teacherclient_dataset_path

// buffer de leitura dados (same value as the node)
#define READ_BUFFER_SIZE 1024

#endif
