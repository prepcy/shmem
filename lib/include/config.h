#ifndef __READ_CONFIG_H__
#define __READ_CONFIG_H__

#include <stdio.h>
#include <string.h>

int get_ini_key_string(char *title,char *key,char *filename,char *val);
int put_ini_key_string(char *title,char *key,char *filename,char *val);

#endif